/** @file
 *  @brief         Модуля вотчдога сервера
 *  @author unnamed
 *  @date   created 15.07.2017
 *  @date   modified 10.01.2020
 *  @version 1.0 (alpha)
 */
#include "../include/error.h"
#include "../include/watchdog.h"
// (с) http://man7.org/linux/man-pages/man2/timer_create.2.html
//#define CLOCKID CLOCK_REALTIME
#define CLOCKID CLOCK_MONOTONIC
#define SIG SIGRTMIN
#define MULTITHREAD

namespace  ns_wd
{

    // Проверить что запущено из основного потока
    class threadhndTester_t
    {
        std::thread::id handle;
        threadhndTester_t()
        {
            handle = std::this_thread::get_id();
        }
        threadhndTester_t(const threadhndTester_t&) = delete;
        threadhndTester_t(threadhndTester_t&&) = delete;
        threadhndTester_t& operator=(const threadhndTester_t&) = delete;
        threadhndTester_t& operator=(const threadhndTester_t&&) = delete;
    public:
        static threadhndTester_t& create_tester()
        {
            static threadhndTester_t tester = threadhndTester_t();
            return tester;
        }
        auto get() const { return handle; }
    };

    static threadhndTester_t& mainth_tester =
                            threadhndTester_t::create_tester();

#ifdef POSIX_TIMER
    // UNUSED CODE (ONLY FOR LEARNING)
    /*
    TSigFunc1 prev_handler1 = nullptr;
    TSigFunc2 prev_handler2 = nullptr;
    */
    TCalbackWD notify_timeout();
    void signal_handler(int _sig, siginfo_t * _si, void * _uc)
    {

        bool result = false;
        if ((_sig == SIG) && (_si))
        {
            if (_si->si_code == SI_TIMER)
            {
                auto _callback = notify_timeout();
                (_callback)?((*_callback)(_si)):void(0);
                DBGOUT("!!! caught wd valid signal")
                result = true;
            }
            else
            {
                errspace::show_errmsg("wd: caught invalid signal");
            }
        }
        if (!result)
        {
            errspace::show_errmsg("wd: caught invalid signal");
        }
// UNUSED CODE (ONLY FOR LEARNING)
/*
        if ((prev_handler1) || (prev_handler2))
        {
            errspace::show_errmsg("wd: previous found");
            if (prev_handler1) prev_handler1(_sig, _si, _uc);
            if (prev_handler2) prev_handler2(_sig);
        }
*/
    }
#endif

    class watchDogImpl_t
    {
        using error_code = boost::system::error_code;
        using io_service = boost::asio::io_service;
#ifdef MULTITHREAD
        typedef ticketspinlock_t TSpinLock;
#endif // MULTITHREAD
        typedef void (*TSigFunc1)(int, siginfo_t *, void *);
        typedef void (*TSigFunc2)(int);
    public:
        bool start_wd(std::size_t _msecs,
                      TCalbackWD _cback,
                      unsigned char _numthreads);
        void stop_wd();
        bool reset_wd(wdticket_t &_tk);

        wdticket_t registerThread();
        void unregisterThread(wdticket_t &_tk);
        static TCalbackWD notify_timeout(watchDogImpl_t * _wdobj)
        {
            isWDTimeout = true;
            if (is_wdstarted)
            {
                #ifdef MULTITHREAD
                slock_reg.lock(); // noexcept
                #endif
                auto total =_wdobj->m_totalthreads;
                TCalbackWD callback = nullptr;
                if (total > 0)
                {
                    callback = m_callback;
                }
                #ifdef MULTITHREAD
                slock_reg.unlock();
                #endif
                return callback;
            }
            else
            {
                return nullptr;
            }
        }

        static bool is_timeout()
        {
            return isWDTimeout;
        }
        ~watchDogImpl_t()
        {
            // watchdog should be stoped manually
            assert(!is_wdstarted);
        }
    #ifndef POSIX_TIMER
        void asio_handler(const error_code & _err, twdparam* _param);
        void process_wd();
    #endif
    private:
        bool reset_wd_impl(wdticket_t &_tk);
        void reset_timer();
        bool run_timer(std::size_t _msecs, TCalbackWD _cback);
        void stop_timer();
        using threadlist_t = std::array<native_handle_type, THREADS_NUMBER>;
        using TBool = std::atomic_bool;
        using threadmarklist_t = std::array<TBool, THREADS_NUMBER>;
    #ifdef POSIX_TIMER
        timer_t m_timerid;
        static constexpr auto m_error_code = -1;
    #else
        typedef boost::asio::deadline_timer TTimer;
        std::unique_ptr<TTimer> m_ptimer;
        boost::asio::io_service m_ioservice;
    #endif
        static TSpinLock slock_reg;
        static std::atomic_bool isWDTimeout;
        std::size_t msecs = 0;
        static std::atomic_bool is_wdstarted;

