/** @file
 *  @brief         Модуль сервера
 *  @author unnamed
 *  @date   created 2020
 *  @date   modified 2020
 *  @version 1.0
 */
#include "../include/server.h"


using tcp = boost::asio::ip::tcp;

//==========================================================================
//                                   serversocket_t
//==========================================================================

// serversocket_t() is deleted;
serversocket_t::serversocket_t(io_service& _srv,
                       IServerBase* _base,
                       thcode_t _code,
                       IServerProcessor* _proc)
    : m_sock(_srv),
      m_base(_base),
      m_timer(_srv),
      m_timer2(_srv),
      m_proc(_proc),
      m_ticket(this),
      m_thcode(_code)
{
    if (!m_base) throw std::runtime_error("server base: got nullptr");
    char * ptr = (BUFFLIMIT > 0)?new char[BUFFLIMIT]:nullptr;
    memset(ptr, 0, ptr?BUFFLIMIT:0);
    m_request_bin_owner.first.reset(ptr);
    DBGOUT2("s: server socket is created with size:", BUFFLIMIT)
    if (m_proc->getComplType() != eCompl_t::none)
    {
        auto complptr = new complwrapper_t(m_proc->getComplType(),
                                              m_request_bin_owner);
        m_gencompl.reset(complptr);
    }
}

serversocket_t::~serversocket_t()
{
    const char * FUNCTION = __FUNCTION__;
    try
    {
        m_isdestruction_sockstarted = true;
        error_code err;
        DBGOUT("s: server socket is destroing")
        Cancel();
        // possibly exception will be thrown
        DBGOUT("s: server socket was closed")
    }
    catch (...)
    {
        errspace::show_errmsg(FUNCTION);
    }
}

bool serversocket_t::Cancel(bool _isthstopped)
{
    error_code err;
    m_thcode = _isthstopped?noThCode:m_thcode;
    m_sock.cancel(err);
    m_sock.close(err);
    m_timer.cancel(err);
    m_timer2.cancel(err);
    return err == 0;
}

void serversocket_t::SetAction()
{
    using readyHandler_t = IServerProcessor::readyHandler_t;
    auto weak = weak_from_this();
    readyHandler_t hnd = [weak]()
    {
        const auto shared = weak.lock();
        if (shared)
        {
            shared->do_notify();
        }
    };
    m_handlerptr = std::make_shared<readyHandler_t>(hnd);
}

void serversocket_t::Close()
{
    m_isdestruction_sockstarted = true;
    Cancel();
    m_base->removeSocket(shared_from_this(), m_thcode);
}

void serversocket_t::OnReceived(const error_code & _err,
                std::size_t _bytes_txfed)
{
    const char * FUNCTION = __FUNCTION__;
    try
    {
        DBGOUT("s: OnReceivedBin")
        static auto hcode = m_base->getHandlerCode(FUNCTION);
        if (m_isdestruction_sockstarted) return;
        if (m_isthreadserver_stopped)
        {
            m_base->sendLastState(hcode, TServer::eLastState::error1);
            return;
        }
        error_code err;
        m_timer.cancel(err);
        if (_err)
        {
            LOG << "s: data is not received";
            m_base->sendLastState(hcode, TServer::eLastState::error1);
            Close();
        }
        DBGOUT2("s: #received", _bytes_txfed)
        DBGOUT2("s: data buff size:", m_request->size())
        if (m_sock.available(err))
        {
            do_read();
            return;
        }
        LOG << "s: request receiving is compleated:";
        if (!m_proc)
        {
            LOG << "s: request processor is not found";
            m_base->sendLastState(hcode, TServer::eLastState::error1);
            Close();
            return;
        }
        bool result = m_proc->process_output_data(std::move(m_request),
                                    m_ticket);
        if (!result)
        {
            LOG << "s: request is not valid";
            m_base->sendLastState(hcode, TServer::eLastState::error1);
            Close();
        }
        if (!m_proc->isAvailable(m_ticket))
        {
            LOG << "s: request haven't been processed yet";
            do_wait_processcompl();
            do_compose_send_busy();
            return;
        }
        if (get_response())
        {
            LOG << "s: request was processed";
            do_write();
        }
        else
        {
            LOG << "s: request processing fail";
            m_base->sendLastState(hcode, TServer::eLastState::error1);
            Close();
            return;
        }
    } catch (const boost::system::system_error& _err)
    {
        errspace::show_errmsg(_err.what());
        errspace::show_errmsg(FUNCTION);
        // possibly new exception will be thrown
        Close();
    } catch (...)
    {
        errspace::show_errmsg(FUNCTION);
        // possibly new exception will be thrown
        Close();
    }
}

