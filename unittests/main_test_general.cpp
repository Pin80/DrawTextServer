/** @file
 *  @brief         Модульный тест
 *  @author unnamed
 *  @date   created 10.01.2020
 *  @date   modified 10.02.2020
 *  @version 1.0 (alpha)
 **/
#include <iostream>
#include <gtest/gtest.h>
#include "../include/common.h"
#include "../include/error.h"
#include "../include/logger.h"
#include "../include/watchdog.h"
#include "../include/mng_thread.h"
#include "../include/processor.h"
#include "../include/comb_packet.h"

DEFINEWATCHPOINT

#if defined(__clang__) || defined (__GNUC__)
# define ATTRIBUTE_NO_SANITIZE_ADDRESS __attribute__((no_sanitize_address))
#else
# define ATTRIBUTE_NO_SANITIZE_ADDRESS
#endif

static std::atomic_bool  is_wdtimeout = false;

void on_wdtimeout(siginfo_t *si) noexcept
{
    is_wdtimeout = true;
    errspace::show_errmsg("watchdog timeout");
}


///@class TTestBench
///@brief Class for the test of algorithm.
class TTestBench_thread : public ::testing::Test
{
public:
    using error_code = boost::system::error_code;
    class MockCompleter : public binpack_completer_t
    {
    public:
        MockCompleter(upair_t& _buff) : binpack_completer_t(_buff)
        {
            //
        }
    };
    class TDummy {};
    /// Run of testing for some dataset or user input before
    bool Run();
    /// Destructor of TTestBench class
    virtual ~TTestBench_thread() = default;
    void test1_thread_func(IThread *,TDummy *, TDummy&)
    {
        bool isRunning = m_testthread->isRunning();
        while(isRunning)
        {
            isRunning = m_testthread->isRunning();
            is_thread_passed = true;
        }
    }
    void test2_thread_func(IThread *)
    {
        throw "exception";
    }
    void test3_thread_func(IThread *)
    {
         while(1)
         {
             std::cerr << "ping" << std::endl;
             std::this_thread::sleep_for(std::chrono::milliseconds(1000));
         };
    }
    void Process(IThread * _th, char _code = 0)
    {
        bool isRunning = _th->isRunning();
        auto tid = _th->GetThreadID();
        LOG << "reset loop is started";
        while(isRunning)
        {
            isRunning = _th->isRunning();
            auto delay = 100 + 50*_code;
            std::this_thread::sleep_for(std::chrono::milliseconds(delay));
            bool result = _th->feedwd();
            DBGOUT4("watch dog feed=", result, str_sum("code:", _code), 0)
            result = _th->feedwd();
            DBGOUT4("watch dog feed=", result, str_sum("code:", _code), 0)
        }
        LOG << "reset loop is stoped";
    }

