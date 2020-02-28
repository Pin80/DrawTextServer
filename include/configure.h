/** @file
 *  @brief         Заголовочный файл модуля конфигурирования приложения
 *  @author unnamed
 *  @date   created 2020
 *  @date   modified 2020
 *  @version 1.0
 */
#ifndef CONFIGURE_H
#define CONFIGURE_H
#include "../include/common.h"
#include "../include/error.h"
#include "../include/server.h"
#include "../include/client.h"

#define FILEPATH  "./Resources"
static const char * fullname_iimg = FILEPATH"/img_in.png";
static const char * fullname_oimg = FILEPATH"/img_out.png";
static const char * fullname_vimg = FILEPATH"/img_val.png";

struct appconfig_t
{
    serverconfig_t m_sc;
    clientconfig_t m_cc;
    bool m_clientmode = false;
    int m_signal = SIGUSR1;
    char m_first = 'q';
};

void make_configuration(appconfig_t& _ac, int argc, char *argv[]);

#endif // CONFIGURE_H