        enum ethstates{eInvalid = -1, eZero = 0, eOne = 1};
        // atomic is an excess possibility for current implementation
        // but can be usefull in reenterable implementation
        std::vector<atomwrapper<char>> m_thstatelist;
        std::atomic_char m_expected;
        std::atomic_char m_desired;
        std::list<unsigned char> m_ticketlist;
        unsigned char m_maxthreads;
        unsigned char m_totalthreads;
        unsigned char m_processedthreads;
        static TCalbackWD m_callback;
    };

    std::atomic_bool watchDogImpl_t::is_wdstarted = false;
    std::atomic_bool watchDogImpl_t::isWDTimeout = false;
    std::unique_ptr<watchDogImpl_t> p_wdimpl;
    watchDogImpl_t::TSpinLock watchDogImpl_t::slock_reg;
    TCalbackWD watchDogImpl_t::m_callback = nullptr;
//===========================================================================
//                    IMPLEMENTATION'S FUNCTIONS
//===========================================================================
    bool watchDogImpl_t::start_wd(std::size_t _msecs,
                                  TCalbackWD _cback,
                                  unsigned char _numthreads)
    {
    #ifdef MULTITHREAD
     std::lock_guard<ticketspinlock_t> lock(slock_reg);
    #endif
       if (is_wdstarted) return false;
       m_maxthreads = _numthreads;
       m_ticketlist.resize(m_maxthreads);
       std::iota(m_ticketlist.begin(),
                 m_ticketlist.end(),
                 0);
       m_thstatelist.resize(m_maxthreads);
       for (auto& item: m_thstatelist)
       {
           item._a = eInvalid;
       }
       m_processedthreads = 0;
       m_totalthreads = 0;
       m_expected = eZero;
       m_desired = eOne;
       return run_timer(_msecs, _cback);
    }

    bool watchDogImpl_t::reset_wd(wdticket_t& _tk)
    {
    #ifdef MULTITHREAD
         std::lock_guard<ticketspinlock_t> lock(slock_reg);
    #endif
         if (!is_wdstarted) return false;
         return reset_wd_impl(_tk);
    }

    bool watchDogImpl_t::reset_wd_impl(wdticket_t& _tk)
    {
        assert(_tk < m_maxthreads);
        auto expected = m_expected.load();
        auto desired = m_desired.load();
        auto casres = m_thstatelist[_tk]._a.compare_exchange_weak(
                                            expected,
                                            desired);
        // uncomment for detail debug
        //DBGOUT6("ticket:", _tk, "casres:", casres, "process", m_processedthreads )
        m_processedthreads += (char)(casres);
        if (m_processedthreads < m_totalthreads)
        { return casres; }
        m_desired = m_expected.exchange(m_desired);
        // uncomment for detail debug
        //DBGOUT4("exp:", m_expected, "des:", m_desired )
        m_processedthreads = 0;
        reset_timer();
        return casres;
    }

    void watchDogImpl_t::stop_wd()
    {
        DBGOUT("stop wd")
    #ifdef MULTITHREAD
        std::lock_guard<TSpinLock> lock(slock_reg);
    #endif
        if (!is_wdstarted) return;
        stop_timer();
        is_wdstarted = false;
    }

    wdticket_t watchDogImpl_t::registerThread()
    {
        assert(m_ticketlist.size() != 0);
#ifdef MULTITHREAD
    std::lock_guard<TSpinLock> lock(slock_reg);
#endif
        if (!is_wdstarted) return 0;
        auto ticket = m_ticketlist.back();
        m_ticketlist.pop_back();
        m_totalthreads++;
        m_processedthreads++;
        m_thstatelist[ticket]._a.store(m_desired);
        DBGOUT4("total", m_totalthreads, "process:", m_processedthreads )
        std::this_thread::sleep_for(std::chrono::microseconds(10));
        reset_wd_impl(ticket);
        std::this_thread::sleep_for(std::chrono::microseconds(10));
        return ticket;
    }

