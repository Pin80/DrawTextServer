/** @file
 *  @brief         Заголовочный файл менеджмента потока
 *  @author unnamed
 *  @date   created 2018
 *  @date   modified 2020
 *  @version 1.0
 */
#ifndef THREAD_H
#define THREAD_H

#include <cassert>
#include "../include/common.h"
#include "../include/utility.h"

//#define NOLOGSUPPORT
//#define NOWATCHDOG
#ifndef NOLOGSUPPORT
    #include "../include/error.h"
#endif

#ifndef NOWATCHDOG
#include "../include/watchdog.h"
#endif

/// Таймаут ожидания при удаления потока
#define THREAD_TIMEOUT 3000

// Unsafe operation !!!
//#define ALLOW_FORCED_THREAD_TERMINATION

#define PLATFORM_TYPE_INVALID 0
#define PLATFORM_TYPE_POSIX_NOT_LINUX 1
#define PLATFORM_TYPE_LINUX 2
#define PLATFORM_TYPE_WINDOWS_NOT_POSIX 3

#ifndef CURRENT_PLATFORM
    #ifndef CURRENT_PLATFORM_PLATFORM_TYPE_INVALID
        #define CURRENT_PLATFORM_PLATFORM_TYPE_INVALID
    #endif
#endif

#if defined(__linux)
        #include <unistd.h>
        #include <sys/types.h>
        #include <linux/unistd.h>
        #include <sys/syscall.h>
        #define CURRENT_PLATFORM PLATFORM_TYPE_LINUX
#elif _POSIX_THREADS
        #include <pthread.h>
        #define CURRENT_PLATFORM PLATFORM_TYPE_POSIX_NOT_LINUX
#elif defined(_WIN32) || defined(_WIN64)
        #include <windows.h>
        #include <tchar.h>
        #define CURRENT_PLATFORM PLATFORM_TYPE_WINDOWS_NOT_POSIX
#else
    #error "Unknown platform"
#endif
namespace errspace
{
    // when NOLOGSUPPORT commneted,
    // it should be overloaded non-template function in error.h
    template<typename T1 = void, typename T2>
    inline void show_errmsg(const char * _msg) { assert(false); }
}

namespace ns_wd
{
    // when NOWATCHDOG commneted,
    // it should be overloaded non-template function in watchdog.h
    template<typename T1 = void, typename T2>
    wdticket_t registerThread() { }
    template<typename T1 = void, typename T2>
    void unregisterThread(wdticket_t& _tj) {}
    bool reset_wd(wdticket_t& _tk);
}

/// @class threadErrors_t
/// @brief Класс исключений для менедмента потока
class threadErrors_t: public std::exception
{
public:
    enum Tcodes {error_creation, error_launch, error_procedure};
    threadErrors_t(Tcodes _codes) noexcept : m_codes(_codes) {}
    virtual ~threadErrors_t() = default;
    Tcodes getCode() const noexcept { return m_codes; }
private:
    Tcodes m_codes;
};

/// @class threadErrStatus_t
class threadErrStatus_t
{
      /// Флаг ошибки одного или нескольких потоков в приложении
      static bool m_destructor_error;
protected:
      static bool GetDestructorError()
      { return  m_destructor_error; }
      static void ResetDestructorError()
      { m_destructor_error = false; }
      static void SetDestructorError()
      { m_destructor_error = true; }
};

/// @class IThread
/// @brief Интерфейс класса менеджмента потока
class IThread
{
public:
    virtual void StartLoop() = 0;
    virtual void StopLoop() = 0;
    virtual bool isRunning() const noexcept = 0;
    virtual bool feedwd() = 0;
    virtual bool isParkingEn() const noexcept = 0;
    virtual bool isParked() const noexcept = 0;
    virtual void setParkingState(bool _state) noexcept = 0;
    virtual std::thread::native_handle_type GetThreadID() const = 0;
};

/// @struct cvwrapper_t
/// @brief Специализация класса для индвидуального старта потока
template <typename Func, class Class, bool isStatic>
struct cvwrapper_t
{
    std::uint8_t m_currnumThreads;
    std::mutex m_cvmtx;
    std::condition_variable m_cvrun;
    /// Флаг разрешения работы основного цикла потока
    std::atomic<bool> m_isEnableRunning = {false};
};

