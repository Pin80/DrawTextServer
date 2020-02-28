/** @file
 *  @brief         Заголовочный файл модуля сервера
 *  @author unnamed
 *  @date   created 2020
 *  @date   modified 2020
 *  @version 1.0
 */
#ifndef TSERVER_H
#define TSERVER_H
#include "../include/common.h"
#include "../include/error.h"
#include "../include/logger.h"
#include "../include/mng_thread.h"
#include "../include/server_cfg.h"
#include "../include/processor.h"

class serversocket_t;
class TServer;
using connlim_t = decltype(CONNECTION_LIMIT);
typedef std::shared_ptr<serversocket_t> ssocketPtr_t;
typedef std::uint8_t hCode_t;
typedef short int thcode_t;
/// Класс ошибок операций ввода/вывода и таймера
using error_code = boost::system::error_code;

/** @struct serverconfig_t
 *  @brief класс настроек сервера
 */
struct serverconfig_t : boost::noncopyable
{
    /// Тип указателя на обработчик запросов
    using IProcessorPtr = std::unique_ptr<IServerProcessor>;
    std::string m_addr;
    std::uint16_t m_port;
    IProcessorPtr m_proc;
    std::uint16_t m_connlimit = CONNECTION_LIMIT;
    std::uint8_t m_threadnumber = THREAD_NUMBER;
    int sig = -1;
};

/** @class IServerBase
 *  @brief Класс интерфейса сервера для "серверных сокетов" со статистикой
 */
class IServerBase : boost::noncopyable
{
public:
    /// статистика (опционально)
    enum class eLastState {success, error1, error2, last_error};
    virtual void insertSocket(const ssocketPtr_t& _ptr, thcode_t _code) = 0;
    virtual void removeSocket(const ssocketPtr_t& _ptr, thcode_t _code) = 0;
    virtual std::size_t getSockListSize(thcode_t _code) const = 0;
    virtual ~IServerBase() = default;
    /// статистика (опционально)
    std::string getHandlerStatistics();
    /// статистика (опционально)
    hCode_t getHandlerCode(const char* _funcname);
    /// статистика (опционально)
    void sendLastState(const hCode_t& _code, eLastState _state);
    /// статистика (опционально)
    thcode_t increaseConnCounter(thcode_t _code);
    /// Объект службы операций ввода/вывода
    boost::asio::io_service m_ioservice;
protected:
    virtual void clearGarbage(thcode_t _code) = 0;
private:
    /// Мьютекс для многопоточного доступа к статистике
    std::mutex m_statmtx;
    /// статистика (опционально)
    std::map<hCode_t, std::string> m_hcodename;
    /// статистика (опционально)
    static constexpr std::size_t hstat_size =
            static_cast<std::size_t>(eLastState::last_error) + 1;
    /// статистика (опционально)
    static constexpr auto max_events = 128;
    /// статистика (опционально)
    using Thstat = std::array<std::uint8_t, hstat_size>;
    /// статистика (опционально)
    std::map<hCode_t, Thstat> m_handlersStat;
    /// статистика (опционально)
    hCode_t m_lastHCode = 0;
    /// статистика (опционально)
    std::map<thcode_t, std::uint32_t> m_numconnStat;
};


/** @class serversocket_t
 *  @brief Класс клиентского сокета на стороне сервера и его окружения
 */
class serversocket_t : boost::noncopyable,
                      public std::enable_shared_from_this<serversocket_t>