    void watchDogImpl_t::unregisterThread(wdticket_t& _tk)
    {
        assert(m_ticketlist.size() <= m_maxthreads);
    #ifdef MULTITHREAD
        std::lock_guard<TSpinLock> lock(slock_reg);
    #endif
        if (!is_wdstarted) return;
        m_ticketlist.push_front(_tk);
        m_totalthreads--;
        m_thstatelist[_tk]._a.store(eInvalid);
        std::this_thread::sleep_for(std::chrono::microseconds(10));
        reset_wd_impl(_tk);
        std::this_thread::sleep_for(std::chrono::microseconds(10));
    }

#ifndef POSIX_TIMER
    void watchDogImpl_t::asio_handler(const error_code & _err, twdparam* _param)
    {
#ifdef MULTITHREAD
     std::lock_guard<ticketspinlock_t> lock(slock_reg);
#endif
        using namespace boost::asio::ip;
        using namespace boost::asio;

        for (int i1 = 0; i1 < thread_id_list.size(); i1++)
        {
             if (thread_flagreset_list[i1].load(std::memory_order_acquire) == false)
             {
                errspace::show_errmsg("wd: caught valid signal");
                break;
             }
        }
        m_ptimer->expires_from_now(boost::posix_time::milliseconds(msecs));
        auto bnd = boost::bind(&watchDogImpl_t::asio_handler,
                                              this,
                                              placeholders::error,
                                              (void*)callback
                                              );
        m_ptimer->async_wait(bnd);

    }
    void watchDogImpl_t::process_wd()
    {
        m_ioservice.poll();
    }
#endif

    bool watchDogImpl_t::run_timer(std::size_t _msecs,
                                   TCalbackWD _cback)
    {
        static constexpr auto billion = 1'000'000'000;
        static constexpr auto million = 1'000'000;
        m_callback = _cback;
        msecs = _msecs;
        bool result = false;
     #ifdef POSIX_TIMER
        isWDTimeout = false;
        struct sigevent sev;
        sigset_t mask;
        struct sigaction sa;
        struct sigaction old_sa;
        // Establish handler for timer signal
        sa.sa_flags = SA_SIGINFO;
        sa.sa_sigaction = signal_handler;
        sigemptyset(&sa.sa_mask);
        if (sigaction(SIG, &sa, &old_sa) == m_error_code)
        {
            errspace::show_errmsg("wd: Failed to Establish handler");
            return result;
        }
        // UNUSED CODE (ONLY FOR LEARNING)
        /*
        prev_handler1 =  old_sa.sa_sigaction;
        prev_handler2 =  old_sa.sa_handler;
        */
        // Block timer signal temporarily
        sigemptyset(&mask);
        sigaddset(&mask, SIG);
        if (sigprocmask(SIG_SETMASK, &mask, nullptr) == m_error_code)
        {
            errspace::show_errmsg("wd: Failed to Blocking signal");
            return result;
        }

        // Create the timer
        sev.sigev_notify = SIGEV_SIGNAL;
        sev.sigev_signo = SIG;
        sev.sigev_value.sival_ptr = &m_timerid;
        struct itimerspec its;
        if (timer_create(CLOCKID, &sev, &m_timerid) == m_error_code)
        {
            errspace::show_errmsg("wd: Failed to create timer");
            return result;
        }

        // Start the timer
        long long freq_nanosecs;
        freq_nanosecs = million*_msecs;
        its.it_value.tv_sec = freq_nanosecs / billion;
        its.it_value.tv_nsec = freq_nanosecs % billion;
        its.it_interval.tv_sec = its.it_value.tv_sec;
        its.it_interval.tv_nsec = its.it_value.tv_nsec;

        if (timer_settime(m_timerid, 0, &its, nullptr) == m_error_code)
        {
            errspace::show_errmsg("wd: Failed to setup watchdog: set time");
            return result;
        }
        // Unlock the timer signal, so that timer notification can be delivered
        if (sigprocmask(SIG_UNBLOCK, &mask, nullptr) == m_error_code)
        {
            errspace::show_errmsg("wd: Failed to Unblocking signal");
            return result;
        }
     #else
        m_ioservice.reset();
        m_ptimer = std::make_unique<TTimer>(m_ioservice);
        using namespace boost::asio::ip;
        using namespace boost::asio;
        m_ptimer->expires_from_now(boost::posix_time::milliseconds(_msecs));
        auto bnd = boost::bind(&watchDogImpl_t::asio_handler,
                                              this,
                                              placeholders::error,
                                              (void*)_cback
                                              );
        m_ptimer->async_wait(bnd);
        m_ioservice.poll();
     #endif
         result = true;
         is_wdstarted = true;
         return result;
    }