/// @struct cvwrapper_t
/// @brief Специализация класса для группового старта потоков
template <typename Func, class Class>
struct cvwrapper_t<Func, Class, true>
{
    inline static std::atomic_uchar m_currnumThreads = 0;
    inline static std::mutex m_cvmtx;
    inline static std::condition_variable m_cvrun;
    /// Флаг разрешения работы основного цикла потока
    inline static std::atomic<bool> m_isEnableRunning = {false};
};

/// @class mngthreadImp_t
/// @brief Класс-обёртка для менедмента потока (базовая часть)
template<typename Func, class Class, bool isGrouped, typename... Args>
class mngthreadImp_t : public threadErrStatus_t,
                       public IThread,
                       cvwrapper_t<Func, Class, isGrouped>
{
    using cvwrapper_t<Func, Class, isGrouped>::m_cvmtx;
    using cvwrapper_t<Func, Class, isGrouped>::m_cvrun;
    using cvwrapper_t<Func, Class, isGrouped>::m_currnumThreads;
    using cvwrapper_t<Func, Class, isGrouped>::m_isEnableRunning;
public:
    /// Deleted methodes
    mngthreadImp_t(const mngthreadImp_t&) = delete;   /// Деструктор класса
   virtual ~mngthreadImp_t();
   /// Разрешить работу основной петли(цикла) потока
   virtual void StartLoop() override
   {
        // m_numThreads = 0 - asynchronous start
        while (m_currnumThreads < m_numThreads)
        {
            std::this_thread::yield();
        }
        if (m_numThreads > 0)
        {
            std::unique_lock<std::mutex> lock(m_cvmtx);
            atomic_store(&m_isEnableRunning, true);
            m_cvrun.notify_all();
        }
        else
        {
            atomic_store(&m_isEnableRunning, true);
        }
   }
   /// Запретить работу основной петли(цикла) потока
   virtual void StopLoop() override
   {
       atomic_store(&m_isEnableRunning, false);
       atomic_store(&m_isExiting, true);
       m_cvrun.notify_all();
   }
   /// Послать сигнал потоку - припарковать поток
   /// (требуется реализация в Функции потока)
   void ParkLoop() noexcept
   {
        atomic_store(&m_isEnableParking, true);
   }
   /// Получить статус поток запущен/незапущен
   bool isActive() const noexcept
   {
       return (atomic_load(&m_active_flag) && m_active_flag_getter.valid());
   }
   /// Полуить флаг запуска основной петли потока/состояния парковки потока
   virtual bool isRunning() const noexcept override
   { return atomic_load(&m_isEnableRunning); }
   /// Получить сигнал включения парковки (опционально)
   /// (требуется реализация в Функции потока)
   virtual bool isParkingEn() const noexcept override
   { return atomic_load(&m_isEnableParking); }
   /// Полуить флаг состояния парковки (опционально)
   /// (требуется реализация в Function)
   virtual bool isParked() const noexcept override
   { return atomic_load(&m_isParked); }
   /// Полуить флаг состояния парковки (опционально)
   /// (требуется реализация в Function)
   virtual void setParkingState(bool _state) noexcept override
   { m_isParked = _state; }
   /// Проверить на наличие исключения
   bool isException() const noexcept
   {
       // The standard has no information about thread-safety
       // of std::exception_ptr
       return ((!isActive())? (m_expointer != nullptr)?true : false: false);
   }
   /// Получить указатель исключения
   std::exception_ptr getExceptionPtr() const noexcept
   {
       // The standard has no information about thread-safety
       // of std::exception_ptr
       return ((!isActive())?m_expointer:nullptr);
   }
   /// Получить платформозависимый идентификатор потока
   virtual std::thread::native_handle_type GetThreadID() const override
   {
       return m_handle;
   }
   /// Получить платформозависимый идентификатор потока
   auto GetPDThreadID() const
   {
       return m_tid;
   }
   void join()
   {
       if (!m_isDetached)
       {
           m_pthread->join();
       }
   }
   static unsigned getCoresNumber()
   { return std::thread::hardware_concurrency(); }
   virtual bool feedwd() override
   { return ns_wd::reset_wd(m_wdticket); }
protected:
   explicit mngthreadImp_t()  = default;
   /// Конструктор класса
   explicit mngthreadImp_t(Func _func,
                           Class* _obj,
                           std::exception_ptr * _eptr,
                           bool _isDetached,
                           std::uint8_t _numThreads,
                           int _sig,
                           Args... _args);

   template <typename Func2>
   explicit mngthreadImp_t(Func2 _func,
                           Class* _obj,
                           std::exception_ptr * _eptr,
                           bool _isDetached,
                           std::uint8_t _numThreads,
                           int _sig,
                           Args... _args)
   {    }
   mngthreadImp_t(const mngthreadImp_t && _tf) = delete;
   mngthreadImp_t& operator=(const mngthreadImp_t&& _tf) = delete;
   ///
   bool isExiting() const noexcept {return atomic_load(&m_isExiting);}
   /// Изменить флаг состояния активности потока
   void SetThreadeState(bool _state) noexcept
   { atomic_store(&m_active_flag, _state); }
   /// Изменить флаг состояния парковки
   /// (вызывается из Function)
   void SetParkedState(bool _state) noexcept
   { atomic_store(&m_isParked, _state); }
   /// Установить указатель исключения
   void setExceptionPtr(std::exception_ptr& _eptr) noexcept
   {
       m_expointer = _eptr;
   }
   bool isDetached() const {return m_isDetached; }
   void notifyExit()
   {
       m_active_flag_setter.set_value_at_thread_exit();
   }
   /// Функция-обертка для потока
   void threadFunction(Func _func, Class * _obj, Args... _args) noexcept ;
private:
   static constexpr auto min_timeout = 10;
   /// Флаг состояния потока активный/неактивный
   std::atomic<bool> m_active_flag = {false};
   /// Флаг/сигнал завершения потока
   std::promise<void> m_active_flag_setter;
   std::future<void> m_active_flag_getter;
   /// Флаг включения парковки потока (опционально)
   /// (перевод исполнения в безопасную точку)
   std::atomic<bool> m_isEnableParking = {false};
   /// Флаг состояния парковки: припаркован/не припаркован (опционально)
   std::atomic<bool> m_isParked = {false};
   /// Указатель на исключениев потоке
   /// [ Note: An implementation might use a reference-counted smart pointer
   /// as exception_ptr. —end     note ]
   /// 18.8.5 The default constructor of exception_ptr produces
   /// the null value of the type.
   /// For purposes of determining the presence of a data race,
   /// operations on exception_ptr
   /// objects shall access and modify only the exception_ptr
   /// objects themselves
   /// and not the exceptions they refer to.
   std::exception_ptr m_expointer;
   /// Указатель на указатель на исключение в потоке
   /// Актуальное значение может быть получено
   /// извне после уничтожения объекта managedThread_t
   std::exception_ptr * m_pexpointer;
   std::atomic_bool m_isDetached = false;
   /// Указатель на объект потока
   std::thread * m_pthread = nullptr;
   /// Описатель потока (платформонезависимый)
   std::atomic<std::thread::native_handle_type> m_handle{0};
   /// Описатель потока (платформозависимый)
#if (CURRENT_PLATFORM == PLATFORM_TYPE_LINUX)
    std::atomic<pid_t> m_tid = {0};
#elif (CURRENT_PLATFORM == PLATFORM_TYPE_WINDOWS_NOT_POSIX)
    HANDLE m_tid;
#else
    pthread_t m_tid; // не используется
#endif
    /// Флаг начала процесса остановки потока
    inline static std::atomic<bool> m_isExiting = {false};
    std::uint8_t m_numThreads;
    int m_sig;
    ns_wd::wdticket_t m_wdticket;
};