void serversocket_t::OnReceivedBin(const error_code &_err,
                                  std::size_t _bytes_txfed)
{
    const char * FUNCTION = __FUNCTION__;
    try
    {
        static auto hcode = m_base->getHandlerCode(FUNCTION);
        DBGOUT("s: OnReceivedBin")
        if (m_isdestruction_sockstarted) return;
        if (m_isthreadserver_stopped)
        {

            m_base->sendLastState(hcode, TServer::eLastState::error1);
            return;
        }
        error_code err;
        m_timer.cancel(err);
        if ((_err) || (m_gencompl->isError()))
        {
            LOG << "s: io error or compl error. request is not received";
            m_base->sendLastState(hcode, TServer::eLastState::error1);
            Close();
            return;
        }
        DBGOUT2("s: received", _bytes_txfed)

        if (m_sock.available(err))
        {
            do_read_bin();
            return;
        }
        LOG << "s: request receiving is compleated";
        if (!m_proc)
        {
            LOG << "s: request processor is not found";
            m_base->sendLastState(hcode, TServer::eLastState::error1);
            Close();
            return;
        }
        std::unique_lock<ticketspinlock_t> lock(m_resumemtx);
        m_request_bin_owner.second = _bytes_txfed;
        bool result = m_proc->process_output_data
                (std::move(m_request_bin_owner),
                 m_ticket,
                 m_gencompl->getNativeCompl(),
                 m_handlerptr);

        if (!result)
        {
            LOG << "s: request is not valid or request queue is full";
            m_base->sendLastState(hcode, TServer::eLastState::error1);
            Close();
            return;
        }
        if (!m_proc->isAvailable(m_ticket))
        {
            DBGOUT("s: request haven't been processed yet")
            do_wait_processcompl();
            do_compose_send_busy();
            return;
        }
        if (get_response())
        {
            LOG << "s: request was processed";
            do_write();
            return;
        }
        else
        {
            LOG << "s: request processing failed";
            m_base->sendLastState(hcode, TServer::eLastState::error1);
            Close();
            return;
        }

    } catch (const boost::system::system_error& _err)
    {
        errspace::show_errmsg(_err.what());
        errspace::show_errmsg(FUNCTION);
        // possibly new exception will be thrown
        Close();
    } catch (...)
    {
        errspace::show_errmsg(FUNCTION);
        // possibly new exception will be thrown
        Close();
    }
}

void serversocket_t::OnProcessTimeout(const error_code &_err)
{
    const char * FUNCTION = __FUNCTION__;
    try
    {
        DBGOUT("s: OnProcessTimeout")
        static auto hcode = m_base->getHandlerCode(FUNCTION);
        if (m_isdestruction_sockstarted) return;
        if (m_isthreadserver_stopped)
        {
            m_base->sendLastState(hcode, TServer::eLastState::error1);
            return;
        }
        std::unique_lock<ticketspinlock_t> lock(m_resumemtx);
        error_code err;
        m_timer2.cancel(err);
        constexpr auto aborted = boost::asio::error::operation_aborted;
        m_proctimecnt = m_proctimecnt? m_proctimecnt--: 0;
        if ( ((_err) && !(_err.value() == aborted )) || (m_proctimecnt == 0))
        {
            LOG << "s: request can't be processed due to io error";
            m_base->sendLastState(hcode, TServer::eLastState::error2);
            Close();
            return;
        }

        if (!m_proc)
        {
            LOG << "s: request processor is not found";
            m_base->sendLastState(hcode, TServer::eLastState::error2);
            Close();
            return;
        }
        if (!m_proc->isAvailable(m_ticket))
        {
            DBGOUT("s: request haven't been processed yet")
            do_wait_processcompl();
            do_compose_send_busy();
            return;
        }
        if (get_response())
        {
            LOG << "s: request was processed";
            do_write();
            return;
        }
        else // do never send empty response
        {
            LOG << "s: request processing failed";
            m_base->sendLastState(hcode, TServer::eLastState::error1);
            Close();
            return;
        }

    } catch (...)
    {
        errspace::show_errmsg(FUNCTION);
        throw;
    }
}