{
    using io_service = boost::asio::io_service;
    using socket_t = boost::asio::ip::tcp::socket;
public:
    /// Маркер начала удаления потока или сокет
    /// пока не инициаллизирован
    /// (для холостого режима работы обработчиков)
    enum eTHCodes {noThCode = -1};
    /// Конструктор класса "серверных сокетов"
    explicit serversocket_t(io_service& _srv,
                           IServerBase* _base,
                           thcode_t _code,
                           IServerProcessor* _proc = nullptr);
    /// Деструктор класса
    virtual ~serversocket_t();
    /// Отменить все операции
    bool Cancel(bool _isthstopped  = false);
    /// Завершить соединение с клиентом
    void Close();
    /// Установить уведомитель готовности ответа
    void SetAction();
    /// Регистрация сокета
    void registersocket();
    ///
    thcode_t getThreadCode() const noexcept;
    ///
    void resetThreadCode() noexcept;
    ///
    bool isDestrStarted() const noexcept;
    ///
    virtual void do_first();
    /// Получить ссылку на сокет
    /// (небезопасная фича, но как лучше пока не придумал)
    socket_t& getSocket();
    /// Получить ссылку на сокет
    /// (небезопасная фича, но как лучше пока не придумал)
    boost::asio::deadline_timer &getTimer();
protected:
    /// Обработчик соединения
    void OnConnected(ssocketPtr_t _client, const error_code & _err);
    /// Обработчик завершения чтения без комплитера
    void OnReceived(const error_code & _err,
                    std::size_t _bytes_txfed);
    /// Обработчие завершения чтения с комплитером
    void OnReceivedBin(const error_code & _err,
                    std::size_t _bytes_txfed);
    /// Обработчик завершения передачи
    void OnSent(const error_code & _err,
                    std::size_t _bytes_txfed);
    /// Обработчик таймаута операции
    void OnConnectTimeout(const error_code & _err);
    /// Обработчик окончания отсоединения
    void OnDisconnected(const error_code & _err);
    /// Обработчик проверки готовности обработки запроса
    void OnProcessTimeout(const error_code & _err);
    /// Обработчик завершения отправки ответа о том
    /// что запрос пока не обработан
    void OnBusy(const error_code & _err);
    // Запустить операцию чтения
    virtual void do_read();
    /// Забрать ответ из обработчика запросов
    bool get_response();
    /// Забрать ответ "занят" из обработчика запросов
    virtual void do_compose_send_busy();
    /// Запустить операцию записи
    void do_write();
    /// Запустить операцию чтения бинарных данных
    virtual void do_read_bin();
    // Запустить операцию записи бинарных данных заглушка на -> do_write()
    virtual void do_write_bin();
    /// Запустить операцию ожидания обработки запроса
    virtual void do_wait_processcompl();
    /// Запустить операцию ожидания отсоединения со стороны клиента
    virtual void do_wait_disconnect();
    /// Уведомить о завершении обработки запроса
    virtual void do_notify();
private:
    //friend TServer;
    /// Указатель на интерфейс сервера
    IServerBase* m_base = nullptr;
    /// Сокет
    socket_t m_sock;
    /// Таймер ожидания завершения операции
    boost::asio::deadline_timer m_timer;
    /// Таймер ожидания обработки запроса
    boost::asio::deadline_timer m_timer2;
    // Буффер запроса (http данные) для тракта передачи с копированием
    // и без комплитера.
    // Данный тракт передачи не тестировался и может иметь ошибки
    cpconv_t::bstmbuffptr_t m_request;
    /// Буффер запроса (структурир. пакет) для тракта передачи
    /// с комплитеом на основе семантики перемещения
    upair_t m_request_bin_owner;
    /// Буффер типового ответа, о том что запрос пока не обработан.
    /// для варианта тракта передачи с комплитером на основе симантики
    /// перемещения
    cpconv_t::bstmbuffptr_t m_response_busy;
    // Буффер ответа (http данные) для тракта передачи с копированием
    // и без комплитера.
    // Данный тракт передачи не тестировался и может иметь ошибки
    // cpconv_t::bstmbuffptr_t m_response;
    /// Буффер ответа (структурир. пакет) для тракта передачи
    /// с комплитеом на основе семантики перемещения
    spair_t m_response_owner;
    /// Указатель на интерфейс обработчика запросов
    IServerProcessor * m_proc = nullptr;
    /// Указатель на комплитер
    std::unique_ptr<complwrapper_t> m_gencompl;
    /// Флаг начала удаления объекта
    /// (для холостого режима работы обработчиков)
    bool m_isdestruction_sockstarted = false;
    /// Флаг начала удаления потока
    /// (для холостого режима работы обработчиков)
    bool m_isthreadserver_stopped = false;
    /// Уникальный билетик(тикет) сокета
    ticket_t m_ticket;
    /// Код потока, связанный с сокетом
    thcode_t m_thcode = noThCode;
    /// Указатель на обработчик сигнала завершения
    /// обработки запроса
    IServerProcessor::rdyHandlerPtr_t m_handlerptr;
    /// Счётчик циклов ожидания обработки запроса
    std::uint8_t m_proctimecnt = MAX_ATTEMPTS;
    /// Спинлок для синхронизации при попытки пробуждения таймера
    /// ожидания окончания запроса
    ticketspinlock_t m_resumemtx;
};

/** @class TServer
 *  @brief Класс сервера
 */
