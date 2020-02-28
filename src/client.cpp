#include "../include/client.h"

TClient::TClient(clientconfig_t &_cc)
    : m_host(_cc.m_host),
      m_port(_cc.m_port),
      m_pproc(_cc.m_pproc),
      m_paral(_cc.m_paral)
{
    const char * FUNCTION = __FUNCTION__;
    try
    {
        m_do_start_all_operations = false;
        m_socketpool.resize(m_paral);
        m_timerpool.resize(m_paral);
        m_requestpool.resize(m_paral);
        m_responsepool.resize(m_paral);
        m_statepool.resize(m_paral);
        m_responsebin_ownerpool.resize(m_paral);
        m_gencomplpool.resize(m_paral);
        m_ticketpool.resize(m_paral);
        for(std::size_t i= 0; i < m_paral; i++)
        {
            m_socketpool[i] = std::make_unique<socket_t>(m_ioservice);
            m_timerpool[i] = std::make_unique<TTimer>(m_ioservice);
            m_statepool[i] << state::disconnected;
        }
        auto func = &TClient::Process_iooperations;
        auto pthread = new managedThread_t{ func, this };
        m_pthread.reset(pthread);
        m_pthread->StartLoop();
        while(!isStarted())
        {
            std::this_thread::sleep_for(std::chrono::microseconds(100));
        }
    } catch (...)
    {
        errspace::show_errmsg(FUNCTION);
        throw;
    }
}

TClient::~TClient()
{
    const char * FUNCTION = __FUNCTION__;
    try
    {
         m_pthread->StopLoop();
    } catch (...)
    {
         errspace::show_errmsg(FUNCTION);
    }
}

bool TClient::ConnectAsync()
{
    const char * FUNCTION = __FUNCTION__;
    using namespace boost::asio::ip;
    using namespace boost::asio;
    bool result = false;
    error_code ec;
    try
    {
        if (!m_pproc)
        {
            LOG << "c: Client processor not found";
            return result;
        }
        // Check unclosed connection
        // Return when unclosed connection is found
        auto predic_conn = [](const clientState_t& _state )
            { return _state(state::connected);    };
        if (std::any_of(m_statepool.begin(),
                        m_statepool.end(),
                        predic_conn))
        {
            LOG << "c: Some connections is not completed";
            return result;
        }
        // Check of IP address setup.
        // Return if ip address have not been set
        if (m_endpoint.address().to_v4().to_ulong() == 0)
        {
            result = do_resolve();
            // If resolving fail then Return
            if (!result)
            {
                LOG << "Address can't be resolved";
                return result;
            }
        }

        LOG << str_sum("c: Resolved Valid Host Name:",
                       m_endpoint.address().to_string());
        // Compose requests
        do_compose_all(m_host);
        DBGOUT("c: all requests are composed")
        LOG << "c: Client is connecting";
        // Setup of status - expectation of connection
        for(auto& _state: m_statepool)
            { _state << state::connecting;  }
        m_do_start_all_operations = true;
        m_isworking = true;
        return true;
    }
    catch(...)
    {
        errspace::show_errmsg(FUNCTION);
        throw;
    }
}

bool TClient::Connect()
{
    std::uint16_t nattempts = 10;
    bool result = false;

    do
    {
        result = ConnectAsync();
        if (result)
        {
            bool is_working = isWorking();
            while (is_working)
            {
                is_working = isWorking();
                std::this_thread::sleep_for(std::chrono::microseconds(100));
            }
            result = allConnected();
            if (!result)
            {

                std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            }
        }
        nattempts--;
    } while(!result && nattempts);
    return result;
}