/// Конструктор класса
template<typename Func, class Class, bool isGrouped, typename... Args>
mngthreadImp_t<Func, Class, isGrouped, Args...>::mngthreadImp_t(Func _func,
                               Class* _obj,
                               std::exception_ptr * _eptr,
                               bool _isDetached,
                               std::uint8_t _numThreads,
                               int _sig,
                               Args... _args):
m_pexpointer(_eptr),
m_isDetached(_isDetached),
m_pthread(nullptr),
m_numThreads(_numThreads),
m_sig(_sig)
{
    const char * funcname = __FUNCTION__;
    try
    {
        typedef void (mngthreadImp_t::
                      * const TMng_func_ptr)(Func, Class*, Args... );
        static_assert(!std::is_pointer<Class>::value,
                      "Class parameter is not object pointer");
         SetThreadeState(false);
         StopLoop();
         m_isExiting = false;
         m_active_flag_getter = m_active_flag_setter.get_future();
         std::unique_ptr<std::thread> spthread;
         TMng_func_ptr func_ptr = &mngthreadImp_t<Func,
                                                  Class,
                                                  isGrouped,
                                                  Args...>::threadFunction;
         // smart pointers don't create variadic constructor's object
         auto pthread = new std::thread(func_ptr, this,
                                         _func,
                                         _obj,
                                         std::forward<Args>(_args)...);
         spthread.reset(pthread);
         if (_isDetached)
         {
            spthread->detach();
         }
         // Возможный вариант с автостартом: //StartLoop();
         // native_handle return posix id in gcc
         m_handle = spthread->native_handle();
         DBGOUT2("th: thread is created with native_handle:", m_handle);
         m_wdticket = ns_wd::registerThread();
         m_pthread =  spthread.release();
    }
    catch(...)
    {
        errspace::show_errmsg(funcname);
        throw (threadErrors_t(threadErrors_t::error_creation));
    }
}

