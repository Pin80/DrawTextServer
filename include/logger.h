/** @file
 *  @brief         Заголовочный файл модуля логгера
 *  @author unnamed
 *  @date   created 2018
 *  @date   modified 2018
 *  @version 1.0
 */
#ifndef LOGGER_H
#define LOGGER_H

#include "../include/common.h"
#include "../include/utility.h"
#include "../include/error.h"

inline std::string str_sum(const std::string& _s1, std::size_t _n)
{ return _s1 + std::to_string(_n); }
inline std::string str_sum(const std::string& _s1, const std::string& _s2)
{ return _s1 + _s2; }

#ifdef NOLOGGER_OUTPUT
class FakeLogger
{
public:
    void write_log(const char *) { };
    void pushfifo_log(const std::string&) {};
    const FakeLogger& operator << (const std::string& _msg) const
    {
        return *this;
    }
    const ILogger& operator + (const std::string& _msg) const
    {
        return *ptr;
    }
    static void start_log(bool _real = false);
    static void stop_log(bool _real = false);
    static const FakeLogger* getInstance();
};
typedef  FakeLogger ILogger;
#define LOG FakeLogger()
#else
class ILogger : boost::noncopyable
{
protected:
    ILogger() = default;
public:
    virtual void write_log(const char *) const = 0;
    virtual void pushfifo_log(const std::string&) const = 0;
    virtual ~ILogger() = default;
    static const ILogger* getInstance();
    // Optional operator in order to avoid additive conversion
    const ILogger& operator << (const char * _msg) const
    {
        const ILogger* ptr = getInstance();
        // logger is not started
        assert(ptr);
        if (ptr)
        {
            ptr->write_log(_msg);
        }
        return *ptr;
    }
    // also (std::string) may be used as (const char *)
    const ILogger& operator << (const std::string_view& _msg) const
    {
        const ILogger* ptr = getInstance();
        // logger is not started
        assert(ptr);
        if (ptr)
        {
            ptr->write_log(_msg.data());
        }
        return *ptr;
    }
    const ILogger& operator + (const std::string& _msg) const
    {
        const ILogger* ptr = getInstance();
        // logger is not started
        assert(ptr);
        if (ptr)
        {
            ptr->pushfifo_log(_msg);
        }
        return *ptr;
    }
    static void start_log(bool _real = true);
    static void stop_log(bool _real = true);
};
const ILogger* PLOG();
#define LOG *PLOG()
#endif
#endif // LOGGER_H
