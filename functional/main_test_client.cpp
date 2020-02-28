/** @file
 *  @brief         Функциональный тест
 *  @author unnamed
 *  @date   created 10.01.2020
 *  @date   modified 10.02.2020
 *  @version 1.0 (alpha)
 **/

#include <iostream>
#include <gtest/gtest.h>
#include "../include/common.h"
#include "../include/utility.h"
#include "../include/client.h"
#include "../include/error.h"
#include "../include/logger.h"
#include "../include/watchdog.h"
#include "../include/processor.h"
#include "./include/mock_processor2.h"

#if BUILDTYPE == BUILD_DEBUG
DEFINEWATCHPOINT
#endif

///@class TTestBench
///@brief Class for the test of algorithm.
class TTestBench : public ::testing::Test
{
public:
    /// Run of testing for some dataset or user input before
    bool Run();
    /// Destructor of TTestBench class
    virtual ~TTestBench() = default;
    std::shared_ptr<TClient> m_client;
    clientconfig_t cc;
protected:
    void SetUp()
    {
        ILogger::start_log();

        cc.m_host = "www.ya.ru";
        cc.m_port = 80;
        cc.m_paral = 4;
        cc.m_pproc = std::make_shared<ClientProcessor>(cc.m_host);
        m_client = std::make_shared<TClient>(cc);
    }
    void TearDown()
    {
        m_client.reset();
        cc.m_pproc.reset();
        ILogger::stop_log();
    }
};

TEST_F(TTestBench, test1)
{
    //const std::string test_string = R"([  "Hello World"  ])";
    //std::cout << test_string << std::endl;
    m_client->Connect();
    bool isWorking = m_client->isWorking();
    while (isWorking)
    {
        isWorking = m_client->isWorking();
        std::this_thread::yield();
    }
    auto ptr = dynamic_cast<ClientProcessor*>(cc.m_pproc.get());
    ASSERT_EQ(ptr->m_succescounter<>, 4);
}

TEST_F(TTestBench, test2)
{
    //const std::string test_string = R"([  "Hello World"  ])";
    //std::cout << test_string << std::endl;
    auto result = 1;
    ASSERT_EQ(result, 1);
}

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
        LOG << "Client is started";
        auto ppcproc = std::make_unique<ClientProcessor>("www.ya.ru");

        auto pclient = std::make_shared<TClient>("www.ya.ru",
                                                 80,
                                                 2,
                                                 ppcproc.get());
        std::string str_command;
        while(1)
        {
            std::cerr << "enter command:" << std::endl;
            std::getline(cin, str_command);
            std::cerr << "command:" << str_command << " was got" << std::endl;
            if (str_command == "q") break;
            if (str_command == "e") throw 0;
            if (str_command == "c") pclient->Connect();
            if (str_command == "g")
            {
                std::cerr << "#";
                std::cerr << std::to_string(ppcproc->m_succescounter<>);
                std::cerr << std::endl;
            }
            //if (str_command == "d") pclient->Disconnect(0);
            //if (pclient->isError()) break;
            str_command = "";
            //pclient->Process();
        }
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