/// Функция-обертка для потока
template <class Func, class Class, bool isGrouped, typename... Args>
void mngthreadImp_t<Func,
                    Class,
                    isGrouped,
                    Args...>::threadFunction(Func _func,
                                                    Class * _obj,
                                                    Args... _args) noexcept
{
   const char * funcname = __FUNCTION__;
   std::exception_ptr e_ptr;
   bool bisRunning = false;
#if (CURRENT_PLATFORM == PLATFORM_TYPE_LINUX)
    m_tid = syscall(SYS_gettid);
#elif (CURRENT_PLATFORM == PLATFORM_TYPE_WINDOWS_NOT_POSIX)
    m_tid = GetCurrentThread();
#elif (CURRENT_PLATFORM == PLATFORM_TYPE_POSIX_NOT_LINUX)
   m_tid = m_handle;
#endif

#ifdef  ALLOW_FORCED_THREAD_TERMINATION
    #if (CURRENT_PLATFORM == PLATFORM_TYPE_POSIX_NOT_LINUX)
       int oldtype;
       auto code = pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS,
                                         &oldtype);
       if (code != 0) throw threadErrors_t(threadErrors_t::error_launch);
    #endif
#endif
//pthread_cancel приводит к завершению приложения
//при вызове в обработчике исключения
#if (CURRENT_PLATFORM != PLATFORM_TYPE_POSIX_NOT_LINUX) || \
   !defined(ALLOW_FORCED_THREAD_TERMINATION)
   try
   {
#endif
       SetThreadeState(true);
       // Thread function return before detaching causes crash
       // in case of forced destroying.

       {
            std::unique_lock<std::mutex> lock(m_cvmtx);
            m_currnumThreads++;
            bisRunning = isRunning() || isExiting();
            static constexpr auto sleeptime = min_timeout/2;
            DBGOUT2("th: numThreads:", m_currnumThreads)
            while (!bisRunning)
            {
               if (m_numThreads > 0)
               {
                   m_cvrun.wait_for(lock,
                                    std::chrono::milliseconds(sleeptime));
               }
               bisRunning = isRunning() || isExiting();
            }
        }
        if (!isExiting())
        {
            IThread * parent_this = this;
            (_obj->*_func)(parent_this, std::forward<Args>(_args)...);
        }
        SetThreadeState(false);
        notifyExit();
        m_currnumThreads--;
#if (CURRENT_PLATFORM != PLATFORM_TYPE_POSIX_NOT_LINUX)
   }
   catch([[maybe_unused]]const threadErrors_t& _e)
   {
        errspace::show_errmsg(funcname);
        e_ptr = std::current_exception();
        setExceptionPtr(e_ptr);
        SetThreadeState(false);
        if (m_sig >=0)
        {
            std::raise(m_sig); // notthrow
            errspace::show_errmsg("signal from thread ex hnd was emited");
        }
        m_currnumThreads--;
   }
   catch(...)
   {
        errspace::show_errmsg(funcname);
        e_ptr = std::current_exception();
        setExceptionPtr(e_ptr);
        SetThreadeState(false);
        if (m_sig >=0)
        {
            std::raise(m_sig); // notthrow
            errspace::show_errmsg("signal from thread ex hnd was emited");
        }
        m_currnumThreads--;
   }