void TClient::Process_iooperations(IThread*)
{
    using namespace boost::asio::ip;
    using namespace boost::asio;
    using std::chrono::milliseconds;
    static constexpr auto AVOIDERRORFLOOD = 500;
    LOG << "c: client's thread's loop is started";
    error_code  err;
    std::size_t nopers = 0;
    bool isPolling = false;
    auto isrng = m_pthread->isRunning();
    auto is_all_alive = [&]() -> bool
    {
           return std::any_of(m_socketpool.begin(),
                              m_socketpool.end(),
                              [](const TSockPtr& _sock)
                        { return _sock->is_open();  });
    };
    m_isStarted = true;
    while(isrng)
    {
        // Start all operations
        if (m_do_start_all_operations)
        {
            isPolling = do_start_all_operations();
            m_do_start_all_operations = false;
        }

        if (isPolling)
        {
            const char * FUNCTION = __FUNCTION__;
            try
            {
                m_ioservice.poll(err);
            }
            catch (...)
            {
                errspace::show_errmsg(FUNCTION);
                isPolling = false;
            }
            // If io error in polling happened, then process io error
            if (err)
            {
                LOG << str_sum("c: io service error:", err.message());
                err.clear();
                // In order to avoid "log waterflood"
                std::this_thread::sleep_for(milliseconds(AVOIDERRORFLOOD));
            }
            isPolling = is_all_alive();
            if(!isPolling)
            {
                DBGOUT("c: polling is stopping")
                m_isStarted = false;
                for(auto& _ptr: m_timerpool)
                {
                    _ptr->cancel();
                }
                m_ioservice.run();
                m_ioservice.stop();
                DBGOUT("c: polling is stopped")
                m_isworking = false;
            }
        }
        isrng = m_pthread->isRunning();
    }
    m_isStarted = false;
    LOG << "c: clent's thread's loop is finished";
}

void TClient::OnConnected(const error_code & _err, std::size_t _idx)
{
    const char * FUNCTION = __FUNCTION__;
    try
    {
        DBGOUT2("c: OnConnected",_idx)
        static auto hcode = getHandlerCode(FUNCTION);
        m_timerpool[_idx]->cancel();

        if (!_err)
        {
            m_statepool[_idx] << state::connected;
            LOG << "c: connected state established";
            do_write(_idx);
        }
        else
        {
            do_showerrordetails(_err);
            LOG << "c: client is not connected";
            sendLastState(hcode, eLastState::error1);
            do_disconnect(_idx);
        }
    } catch (...)
    {
         errspace::show_errmsg(FUNCTION);
         throw;
    }
}

void TClient::OnConnectTimeout(const error_code & _err, std::size_t _idx)
{
    const char * FUNCTION = __FUNCTION__;
    try
    {
        DBGOUT2("c: OnConnectTimeout", _idx)
        static auto hcode = getHandlerCode(FUNCTION);
        if (m_statepool[_idx](state::disconnected)) return;
        if (_err.value() !=  boost::asio::error::operation_aborted)
        {
            DBGOUT2("c: timer at timeout", _idx)
            sendLastState(hcode, eLastState::error1);
            do_disconnect(_idx);
        }
        else
        {
            DBGOUT2("c: timer is aborted",_idx)
        }
    } catch (...)
    {
        errspace::show_errmsg(FUNCTION);
        throw;
    }
}

void TClient::OnSent(const error_code & _err, std::size_t _idx)
{
    const char * FUNCTION = __FUNCTION__;
    try
    {
        DBGOUT2("c: OnSent", _idx)
        static auto hcode = getHandlerCode(FUNCTION);
        if (m_statepool[_idx](state::disconnected)) return;
        m_timerpool[_idx]->cancel();
        if (_err)
        {
            do_showerrordetails(_err);
            LOG << "c: request wasn't sent";
            sendLastState(hcode, eLastState::error1);
            do_disconnect(_idx);
            return;
        }
        DBGOUT("c: ----------------------------------------------")
        LOG << "c: request was sent";
        if (!m_pproc)
        {
            do_disconnect(_idx);
            return;
        }
        if (m_pproc->getComplType() != eCompl_t::none)
        {
            do_read_bin(_idx);
        }
        else
        {
            do_read(_idx);
        }

    } catch (...)
    {
        errspace::show_errmsg(FUNCTION);
        throw;
    }
}

