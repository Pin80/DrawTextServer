/** @file
 *  @brief         Заголовочный файл модуля вывода сообщений  об ошибки
 *  @author unnamed
 *  @date   created 15.07.2017
 *  @date   modified 15.07.2017
 *  @version 1.0 (alpha)
 */
#ifndef ERROR_H
#define ERROR_H
#include "../include/common.h"

namespace errspace
{

#define USECYCLICERRLOG

extern const char * error_log_filename;
extern FILE * logFile;
extern bool doWriteToFile;
extern bool doShowtoDisplay;

void show_errmsg(const char * msg,
                 const unsigned char code = 0) noexcept;

bool is_errorlogfile_opened() noexcept;

}

#endif // ERROR_H