    void Process_nofeedwd(IThread * _th, char _code = 0)
    {
        bool isRunning = _th->isRunning();
        auto tid = _th->GetThreadID();
        LOG << "reset loop is started";
        //ns_wd::stop_wd();
        while(isRunning)
        {
            isRunning = _th->isRunning();
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
        LOG << "reset loop is stoped";
    }
protected:
    using TRef = std::reference_wrapper<TDummy>;
    using threadfunc_t = void (TTestBench_thread::*)(IThread *,TDummy*, TDummy&);
    using threadfunc_t2 = void (TTestBench_thread::*)(IThread *);
    using Tmng1 = managedThread_t<threadfunc_t2, TTestBench_thread, false>;
    managedThread_t<threadfunc_t, TTestBench_thread, false, TDummy*, TRef > * m_testthread;
    managedThread_t<threadfunc_t2, TTestBench_thread, false> * m_testthread2;
    managedThread_t<threadfunc_t2, TTestBench_thread, false> * m_testthread3;
    TTestBench_thread* m_this = nullptr;
    std::atomic_bool is_thread_passed = false;
    void SetUp()
    {
        m_this = this;
    }
    void TearDown()  {     }
};

///@class TTestBench
///@brief Class for the test of algorithm.
class TTestBench_other : public ::testing::Test
{
public:
    using error_code = boost::system::error_code;
    class MockCompleter : public binpack_completer_t
    {
    public:
        MockCompleter(upair_t& _buff) : binpack_completer_t(_buff)
        {
            //
        }
    };
    class TDummy {};
    /// Run of testing for some dataset or user input before
    bool Run();
    /// Destructor of TTestBench class
    virtual ~TTestBench_other() = default;

protected:
    TTestBench_other* m_this = nullptr;
    std::atomic_bool is_thread_passed = false;
    static const char * fullname_http;
    static const char * fullname_bin;
    upair_t m_binpack_valid;
    cpconv_t::bnownbuffptr_t m_boostbinpack;
    charuptr_t m_binpack;
    void SetUp()
    {
        m_this = this;
    }
    void TearDown()  {     }
};

#define FILEPATH  "./Resources"
const char * TTestBench_other::fullname_bin =
        FILEPATH"/package1.bin";
const char * TTestBench_other::fullname_http =
        FILEPATH"/package2.http";

TEST_F(TTestBench_other, error_test1)
{
    bool result = errspace::is_errorlogfile_opened();
    ASSERT_EQ(result, true);
}

TEST_F(TTestBench_other, error_test2)
{
    bool result = false;
    const char * error_log_filename = TARGET"_.errlog";
    std::error_code ec;
    std::experimental::filesystem::path fpath;
    namespace fs = std::experimental::filesystem;
    fpath = fs::current_path(ec) / error_log_filename;
    if (ec) { ASSERT_EQ(result, true); return;}
    result = (fs::file_size(fpath) == 0);
    if (!result) { ASSERT_EQ(result, true); return; }
    errspace::show_errmsg("12");
    for(int i = 0; i < 4; i++)
    {
        errspace::show_errmsg("1234567890");
    }
    auto size = fs::file_size(fpath);
    result = ((size <= 64) && (size > 20));
    ASSERT_EQ(result, true);
}

TEST_F(TTestBench_other, logger_test1)
{
    bool result = false;
    const char * log_filename = TARGET"_.log";
    // trunc to 0
    auto logFile = fopen(log_filename, "w");
    fflush(logFile);
    fclose(logFile);
    std::error_code ec;
    std::experimental::filesystem::path fpath;
    namespace fs = std::experimental::filesystem;
    fpath = fs::current_path(ec) / log_filename;
    if (ec) { ASSERT_EQ(result, true); return;}
    result = (fs::file_size(fpath) == 0);
    if (!result) { ASSERT_EQ(result, true); return; }
    ILogger::start_log();
    ILogger::stop_log();
    ILogger::start_log();
    for(int i = 0; i < 5; i++)
    {
        LOG << "1234567890";
    }
    auto size = fs::file_size(fpath);
    //std::cerr << "____________________size:" << size << std::endl;
    result = ((size <= 256) && (size > 10));
    ILogger::stop_log();
    ASSERT_EQ(result, true);
}

TEST_F(TTestBench_other, logger_test2)
{
    bool result = false;
    const char * log_filename = TARGET"_.log";
    // trunc to 0
    auto logFile = fopen(log_filename, "w");
    fflush(logFile);
    fclose(logFile);
    std::error_code ec;
    std::experimental::filesystem::path fpath;
    namespace fs = std::experimental::filesystem;
    fpath = fs::current_path(ec) / log_filename;
    if (ec) { ASSERT_EQ(result, true); return;}
    result = (fs::file_size(fpath) == 0);
    if (!result) { ASSERT_EQ(result, true); return; }
    ILogger::start_log();
    LOG + "123__45__890";
    LOG + "12%%345++890";
    LOG + "1#345678#0";
    LOG << "errt";
    auto size = fs::file_size(fpath);
    result = ((size <= 256) && (size > 110));
    ILogger::stop_log();
    ASSERT_EQ(result, true);
}


TEST_F(TTestBench_thread, managedThread_t_test1)
{
    auto result = 1;
    auto pfunc = &TTestBench_thread::test1_thread_func;
    TDummy dm; TDummy& rdm = dm;
    m_testthread = new managedThread_t{pfunc, m_this, &dm, std::ref(dm)};
    m_testthread->StartLoop();
    is_thread_passed = false;
    std::this_thread::sleep_for(std::chrono::seconds(1));
    delete m_testthread;
    ASSERT_EQ(is_thread_passed, true);
}

TEST_F(TTestBench_thread, managedThread_t_test2)
{
    auto result = 1;
    auto pfunc = &TTestBench_thread::test2_thread_func;
    m_testthread2 = new managedThread_t{pfunc, m_this };
    m_testthread2->StartLoop();
    std::this_thread::sleep_for(std::chrono::seconds(1));
    bool testexpr = m_testthread2->isException() && !m_testthread2->isActive();
    ASSERT_EQ(testexpr, true);
    delete m_testthread2;
}


TEST_F(TTestBench_thread, managedThread_t_test3)
{
    auto pfunc = &TTestBench_thread::test1_thread_func;
    TDummy dm;
    m_testthread = new managedThread_t{pfunc, m_this, &dm, std::ref(dm) };
    m_testthread->StartLoop();
    std::this_thread::sleep_for(std::chrono::seconds(1));
    ASSERT_EQ(m_testthread->isActive() && !m_testthread->isException(), true);
    delete m_testthread;
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
}

/*
TEST_F(TTestBench_thread, managedThread_t_test4)
{
    auto pfunc = &TTestBench_thread::test3_thread_func;
    TDummy dm;
    Tmng1::ResetDestructorError();
    m_testthread2 = new managedThread_t{pfunc, m_this};
    m_testthread2->StartLoop();
    std::this_thread::sleep_for(std::chrono::seconds(1));
    delete m_testthread2;
    ASSERT_EQ(Tmng1::GetDestructorError(), true);
    Tmng1::ResetDestructorError();
}
*/


TEST_F(TTestBench_thread, managedThread_t_test5)
{
    auto pfunc = &TTestBench_thread::test3_thread_func;
    TDummy dm;
    Tmng1::ResetDestructorError();
    m_testthread2 = new managedThread_t{pfunc, m_this };
    std::this_thread::sleep_for(std::chrono::seconds(1));
    DBGOUT("atempt of deleting thread")
    delete m_testthread2;
    ASSERT_EQ(Tmng1::GetDestructorError(), false);
    Tmng1::ResetDestructorError();
}

void on_wdtimeout(siginfo_t *si) noexcept;

TEST_F(TTestBench_thread, WDTimer_test1)
{
    ILogger::start_log(false);
    auto pfunc = &TTestBench_thread::Process;
    TDummy dm;
    is_wdtimeout = false;
    Tmng1::ResetDestructorError();
    auto testthread2 = new managedThread_t{pfunc, m_this, 0 };
    testthread2->StartLoop();
    ns_wd::start_wd(500,10, &on_wdtimeout);
    std::this_thread::sleep_for(std::chrono::milliseconds(550));
    ns_wd::stop_wd();
    delete testthread2;
    ILogger::stop_log();
    ASSERT_EQ(is_wdtimeout, false);
}


TEST_F(TTestBench_thread, WDTimer_test2)
{
    auto pfunc = &TTestBench_thread::Process;
    TDummy dm;
    is_wdtimeout = false;
    ns_wd::start_wd(400,10, &on_wdtimeout);
    std::this_thread::sleep_for(std::chrono::milliseconds(550));
    ns_wd::stop_wd();
    ASSERT_EQ(is_wdtimeout, false);
}

TEST_F(TTestBench_thread, WDTimer_test3)
{
    ILogger::start_log(false);
    auto pfunc = &TTestBench_thread::Process_nofeedwd;
    TDummy dm;
    is_wdtimeout = false;
    Tmng1::ResetDestructorError();
    ns_wd::start_wd(500,10, &on_wdtimeout);
    auto testthread2 = new managedThread_t{pfunc, m_this, 0 };
    testthread2->StartLoop();
    std::this_thread::sleep_for(std::chrono::milliseconds(700));
    ns_wd::stop_wd();
    delete testthread2;
    ILogger::stop_log();
    ASSERT_EQ(is_wdtimeout, true);
}


TEST_F(TTestBench_thread, WDTimer_test4)
{
    ILogger::start_log(false);
    auto pfunc = &TTestBench_thread::Process_nofeedwd;
    TDummy dm;
    is_wdtimeout = false;
    Tmng1::ResetDestructorError();
    ns_wd::start_wd(500,10, &on_wdtimeout);
    auto testthread2 = new managedThread_t{pfunc, m_this, 0 };
    testthread2->StartLoop();
    delete testthread2;
    std::this_thread::sleep_for(std::chrono::milliseconds(800));
    ns_wd::stop_wd();
    ILogger::stop_log();
    ASSERT_EQ(is_wdtimeout, false);
}


TEST_F(TTestBench_thread, WDTimer_test5)
{
    ILogger::start_log(false);
    auto pfunc = &TTestBench_thread::Process;
    TDummy dm;
    is_wdtimeout = false;
    Tmng1::ResetDestructorError();
    ns_wd::start_wd(500,10, &on_wdtimeout);
    auto testthread2 = new managedThread_t{pfunc, m_this, 0 };
    testthread2->StartLoop();
    auto testthread3 = new managedThread_t{pfunc, m_this, 1 };
    testthread3->StartLoop();

    std::this_thread::sleep_for(std::chrono::milliseconds(2000));
    ns_wd::stop_wd();
    delete testthread2;
    delete testthread3;
    ILogger::stop_log();
    ASSERT_EQ(is_wdtimeout, false);
}

// bstmbuffptr_t -> bstmbuffptr_t
TEST_F(TTestBench_other, Outtype_test1)
{
    static constexpr auto TEST_SIZE = 123;
    cpconv_t::bstmbuffptr_t bufptr1 = std::make_unique<bstmbuff_t>();
    auto bufptr2 = charuptr_t();
    std::string s;
    s.resize(TEST_SIZE, '#');
    auto lambda_outbufbuf = [&s](cpconv_t _bufptr)
    {
        cpconv_t::bstmbuffptr_t bufptr = std::make_unique<bstmbuff_t>();
        std::ostream os(bufptr.get());
        os << s;
        _bufptr = bufptr;
    };
    lambda_outbufbuf(bufptr1);

    using iterator = std::istreambuf_iterator<char>;
    const auto size = bufptr1.get()->size();

    char * newbuf = (size > 0)?new char[size]:nullptr;
    auto it_dest_start = newbuf;
    iterator it_src_start(bufptr1.get());
    iterator it_src_end;
    std::copy(it_src_start, it_src_end, it_dest_start );

    bufptr2.reset(newbuf);
    auto is_equal = std::memcmp(s.c_str(), bufptr2.get(), TEST_SIZE);
    //std::cerr << "size:" << size << std::endl;
    bool result = ((size == TEST_SIZE) && (is_equal == 0));
    ASSERT_EQ(result, true);
    //bufptr1.reset();
}

// bstmbuffptr_t -> upair_t
TEST_F(TTestBench_other, Outtype_test2)
{
    static constexpr std::size_t TEST_SIZE = 123;
    upair_t pair1{};
    std::string src_str = "";
    src_str.resize(TEST_SIZE - 100, '#');
    src_str.resize(TEST_SIZE - 50, 0);
    src_str.resize(TEST_SIZE, '&');
    auto lambda_outbufbuf = [&src_str](cpconv_t _bufptr)
    {
        cpconv_t::bstmbuffptr_t bufptr = std::make_unique<bstmbuff_t>();
        std::ostream os(bufptr.get());
        os.write(src_str.c_str(), src_str.size());
        os.flush();
        _bufptr = bufptr;
    };
    lambda_outbufbuf(pair1);
    auto is_equal = std::memcmp(pair1.first.get(), src_str.c_str(), TEST_SIZE);
    //std::cerr << "cnt:" << pair1.second << " cmpres=" << is_equal << std::endl;
    bool result = (pair1.first.get() != nullptr) &&
            (pair1.second == TEST_SIZE) && (is_equal == 0);
    ASSERT_EQ(result, true);
}

// upair_t -> bstmbuffptr_t
TEST_F(TTestBench_other, Outtype_test3)
{
    static constexpr auto TEST_SIZE = 323;
    cpconv_t::bstmbuffptr_t bufptr1 = std::make_unique<bstmbuff_t>();
    upair_t pair1{};
    auto bufptr2 = charuptr_t();
    auto lambda_outbufbuf = [&pair1](cpconv_t _bufptr)
    {

        unsigned char * ptr_start = new unsigned char[TEST_SIZE];
        unsigned char * ptr_end = ptr_start + TEST_SIZE;
        pair1.first.reset((char *)ptr_start);
        std::size_t i1 = 0;
        while (i1++ < TEST_SIZE ) { *ptr_start++ = (unsigned char)i1; }
        pair1.second = TEST_SIZE;
        _bufptr = pair1;
    };
    lambda_outbufbuf(bufptr1);
    using iterator = std::istreambuf_iterator<char>;
    const auto size = bufptr1.get()->size();

    char * newbuf = new char[size];
    auto it_dest_start = newbuf;
    iterator it_src_start(bufptr1.get());
    iterator it_src_end;
    std::copy(it_src_start, it_src_end, it_dest_start );

    bufptr2.reset(newbuf);
    auto is_equal = std::memcmp(pair1.first.get(), bufptr2.get(), TEST_SIZE);
    //std::cerr << "size:" << size << std::endl;
    bool result = ((size == TEST_SIZE) && (is_equal == 0));
    ASSERT_EQ(result, true);
}

// upair_t -> upair_t
TEST_F(TTestBench_other, Outtype_test4)
{
    static constexpr auto TEST_SIZE = 323;
    upair_t pair_src{};
    upair_t pair1{};
    auto bufptr2 = charuptr_t();
    auto lambda_outbufbuf = [&pair1](cpconv_t _bufptr)
    {

        unsigned char * ptr_start = new unsigned char[TEST_SIZE];
        unsigned char * ptr_end = ptr_start + TEST_SIZE;
        pair1.first.reset((char *)ptr_start);
        std::size_t i1 = 0;
        while (i1++ < TEST_SIZE ) { *ptr_start++ = (unsigned char)i1; }
        pair1.second = TEST_SIZE;
        _bufptr = pair1;
    };
    lambda_outbufbuf(pair_src);
    using iterator = std::istreambuf_iterator<char>;
    const auto size = pair_src.second;


    auto is_equal = std::memcmp(pair1.first.get(),
                                pair_src.first.get(), TEST_SIZE);
    //std::cerr << "size:" << size << std::endl;
    bool result = ((size == TEST_SIZE) && (is_equal == 0));
    ASSERT_EQ(result, true);
}

// bstmbuffptr_t -> bstmbuffptr_t
TEST_F(TTestBench_other, Intype_test1)
{
    static constexpr std::size_t TEST_SIZE = 123;

    std::string src_str = "";
    src_str.resize(TEST_SIZE - 100, '#');
    src_str.resize(TEST_SIZE - 50, 0);
    src_str.resize(TEST_SIZE, '&');
    cpconv_t::bstmbuffptr_t bufptr = std::make_unique<bstmbuff_t>();
    auto lambda_inpbufbuf = [&src_str](mvconv_t _bufptr) -> bool
    {
        cpconv_t::bstmbuffptr_t bufptr_dest = _bufptr.extractBufPtr();
        using iterator = std::istreambuf_iterator<char>;
        const auto size = bufptr_dest->size();

        char * newbuf = new char[size];
        auto it_dest_start = newbuf;
        iterator it_src_start(bufptr_dest.get());
        iterator it_src_end;
        std::copy(it_src_start, it_src_end, it_dest_start );
        auto bufptr2 = charuptr_t();
        bufptr2.reset(newbuf);
        auto is_equal = std::memcmp(src_str.c_str(), bufptr2.get(), TEST_SIZE);
        return is_equal == 0;
    };
    std::ostream os(bufptr.get());
    os.write(src_str.c_str(), src_str.size());
    os.flush();
    bool result = lambda_inpbufbuf(std::move(bufptr));
    ASSERT_EQ(result, true);
}

// bstmbuffptr_t -> upair_t
TEST_F(TTestBench_other, Intype_test2)
{
    static constexpr auto TEST_SIZE = 323;
    std::string src_str = "";
    src_str.resize(TEST_SIZE - 100, '#');
    src_str.resize(TEST_SIZE - 50, 0);
    src_str.resize(TEST_SIZE, '&');
    auto lambda_inpbufbuf = [&src_str](mvconv_t _bufptr) -> bool
    {
        upair_t pair1 = _bufptr.extractPair();
        if (pair1.second != TEST_SIZE)
        {
            return false;
        }
        auto ptr = pair1.first.get();
        auto is_equal = std::memcmp(src_str.c_str(), ptr, TEST_SIZE);
        return is_equal == 0;
    };
    cpconv_t::bstmbuffptr_t bufptr = std::make_unique<bstmbuff_t>();
    std::ostream os(bufptr.get());
    os.write(src_str.c_str(), src_str.size());
    os.flush();
    bool result = lambda_inpbufbuf(std::move(bufptr));
    ASSERT_EQ(result, true);
}

// upair_t -> bstmbuffptr_t
TEST_F(TTestBench_other, Intype_test3)
{
    static constexpr std::size_t TEST_SIZE = 123;
    unsigned char * ptr = new unsigned char[TEST_SIZE];
    charuptr_t ptr_smart;
    ptr_smart.reset(reinterpret_cast<char *>(ptr));
    std::size_t i1 = 0;
    while (i1++ < TEST_SIZE ) { *ptr++ = (unsigned char)i1; }
    upair_t pair1{};
    unsigned char * ptr_start = new unsigned char[TEST_SIZE];
    unsigned char * ptr_end = ptr_start + TEST_SIZE;
    pair1.first.reset((char *)ptr_start);
    memcpy(ptr_start, ptr_smart.get(), TEST_SIZE);
    auto lambda_inpbufbuf = [&ptr_smart](mvconv_t _bufptr) -> bool
    {

        cpconv_t::bstmbuffptr_t bufptr_dest = _bufptr.extractBufPtr();
        if (!bufptr_dest) return false;
        const auto size = bufptr_dest.get()->size();
        if (size != TEST_SIZE)
        {
            return false;
        }
        //std::cerr << "size=" << size << std::endl;
        using iterator = std::istreambuf_iterator<char>;
        iterator it_src_start(bufptr_dest.get());
        iterator it_src_end;


        char * newbuf = new char[size];
        auto bufptr2 = charuptr_t();
        bufptr2.reset(newbuf);
        auto it_dest_start = newbuf;
        std::copy(it_src_start, it_src_end, it_dest_start );
        bool is_equal = false;
        //return false;
        is_equal = std::memcmp(ptr_smart.get(),
                                    newbuf, TEST_SIZE);
        return is_equal == 0;
    };
    pair1.second = TEST_SIZE;

    bool result = lambda_inpbufbuf(std::move(pair1));
    ASSERT_EQ(result, true);
}

// upair_t -> upair_t
TEST_F(TTestBench_other, Intype_test4)
{
    static constexpr std::size_t TEST_SIZE = 123;
    unsigned char * ptr = new unsigned char[TEST_SIZE];
    charuptr_t ptr_smart;
    ptr_smart.reset(reinterpret_cast<char *>(ptr));
    std::size_t i1 = 0;
    while (i1++ < TEST_SIZE ) { *ptr++ = (unsigned char)i1; }
    auto lambda_inpbufbuf = [&ptr_smart](mvconv_t _bufptr) -> bool
    {

        upair_t ptr_dest = _bufptr.extractPair();
        if (!ptr_dest.first) return false;
        const auto size = ptr_dest.second;
        if (size != TEST_SIZE)
        {
            return false;
        }
        //std::cerr << "size=" << size << std::endl;

        bool is_equal = std::memcmp(ptr_smart.get(),
                                    ptr_dest.first.get(), TEST_SIZE);
        return is_equal == 0;
    };
    upair_t pair1{};
    unsigned char * ptr_start = new unsigned char[TEST_SIZE];
    unsigned char * ptr_end = ptr_start + TEST_SIZE;
    pair1.first.reset((char *)ptr_start);
    memcpy(ptr_start, ptr_smart.get(), TEST_SIZE);
    pair1.second = TEST_SIZE;
    bool result = lambda_inpbufbuf(std::move(pair1));
    ASSERT_EQ(result, true);
}

TEST_F(TTestBench_other, Compl_test1)
{
    bool result = false;
    ILogger::start_log();
    std::ifstream ifstm;
    ifstm.open(fullname_bin);
    if (!ifstm.is_open())
    {
        ASSERT_EQ(result, true);
    }
    // get length of file:
    ifstm.seekg (0, std::ios::end);
    m_binpack_valid.second = ifstm.tellg();
    ifstm.seekg (0, std::ios::beg);
    char * ptr = new char[m_binpack_valid.second];
    ifstm.read(ptr, m_binpack_valid.second);
    m_binpack_valid.first.reset(ptr);
    std::string tmpstr =  "read from file:" +
            std::to_string(m_binpack_valid.second);
    ptr = new char[m_binpack_valid.second*10];
    m_binpack.reset(ptr);
    m_boostbinpack = std::make_unique<bnownbuff_t>(ptr,
                                          m_binpack_valid.second*10);
    //std::cerr << tmpstr;
    MockCompleter mockcompl(m_binpack_valid);
    auto size = boost::asio::buffer_size(*m_boostbinpack.get());
    std::cerr << "boost buff size:" << size << std::endl;
    error_code code;
    constexpr auto defsize = ICompleter_t::default_max_transfer_size;
    // No signat
    result = (mockcompl.do_completer(code, 0, 0) == defsize);
    if (!result)
    {
            ASSERT_EQ(result, true);
            return;
    }
    std::memcpy((void *)m_binpack.get(), m_binpack_valid.first.get(), 3);
    result = (mockcompl.do_completer(code, 3, 0) == defsize);
    if (!result)
    {
            ASSERT_EQ(result, true);
            return;
    }
    result = COMPARE(1, binpack_completer_t::eCmplState::noSign)
    if (!result)
    {
            ASSERT_EQ(result, true);
            return;
    }
    std::memcpy((void *)m_binpack.get(), m_binpack_valid.first.get(), 6);
    result = (mockcompl.do_completer(code, 6, 0) == defsize);
    if (!result)
    {
            ASSERT_EQ(result, true);
            return;
    }
    result = COMPARE(2, binpack_completer_t::eCmplState::noLng)
    if (!result)
    {
            ASSERT_EQ(result, true);
            return;
    }
    std::memcpy((void *)m_binpack.get(), m_binpack_valid.first.get(), 12);
    result = (mockcompl.do_completer(code, 12, 0) == defsize);
    if (!result)
    {
            ASSERT_EQ(result, true);
            return;
    }
    result = COMPARE(2, binpack_completer_t::eCmplState::noLng)
    if (!result)
    {
            ASSERT_EQ(result, true);
            return;
    }
    std::memcpy((void *)m_binpack.get(),
                m_binpack_valid.first.get(),
                m_binpack_valid.second);
    result = (mockcompl.do_completer(code, m_binpack_valid.second, 0) == 0);
    ILogger::stop_log();
    ASSERT_EQ(result, true);
}


TEST_F(TTestBench_other, Compl_test2)
{
    bool result = false;
    ILogger::start_log();
    std::ifstream ifstm;
    ifstm.open(fullname_http);
    if (!ifstm.is_open())
    {
        ASSERT_EQ(result, true);
    }
    // get length of file:
    ifstm.seekg (0, std::ios::end);
    m_binpack_valid.second = ifstm.tellg();
    ifstm.seekg (0, std::ios::beg);
    char * ptr = new char[m_binpack_valid.second];
    ifstm.read(ptr, m_binpack_valid.second);
    m_binpack_valid.first.reset(ptr);
    std::string tmpstr =  "read from file:" +
            std::to_string(m_binpack_valid.second);
    ptr = new char[m_binpack_valid.second*10];
    m_binpack.reset(ptr);
    m_boostbinpack = std::make_unique<bnownbuff_t>(ptr,
                                          m_binpack_valid.second*10);
    //std::cerr << tmpstr;
    MockCompleter mockcompl(m_binpack_valid);
    auto size = boost::asio::buffer_size(*m_boostbinpack.get());
    std::cerr << "boost buff size:" << size << std::endl;
    error_code code;
    constexpr auto defsize = ICompleter_t::default_max_transfer_size;
    // No signat
    result = (mockcompl.do_completer(code, 0, 0) == defsize);
    if (!result) { ASSERT_EQ(result, true); return; }

    std::memcpy((void *)m_binpack.get(), m_binpack_valid.first.get(), 3);
    result = (mockcompl.do_completer(code, 3, 0) == defsize);
    if (!result) { ASSERT_EQ(result, true); return; }

    result = COMPARE(1, binpack_completer_t::eCmplState::noSign)
    if (!result) { ASSERT_EQ(result, true); return; }

    std::memcpy((void *)m_binpack.get(), m_binpack_valid.first.get(), 6);
    result = ((mockcompl.do_completer(code, 6, 0) == 0) && mockcompl.isError());
    ASSERT_EQ(result, true);
    ILogger::stop_log();
    ASSERT_EQ(result, true);
}

struct TCairoCl
{
    std::unique_ptr<value_t[]> m_ptr;
    std::size_t buffsize = 0;
};

cairo_status_t read_from_pngbuff(void * _closure,
                                 value_t * _data,
                                 unsigned int _length)
{
    auto closure = reinterpret_cast<TCairoCl *>(_closure);
    if (!closure)
    {
        return CAIRO_STATUS_PNG_ERROR;
    }
    auto offset = closure->buffsize;
    auto * currptr = closure->m_ptr.get() + offset;
    if (!currptr)
    {
        return CAIRO_STATUS_PNG_ERROR;
    }
    memcpy(_data, currptr , _length);
    closure->buffsize += _length;
    return CAIRO_STATUS_SUCCESS;
}

cairo_status_t write_to_pngbuff(void * _closure,
                                const value_t * _data,
                                unsigned int _length)
{
    auto closure = reinterpret_cast<TCairoCl *>(_closure);
    if (!closure)
    {
        return CAIRO_STATUS_PNG_ERROR;
    }
    auto totalsize = closure->buffsize + _length;
    std::unique_ptr<value_t[]> spcurrptr;
    auto currptr = new value_t[totalsize];
    spcurrptr.reset(currptr);
    if (closure->buffsize)
    {
        memcpy(currptr, closure->m_ptr.get(), closure->buffsize);
    }
    memcpy(currptr + closure->buffsize, _data, _length);
    closure->buffsize += _length;
    closure->m_ptr = std::move(spcurrptr);
    return CAIRO_STATUS_SUCCESS;
}

TEST_F(TTestBench_other, Cairo_test1)
{
    ILogger::start_log();
    static constexpr auto path = "./Resources/";

    typedef std::unique_ptr<value_t> TCairoBuffPtr;
    using Tpackstruct = std::shared_ptr<compndpack_t::unpacked_tComb>;
    std::string fullname = path;
    std::string fullname_out = path;
    fullname += "img_in.png";
    fullname_out += "test_out.png";
    bool result = false;
    std::ifstream istm;
    istm.open(fullname);
    istm.seekg(0, std::ios::end);
    auto fsize = istm.tellg();
    istm.seekg(0, std::ios::beg);
    upair_t pair;
    compndpack_t topack;
    auto ptr_in = new char[fsize];
    pair.first.reset(ptr_in);
    pair.second = fsize;
    istm.read(ptr_in, fsize);
    istm.close();
    std::string imgstr =  "Hellow World (!!!)";

    // ========================= make packet ============================
    upair_t packet = topack.make_packet(pair, imgstr.c_str());
    pair.first.reset();
    //=========================== unpack ====================================
    compndpack_t frompack;
    //packet.first.reset();

    auto unpack = frompack.parse_packet(packet);
    result = unpack.m_current.has_value();
    if (!result)
    {
        unpack.m_next.reset();
        unpack.m_current.reset();
        ASSERT_EQ(result, true);
        ILogger::stop_log();
        return;
    }
    auto uc = std::any_cast<Tpackstruct>(unpack.m_current);
    unpack.m_next.reset();
    unpack.m_current.reset();
    auto closure_ptr_read = std::make_unique<TCairoCl>();
    closure_ptr_read->m_ptr = std::move(uc->m_data);
    imgstr = uc->m_str;
    uc.reset();

    //============================== create image matrix =========================
    auto surf_src = cairo_image_surface_create_from_png_stream
                                   (&read_from_pngbuff,
                                    closure_ptr_read.get());
    result = (cairo_surface_status(surf_src) == CAIRO_STATUS_SUCCESS);
    closure_ptr_read.reset();
    upair_t pair2;
    //============================== make image with text ===================
    if (result)
    //if (0)
    {
        cairo_user_data_key_t key;
        auto img_buffer = cairo_image_surface_get_data(surf_src);
        auto stride = cairo_image_surface_get_stride (surf_src);
        auto columns = cairo_image_surface_get_width(surf_src);
        auto rows = cairo_image_surface_get_height (surf_src);
        LOG << str_sum("stride:", stride);
        LOG << str_sum("rows:", rows);

        auto newbuff = new value_t[stride*rows];
        memcpy(newbuff, img_buffer, stride*rows);
        auto surf_dest = cairo_image_surface_create_for_data (newbuff,
                                                              CAIRO_FORMAT_RGB24,
                                                              columns,
                                                              rows,
                                                              stride);

        //delete img_buffer;

        auto closure_ptr_write = std::make_unique<TCairoCl>();

        cairo_surface_write_to_png_stream(surf_dest,
                                          &write_to_pngbuff,
                                          closure_ptr_write.get());
        result = (cairo_surface_status(surf_dest) == CAIRO_STATUS_SUCCESS);
        delete []newbuff;
        cairo_surface_destroy(surf_dest);
        char * ptr = valconv_t(closure_ptr_write->m_ptr.get());
        closure_ptr_write->m_ptr.release();
        pair2.first.reset(ptr);
        pair2.second = closure_ptr_write->buffsize;
        closure_ptr_write.reset();
    }
    cairo_surface_destroy(surf_src);
    ASSERT_EQ(result, true);
    ILogger::stop_log();
    compndpack_t topack2;
    auto packet2 = topack2.make_packet(pair2,"");
    compndpack_t frompack2;
    if (result)
    {
        pair2.first.reset();
        auto unpack2 = frompack2.parse_packet(packet2);
        result = unpack2.m_current.has_value();
        if (!result)
        {
            unpack.m_next.reset();
            unpack.m_current.reset();
            ASSERT_EQ(result, true);
            ILogger::stop_log();
            return;
        }
        //===============================================================
        if (result)
        {
            auto uc2 = std::any_cast<Tpackstruct>(unpack2.m_current);
            unpack2.m_next.reset();
            unpack2.m_current.reset();
            std::ofstream ostm;
            ostm.open(fullname_out);
            ostm.write(valconv_t(uc2->m_data.get()), uc2->m_imgsize);
            ostm.close();
        }
    }
    ASSERT_EQ(result, true);
    ILogger::stop_log();
}


TEST_F(TTestBench_other, Cairo_test2)
{
    ILogger::start_log();
    bool result = false;
    static constexpr auto path = "./Resources/";
    std::string fullname = path;
    std::string fullname_out = path;
    fullname += "img_in.png";
    fullname_out += "test_out2.png";
    auto surf_src = cairo_image_surface_create_from_png(fullname.c_str());
    result = (cairo_surface_status(surf_src) == CAIRO_STATUS_SUCCESS);

    //============================== make image with text ===================
    if (result)
    {
        cairo_user_data_key_t key;
        auto img_buffer = cairo_image_surface_get_data(surf_src);
        auto stride = cairo_image_surface_get_stride (surf_src);
        auto columns = cairo_image_surface_get_width(surf_src);
        auto rows = cairo_image_surface_get_height (surf_src);
        LOG << str_sum("stride:", stride);
        LOG << str_sum("rows:", rows);

        cairo_t *cr = cairo_create(surf_src);

        cairo_set_source_surface (cr, surf_src, 0, 0);
        cairo_paint(cr);

        // Draw some text
        cairo_select_font_face (cr, "monospace",
                                CAIRO_FONT_SLANT_NORMAL,
                                CAIRO_FONT_WEIGHT_NORMAL);
        cairo_set_font_size (cr, 14);
        cairo_set_source_rgb (cr, 0, 0, 0);
        cairo_move_to (cr, 50, 50);
        //cairo_show_text (cr, imgstr.c_str());
        cairo_show_text (cr, "12345677");
        cairo_surface_write_to_png(surf_src, fullname_out.c_str());
        result = (cairo_surface_status(surf_src) == CAIRO_STATUS_SUCCESS);
        cairo_destroy(cr);
        cairo_surface_destroy(surf_src);
        cairo_debug_reset_static_data();
        FcFini();
    }
    ASSERT_EQ(result, true);
    ILogger::stop_log();
}


TEST_F(TTestBench_other, lfu_test1)
{
    struct Y
    {
        std::unique_ptr<int> u;
    };
    struct A
    {
        std::any a;
    };
    A a;
    Y y;
   //a.a = std::move(y);
}

class Reseter
{
    using threadfunc_t = void (Reseter::*)(IThread *);
    using Mng = managedThread_t<threadfunc_t, Reseter, false>;
    using MngPtr = std::unique_ptr<Mng>;
    MngPtr m_pthread;
public:
    Reseter()
    {
        Mng* pthread = new managedThread_t{&Reseter::Process, this};
        m_pthread.reset(pthread);
        m_pthread->StartLoop();
    }
    virtual ~Reseter()
    {
        if (m_pthread)
        {
            m_pthread.reset();
        }
        LOG << "Reseter is terminated";
    }
    void Process(IThread * _th)
    {
        auto pthread = m_pthread.get();
        bool isRunning = pthread->isRunning();
        auto tid = pthread->GetThreadID();
        LOG << "reset loop is started";
        //ns_wd::stop_wd();
        while(isRunning)
        {
            isRunning = pthread->isRunning();
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            _th->feedwd();
        }
        LOG << "reset loop is stoped";
    }
};

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
        LOG << "Server is started";
        std::string str_command;
        Reseter * reseter = nullptr;
        ClientProcessor * cproc = nullptr;
        ServerProcessor * sproc = nullptr;
        while(!ns_wd::is_timeout())
        {
            if (!std::cin)
            {
              std::cin.clear();
            }
            else
            {
                LOG << "enter command:";
            }
            if (std::getline(cin, str_command))
            {
                LOG << "command:" << str_command << " was got";
                if (str_command == "q") break;
                if (str_command == "e") throw 0;
                if (str_command == "s")
                {
                    ns_wd::start_wd(1000, &on_wdtimeout);
                }
                if (str_command == "p")
                {
                    ns_wd::stop_wd();
                }
                if (str_command == "r")
                {
                    reseter = new Reseter;
                }
                if (str_command == "d")
                {
                    delete reseter;
                    reseter = nullptr;
                }
                if (str_command == "y")
                {
                    cproc = new ClientProcessor;
                    sproc = new ServerProcessor;
                }
                if (str_command == "n")
                {
                    delete cproc;
                    delete sproc;
                }
                if (str_command == "k")
                {
                    sproc->debug(cproc);
                }
            }
            //if (pclient->isError()) break;
            str_command = "";
            //pclient->Process();
        }
        if (reseter)
        {
            delete reseter;
            reseter = nullptr;
        }
        LOG << "Server app is terminated";
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
