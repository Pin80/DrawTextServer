/** @file
 *  @brief         Заголовочный файл модуля вотчдога сервера
 *  @author unnamed
 *  @date   created 2020
 *  @date   modified 2020
 *  @version 1.0
 */
#ifndef WATCHDOG_H
#define WATCHDOG_H
#include "../include/common.h"
#include "../include/utility.h"

namespace ns_wd
{

#define THREADS_NUMBER 10
using native_handle_type = std::thread::native_handle_type;
#define POSIX_TIMER
#ifdef POSIX_TIMER
    typedef siginfo_t twdparam;
#else
    typedef void twdparam;
#endif
typedef void (*TCalbackWD )(twdparam *si) noexcept;
typedef unsigned char wdticket_t;
bool start_wd(std::size_t _msecs,
              unsigned char _numthreads,
              TCalbackWD _cback = nullptr);
void stop_wd();
bool reset_wd(wdticket_t& _tk);
#ifndef POSIX_TIMER
void process_wd();
#endif
bool is_timeout();
wdticket_t registerThread();
void unregisterThread(wdticket_t &_th);

}
#endif // WATCHDOG_H