void TClient::OnReceived(const error_code & _err,
                std::size_t _bytes_txfed,
                std::size_t _idx)
{
    const char * FUNCTION = __FUNCTION__;
    try
    {
        DBGOUT2("c: OnReceived", _idx)
        static auto hcode = getHandlerCode(FUNCTION);
        if (m_statepool[_idx](state::disconnected)) return;
        if (_err)
        {
            m_timerpool[_idx]->cancel();
            LOG << "c: error: data was not received";
            do_showerrordetails(_err);
            sendLastState(hcode, eLastState::error1);
            do_disconnect(_idx);
            return;
        }
        m_timerpool[_idx]->cancel();
        DBGOUT2("c: received:", _bytes_txfed)
        DBGOUT2("c: data buff size:", m_responsepool[_idx]->size())
        if (m_socketpool[_idx]->available())
        {
            do_read(_idx);
            return;
        }
        LOG << "c: data receiving is compleated";
        if (m_pproc)
        {
            m_pproc->process_output_data(
                        std::move(m_responsepool[_idx]),
                        m_ticketpool[_idx]);
            //no special protocol's "busy" packet (always success)
        }
        sendLastState(hcode, eLastState::success);
        do_disconnect(_idx);
    } catch (...)
    {
        errspace::show_errmsg(FUNCTION);
        throw;
    }
}

void TClient::OnReceivedBin(const error_code & _err,
                std::size_t _bytes_txfed,
                std::size_t _idx)
{
    const char * FUNCTION = __FUNCTION__;
    try
    {
        DBGOUT2("c: OnReceivedBin", _idx)
        static auto hcode = getHandlerCode(FUNCTION);
        if (m_statepool[_idx](state::disconnected)) return;
        if ((_err) || (m_gencomplpool[_idx]->isError()))
        {
            m_timerpool[_idx]->cancel();
            LOG << "c: data was not received";
            do_showerrordetails(_err);
            sendLastState(hcode, eLastState::error1);
            do_disconnect(_idx);
            return;
        }
        m_timerpool[_idx]->cancel();
        std::size_t size = m_responsebin_ownerpool[_idx].second;
        DBGOUT2("c: received:", _bytes_txfed)
        DBGOUT2("c: data buff size:", size)


        if (m_socketpool[_idx]->available())
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
            do_read_bin(_idx);
            return;
        }

        LOG << "c: data bin receiving is completed";
        if (m_pproc)
        {
            m_responsebin_ownerpool[_idx].second = _bytes_txfed;
            bool result = m_pproc->process_output_data(
                        std::move(m_responsebin_ownerpool[_idx]),
                        m_ticketpool[_idx],
                        m_gencomplpool[_idx]->getNativeCompl());
            if (result && (!m_pproc->isAvailable(m_ticketpool[_idx])))
            {
                result = refresh_buffandcomplbin(_idx, _bytes_txfed);
                if (result)
                {
                    do_read_bin(_idx);
                    return;
                }
            }
        }
        sendLastState(hcode, eLastState::success);
        do_disconnect(_idx);
    } catch (...)
    {
        errspace::show_errmsg(FUNCTION);
        throw;
    }
}

void TClient::do_compose_all(const std::string& server)
{
    if (!m_pproc)
    {
        DBGOUT("c: processor is not found")
        return;
    }
    std::size_t idx = 0;
    for(auto& request: m_requestpool)
    {
        request = std::make_unique<bstmbuff_t>(CBUFFLIMIT);
        m_pproc->get_input_data(request, m_ticketpool[idx]);
        idx++;
        DBGOUT2("c: composed req busize:", request.get()->size())
    }
}

bool TClient::do_start_all_operations()
{
    bool result = true;
    DBGOUT("c: run all operations")
    m_ioservice.reset();
    for(auto& _ptr: m_socketpool)
    {
        _ptr->open(tcp::v4());
        result &= _ptr->is_open();
    }

    // If any one socket haven't been opened then will close all
    // and return to beginning state
    if (result)
    {
        do_connect_all();
        result = true;
    }
    else
    {
        LOG << "c: Sockets can't be opened";
        for(std::size_t i = 0; i < m_socketpool.size(); i++)
        {
            m_socketpool[i]->close();
            m_statepool[i] << state::disconnected;
        }
        LOG << "c: All sockets was closed";
        m_isworking = false;
    }
    return result;
}