#endif
}

// whether value is convertable
template <auto _from, typename TTo>
constexpr bool is_conv() noexcept
{
    constexpr auto lim_max = std::numeric_limits<TTo>::max();
    constexpr auto lim_min = std::numeric_limits<TTo>::min();
    static_assert(_from < lim_max, "Value is too big for target type");
    static_assert ((_from > lim_min), "Value is below minumum target value");
    return (_from < lim_max) && (_from >= lim_min);
}

template <auto _q, auto _n>
struct const_pow
{ static constexpr auto value = _q * const_pow< _q , _n - 1>::value; };

template <auto _q>
struct const_pow<_q, 0>
{ static constexpr auto value = 1; };

template <typename Ttarget, auto _sum, auto _q, auto _n>
struct get_first_gm_member
{
    static_assert ((std::is_integral<decltype(_sum)>::value) && (_sum >= 1),
                   "Value should has an integral type" );
    static_assert(is_conv<_sum, Ttarget>(), "Illegal value, not in range");
    static constexpr auto powqn = const_pow<_n ,_q>::value;
    static constexpr auto value =  _sum*(1 - _q)/((1 - powqn)) + 1;
    static_assert(is_conv<value, Ttarget>(), "Illegal value");
    static_assert(std::clamp(value, 1, _sum) == value, "Illegal value" );
};

/// Деструктор класса
template<typename Func, class Class, bool isGrouped, typename... Args>
mngthreadImp_t<Func, Class, isGrouped, Args...>::~mngthreadImp_t()
{
   const char * funcname = __FUNCTION__;
   bool result = false;
   try
   {
       using Ttarget  = std::uint16_t;
       static constexpr Ttarget q = 2;
       static constexpr Ttarget max_attempts = 4;
       static constexpr auto timeout = std::max(min_timeout,
                                                THREAD_TIMEOUT);
       static constexpr Ttarget first_gm = get_first_gm_member<Ttarget,
                                              timeout,
                                              q,
                                              max_attempts>::value;
       ns_wd::unregisterThread(m_wdticket);
       StopLoop();
       if (m_pthread != nullptr)
       {
           result = !isActive();
           if (!result)
           {
               Ttarget attempt = 0;
               Ttarget curr_delay = first_gm;
               attempt = max_attempts;
               do
               {
                   DBGOUT2("gm1:", curr_delay);
                   m_active_flag_getter.wait_for(
                               std::chrono::milliseconds(curr_delay)
                   );
                   result = !isActive();
                   curr_delay = curr_delay*q;
                   attempt--;
               } while (attempt && (!result));
           }
           if (!result)
           {
               SetDestructorError();
               errspace::show_errmsg(funcname);
               std::this_thread::sleep_for(
                           std::chrono::milliseconds(THREAD_TIMEOUT)
                           );
               result = !isActive();
           }
           if (!result)
           {
                if (!m_isDetached)
                {
                   m_pthread->detach();
                }
                // idle priority for invalid threads
                #if ((CURRENT_PLATFORM == PLATFORM_TYPE_LINUX) || \
                (CURRENT_PLATFORM == PLATFORM_TYPE_POSIX_NOT_LINUX))
                   sched_param sp;
                   #define SHEDULER SCHED_IDLE
                   sp.sched_priority = {sched_get_priority_min(SHEDULER)};
                   auto tid = GetThreadID();
                   auto ret0 = pthread_setschedparam(tid, SHEDULER, &sp);
                   auto cstr = std::to_string(ret0).c_str();
                   errspace::show_errmsg(cstr);
                #elif (CURRENT_PLATFORM == PLATFORM_TYPE_WINDOWS_NOT_POSIX)
                    SetThreadPriority(m_tid, THREAD_PRIORITY_IDLE);
                #endif
                // unsafe forced thread's termination
                #ifdef ALLOW_FORCED_THREAD_TERMINATION
                    errspace::show_errmsg("forced thread termnation");
                    #if (CURRENT_PLATFORM == PLATFORM_TYPE_LINUX)
                        // It terminates all threads, except current thread
                        auto ret = fork();
                        if ((ret != -1 ) && (ret != 0)) abort();
                    #elif (CURRENT_PLATFORM == PLATFORM_TYPE_WINDOWS_NOT_POSIX)
                        TerminateThread(m_tid, 0);
                    #elif (CURRENT_PLATFORM == PLATFORM_TYPE_POSIX_NOT_LINUX)
                        pthread_cancel(GetThreadID());
                    #endif
                #endif
           }
           else
           {
               if (isException())
               {
                   SetDestructorError();
               }
               if (!m_isDetached)
               {
                    m_pthread->join();
               }
           }
       }
       delete m_pthread; // noexcept according to C++11 standard
       m_pthread = nullptr;
       if (m_pexpointer != nullptr)
       {
           *m_pexpointer = m_expointer;
       }
   }
   catch (...)
   {
       errspace::show_errmsg(funcname);
       if (!m_isDetached)
       {
           m_pthread->detach();
       }
       // warning !!!
       // delete with noexcept according to C++11 standard
       // if thread is joining while destroing, then
       // delete operator can provoke segfault
       delete m_pthread;
       m_pthread = nullptr;
       if (m_pexpointer != nullptr)
       {
           *m_pexpointer = m_expointer;
       }
   }
}

