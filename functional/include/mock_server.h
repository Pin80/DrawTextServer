#ifndef MOCK_SERVER_H
#define MOCK_SERVER_H
#include "../../include/common.h"
#include "../../include/server.h"
#include "../../include/client.h"
#include "../../include/error.h"
#include "../../include/logger.h"
#include "../../include/watchdog.h"
#include "../../include/server.h"

class MockServer;

class MockSocket : public serversocket_t

{
    using error_code = boost::system::error_code;
    using io_service = boost::asio::io_service;
    using socket_t = boost::asio::ip::tcp::socket;
public:
    /// Конструктор класса "серверных сокетов"
    explicit MockSocket(io_service& _srv,
                           IServerBase* _base,
                           IServerProcessor* _proc = nullptr);
    /// Деструктор класса
    virtual ~MockSocket();
    friend MockServer;
};


class MockServer: public TServer
{
public:
    /// Конструктор класса
    MockServer(serverconfig_t &_sc) : TServer(_sc)
    {
    }
    /// Деструктор класса
    virtual ~MockServer() override
    {     }
    /// Запустить сервер
    // two step initialization for mock overriding
    virtual bool Start(const char* _addr = nullptr,
                       const std::uint16_t _port = 0) override
    {
        return TServer::Start(_addr, _port);
    }
    /// Остановить сервер
    virtual bool Stop() override //noexcept
    {
        return TServer::Stop();
    }
protected:
    /// Процедыра потока (делает опрос операций)
    virtual void process_iooperations(IThread* _th, thcode_t _idx) override
    {
        TServer::process_iooperations(_th, _idx);
    }
private:
};


class MockServer2: public TServer
{
public:
    /// Конструктор класса
    MockServer2(serverconfig_t &_sc, IServerProcessor* _ptr) : TServer(_sc)
    {
        auto ptr = _ptr;
        m_procmock = ptr;
    }
    /// Деструктор класса
    virtual ~MockServer2() override
    {

    }
    /// Запустить сервер
    // two step initialization for mock overriding
    virtual bool Start(const char* _addr = nullptr,
                       const std::uint16_t _port = 0) override
    {
        return TServer::Start(_addr, _port);
    }
    /// Остановить сервер
    virtual bool Stop() override //noexcept
    {
        return TServer::Stop();
    }
protected:
    /// Процедыра потока (делает опрос операций)
    virtual void process_iooperations(IThread* _th, thcode_t _idx) override
    {
        TServer::process_iooperations(_th, _idx);
    }

    void do_accept(thcode_t _code) override
    {

        DBGOUT2("s: do_accept ==========================", _code)
        using namespace boost::asio::ip;
        using namespace boost::asio;

        auto ssocket = std::make_shared<serversocket_t>(std::ref(m_ioservice),
                                                       this,
                                                       _code,
                                                       m_procmock);

        //ssocket->m_thcode = _code;
        ssocket->SetAction();
        increaseConnCounter(_code);
        ssocket->registersocket();
        //auto pfunc = &TServer::OnConnected;
        //auto bnd = boost::bind(pfunc, this, ssocket, placeholders::error);
        //m_acceptor.async_accept(ssocket->getSocket(), bnd);

        DBGOUT("s: do_accept")
        using namespace boost::asio::ip;
        using namespace boost::asio;
        using boost::posix_time::milliseconds;
        auto pfunc = &MockServer2::OnTriedTest;
        auto bnd = boost::bind(pfunc, this, ssocket, placeholders::error);
        ssocket->getTimer().expires_from_now(milliseconds(500));
        ssocket->getTimer().async_wait(bnd);

    }
    void OnTriedTest(ssocketPtr_t _client, const error_code & _err)
    {
        DBGOUT("s: OnTriedTest")
        throw std::exception();
    }
private:
    IServerProcessor* m_procmock;
};


#endif // MOCK_SERVER_H

