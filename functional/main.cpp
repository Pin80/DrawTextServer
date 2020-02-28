#include <iostream>
#include <gtest/gtest.h>
#include "../include/common.h"
#include "../include/client.h"
#include "../include/error.h"
#include "../include/logger.h"

const char * img_path = "/home/user/MySoftware/itv_testtask/itv_testtask/Resources/";

///@class TTestBench
///@brief Class for the test of algorithm.
class TTestBench : public ::testing::Test
{
public:
    /// Run of testing for some dataset or user input before
    bool Run();
    /// Destructor of TTestBench class
    virtual ~TTestBench() = default;
protected:
    void SetUp()
    {

    }
    void TearDown()
    {

    }
};

TEST_F(TTestBench, test1)
{
    const std::string test_string = R"([  "Hello World"  ])";
    std::cout << test_string << std::endl;
    auto result = 1;
    ASSERT_EQ(result, 1);
}

TEST_F(TTestBench, test2)
{
    const std::string test_string = R"([  "Hello World"  ])";
    std::cout << test_string << std::endl;
    auto result = 1;
    ASSERT_EQ(result, 1);
}


#if defined(__GNUC__) || defined(__MINGW32__)
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wunused-parameter"
#endif
int main(int argc, char *argv[])
{
        //::testing::InitGoogleTest(&argc, argv);
        //    return RUN_ALL_TESTS();
        const int BUFFER_SIZE = 200*1024;
        char buffer[BUFFER_SIZE];
        memset(buffer, 0, BUFFER_SIZE);
        std::ifstream ifs;
        std::string fname = img_path;
        fname += "img_1.jpg";
        LOG << "Start";
        ifs.open(fname.c_str(), std::ifstream::binary | std::ifstream::in);
        if (ifs.is_open())
        {
            ifs.read(buffer,BUFFER_SIZE);
            std::cout << "buffer size: " << ifs.gcount() << std::endl;
        }
            const char * FUNCTION = __FUNCTION__;
            try
            {
                using std::cout;
                using std::cin;
                using std::endl;
                cout << "Hello World!" << endl;
                //A a;
                auto pclient = std::make_shared<TClient>("ya.ru", 80);
                std::string str_command;
                while(1)
                {
                    LOG << "enter command:";
                    std::getline(cin, str_command);
                    LOG << "command:" << str_command << " was got";
                    if (str_command == "q") break;
                    if (str_command == "e") throw 0;
                    if (str_command == "c") pclient->Connect();
                    if (str_command == "d") pclient->Disconnect();
                    //if (pclient->isError()) break;
                    str_command = "";
                    //pclient->Process();
                }
            }
            catch (...)
            {
                errspace::show_errmsg(FUNCTION);
            }
            return 0;
}
#if defined(__GNUC__) || defined(__MINGW32__)
#pragma GCC diagnostic pop
#endif
