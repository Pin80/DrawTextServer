/** @file
 *  @brief         Функциональный тест
 *  @author unnamed
 *  @date   created 10.01.2020
 *  @date   modified 10.02.2020
 *  @version 1.0 (alpha)
 */
#include <iostream>
#include <gtest/gtest.h>
#include "../include/common.h"
#include "../include/server.h"
#include "../include/client.h"
#include "../include/error.h"
#include "../include/logger.h"
#include "../include/watchdog.h"
#include "../include/processor.h"
#include "./include/mock_processor1.h"
#include "./include/mock_server.h"

#if BUILDTYPE == BUILD_DEBUG
DEFINEWATCHPOINT
#endif

static std::atomic_bool isRaised = false;

void signal_handler(int _signal)
{
  if (_signal == SIGUSR1)
  {
      std::atomic_signal_fence(std::memory_order_release);
      isRaised.store(true, std::memory_order_relaxed);
  }
}

void delay(std::uint32_t _msec)
{
    using namespace std::chrono;
    //std::this_thread::sleep_for(std::chrono::milliseconds(_msec));
    auto start = system_clock::now();
    using std::chrono::milliseconds;
    //using std::chrono::duration_cast<milliseconds>;
    unsigned elapsed_seconds;
    do
    {
        std::this_thread::yield();
        auto end = system_clock::now();
        elapsed_seconds = duration_cast<milliseconds>(end - start).count();
    } while (_msec > elapsed_seconds);

}

///@class TTestBench
///@brief Class for the test of algorithm.
class TTestBench : public ::testing::Test
{
public:
    /// Run of testing for some dataset or user input before
    bool Run();
    /// Destructor of TTestBench class
    virtual ~TTestBench() = default;
    std::shared_ptr<TServer> m_pserver;
    std::shared_ptr<MockServer> m_pmserver;
    std::shared_ptr<MockServer2> m_pmserver2;
    std::shared_ptr<TClient> m_pclient;
    serverconfig_t sc;
    clientconfig_t cc;
protected:
    void SetUp()
    {     }
    void TearDown()
    {
        //m_pserver.reset();
        //m_pclient.reset();
        //m_psproc.reset();
        //m_pcproc.reset();
    }
};

/*
TEST_F(TTestBench, server_abort)
{
    auto uproc = std::make_unique<ServerProcessor>(true);
    sc.m_proc.reset(uproc.release());
    sc.m_addr = "127.0.0.1";
    sc.m_port = 1080;
    sc.m_connlimit = 2;
    sc.m_threadnumber = 2;
    m_pserver = TServer::CreateServer(sc);

    cc.m_host = "127.0.0.1";
    cc.m_port = 1080;
    cc.m_paral = 4;
    cc.m_pproc = std::make_shared<ClientProcessor>();

    m_pclient = std::make_shared<TClient>(cc);
    delay(500);
    m_pserver->Start();
    delay(500);
    m_pclient->Connect();
    delay(500);
    LOG << m_pclient->getHandlerStatistics();
    LOG << m_pserver->getHandlerStatistics();
    m_pserver->Stop();
    delay(500);

    m_pserver.reset();
    m_pclient.reset();
    sc.m_proc.reset();
    cc.m_pproc.reset();
    TServer::DestroyServer();
    auto result = 1;

    ASSERT_EQ(result, 1);
}
*/


TEST_F(TTestBench, server_match)
{
    ILogger::start_log();
    bool result = false;
    sc.m_proc = std::make_unique<ServerProcessor>(true);
    cc.m_pproc = std::make_shared<ClientProcessor>();
    auto x = dynamic_cast<ServerProcessor*>(sc.m_proc.get());
    auto y = dynamic_cast<ClientProcessor*>(cc.m_pproc.get());
    sc.m_addr = "127.0.0.1";
    sc.m_port = 1080;
    sc.m_connlimit = 9;
    cc.m_host = sc.m_addr;
    cc.m_port = sc.m_port;
    cc.m_paral = sc.m_connlimit - 1;
    sc.m_threadnumber = 2;
    m_pserver = TServer::CreateServer(sc);
    m_pclient = std::make_shared<TClient>(cc);
    result = m_pserver->Start();
    if (result)
    {
        delay(100);

        m_pclient->Connect();
        LOG << "+====================0========================";
        bool isWorking = m_pclient->isWorking();
        while (isWorking)
        {
            isWorking = m_pclient->isWorking();
            std::this_thread::yield();
        }

        //delay(5500);
        LOG << "+====================1========================";
        delay(100);
        LOG << "+====================2========================";
        m_pserver->Stop();
        delay(100);
        LOG << m_pclient->getHandlerStatistics();
        LOG << m_pserver->getHandlerStatistics();
        LOG + "s: matched:" << std::to_string(x->m_successes);
        LOG + "s: mismatched:" << std::to_string(x->m_fails);
        LOG + "c: matched:" << std::to_string(y->m_successes);
        LOG + "c: mismatched:" << std::to_string(y->m_fails);
        delay(100);
        result = ((x->m_successes ==  8) && (y->m_successes == 8));
    }
    TServer::DestroyServer();
    m_pserver.reset();
    m_pclient.reset();
    sc.m_proc.reset();
    cc.m_pproc.reset();
    delay(1000);
    ILogger::stop_log();
    ASSERT_EQ(result, true);
}


