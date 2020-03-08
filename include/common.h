/** @file
 *  @brief         Заголовочный файл общих типов, макросов, констант
 *  @author unnamed
 *  @date   created 2020
 *  @date   modified 2020
 *  @version 1.0
 */
#ifndef COMMON_H
#define COMMON_H
#include <iostream>
#include <thread>
#include <chrono>
#include <future>
#include <set>
#include <map>
#include <mutex>
#include <memory>
#include <fstream>
#include <signal.h>
#include <time.h>
#include <cstdlib>
#include <any>
#include <numeric>
#include <iterator>
#include <algorithm>
#include <experimental/filesystem>
#include <functional>
#include <fontconfig/fontconfig.h>

#define BOOST_LIBRARY
#ifdef BOOST_LIBRARY
    #include <boost/asio.hpp>
    #include <boost/bind.hpp>
    #include <boost/atomic.hpp>
    #include <boost/thread.hpp>
    #include <boost/crc.hpp>
    #include <boost/numeric/conversion/converter.hpp>
    #include <boost/program_options.hpp>
    #include <boost/lockfree/queue.hpp>
    #include <boost/swap.hpp>
#endif

#include <cairo.h>

#define BUILD_DEBUG 0
#define BUILD_RELEASE 1

// Потокобезопасный логгер
#define MULTITHREAD_LOG
// Фкйковый логгер
//#define NOLOGGER_OUTPUT
// Фкйковый логгер рантайм
//#define NOLOGGER_OUTPUT_RUNTIME

//extern std::ostream& operator << (std::ostream&, const std::string&);

#if BUILDTYPE == BUILD_RELEASE
    #ifndef NDEBUG
        #define NDEBUG
    #endif
#endif
extern std::mutex m_loggermtx;
#define LOCK  std::unique_lock<std::mutex> lock(m_loggermtx);
#define OUT(str) { LOCK  std::cerr << str << std::endl; };
#if (USE_DO == 1) && (BUILDTYPE == BUILD_DEBUG)
    #define DBGOUT(str) { LOCK  std::cerr << str << std::endl; }
    #define DBGOUT2(str, num) { LOCK \
        std::cerr << str << std::to_string(num) << std::endl; }
    #define DBGOUT4(str, num, str2, num2) { LOCK \
        std::cerr << str << std::to_string(num) << str2 << std::to_string(num2) \
        << std::endl; }
    #define DBGOUT6(str, num, str2, num2, str3, num3) { LOCK \
        std::cerr << str << std::to_string(num) << str2 << std::to_string(num2) \
        << str3 << std::to_string(num3) << std::endl;}
#else
    #define DBGOUT(str) ;
    #define DBGOUT2(str, num)
    #define DBGOUT4(str, num, str2, num2)
    #define DBGOUT6(str, num, str2, num2, str3, num3)
#endif

#endif // COMMON_H