void serversocket_t::OnSent(const error_code &_err,
                           std::size_t _bytes_txfed)
{
    const char * FUNCTION = __FUNCTION__;
    try
    {
        DBGOUT("s: OnSent")
        static auto hcode = m_base->getHandlerCode(FUNCTION);
        if (m_isdestruction_sockstarted) return;
        if (m_isthreadserver_stopped)
        {
            m_base->sendLastState(hcode, TServer::eLastState::error1);
            return;
        }
        error_code err;
        if (!_err)
        {
            m_timer.cancel(err);
            LOG << "s: reply was sent --------------------------:";
            do_wait_disconnect();
            return;
        }
        else
        {
            LOG << "s: reply was not sent";
            m_base->sendLastState(hcode, TServer::eLastState::error1);
            Close();
            return;
        }
    } catch (...)
    {
        errspace::show_errmsg(FUNCTION);
        throw;
    }
}

void serversocket_t::OnConnectTimeout(const error_code & _err)
{
    const char * FUNCTION = __FUNCTION__;
    try
    {
        DBGOUT("s: OnConnectTimeout")
        static auto hcode = m_base->getHandlerCode(FUNCTION);
        if (m_isdestruction_sockstarted) return;
        if (m_isthreadserver_stopped)
        {
            m_base->sendLastState(hcode, TServer::eLastState::error1);
            return;
        }
        if (_err.value() !=  boost::asio::error::operation_aborted)
        {
            LOG << "s: error: timeout for some operation happened";
            if (_err)
                { LOG << _err.message(); }
            m_base->sendLastState(hcode, TServer::eLastState::error1);
            Close();
            return;
        }
        DBGOUT("s: timer is canceled")
    } catch (...)
    {
        errspace::show_errmsg(FUNCTION);
        throw;
    }
}

void serversocket_t::OnBusy(const error_code &_err)
{
    const char * FUNCTION = __FUNCTION__;
    try
    {
        DBGOUT("s: OnBusy")
        error_code err;
        static auto hcode = m_base->getHandlerCode(FUNCTION);
        if (m_isdestruction_sockstarted) return;
        if (m_isthreadserver_stopped)
        {
            m_base->sendLastState(hcode, TServer::eLastState::error1);
            return;
        }
        if (_err)
        {
            DBGOUT("s: error busy respone was not been sent")
            m_base->sendLastState(hcode, TServer::eLastState::success);
            Close();
            return;
        }
        m_timer.cancel(err);
    } catch (...)
    {
        errspace::show_errmsg(FUNCTION);
        throw;
    }
}

void serversocket_t::OnDisconnected(const error_code &_err)
{
    const char * FUNCTION = __FUNCTION__;
    try
    {
        DBGOUT("s: OnDisconnected")
        static auto hcode = m_base->getHandlerCode(FUNCTION);
        if (m_isdestruction_sockstarted) return;
        if (m_isthreadserver_stopped)
        {
            m_base->sendLastState(hcode, TServer::eLastState::error1);
            return;
        }
        error_code err;
        if (_err.value() ==  boost::asio::error::operation_aborted)
        {
            // we never get here
            DBGOUT("s: timer for disconnection was canceled")
        }
        m_timer.cancel(err);
        DBGOUT("s: socket will be disconnected")
        m_base->sendLastState(hcode, TServer::eLastState::success);
        Close();
    } catch (...)
    {
        errspace::show_errmsg(FUNCTION);
        throw;
    }
}

