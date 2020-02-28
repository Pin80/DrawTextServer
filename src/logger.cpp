/** @file
 *  @brief         Модуля логгера
 *  @author unnamed
 *  @date   created 15.07.2017
 *  @date   modified 10.01.2020
 *  @version 1.0 (alpha)
 */
#include <ctime>
#include "../include/logger.h"
//#define NOLOGSUPPORT

#ifndef NOLOGSUPPORT
    #include "../include/error.h"
#endif

static const ILogger* _PLOG = nullptr;
static std::atomic_bool is_log_started = false;

namespace errspace
{
    // when NOLOGSUPPORT commneted,
    // it should be overloaded non-template function in error.h
    template<typename T1 = void, typename T2>
    inline void show_errmsg(const char * _msg) { assert(false); }
}


class loggerImpl : boost::noncopyable
{
protected:
    static constexpr char prompt[] = "LOG:";
    static constexpr char rt[] = "\n";
public:
    static constexpr auto prefix_size = sizeof(prompt) + sizeof (rt);
    loggerImpl() = default;
    virtual void write_log(const char *, FILE* device = nullptr) const = 0;
    virtual ~loggerImpl() = default;
};

class loggerImplST_t : public loggerImpl
{
public:
    loggerImplST_t() = default;
    virtual void write_log(const char * _msg,
                           FILE* device) const noexcept override final
    {
        if (device)
        {
            fputs(prompt, device);
            fputs(_msg, device);
            fputs(rt, device);
            fflush(device);
        }
        else
        {
            // for test print: nc -lp 8080
            const char* fmt_header = "echo \"";
            const char* fmt_footer = " \" | nc -w3 127.0.0.1 8080";
            std::string str = fmt_header;
            str += _msg;
            str += fmt_footer;
            system(str.c_str());
        }
    }
};

extern "C++"
{
#ifdef MULTITHREAD_LOG
std::mutex m_loggermtx;
#endif
}

class loggerImplMT_t : public loggerImpl
{
private:
public:
    loggerImplMT_t() = default;
    virtual void write_log(const char * _msg,
                           FILE* device) const override final
    {
        const char * FUNCTION = __FUNCTION__;
        try
        {
            std::unique_lock<std::mutex> lock(m_loggermtx);
            if (device)
            {
                fputs(prompt, device);
                fputs(_msg, device);
                fputs(rt, device);
                fflush(device);
            }
            else
            {
                // AS IS CODE
                // for test print: nc -lp 8080
                const char* fmt_header = "echo \"";
                const char* fmt_footer = " \" | nc -w3 127.0.0.1 8080";
                std::string str = fmt_header;
                str += _msg;
                str += fmt_footer;
                system(str.c_str());
            }
        } catch (...)
        {
            errspace::show_errmsg(FUNCTION);
        }
    }
};

class baseLogger_t : public ILogger
{
public:
#ifdef MULTITHREAD_LOG
    static std::recursive_mutex m_mtx;
#endif
    virtual ~baseLogger_t()
    {
        // logger should be destroyed before return from main
        assert(!is_log_started);
    }
    virtual void pushfifo_log(const std::string& _str) const override final
    {
        std::unique_lock<std::recursive_mutex> lock(m_mtx);
        if (m_queue.size() < max_queue_size)
        {
            m_queue.push_back(_str);
        }
        if (m_prevlogger)
        {
            m_prevlogger->pushfifo_log(_str);
        }
    }
protected:
    static constexpr auto max_queue_size = 8;
    mutable std::list<std::string> m_queue;
    static std::unique_ptr<loggerImpl> m_pimpl;
    std::unique_ptr<baseLogger_t> m_prevlogger;
    std::string getTimeDateStr(const char * _msg) const
    {
        std::time_t result = std::time(nullptr);
        std::string str = "[";
        std::string tmpstr = ctime(&result);
        tmpstr.erase(std::remove(tmpstr.begin(), tmpstr.end(), '\n'),
                     tmpstr.end());
        str += tmpstr;
        str += "]:";
        str += _msg;
        return str;
    }
};


