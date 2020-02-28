#ifndef MOCK_PROCESSOR2_H
#define MOCK_PROCESSOR2_H
#include <iostream>
#include <gtest/gtest.h>
#include "../../include/common.h"
#include "../../include/utility.h"
#include "../../include/client.h"
#include "../../include/error.h"
#include "../../include/logger.h"
#include "../../include/watchdog.h"
#include "../../include/processor.h"

class ClientProcessor: public IProcessor
{
public:
    ClientProcessor();
    ClientProcessor(std::string _host);
    virtual void get_input_data(cpconv_t _data,
                                const ticket_t& _tj);
    virtual bool process_output_data(mvconv_t _data,
                                     const ticket_t& _tk);
    virtual bool process_output_data(mvconv_t _data,
                                const ticket_t& _tk,
                                abstcomplptr_t _compl) override final
    { }
    virtual bool isAvailable(const ticket_t& _tk);
    bool compare_resp();
    bool compare_busy();
    template<class T = unsigned>
    static unsigned m_succescounter;
private:
    static const char * fullname_bin;
    static const char * fullname_http;
    static const char * simple_request;
    charuptr_t m_pcbuffer_valid;
    bstmbuffptr_t m_pbbuffer_req;
    upair_t m_pbbuffer_http_resp;
    charuptr_t m_pcbuffer_resp;
    std::size_t m_buffsize_val = 0;
    friend class ServerProcessor;
    void do_compose(const std::string& server);
    std::size_t m_size_valid;
};

template<>
unsigned ClientProcessor::m_succescounter<unsigned> = 0;

#define FILEPATH  "./Resources"
const char * ClientProcessor::fullname_bin =
        FILEPATH"/package1.bin";
const char * ClientProcessor::fullname_http =
        FILEPATH"/package2.http";

//        const auto path = "//";
//        std::ostream request_stream(&m_request);
//        request_stream << "GET " << path << " HTTP/1.0\r\n";
//        request_stream << "Host: " << server << "\r\n";
//        request_stream << "Accept: */*\r\n";
//        request_stream << "Connection: close\r\n\r\n";
const char * ClientProcessor::simple_request =
        "GET path HTTP/1.0\r\n"
        "Host: 127.0.0.1\r\n"
        "Accept: */*\r\n"
        "Connection: close\r\n\r\n";

ClientProcessor::ClientProcessor()
{
    const char * FUNCTION = __FUNCTION__;
    try
    {
        // Читаем из файла бинарный пакет для валидации
        char * buffer;
        std::ifstream istm;
        istm.open(fullname_bin, std::ios::binary );
        // get length of file:
        istm.seekg (0, std::ios::end);
        m_buffsize_val = istm.tellg();
        istm.seekg (0, std::ios::beg);
        // allocate memory:
        buffer = (m_buffsize_val > 0)?new char [m_buffsize_val]:nullptr;
        // read data as a block:
        istm.read (buffer, m_buffsize_val);
        istm.close();
        m_pcbuffer_valid.reset(buffer);
        LOG << "read:" << std::to_string(m_buffsize_val);
    }
    catch (...)
    {
        errspace::show_errmsg(FUNCTION);
        throw;
    }
}

ClientProcessor::ClientProcessor(std::string _host)
{
    const char * FUNCTION = __FUNCTION__;
    try
    {
        //char * ptr = new char; // test memory leakage
        //Читаем из файла текстовый пакет для валидации
        //m_pbbuffer_valid = std::make_unique<bstmbuff_t>();
        //std::ostream ostm(m_pbbuffer_valid.get());
        std::ifstream ifstm;
        ifstm.open(fullname_http);
        if (ifstm.is_open())
        {
            // get length of file:
            ifstm.seekg (0, std::ios::end);
            m_buffsize_val = ifstm.tellg();
            ifstm.seekg (0, std::ios::beg);
            char * ptr = (m_buffsize_val > 0)?new char[m_buffsize_val]:nullptr;
            ifstm.read(ptr, m_buffsize_val);
            m_pcbuffer_valid.reset(ptr);
            ifstm.close();
            LOG << "read:" << std::to_string(m_buffsize_val);
            do_compose(_host);
        }
        else
        {
            throw "error";
        }
    }
    catch (...)
    {
        errspace::show_errmsg(FUNCTION);
        throw;
    }
}

void ClientProcessor::get_input_data(cpconv_t _data, const ticket_t& _tk)
{
    if (_data.getType() == cpconv_t::ebstmbuffptr_t)
    {
        if (m_pbbuffer_req.get())
        {
            _data = m_pbbuffer_req;
        }
    }
}

bool ClientProcessor::process_output_data(mvconv_t _data, const ticket_t& _tk)
{
    using iterator = std::istreambuf_iterator<char>;
    bool result = false;
    if (_data.getType() == cpconv_t::ebstmbuffptr_t)
    {
        bstmbuffptr_t pbbuffer_resp = _data.extractBufPtr();
        mvconv_t adapter = std::move(pbbuffer_resp);
        m_pbbuffer_http_resp = adapter.extractPair();
        result = compare_resp();
        std::cerr << "compare result:" << std::to_string(result) << std::endl;
    }
    return true;
}

bool ClientProcessor::compare_resp()
{
    std::size_t curr_size = m_pbbuffer_http_resp.second;
    //std::cerr << "size_val:" << m_buffsize_val;
    //std::cerr << "size1:" << curr_size << std::endl;

    std::string sv_val(m_pcbuffer_valid.get(), m_buffsize_val );
    std::string sv2(m_pbbuffer_http_resp.first.get(), curr_size );
    sv_val.resize(sv_val.size() + 1, 0);
    sv2.resize(sv_val.size() + 1, 0);
    std::string::iterator it_start = sv2.begin();
    std::string::iterator it_end = sv2.begin() + 10;
    std::string::iterator it_start_val = sv_val.begin();
    std::string::iterator it_end_val = sv_val.begin() + 10;

    bool result = false;
    //std::cerr << "--------val----------" << std::endl;
    //std::cerr << sv_val << std::endl;
    //std::cerr << "--------------------" << std::endl;
    //std::cerr << sv2 << std::endl;
    if (curr_size <= m_buffsize_val)
    {
        result =  std::equal(it_start, it_end, it_start_val);
    }
    else
    {
        result =  std::equal(it_start_val, it_end_val, it_start);
    }
    m_succescounter<> = (result)?m_succescounter<> + 1: m_succescounter<>;
    return  result;
}

void ClientProcessor::do_compose(const std::string &server)
{
    const auto path = "//";
    if (!m_pbbuffer_req.get())
    {
        m_pbbuffer_req = std::make_unique<bstmbuff_t>();
    }
    if (m_pbbuffer_req->size() == 0)
    {
        std::ostream request_stream(m_pbbuffer_req.get());
        request_stream << "GET " << path << " HTTP/1.0\r\n";
        request_stream << "Host: " << server << "\r\n";
        request_stream << "Accept: */*\r\n";
        request_stream << "Connection: close\r\n\r\n";
    }
}

bool ClientProcessor::isAvailable(const ticket_t& _tk)
{
    return true;
}

#endif // MOCK_PROCESSOR2_H