void serversocket_t::registersocket()
{
    m_base->insertSocket(shared_from_this(), m_thcode);
}

thcode_t serversocket_t::getThreadCode() const noexcept
{
    return m_thcode;
}

void serversocket_t::resetThreadCode() noexcept
{
    m_thcode = noThCode;
}

bool serversocket_t::isDestrStarted() const noexcept
{
    return m_isdestruction_sockstarted;
}

void serversocket_t::do_first()
{
    if (m_proc)
    {
        if (m_proc->getComplType() != eCompl_t::none)
        {
            do_read_bin();
        }
        else
        {
            do_read();
        }
    }
    else
    {
        Close();
    }

}

serversocket_t::socket_t &serversocket_t::getSocket()
{ return m_sock; }

boost::asio::deadline_timer &serversocket_t::getTimer()
{     return m_timer; }

void serversocket_t::do_read()
{
    using namespace boost::asio::ip;
    using namespace boost::asio;
    static constexpr char term_tor[] = "\r\n";
    auto self(shared_from_this());
    auto request = new bstmbuff_t(BUFFLIMIT);
    m_request.reset(request);
    boost::asio::async_read_until(m_sock, *m_request.get(), term_tor,
        boost::bind(&serversocket_t::OnReceived,
                    self,
                    placeholders::error,
                    placeholders::bytes_transferred));
    m_timer.expires_from_now(boost::posix_time::seconds(TIMEOUT_READ));
    m_timer.async_wait(boost::bind(&serversocket_t::OnConnectTimeout,
                                   self,
                                   placeholders::error));
}

bool serversocket_t::get_response()
{
    if (m_proc)
    {
        m_proc->get_input_data(m_response_owner, m_ticket);
        auto size = m_response_owner.second;
        DBGOUT2("s: composed out packet with size:" , size)
        return m_response_owner.first.operator bool();
    }
}

void serversocket_t::do_compose_send_busy()
{
    using namespace boost::asio::ip;
    using namespace boost::asio;
    if (!m_proc)
    { return;   }
    auto response_busy = new bstmbuff_t(BUFFLIMIT);
    m_response_busy.reset(response_busy);
    m_proc->get_busy_data(m_response_busy);
    auto size = m_response_busy->size();
    DBGOUT2("s: composed out packet with size:" , size)
    auto self(shared_from_this());

    boost::asio::async_write(m_sock, *m_response_busy.get(),
        boost::bind(&serversocket_t::OnBusy,
                    self,
                    placeholders::error));

    m_timer.expires_from_now(boost::posix_time::seconds(TIMEOUT_READ));
    m_timer.async_wait(boost::bind(&serversocket_t::OnConnectTimeout,
                                   self,
                                   placeholders::error));
}

void serversocket_t::do_write()
{

    using namespace boost::asio::ip;
    using namespace boost::asio;
    auto self(shared_from_this());
    std::unique_ptr<bnownbuff_t> response;
    std::size_t size = 0;
    if (m_response_owner.first)
    {
        auto ptr = const_cast<char *>(m_response_owner.first.get());
        size = m_response_owner.second;
        response = std::make_unique<bnownbuff_t>(ptr, size);
        boost::asio::async_write(m_sock, *response.get(),
            boost::bind(&serversocket_t::OnSent,
                        self,
                        placeholders::error,
                        placeholders::bytes_transferred));
    }
    else
    {   // we never get here (reserved for future)
        boost::asio::async_write(m_sock, boost::asio::null_buffers(),
            boost::bind(&serversocket_t::OnSent,
                        self,
                        placeholders::error,
                        placeholders::bytes_transferred));
    }
    m_timer.expires_from_now(boost::posix_time::seconds(TIMEOUT_READ));
    m_timer.async_wait(boost::bind(&serversocket_t::OnConnectTimeout,
                                   self,
                                   placeholders::error));
}