class consoleLogger_t : public baseLogger_t
{
public:
    consoleLogger_t()
    {
        const char * FUNCTION = __FUNCTION__;
        try
        {
#ifdef MULTITHREAD_LOG
            if (!m_pimpl) m_pimpl = std::make_unique<loggerImplMT_t>();
#else
            if (!m_pimpl) m_pimpl = std::make_unique<loggerImplST_t>();
#endif
        } catch (...)
        {
            errspace::show_errmsg(FUNCTION);
        }

    }
    consoleLogger_t(std::unique_ptr<baseLogger_t> _logger)
    {
        m_prevlogger = std::move(_logger);
    }
    virtual void write_log(const char * _msg) const override final
    {
        const char * FUNCTION = __FUNCTION__;
        try
        {
#ifdef MULTITHREAD_LOG
            std::unique_lock<std::recursive_mutex> lock(m_mtx);
#endif
            std::string fullstr;
            while(!m_queue.empty())
            {
                fullstr += m_queue.front();
                m_queue.pop_front();
            }
            fullstr += _msg;
            m_pimpl->write_log(getTimeDateStr(fullstr.c_str()).c_str(), stderr);
            if (m_prevlogger) m_prevlogger->write_log(_msg);
        } catch (...)
        {
            errspace::show_errmsg(FUNCTION);
        }
    }
    virtual ~consoleLogger_t() = default;
};

//#undef USECYCLICERRLOG

class fileLogger_t : public baseLogger_t
{
public:
    fileLogger_t()
    {
        const char * FUNCTION = __FUNCTION__;
        try
        {
#ifdef MULTITHREAD_LOG
            if (!m_pimpl) m_pimpl = std::make_unique<loggerImplMT_t>();
#else
            if (!m_pimpl) m_pimpl = std::make_unique<loggerImplST_t>();
#endif
            init();
        } catch (...)
        {
            errspace::show_errmsg(FUNCTION);
        }
    }
    fileLogger_t(std::unique_ptr<baseLogger_t> _logger)
    {
        m_prevlogger = std::move(_logger);
        init();
    }
    virtual void write_log(const char * _msg) const override final
    {
        const char * FUNCTION = __FUNCTION__;
        try
        {
#ifdef MULTITHREAD_LOG
            std::unique_lock<std::recursive_mutex> lock(m_mtx);
#endif
            if (!doWriteToFile)
            {
                if (m_prevlogger) m_prevlogger->write_log(_msg);
                return;
            }
            std::string fullstr;
            while(!m_queue.empty())
            {
                fullstr += m_queue.front();
                m_queue.pop_front();
            }
            fullstr += _msg;
            fullstr = getTimeDateStr(fullstr.c_str());
            auto psize = loggerImpl::prefix_size;
            m_file_buff_size += psize + fullstr.size();
            if (m_file_buff_size > max_logfilesize)
            {
#ifdef USECYCLICERRLOG
                std::fclose(m_filelog);
                m_filelog = fopen(m_log_filename, "w");
                if (m_filelog == nullptr)
                {
                    doWriteToFile = false;
                }
                m_file_buff_size = psize + fullstr.size();
#else
                doWriteToFile = false;
#endif
            }
            if (doWriteToFile)
            {
                m_pimpl->write_log(fullstr.c_str(), m_filelog);
            }
            if (m_prevlogger) m_prevlogger->write_log(_msg);
        } catch (...)
        {
            errspace::show_errmsg(FUNCTION);
        }
    }
    virtual ~fileLogger_t()
    {
        fclose(m_filelog);
    }
private:
    mutable FILE * m_filelog = nullptr;
    mutable bool doWriteToFile = false;
#if BUILDTYPE == BUILD_DEBUG
    static constexpr auto max_logfilesize = 256;
#else
    static constexpr auto max_logfilesize = 65536;
#endif
    static const char * m_log_filename;
    mutable std::size_t m_file_buff_size = 0;
    void init()
    {
        m_filelog = fopen(m_log_filename, "a");
        if (m_filelog == nullptr)
        {
            return;
        }
        m_file_buff_size = std::ftell(m_filelog);
        if (m_file_buff_size < max_logfilesize)
        {
            doWriteToFile = true;
        }
        else
        {
#ifdef USECYCLICERRLOG
            std::fseek(m_filelog, 0, SEEK_SET); // seek to start
            m_file_buff_size = 0;
            doWriteToFile = true;
#endif
        }
    }
};