enum ThreadDummy {cDummy};

/// @class managedThread_t
/// @brief Класс-обёртка для менедмента потока (обеспечение управления)
template<class Func, class Class, bool isGrouped, typename... Args>
class managedThread_t:
        public mngthreadImp_t<Func, Class, isGrouped, Args...>
{

    using Base = mngthreadImp_t<Func, Class, isGrouped, Args...>;
    using Base::SetThreadeState;
    using Base::setExceptionPtr;
    using Base::isDetached;
    using Base::notifyExit;
public:
    using Base::isRunning;
    using Base::StopLoop;
    using Base::GetDestructorError;
    using Base::ResetDestructorError;
    /// Конструктор класса
    explicit managedThread_t(Func _func, Class* _obj, Args&&... _args) :
        managedThread_t(cDummy, _func, _obj, nullptr, false, 0, -1,
                       std::forward<Args>(_args)...)
    {     }

    /// Конструктор класса
    explicit managedThread_t(const ThreadDummy&,
                           Func _func,
                           Class * _obj,
                           std::exception_ptr * _eptr,
                           bool _isDetached,
                           std::uint8_t _numThreads, // 0 - async
                           int _sig,
                           Args&&... _args) :
        mngthreadImp_t<Func, Class, isGrouped, Args...>
        (_func,
         _obj,
         _eptr,
         _isDetached,
         _numThreads,
         _sig,
         std::forward<Args>(_args)...)
    {
        static_assert(std::is_member_function_pointer<Func>::value, R"(
                      Invalid thread function type
                                                         )");
        const auto isinvok = std::is_invocable<Func,
                                        Class,
                                        IThread*,
                                        Args&&...>::value;
        static_assert(isinvok, R"(Invalid thread function type )");
    }

    /// Конструктор класса
    managedThread_t(managedThread_t&&) = default;
    managedThread_t& operator=(const managedThread_t&& _tf)
    {
        mngthreadImp_t<Class, Args...>::operator=
                (std::move<const managedThread_t&&>(_tf));
    }
    /// Деструктор
    virtual ~managedThread_t() = default;
    /// Разрешаем управлять потоком из объекта функции потока
    friend Class;
private:
    managedThread_t() = delete;
    managedThread_t(const managedThread_t&) = delete;
    managedThread_t& operator=(const managedThread_t&) = delete;
};


template <class Func, class Class, typename... Args>
managedThread_t(Func, Class*, Args&&...) ->
                managedThread_t<Func, Class, false, Args...>;


#endif // MNG_THREAD_H