void serversocket_t::do_read_bin()
{

    using namespace boost::asio::ip;
    using namespace boost::asio;
    auto self(shared_from_this());
    auto ptr = m_request_bin_owner.first.get();
    boost::asio::async_read(m_sock, bnownbuff_t(ptr, BUFFLIMIT),
                            *m_gencompl.get(),
                            boost::bind(&serversocket_t::OnReceivedBin,
                                        self,
                                        placeholders::error,
                                        placeholders::bytes_transferred));
    m_timer.expires_from_now(boost::posix_time::seconds(TIMEOUT_READ));
    m_timer.async_wait(boost::bind(&serversocket_t::OnConnectTimeout,
                                   self,
                                   placeholders::error));
}

void serversocket_t::do_write_bin()
{
    do_write();
}

void serversocket_t::do_wait_processcompl()
{
    using namespace boost::asio::ip;
    using namespace boost::asio;
    auto self(shared_from_this());
    using boost::posix_time::milliseconds;
    m_timer2.expires_from_now(milliseconds(TIMEOUT_PROC));
    m_timer2.async_wait(boost::bind(&serversocket_t::OnProcessTimeout,
                                   self,
                                   placeholders::error));
}

void serversocket_t::do_wait_disconnect()
{
    using namespace boost::asio::ip;
    using namespace boost::asio;
    auto self(shared_from_this());
    m_request = std::make_unique<bstmbuff_t>(BUFFLIMIT);
    boost::asio::async_read_until(m_sock, *m_request.get(),
                                  boost::asio::error::eof,
        boost::bind(&serversocket_t::OnDisconnected,
                    self,
                    placeholders::error));
    m_timer.expires_from_now(boost::posix_time::seconds(TIMEOUT_DISC));
    m_timer.async_wait(boost::bind(&serversocket_t::OnConnectTimeout,
                                   self,
                                   placeholders::error));
}

void serversocket_t::do_notify()
{
    std::unique_lock<ticketspinlock_t> lock(m_resumemtx);
    error_code err;
    m_timer2.cancel(err);
    DBGOUT("serversocket_t::do_notify()")
}

//==========================================================================
//                                   TServer
//==========================================================================
std::shared_ptr<TServer> TServer::psrv;
std::atomic_int TServer::serverstate_t::counter = 0;
std::shared_ptr<TServer>& TServer::CreateServer(serverconfig_t &_sc)
{
    if (!psrv)
    {
        psrv = TPServer(new TServer(_sc));
        psrv->m_addr = _sc.m_addr;
        psrv->m_port = _sc.m_port;
    }
    return psrv;
}

void TServer::DestroyServer()
{
    psrv.reset();
}

TServer::TServer(serverconfig_t &_sc)
    : m_acceptor(m_ioservice),
      m_state(_sc.m_threadnumber),
      m_addr(_sc.m_addr),
      m_proc(std::move(_sc.m_proc)),
      m_connection_limit(_sc.m_connlimit),
      m_threadnumber(_sc.m_threadnumber),
      m_sig(_sc.sig)
{
    const char* FUNCTION = __FUNCTION__;
    try
    {
        m_state << state::shutdowned;
#if BUILDTYPE == BUILD_DEBUG
        if (m_proc)
        {
            if (m_proc->getComplType() != eCompl_t::none)
            {
                DBGOUT("Reading with completion condition")
            }
            else
            {
                DBGOUT("Reading without completion condition")
            }
        }
        else
        {
            DBGOUT("Fake processor")
        }
#endif
    } catch (...)
    {
        errspace::show_errmsg(FUNCTION);
        throw;
    }
}

TServer::~TServer()
{
    const char * FUNCTION = __FUNCTION__;
    try
    {
        Stop();
    }
    catch (...)
    {
        errspace::show_errmsg(FUNCTION);
        m_isError = true;
    }
}

