/** @file
 *  @brief         Заголовочный файл модуля клиента
 *  @author unnamed
 *  @date   created 2020
 *  @date   modified 2020
 *  @version 1.0
 */
#ifndef TCLIENT_H
#define TCLIENT_H

#include "../include/common.h"
//#include "../include/error.h"
#include "../include/logger.h"
#include "../include/mng_thread.h"
#include "client_cfg.h"
#include "../include/processor.h"

#ifndef NOLOGSUPPORT
    #include "../include/error.h"
#endif
/// Таймаут ожидания при удаления потока
#ifndef THREAD_TIMEOUT
    #define THREAD_TIMEOUT 250
#endif
extern "C++"
{

struct clientconfig_t
{
    clientconfig_t() = default;
    clientconfig_t(const clientconfig_t& _cc)
    {
        m_host = _cc.m_host;
        m_port = _cc.m_port;
        m_paral = _cc.m_paral;
        m_pproc = _cc.m_pproc;
        m_max_reconnattempts = _cc.m_max_reconnattempts;
        m_reconndelay = _cc.m_max_reconnattempts;
    }
    std::string m_host;
    std::uint16_t m_port;
    std::size_t m_paral = 1;
    std::shared_ptr<IProcessor> m_pproc;
    std::uint16_t m_max_reconnattempts = 10;
    std::uint16_t m_reconndelay = 3;
};

/** @class TClient
 *  @brief Класс клиентского сокета на стороне клиента
 */
class CLIENT_EXPORT TClient : public std::enable_shared_from_this<TClient>,
                                     boost::noncopyable
{
public:
    using error_code = boost::system::error_code;
    using tcp = boost::asio::ip::tcp;
    using socket_t = tcp::socket;
    using threadfunc_t = void (TClient::*)(IThread *);
    using mngthread_t = managedThread_t<threadfunc_t, TClient, false>;
    using TTimer = boost::asio::deadline_timer;
    using TSockPtr = std::unique_ptr<socket_t>;
    using bstmbuff_t = boost::asio::streambuf;
    using TBoostBufPtr = cpconv_t::bnownbuffptr_t;
    using bstmbuffptr_t = std::unique_ptr<bstmbuff_t>;
    enum class eLastState {success, error1, error2, last_error};
    typedef std::uint8_t hCode_t;
protected:
    struct clientState_t
    {
        clientState_t() = default;
        clientState_t(const clientState_t& _cs) noexcept
        { m_state.store(_cs.m_state);}
        enum EState:char {invalid, disconnected, connecting, connected};
        void operator <<(const EState& _newstate) noexcept
        {
            switch(_newstate)
            {
                case disconnected: { m_state = EState::disconnected; break; }
                case connecting:
                {
                    m_state = EState::connecting;
                    m_wasconnected = false;
                    break;
                }
                case connected:
                {
                    m_state = EState::connected;
                    m_wasconnected = true;
                    break;
                }
                default: { m_state = EState::invalid; }
            }
        }
        bool operator()(EState _newst) const noexcept
        { return m_state.load() == _newst; }
        bool wasConnected() const noexcept
        { return m_wasconnected; }
    private:
        std::atomic_char m_state = EState::invalid;
        std::atomic_bool m_wasconnected = false;
    };
    using state = clientState_t::EState;
public:
    TClient(clientconfig_t &_cc);
    ~TClient();
    /// Запустить соединения
    bool ConnectAsync();
    /// Соеденится и произвести обмен даныыми
    bool Connect();
    /// Разорвать соединение
    void do_disconnect(std::size_t idx);
    /// Получить статистику соединений
    std::string getHandlerStatistics();
    /// Проверить что все соединения завершены
    bool isWorking() const { return m_isworking; }
    /// Все сокеты успешно соединились с сервером
    bool allConnected()
    {
        auto was_connected = [](const clientState_t& _st)
        { return _st.wasConnected() == true; };
        return std::all_of(m_statepool.begin(),
                           m_statepool.end(),
                           was_connected);
    }
protected:
    /// Получить код для статистики по имени функции
    hCode_t getHandlerCode(const char *_funcname);
    /// Процедура потока (обработать операции ввода/вывода)
    void Process_iooperations(IThread*);
    void OnConnected(const error_code & _err, std::size_t _idx);
    void OnConnectTimeout(const error_code & _err, std::size_t _idx);
    void OnSent(const error_code & _err, std::size_t _idx);
    void OnReceived(const error_code & _err,
                    std::size_t _bytes_txfed,
                    std::size_t _idx);
    void OnReceivedBin(const error_code & _err,
                    std::size_t _bytes_txfed,
                    std::size_t _idx);
    void do_compose_all(const std::string& server);
    bool do_start_all_operations();
    bool do_resolve();
    void do_connect_all();
    void do_write(std::size_t _idx);
    void do_read(std::size_t _idx);
    void do_read_bin(std::size_t _idx);
    void do_showerrordetails(const error_code & _err);
    bool isError();
    void sendLastState(const hCode_t& _code, eLastState _state);
    bool refresh_buffandcomplbin(std::size_t _idx, std::size_t _txfed);
    bool refresh_buffandcompl(std::size_t _idx);
private:
    std::string m_host;
    std::uint16_t m_port;
    tcp::endpoint m_endpoint;
    boost::asio::io_service m_ioservice;
    std::unique_ptr<mngthread_t> m_pthread;
    std::atomic_bool m_do_start_all_operations = false;
    std::shared_ptr<IProcessor> m_pproc;
    std::size_t m_paral = 0;
    std::vector<TSockPtr> m_socketpool;
    std::vector<clientState_t> m_statepool;
    std::vector<std::unique_ptr<TTimer>>  m_timerpool;
    std::vector<bstmbuffptr_t> m_requestpool;
    std::vector<bstmbuffptr_t> m_responsepool;
    std::vector<upair_t> m_responsebin_ownerpool;
    std::vector<std::unique_ptr<complwrapper_t> > m_gencomplpool;

    std::mutex m_statmtx;
    std::map<hCode_t, std::string> m_hcodename;
    static constexpr std::size_t hstat_size =
            static_cast<std::size_t>(eLastState::last_error) + 1;
    static constexpr auto max_events = 128;
    using Thstat = std::array<std::uint8_t, hstat_size>;
    std::map<hCode_t, Thstat> m_handlersStat;
    hCode_t m_lastHCode = 0;
    std::vector<ticket_t> m_ticketpool;
    std::atomic_bool m_isworking = false;
};
}

#endif // TCLIENT_H