class TServer: public IServerBase
{
    /// Тип процедуры потока
    using threadfunc_t = void (TServer::*)(IThread *, thcode_t);
    /// Тип класса потока
    using mngThread_t = managedThread_t<threadfunc_t,
                                        TServer,
                                        true,
                                        thcode_t>;
    /// Тип указатель на экземляр сервера
    typedef std::shared_ptr<TServer> TPServer;
    /// Класс автомата состояний сервера
    struct serverstate_t
    {
        enum eState:char {invalid, shutdowned, running, run};
        serverstate_t(std::uint16_t _thnumber) : m_threadsnumber(_thnumber)
        { }
        void operator <<(const eState& _newstate)
        {
            switch(_newstate)
            {
                case shutdowned: { m_state = eState::shutdowned; break; }
                case running: {  m_state = eState::running;  break; }
                case run:  {  m_state = eState::run;   break; }
                case invalid:  {  m_state = eState::invalid;   break; }
                default: { assert(false);}
            }
            counter = 0;
        }
        bool operator()(eState _newst) const
        { return m_state.load() == _newst; }
        serverstate_t& operator++(int)
        {
            counter++;
            if ((counter == m_threadsnumber) && (m_state == eState::running))
            {
                DBGOUT("switched to run")
                m_state = eState::run;
            }
            return *this;
        }
    private:
        static std::atomic_int counter;
        std::atomic_char m_state = eState::invalid;
        const std::uint16_t m_threadsnumber;
    };
    using state = serverstate_t::eState;
public:
    /// Деструктор класса
    virtual ~TServer();
    /// Создать экземпляр сервера (единственный)
    static std::shared_ptr<TServer>& CreateServer(serverconfig_t &_sc);
    /// Удалить экземпляр сервера
    static void DestroyServer();
    /// Запустить сервер
    // two step initialization for mock overriding
    virtual bool Start(const char* _addr = nullptr,
                       const std::uint16_t _port = 0);
    /// Остановить сервер
    virtual bool Stop(); //noexcept
    /// Проверить, завершилась ли работа сервера с ошибкой
    bool isError();
protected:
    /// Конструктор класса
    explicit TServer(serverconfig_t &_sc);
    /// Обёртка процедыры потока
    void process_iooperations_wrapper(IThread* _th, thcode_t _idx)
    { process_iooperations(_th, _idx);}
    /// Обработчик соединения
    void OnConnected(ssocketPtr_t _client, const error_code & _err);
    /// Процедыра потока (делает опрос операций)
    virtual void process_iooperations(IThread* _th, thcode_t _idx);
    /// Сделать разрешение имени хоста
    bool do_resolve();
    /// Запустить приём нового соединения
    virtual void do_accept(thcode_t _code);
    /// Перезапустить приём нового соединения
    void do_reaccept(ssocketPtr_t _client);
    /// Вставить указатель на "серверный сокет" в пул активных сокетов
    virtual void insertSocket(const ssocketPtr_t& _ptr, thcode_t _code) override final;
    /// Удалить указатель на "серверный сокет" из пула активных сокетов
    virtual void removeSocket(const ssocketPtr_t& _ptr, thcode_t _code) override final;
    /// Получить размер пула сокетов для потока с кодом _code
    virtual std::size_t getSockListSize(thcode_t _code) const override final;
    /// Очистить пул неактивных сокетов
    virtual void clearGarbage(thcode_t _code) override final;
private:
    /// статический указатель на объект сервера
    static TPServer psrv;
    /// Объект приёма сообщений
    boost::asio::ip::tcp::acceptor m_acceptor;
    /// Объект адреса привязки
    boost::asio::ip::tcp::endpoint m_endpoint;
    /// Текущее состояние сервера
    serverstate_t m_state;
    /// Строка адреса сервера
    std::string m_addr;
    /// Строка порта сервера
    std::uint16_t m_port;
    /// Множество открытых сокетов
    std::vector<std::set<ssocketPtr_t>> m_socketset_active;
    /// Множество использованных сокетов
    std::vector<std::set<ssocketPtr_t>> m_socketset_garbage;
    /// Мьютекс для доступа к m_socketset_active
    mutable std::recursive_mutex m_mtx_active;
    /// Мьютекс для доступа к m_mtx_garbage;
    std::recursive_mutex m_mtx_garbage;
    /// Список указателей потоков
    std::list<std::unique_ptr<mngThread_t>> m_pmngthreadpool;
    /// Умный указатель на обработчик запросов(процессор)
    const serverconfig_t::IProcessorPtr m_proc;
    /// Ограничение на число соединений
    connlim_t m_connection_limit;
    /// Мьютекс для обработчика соединений
    std::mutex m_connmtx;
    /// Флаг ошибки
    std::atomic_bool m_isError = false;
    /// Количество потоков
    const std::uint8_t m_threadnumber;
    /// Номер сигнала
    int m_sig;
};



#endif // TSERVER_H