bool TServer::Start(const char* _addr,
           const std::uint16_t _port)
{
    const char * FUNCTION = __FUNCTION__;
    using namespace boost::asio::ip;
    using namespace boost::asio;
    bool result = false;
    error_code ec;
    // if state::run or state::invalid then return
    if (!m_state(state::shutdowned))
    {
        return result;
    }
    if ((_addr) && (_port))
    {
        m_addr = _addr;
        m_port = _port;
    }

    try
    {
        m_ioservice.reset();
        if (!do_resolve())
            { return result; }
        LOG << str_sum("s: Host Name:" , m_endpoint.address().to_string());
        m_acceptor.open(m_endpoint.protocol(), ec);
        if ((ec.value()) || !(m_acceptor.is_open()))
        {
            m_ioservice.stop();
            errspace::show_errmsg("s: Connection can't be opened");
            return result;
        }
        m_acceptor.set_option(tcp::acceptor::reuse_address(true), ec);
        if (ec.value())
        {
            m_acceptor.close(ec);
            m_ioservice.stop();
            errspace::show_errmsg("s: Connection settings can't be changed");
            return result;
        }
        m_acceptor.bind(m_endpoint, ec);
        if (ec.value())
        {
            m_acceptor.close(ec);
            m_ioservice.stop();
            errspace::show_errmsg(str_sum("s: Connection can't be bound:",
                                          ec.message()).c_str());
            return result;
        }
        m_acceptor.listen(m_connection_limit, ec);
        if (ec.value())
        {
            m_acceptor.close(ec);
            m_ioservice.stop();
            errspace::show_errmsg("s: Connection can't be listen");
            return result;
        }
        auto launch_thread = [&](thcode_t _code) -> mngThread_t*
        {
            return new mngThread_t{cDummy,
                    &TServer::process_iooperations_wrapper,
                    this,
                    nullptr,
                    false,
                    m_threadnumber,
                    m_sig,
                    thcode_t(_code)
                    };
        };
        m_socketset_active.resize(m_threadnumber);
        m_socketset_garbage.resize(m_threadnumber);
        mngThread_t* mngthr = nullptr;
        // It doesn't use due to thread group start is implemented
        // mngthr->StartLoop();
        for (std::uint8_t i = 0; i < m_threadnumber; i++)
        {
            mngthr = launch_thread(i);
            m_pmngthreadpool.emplace_back(mngthr);
        }
        if (mngthr)
        {
            mngthr->StartLoop();
        }
        m_state << state::running;
        LOG << "s: Server is running";
        using std::chrono::milliseconds;
        const auto max_attempts = 30;
        const auto delay_quant = 100;
        auto nattempts = 0;
        if (m_threadnumber > 0)
        {
            while(!result)
            {
                result = m_state(state::run);
                std::this_thread::sleep_for(milliseconds(delay_quant));
                if (nattempts++ > max_attempts) break;
            }
            if (!result)
                { Stop(); }
        }
        else
        {
            result = true;
        }
        return result;
    }
    catch(...)
    {
        errspace::show_errmsg(FUNCTION);
        throw;
    }
}

bool TServer::Stop() //noexcept
{
    bool result = false;
    error_code ec;
    if (m_state(state::invalid))
    {
        m_isError = true;
        if (m_pmngthreadpool.size())
        {
            m_pmngthreadpool.clear();
        }
        result = true;
        return result;
    }

    if (!m_state(state::run) && !m_state(state::running))
    {
        DBGOUT("s: Server have already been shutdowned")
        return result;
    }
    m_state << state::shutdowned;
    // delete all threads
    mngThread_t::ResetDestructorError();
    if (m_pmngthreadpool.size())
    {
        m_pmngthreadpool.clear();
    }
    m_isError = mngThread_t::GetDestructorError();
    if (m_isError)
    {
        m_state << state::invalid;
        errspace::show_errmsg("s: Server is not stopped");
        return result;
    }
    m_acceptor.cancel(ec);
    m_ioservice.run(ec);
    {
        std::unique_lock<std::recursive_mutex>  lock(m_mtx_garbage);
        m_socketset_garbage.clear();
    }
    m_acceptor.close(ec);
    m_ioservice.stop();
    result = m_ioservice.stopped();
    LOG << "s: Server is stoped";
    return result;
}