bool TClient::do_resolve()
{
    using namespace boost::asio::ip;
    using namespace boost::asio;
    bool result;
    error_code ec;
    LOG << "c: resolving address";
    address addr = address::from_string(m_host, ec);
    tcp::resolver::iterator iter;
    if (!(ec.value()) || (addr.is_unspecified()))
    {
        tcp::resolver resolver(m_ioservice);
        tcp::resolver::query query(m_host,
                                   std::to_string(m_port));
        iter = resolver.resolve( query, ec);
        if ((ec.value()) || (m_host.empty()))
        {
            LOG << "c: Invalid IP addr or hostname";
        }
        else
        {
            m_endpoint = *iter;
            result = true;
        }
    }
    else
    {
        m_endpoint = tcp::endpoint(addr, m_port);
        result = true;
    }
    return result;
}

void TClient::do_connect_all()
{
    using namespace boost::asio::ip;
    using namespace boost::asio;
    using boost::posix_time::seconds;
    auto self(shared_from_this());
    for (std::size_t i1 = 0; i1 < m_timerpool.size(); i1++)
    {
        if (m_pproc->getComplType() != eCompl_t::none)
        {
            refresh_buffandcomplbin(i1, CBUFFLIMIT);
        }
        else
        {
            refresh_buffandcompl(i1);
        }
        m_timerpool[i1]->expires_from_now(seconds(CONN_TIMEOUT));
        auto bnd_conn = boost::bind(&TClient::OnConnected,
                                    self,
                                    placeholders::error,
                                    i1);
        auto bnd_wait = boost::bind(&TClient::OnConnectTimeout,
                                    self,
                                    placeholders::error,
                                    i1);
        m_timerpool[i1]->async_wait(bnd_wait);
        m_socketpool[i1]->async_connect(m_endpoint, bnd_conn);
    }
}
void TClient::do_write(std::size_t _idx)
{

    using namespace boost::asio::ip;
    using namespace boost::asio;
    using boost::posix_time::seconds;
    auto self(shared_from_this());
    auto bnd_wrt = boost::bind(&TClient::OnSent,
                                self,
                                placeholders::error,
                                _idx);
    auto bnd_wait = boost::bind(&TClient::OnConnectTimeout,
                                self,
                                placeholders::error,
                                _idx);
    m_timerpool[_idx]->expires_from_now(seconds(WRITE_TIMEOUT));
    m_timerpool[_idx]->async_wait(bnd_wait);
    boost::asio::async_write(*m_socketpool[_idx].get(),
                             *m_requestpool[_idx].get(),
                             bnd_wrt);
}

void TClient::do_read(std::size_t _idx)
{
    using namespace boost::asio::ip;
    using namespace boost::asio;
    using boost::posix_time::seconds;
    static constexpr char term_tor[] = "\r\n\r\n";
    auto self(shared_from_this());
    if (!m_responsepool[_idx])
    {
        m_responsepool[_idx] = std::make_unique<bstmbuff_t>(CBUFFLIMIT);
    }
    auto bnd_rd = boost::bind(&TClient::OnReceived,
                                self,
                                placeholders::error,
                                placeholders::bytes_transferred,
                                _idx);
    auto bnd_wait = boost::bind(&TClient::OnConnectTimeout,
                                self,
                                placeholders::error,
                                _idx);
    m_timerpool[_idx]->expires_from_now(seconds(READ_TIMEOUT));
    m_timerpool[_idx]->async_wait(bnd_wait);
    boost::asio::async_read_until(*(m_socketpool[_idx].get()),
                             *(m_responsepool[_idx].get()),
                             term_tor,
                             bnd_rd);
}

void TClient::do_read_bin(std::size_t _idx)
{
    using namespace boost::asio::ip;
    using namespace boost::asio;
    using boost::posix_time::seconds;
    auto self(shared_from_this());
    auto ptr = const_cast<char *>(m_responsebin_ownerpool[_idx].first.get());
    auto bnd_rd = boost::bind(&TClient::OnReceivedBin,
                                self,
                                placeholders::error,
                                placeholders::bytes_transferred,
                                _idx);
    auto bnd_wait = boost::bind(&TClient::OnConnectTimeout,
                                self,
                                placeholders::error,
                                _idx);
    m_timerpool[_idx]->expires_from_now(seconds(READ_TIMEOUT));
    m_timerpool[_idx]->async_wait(bnd_wait);

    boost::asio::async_read(*(m_socketpool[_idx].get()),
                            bnownbuff_t(ptr, CBUFFLIMIT) ,
                            *m_gencomplpool[_idx].get(),
                            bnd_rd);
}