    void watchDogImpl_t::reset_timer()
    {
    #ifdef POSIX_TIMER
        //nreseted = 0;
        static constexpr auto billion = 1'000'000'000;
        static constexpr auto million = 1'000'000;
        struct itimerspec its;
        long long freq_nanosecs;
        freq_nanosecs = million*msecs;
        its.it_value.tv_sec = freq_nanosecs / billion;
        its.it_value.tv_nsec = freq_nanosecs % billion;
        its.it_interval.tv_sec = its.it_value.tv_sec;
        its.it_interval.tv_nsec = its.it_value.tv_nsec;

        if (timer_settime(m_timerid, 0, &its, nullptr) == m_error_code)
        {
            errspace::show_errmsg("wd Failed to set time");
        }
        isWDTimeout = false;
    #endif
    }


    void watchDogImpl_t::stop_timer()
    {
#ifdef POSIX_TIMER
    sigset_t mask;
    struct sigaction action;
    struct itimerspec its;
    // Stop the timer
    its.it_value.tv_sec = 0;
    its.it_value.tv_nsec = 0;
    its.it_interval.tv_sec = 0;
    its.it_interval.tv_nsec = 0;
    // Block timer signal temporarily

    sigemptyset(&mask);
    sigaddset(&mask, SIG);
    if (sigprocmask(SIG_SETMASK, &mask, nullptr) == m_error_code)
    {
        errspace::show_errmsg("wd: Failed to Blocking signal");
        return;
    }


    if (timer_settime(m_timerid, 0, &its, nullptr) == m_error_code)
    {
        errspace::show_errmsg("wd: Failed to set time");
        return;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(msecs));
    errno = 0;
    action.sa_handler = SIG_DFL;
    action.sa_sigaction = nullptr;
    action.sa_flags   = SA_RESETHAND;

    if (sigaction(SIG, &action, nullptr) == m_error_code)
    {
        errspace::show_errmsg("wd: Failed to reset handler");
        return;
    }

    // Unlock the timer signal, so that timer notification
    // can be delivered

    if (sigprocmask(SIG_UNBLOCK, &mask, nullptr) == m_error_code)
    {
        errspace::show_errmsg("wd: Failed to Unblocking signal");
        return;
    }

    if (timer_delete(m_timerid) == m_error_code)
    {
        errspace::show_errmsg("wd: Failed to  delete timer");
    }
#else
    error_code ecode;
    m_ptimer->cancel(ecode);
    m_ioservice.stopped();
    m_ptimer.release();
#endif

    }

//===========================================================================
//                    API FUNCTIONS
//===========================================================================

    // Watchdog initialization should be started
    // only after main function will be called
    bool start_wd(std::size_t _msecs,
                  unsigned char _numthreads,
                  TCalbackWD _cback)
    {
    #ifdef MULTITHREAD
        //start_wd is called not in main thread
        assert((mainth_tester.get() == std::this_thread::get_id()));
    #endif
        if (!p_wdimpl)
        {
            p_wdimpl = std::make_unique<watchDogImpl_t>();
        }
        return p_wdimpl->start_wd(_msecs, _cback, _numthreads);
    }

#ifndef POSIX_TIMER
    void process_wd()
    {
    #ifdef MULTITHREAD
        // start_wd is called not in main thread
        assert((main_thread_test.get() == std::this_thread::get_id()));
    #endif
        if (p_wdimpl)
        {
            p_wdimpl->stop_wd();
        }
    }
#endif

    // Watchdog finitialization only before main will be returned
    void stop_wd()
    {
    #ifdef MULTITHREAD
        //stop_wd is called not in main thread
        assert((mainth_tester.get() == std::this_thread::get_id()));
    #endif
        if (p_wdimpl)
        {
            p_wdimpl->stop_wd();
        }
    }

    bool reset_wd(wdticket_t& _tk)
    {
        if (p_wdimpl)
        {
            return p_wdimpl->reset_wd(_tk);
        }
    }

    TCalbackWD notify_timeout()
    {
        return watchDogImpl_t::notify_timeout(p_wdimpl.get());
    }

    bool is_timeout()
    {
        if (p_wdimpl)
        {
            return watchDogImpl_t::is_timeout();
        }
        else return false;
    }

    wdticket_t registerThread()
    {
        if (p_wdimpl)
        {
            return p_wdimpl->registerThread();
        }
        return 0;
    }

    void unregisterThread(wdticket_t& _tk)
    {
        if (p_wdimpl)
        {
            return p_wdimpl->unregisterThread(_tk);
        }
    }

} // ns_wd