void TServer::process_iooperations(IThread* _th, thcode_t _idx)
{
    const char * FUNCTION = __FUNCTION__;
    using namespace boost::asio::ip;
    using namespace boost::asio;
    error_code ec;
    LOG << str_sum("s: server thread loop is started" ,_idx);
    try
    {
        auto isRunning = _th->isRunning();
        while(isRunning)
        {
            if ((m_state(state::running)))
            {
                do_accept(_idx);
                m_state++;
                break;
            }
            isRunning = _th->isRunning();
        }
        while(isRunning)
        {
            if (_th)
            {
                _th->feedwd();
            }
            if (m_state(state::run))
            {
                m_ioservice.poll(ec);
                if (ec) break;
                clearGarbage(_idx);
            }
            isRunning = _th->isRunning();
        }
        LOG << "s: server thread loop is stopping";
        {
            std::unique_lock<std::recursive_mutex>  lock2(m_mtx_active);
            for(auto& x: m_socketset_active[_idx])
            {
                x->Cancel(true);
            }
            m_socketset_active[_idx].clear();
        }
        LOG << str_sum("s: loop is stopped", _idx);
    } catch (...)
    {
        errspace::show_errmsg(FUNCTION);
        m_state << state::invalid;
        throw;
    }
}

void TServer::OnConnected(ssocketPtr_t _client, const error_code & _err)
{
    const char * FUNCTION = __FUNCTION__;
    try
    {
        DBGOUT("s: OnConnected")
        static auto hcode = getHandlerCode(FUNCTION);
        if (!_client) return;
        if (_client->getThreadCode() == serversocket_t::noThCode)
        {
            sendLastState(hcode, TServer::eLastState::error1);
            return;
        }

        std::unique_lock<std::mutex> lock(m_connmtx);
        if(_err)
        {
            if (_err == boost::asio::error::operation_aborted)
            {
                sendLastState(hcode, TServer::eLastState::error1);
                LOG << "s: connection was canceled";
                return;
            }
            LOG + "s: connection error: " <<  _err.message();
            sendLastState(hcode, TServer::eLastState::error1);
            _client->Close();
            return;
        }
        LOG << "s: connection state established";
        if (getSockListSize(_client->getThreadCode()) >= m_connection_limit)
        {
            LOG << "s: connection limit is exceded !!!";
            do_reaccept(_client);
            return;
        }
        else
        {
            do_accept(_client->getThreadCode());
            _client->do_first();
        }
    } catch (...)
    {
        errspace::show_errmsg(FUNCTION);
        throw;
    }
}



bool TServer::do_resolve()
{
    using namespace boost::asio::ip;
    using namespace boost::asio;
    bool result = false;
    error_code ec;
    DBGOUT("s: resolving address");
    address addr = address::from_string(m_addr, ec);
    tcp::resolver::iterator iter;
    if ((ec.value()) && (!addr.is_unspecified()))
    {
        m_endpoint = tcp::endpoint(addr, m_port);
        result = true;
        return result;
    }
    tcp::resolver resolver(m_ioservice);
    tcp::resolver::query query(m_addr, std::to_string(m_port));
    iter = resolver.resolve( query, ec);
    if ((ec.value()) || (m_addr.empty()))
    {
        LOG << "s: Invalid IP addr or hostname";
    }
    else
    {
        m_endpoint = *iter;
        result = true;
    }
    return result;
}

