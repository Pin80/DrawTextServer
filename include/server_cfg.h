/** @file
 *  @brief         Заголовочный файл модуля конфигурации сервера
 *  @author unnamed
 *  @date   created 2020
 *  @date   modified 2020
 *  @version 1.0
 */
#ifndef SERVER_CFG_H
#define SERVER_CFG_H
#include "../include/common.h"

// Размер буффера запроса и ответа
constexpr static auto BUFFLIMIT = 2*1024*1024;

/* Maximum queue length specifiable by listen.  */
//#define SOMAXCONN	128
constexpr auto BOOST_CONN_LIMIT = boost::asio::socket_base::max_connections;
constexpr auto CONNECTION_LIMIT = boost::asio::socket_base::max_connections;
// Таймаую чтения (и записи)
constexpr static auto TIMEOUT_READ = 20;
// Таймаут ожидания отсоединения клиента
constexpr static auto TIMEOUT_DISC = 5;
// Таймаут ожидания обработки данных
constexpr static auto TIMEOUT_PROC = 100;
// Максимальное число циклов ожидания обработки запроса
constexpr static auto MAX_ATTEMPTS = 20;
// Таймаут попытка запуска нового соединения
// при превышении лимита соединений
constexpr static auto TIMEOUT_ACCEPT = 2000;
/// Количество потоков по умолчанию
static constexpr std::uint8_t THREAD_NUMBER = 2;
#endif // SERVER_CFG_H
