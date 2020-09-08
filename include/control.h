/** @file
 *  @brief         Заголовочный файл модуля управления
 *  @author unnamed
 *  @date   created 2020
 *  @date   modified 2020
 *  @version 1.0
 */
#ifndef CONTROL_H
#define CONTROL_H
#include "../include/common.h"
#include "../include/configure.h"
#include "../include/utility.h"
#include "../include/mng_thread.h"
#include "../include/server.h"
#include "../include/processor.h"
#include "../include/client.h"
#include "../include/image_cproc.h"
#include "../include/image_sproc.h"

class controller_t : boost::noncopyable
{
public:
    controller_t(int argc, char *argv[]);
    virtual ~controller_t();
    void exec();
private:
    void client_connection_action();
    void millidelay(std::uint16_t _del)
    { std::this_thread::sleep_for(std::chrono::milliseconds(_del)); }
    void microdelay(std::uint16_t _del)
    { std::this_thread::sleep_for(std::chrono::microseconds(_del)); }
    appconfig_t m_ac;
    std::shared_ptr<TServer> m_pserver;
    std::shared_ptr<TClient> m_pclient;
};

#endif // CONTROL_H


