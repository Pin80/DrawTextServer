/** @file
 *  @brief         Модуля вывода сообщений об ошибки
 *  @author unnamed
 *  @date   created 15.07.2017
 *  @date   modified 10.01.2020
 *  @version 1.0 (alpha)
 */
#include "../include/error.h"
#define MULTITHREAD
#include <experimental/filesystem>
#ifdef MULTITHREAD
#include <mutex>
#if defined(__GNUC__) || defined(__MINGW32__)
#define BOOST_SMT_PAUSE __asm__ __volatile__( "rep; nop" : : : "memory" );
#else
#define BOOST_SMT_PAUSE
#endif

// Like a spinlock but don't lock
class spinIgnore_t
{
    static std::atomic_bool spin_flag; // noexcept
    static std::atomic_uint mesaured_delay;
    static std::mutex mtxwd;
    static constexpr unsigned default_delay = 100;  // minumum for spinlock
    spinIgnore_t() { }
    // move constructor is deleted by below explicit declaration
    spinIgnore_t(const spinIgnore_t&) = delete;
    // move operator is deleted by below explicit declaration
    spinIgnore_t& operator=(const spinIgnore_t&) = delete;

public:
    static spinIgnore_t& create_spinlock() noexcept
    {
        static spinIgnore_t slock;
        return slock;
    }

    static bool try_lock() noexcept
    {
        constexpr auto max_attempts = 256;
        bool result = false;
        unsigned attempt_number = 0;
        bool expected = false;
        while(!result)
        {
            expected = false;
            // noexcept
            result = spin_flag.compare_exchange_weak(expected,
                                                     true,
                                                     std::memory_order_acquire);
            if (!result)
            {
                attempt_number++;
                if (attempt_number % 16 == 0) BOOST_SMT_PAUSE
                if (attempt_number == max_attempts)
                {
                    break;
                }
            }
        }
        return result;
    }

    static void unlock() noexcept
    {
        spin_flag.store(false, std::memory_order_release);
    }
};
std::atomic_bool spinIgnore_t::spin_flag = false; // noexcept
spinIgnore_t& spin = spinIgnore_t::create_spinlock();
#endif


namespace errspace
{

class errFLogger_t
{
    const char * error_log_filename = TARGET"_.errlog";
#if BUILDTYPE == BUILD_DEBUG
    static constexpr auto max_errlogfilesize = 64;
#else
    static constexpr auto max_errlogfilesize = 512;
#endif
    static FILE * logFile;
    static bool doWriteToFile;
    std::error_code ec;
    std::experimental::filesystem::path fpath;
    // move constructor is deleted by below explicit declaration
    errFLogger_t(const errFLogger_t&) = delete;
    // move operator is deleted by below explicit declaration
    errFLogger_t& operator=(const errFLogger_t&) = delete;
    errFLogger_t() noexcept
    {
        // noexcept
        namespace fs = std::experimental::filesystem;
        fpath = fs::current_path(ec) / error_log_filename;
        if (ec) return;
        // also check access
        fs::resize_file(fpath, max_errlogfilesize, ec);
        if (ec) return;
        logFile = fopen(error_log_filename, "w");
        if (logFile == nullptr)
        {
            return;
        }
        doWriteToFile = true;
    }
public:
    static errFLogger_t& CreateErrLogger() noexcept
    {
        static errFLogger_t elogger;
        return elogger;
    }
    static bool is_errorlogfile_opened() noexcept
    {
        return doWriteToFile;
    }
    // will be destroyed after return from "int main()"
    ~errFLogger_t()
    {
        // noexcept
        doWriteToFile = false;
        if (logFile)
        {
            fclose(logFile);
            logFile = nullptr;
        }
    }
    friend void show_errmsg(const char *, const unsigned char) noexcept;
};
FILE * errFLogger_t::logFile = nullptr;
bool errFLogger_t::doWriteToFile = false;
errFLogger_t& elogger = errFLogger_t::CreateErrLogger();

bool is_errorlogfile_opened() noexcept
{
    return errFLogger_t::is_errorlogfile_opened();
}

bool doShowtoDisplay = true;

void show_errmsg(const char * msg,
                 const unsigned char code) noexcept
{
#ifdef MULTITHREAD
    bool result = spin.try_lock();
    if (!result) return;
#endif
    static constexpr char prompt1[] = "Error occured in: ";
    static constexpr char rt[] = "\n";
    static constexpr char c_codes[] = {' ','1','2','3','4','5'};
    static const char prompt2[] = "with code: ";
    static constexpr auto prefix_size1 = sizeof (prompt1) + sizeof(rt);
    static constexpr auto prefix_size2 = sizeof(prompt2) +
            sizeof(rt) +
            sizeof (c_codes[0]);
    char symbol[2] = {c_codes[0],0};
    static std::size_t file_buffer_size = 0;
    symbol[0] = c_codes[(code + 1) % 5];
    if (doShowtoDisplay)
    {
        fputs(prompt1,stderr);
        fputs(msg,stderr);
        fputs(rt,stderr);
    }
    if ((errFLogger_t::doWriteToFile) && (errFLogger_t::logFile != nullptr))
    {
        file_buffer_size += prefix_size1 + std::strlen(msg);
        if (file_buffer_size >= errFLogger_t::max_errlogfilesize)
        {
    #ifdef USECYCLICERRLOG
            std::fseek(errFLogger_t::logFile, 0, SEEK_SET); // seek to start
            file_buffer_size = prefix_size1 + std::strlen(msg);
    #else
            errFLogger_t::doWriteToFile = false;
    #endif
        }
        if (errFLogger_t::doWriteToFile)
        {
            fputs(prompt1,errFLogger_t::logFile);
            fputs(msg,errFLogger_t::logFile);
            fputs(rt,errFLogger_t::logFile);
            fflush(errFLogger_t::logFile);
        }
    }
    if (code > 0)
    {
        if (doShowtoDisplay)
        {
            fputs(prompt2,stderr);
            fputs(symbol,stderr);
            fputs(rt,stderr);
        }
        if((errFLogger_t::doWriteToFile) && (errFLogger_t::logFile != nullptr))
        {
            if (file_buffer_size < errFLogger_t::max_errlogfilesize)
            {
                file_buffer_size += prefix_size2;
                if (file_buffer_size < errFLogger_t::max_errlogfilesize)
                {
                    fputs(prompt2,errFLogger_t::logFile);
                    fputs(symbol,errFLogger_t::logFile);
                    fputs(rt,errFLogger_t::logFile);
                    fflush(errFLogger_t::logFile);
                }
            }
        }
    }
#ifdef MULTITHREAD
    spin.unlock();
#endif
}


}
// Do Nothing