void TClient::do_showerrordetails(const error_code & _err)
{
    if (_err == boost::asio::error::connection_refused)
    {
        LOG << "c: connection was refused, maybe server is disabled";
    }
    else if (_err == boost::asio::error::connection_aborted)
    {
        LOG << "c: connection was aborted, maybe server is busy";
    }
    else if (_err == boost::asio::error::connection_reset)
    {
        LOG << "c: connection was reseted, maybe server is busy";
    }
    else if (_err == boost::asio::error::eof)
    {
        LOG << "c: eof error, maybe server is busy";
    }
    else
    {
        LOG << "c: connection failed, reason:";
        LOG << _err.message();
    }
}
bool TClient::isError()
{
    return  m_pthread->getExceptionPtr() != nullptr;
}

void TClient::do_disconnect(std::size_t _idx)
{
    const char * FUNCTION = __FUNCTION__;
    try
    {
        DBGOUT2("c: start disconnection", _idx)
        m_statepool[_idx] << state::disconnected;
        m_timerpool[_idx]->cancel();
        m_requestpool[_idx].reset();
        m_responsepool[_idx].reset();
        m_responsebin_ownerpool[_idx].first.reset();
        m_responsebin_ownerpool[_idx].second = 0;
        std::vector<std::unique_ptr<complwrapper_t> > m_gencomplpool;
        if (m_socketpool[_idx]->is_open())
        {
            error_code ec;
            m_socketpool[_idx]->shutdown(tcp::socket::shutdown_both, ec);
            if (ec) LOG << ec.message();
            m_socketpool[_idx]->close();
        }
        else
        {
            LOG << "c: already disconnection";
        }
        DBGOUT2("c: stop disconnection", _idx)
    } catch (...)
    {
        errspace::show_errmsg(FUNCTION);
        throw;
    }
}

TClient::hCode_t TClient::getHandlerCode(const char *_funcname)
{
    m_hcodename[m_lastHCode] = _funcname;
    m_handlersStat[m_lastHCode][0] = 0;
    return m_lastHCode++;
}

void TClient::sendLastState(const TClient::hCode_t &_code,
                            TClient::eLastState _state)
{
    auto x = m_handlersStat[_code][static_cast<std::uint8_t>(_state)];
    if (x < max_events)
    {
        m_handlersStat[_code][static_cast<std::uint8_t>(_state)]++;
    }
}

bool TClient::refresh_buffandcomplbin(std::size_t _idx, std::size_t _txfed)
{
    bool result = false;
    if (!m_responsebin_ownerpool[_idx].first)
    {
        auto ptr = (CBUFFLIMIT > 0)?new char[CBUFFLIMIT]:nullptr;
        m_responsebin_ownerpool[_idx].first.reset(ptr);
    }
    const auto& ref = m_responsebin_ownerpool[_idx].first.get();
    auto vptr = const_cast<char *>(ref);
    memset(vptr, 0, _txfed);
    if (m_responsebin_ownerpool[_idx].first)
    {
        using gc = complwrapper_t;
        auto gtype = m_pproc->getComplType();
        auto gptr = new gc(gtype, m_responsebin_ownerpool[_idx]);
        m_gencomplpool[_idx].reset(gptr);
        result = true;
    }
    return result;
}

bool TClient::refresh_buffandcompl(std::size_t _idx)
{
    m_responsepool[_idx].reset();
    m_responsepool[_idx] = std::make_unique<bstmbuff_t>(CBUFFLIMIT);
}

std::string TClient::getHandlerStatistics()
{
    std::string total_stat;
    std::string h_stat;
    for (auto& x: m_handlersStat)
    {
        hCode_t code = x.first;
        std::string name = m_hcodename[code];
        std::ostringstream ss;
        ss << std::setw(16) << std::left << std::setfill(' ') << name << ":";
        h_stat = ss.str();
        for (int i1 = 0; i1 < x.second.size(); i1++)
        {
            h_stat += std::to_string(x.second[i1]) + ":";
        }
        h_stat += "\n";
        total_stat += h_stat;
    }
    return total_stat;
}