const char * fileLogger_t::m_log_filename = TARGET"_.log";

class socketLogger_t : public baseLogger_t
{
public:
    socketLogger_t()
    {
        const char * FUNCTION = __FUNCTION__;
        try
        {
#ifdef MULTITHREAD_LOG
            if (!m_pimpl) m_pimpl = std::make_unique<loggerImplMT_t>();
#else
            if (!m_pimpl) m_pimpl = std::make_unique<loggerImplST_t>();
#endif
        doWriteToFile = true;
        } catch (...)
        {
            errspace::show_errmsg(FUNCTION);
        }
    }

    socketLogger_t(std::unique_ptr<baseLogger_t> _logger)
    {
        m_prevlogger = std::move(_logger);
        doWriteToFile = true;
    }
    virtual void write_log(const char * _msg) const override final
    {
        const char * FUNCTION = __FUNCTION__;
        try
        {
#ifdef MULTITHREAD_LOG
            std::unique_lock<std::recursive_mutex> lock(m_mtx);
#endif
            std::string fullstr;
            while(!m_queue.empty())
            {
                fullstr += m_queue.front();
                m_queue.pop_front();
            }
            fullstr += _msg;
            m_pimpl->write_log(getTimeDateStr(fullstr.c_str()).c_str());
            if (m_prevlogger) m_prevlogger->write_log(_msg);
        } catch (...)
        {
            errspace::show_errmsg(FUNCTION);
        }
    }
    virtual ~socketLogger_t() = default;
private:
    bool doWriteToFile = false;
};

class emptyLogger_t : public baseLogger_t
{
public:
    emptyLogger_t() { }
    emptyLogger_t(std::unique_ptr<baseLogger_t> _logger)  { }
    virtual void write_log(const char * _msg) const {     }
    virtual ~emptyLogger_t() = default;
};

std::unique_ptr<loggerImpl> baseLogger_t::m_pimpl;
std::recursive_mutex baseLogger_t::m_mtx;

std::unique_ptr<ILogger> getLogDecorator(bool _noreal)
{
    const char * FUNCTION = __FUNCTION__;
    try
    {
#ifdef NOLOGGER_OUTPUT_RUNTIME
        auto fklogger = std::make_unique<emptyLogger_t>();
        return fklogger;
#else
        if (_noreal)
        {
            auto fklogger = std::make_unique<emptyLogger_t>();
            return fklogger;
        }
//#if BUILDTYPE == BUILD_RELEASE
//        auto flogger = std::make_unique<fileLogger_t>();
//        return flogger;
//#else
        auto flogger = std::make_unique<fileLogger_t>();
        // only as example of code
        //auto slogger = std::make_unique<socketLogger_t>();
        auto clogger = std::make_unique<consoleLogger_t>(std::move(flogger));
        return clogger;
//#endif
#endif
    } catch (...)
    {
        errspace::show_errmsg(FUNCTION);
    }
}

const ILogger* ILogger::getInstance()
{
    // log is not started
    assert(_PLOG);
    return _PLOG;
}

void ILogger::start_log(bool _real)
{
#ifdef MULTITHREAD_LOG
            std::unique_lock<std::recursive_mutex> lock(baseLogger_t::m_mtx);
#endif
    if (!is_log_started)
    {
        _PLOG = getLogDecorator(!_real).release();
        is_log_started = true;
        LOG << "log is started";
    }
}

void ILogger::stop_log(bool _real)
{
#ifdef MULTITHREAD_LOG
            std::unique_lock<std::recursive_mutex> lock(baseLogger_t::m_mtx);
#endif
    if (is_log_started)
    {
        LOG << "log is stopped";
        is_log_started = false;
        delete _PLOG;
        _PLOG = nullptr;
    }
}

const ILogger* PLOG()
{
    // logger is not started
    assert(is_log_started);
    return _PLOG;
}