void TServer::do_accept(thcode_t _code)
{
    DBGOUT2("s: do_accept", _code)
    using namespace boost::asio::ip;
    using namespace boost::asio;
    auto ssocket = std::make_shared<serversocket_t>(std::ref(m_ioservice),
                                                   this,
                                                   _code,
                                                   m_proc.get());
    ssocket->SetAction();
    increaseConnCounter(_code);
    ssocket->registersocket();
    auto pfunc = &TServer::OnConnected;
    auto bnd = boost::bind(pfunc, this, ssocket, placeholders::error);
    m_acceptor.async_accept(ssocket->getSocket(), bnd);
}

void TServer::do_reaccept(ssocketPtr_t _client)
{
    DBGOUT("s: do_accept")
    using namespace boost::asio::ip;
    using namespace boost::asio;
    using boost::posix_time::milliseconds;
    auto pfunc = &TServer::OnConnected;
    auto bnd = boost::bind(pfunc, this, _client, placeholders::error);
    _client->getTimer().expires_from_now(milliseconds(TIMEOUT_ACCEPT));
    _client->getTimer().async_wait(bnd);
}

void TServer::insertSocket(const ssocketPtr_t& _ptr, thcode_t _code)
{
    std::unique_lock<std::recursive_mutex>  lock(m_mtx_active);
    assert(m_socketset_active.size() > _code);
    m_socketset_active[_code].insert(_ptr);
}

void TServer::removeSocket(const ssocketPtr_t& _ptr, thcode_t _code)
{
    std::unique_lock<std::recursive_mutex>  lock(m_mtx_garbage);
    std::unique_lock<std::recursive_mutex>  lock2(m_mtx_active);
    assert(m_socketset_active.size() > _code);
    assert(m_socketset_garbage.size() > _code);
    auto it_garbage = m_socketset_garbage[_code].find(_ptr);
    auto it_active = m_socketset_active[_code].find(_ptr);
    if ( it_garbage == m_socketset_garbage[_code].end())
    {
        m_socketset_garbage[_code].insert(_ptr);
    }
    if  ( it_active != m_socketset_active[_code].end())
    {
        m_socketset_active[_code].erase(_ptr);
    }
}

std::size_t TServer::getSockListSize(thcode_t _code) const
{
    std::unique_lock<std::recursive_mutex>  lock(m_mtx_active);
    assert(m_socketset_active.size() >= _code);
    return m_socketset_active[_code].size();
}

void TServer::clearGarbage(thcode_t _code)
{
    std::unique_lock<std::recursive_mutex>  lock(m_mtx_garbage);
    assert(m_socketset_garbage.size() >= _code);
    m_socketset_garbage[_code].clear(); //noexcept
}

//==========================================================================
//                                   IServerBase
//==========================================================================

hCode_t IServerBase::getHandlerCode(const char *_funcname)
{
    std::unique_lock<std::mutex> lock(m_statmtx);
    m_hcodename[m_lastHCode] = _funcname;
    m_handlersStat[m_lastHCode][0] = 0;
    return m_lastHCode++;
}

void IServerBase::sendLastState(const hCode_t &_code,
                                IServerBase::eLastState _state)
{
    std::unique_lock<std::mutex> lock(m_statmtx);
    auto x = m_handlersStat[_code][static_cast<std::uint8_t>(_state)];
    if (x < max_events)
    {
        m_handlersStat[_code][static_cast<std::uint8_t>(_state)]++;
    }
}

thcode_t IServerBase::increaseConnCounter(thcode_t _code)
{
    std::unique_lock<std::mutex> lock(m_statmtx);
    m_numconnStat[_code]++;
    return m_numconnStat.size();
}

std::string IServerBase::getHandlerStatistics()
{
    std::unique_lock<std::mutex> lock(m_statmtx);
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
    h_stat = "connections per threads: \n";
    for (auto& x: m_numconnStat)
    {
        h_stat += std::to_string(x.first) + " : " + std::to_string(x.second);
        h_stat += "\n";
    }
    total_stat += h_stat;
    return total_stat;
}

bool TServer::isError()
{
    return m_isError;
}