/*
TEST_F(TTestBench, server_startstop)
{
    delay(300);
    ILogger::start_log();
    LOG << "+==============______0_______=====================";
    delay(200);
    bool result = false;
    sc.m_proc = std::make_unique<ServerProcessor>(true);
    sc.m_addr = "127.0.0.1";
    sc.m_port = 1080;
    sc.m_connlimit = 2;
    sc.m_threadnumber = 2;
    m_pmserver = std::make_shared<MockServer>(sc);
    result = m_pmserver->Start();
    delay(200);
    if (result)
    {
        delay(200);
        //delay(5500);
        LOG << "+==============______1_______=====================";
        m_pmserver->Stop();
        delay(200);
        result = !m_pmserver->isError();
        if (result)
        {
            result = m_pmserver->Start();
            if (result)
            {
                delay(200);
                LOG << "+==============______2_______=====================";
                m_pmserver->Stop();
            }
        }
    }
    m_pmserver.reset();

    sc.m_proc.reset();
    TServer::DestroyServer();
    ILogger::stop_log();
    ASSERT_EQ(result, true);
}
*/

/*
TEST_F(TTestBench, server_startstop2)
{
    delay(500);
    ILogger::start_log();
    bool result = false;
    sc.m_proc = std::make_unique<ServerProcessor>(true);
    sc.m_addr = "127.0.0.1";
    sc.m_port = 1080;
    sc.m_connlimit = 9;
    sc.m_threadnumber = 0;
    m_pmserver = std::make_shared<MockServer>(sc);
    result = m_pmserver->Start();
    delay(100);
    if (result)
    {
        delay(100);
        //delay(5500);
        LOG << "+==============______3_______=====================";
        m_pmserver->Stop();
        delay(100);
        result = !m_pmserver->isError();
    }
    m_pmserver.reset();

    sc.m_proc.reset();
    TServer::DestroyServer();
    ILogger::stop_log();
    ASSERT_EQ(result, true);
}
*/

/*
TEST_F(TTestBench, server_throw1)
{
    ILogger::start_log();
    LOG << "+===============SYART THROW TEST=====================";
    delay(500);

    bool result = false;

     delay(200);

    auto uproc = std::make_unique<ServerProcessor>(true);
    auto proc = uproc.release();
    sc.m_proc.reset(proc);
    sc.m_addr = "127.0.0.1";
    sc.m_port = 1080;
    sc.m_connlimit = 9;
    sc.m_threadnumber = 1;
    sc.sig = SIGUSR1;
    std::signal(SIGUSR1, signal_handler);
    m_pmserver2 = std::make_shared<MockServer2>(sc, proc);
    result = m_pmserver2->Start();
    delay(100);
    if (result)
    {
        delay(1000);
        //delay(5500);
        LOG << "+====================2========================";
        m_pmserver2->Stop();
    }
    else
    {
        LOG << "+=============++++++++++++++++=======================";
    }
    delay(100);
    result = m_pmserver2->isError() && isRaised;
    m_pmserver2.reset();
    sc.m_proc.reset();
    TServer::DestroyServer();

    ILogger::stop_log();

    ASSERT_EQ(result, true);
}
*/

#define AUTO_TEST
#if defined(__GNUC__) || defined(__MINGW32__)
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wunused-parameter"
#endif
int main(int argc, char *argv[])
{

#ifdef AUTO_TEST
        ::testing::InitGoogleTest(&argc, argv);
            return RUN_ALL_TESTS();
#else
    const char * FUNCTION = __FUNCTION__;
    try
    {
        using std::cout;
        using std::cin;
        using std::endl;
        ILogger::start_log();
        std::unique_ptr<ISMP> psproc = std::make_unique<ServerProcessor>(true);

        auto pserver = TServer::CreateServer("127.0.0.1",
                                             1080,
                                             std::move(psproc),
                                             2);

        //auto pcproc = std::make_unique<ClientProcessor>("127.0.0.1");
        auto pcproc = std::make_unique<ClientProcessor>();
        auto pclient = std::make_shared<TClient>("127.0.0.1",
                                                 1080,
                                                 1,
                                                 pcproc.get());

        std::string str_command;
        while(1)
        {
            LOG << "enter command:";
            std::getline(cin, str_command);
            LOG << "command:" << str_command << " was got";
            if (str_command == "q") break;
            if (str_command == "e") throw 0;
            if (str_command == "s")
            {
                pserver->Start();
            }
            if (str_command == "d") pserver->Stop();
            if (str_command == "c") pclient->Connect();
            if (str_command == "t")
            {
                DBGOUT("server statistics: \n")
                DBGOUT(pserver->getHandlerStatistics())
            }
            if (str_command == "o")
            {
                DBGOUT("server statistics: \n")
                DBGOUT(pclient->getHandlerStatistics())
            }
            //if (pclient->isError()) break;
            str_command = "";
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
        }
        pserver.reset();
        pclient.reset();
        psproc.reset();
        pcproc.reset();
        TServer::DestroyServer();
        std::this_thread::sleep_for(std::chrono::seconds(1));

        ILogger::stop_log();
    }
    catch (...)
    {
        errspace::show_errmsg(FUNCTION);
        return EXIT_FAILURE;
    }
#endif
    return EXIT_SUCCESS;
}
#if defined(__GNUC__) || defined(__MINGW32__)
#pragma GCC diagnostic pop
#endif
