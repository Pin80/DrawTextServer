/** @file
 *  @brief         Функциональный тест
 *  @author unnamed
 *  @date   created 10.01.2020
 *  @date   modified 10.02.2020
 *  @version 1.0 (alpha)
 */
#ifndef MOCK_PROCESSOR_H
#define MOCK_PROCESSOR_H
#include "../../include/common.h"
#include "../../include/server.h"
#include "../../include/client.h"
#include "../../include/error.h"
#include "../../include/logger.h"
#include "../../include/watchdog.h"
#include "../../include/processor.h"

#define FILEPATH  "./Resources"
const char * fullname_bin =
        FILEPATH"/package1.bin";
const char * fullname_http =
        FILEPATH"/package2.http";

//        const auto path = "//";
//        std::ostream request_stream(&m_request);
//        request_stream << "GET " << path << " HTTP/1.0\r\n";
//        request_stream << "Host: " << server << "\r\n";
//        request_stream << "Accept: */*\r\n";
//        request_stream << "Connection: close\r\n\r\n";
const char * simple_request =
        "GET path HTTP/1.0\r\n"
        "Host: 127.0.0.1\r\n"
        "Accept: */*\r\n"
        "Connection: close\r\n\r\n";

const char * simple_reply =
        "REPLY\r\n\r\n";
const char * simple_reply_good =
        "GOOD\r\n\r\n";
const char * simple_reply_bad =
        "BAD\r\n\r\n";
const char * simple_reply_ackw =
        "ACKNOLEDGE\r\n\r\n";
const char * simple_reply_busy =
        "BUSY\r\n\r\n";

inline auto cu_to_cs(const unsigned char * _ptr)
{
    return reinterpret_cast<const char *>(_ptr);
}

class ClientProcessor;

class ServerProcessor: public ISMP
{
public:
    ServerProcessor(bool _binpack = false,
                    const char* _reqvalfname = fullname_bin,
                    const char* _respfname = fullname_bin);
    virtual void get_input_data(spair_t& _data,
                                const ticket_t& _tk) override final;
    virtual bool process_output_data(mvconv_t _data,
                                const ticket_t& _tk) override final
    {}
    virtual bool process_output_data(mvconv_t _data,
                                ticket_t& _tk,
                                abstcomplptr_t _compl,
                                rdyHandlerPtr_t& _hnd) override final;
    virtual eCompl_t getComplType() override;
    virtual void setReadyHandler(const rdyHandlerPtr_t& _hnd,
                                 const ticket_t& _tk) override final;
    void debug(ClientProcessor * _proc);
    bool compare_resp(const ticket_t& _tk);
    virtual bool isAvailable(const ticket_t& _tk) override final;
    virtual void get_busy_data(cpconv_t _data) override final;
public:
    bool m_binarypack = false;
    std::size_t m_successes = 0;
    std::size_t m_fails = 0;
    // Буффер для хранения текстового ответа
    bstmbuffptr_t m_pbbuffer_rep;
    // Буффер для хранения бинарного ответа
    upair_t m_pbbufferbin_rep;
    // Эталон правильного запроса
    upair_t m_pbbuffer_req_valid;
    // Буффер хранения ответа "занят"
    upair_t m_pcbuffer_busy;
    // временно сделанный мбютекс
    std::mutex m_procmtx;
    struct context_t
    {
        // Буффер для копирования экземпляра запроса (бинарного или текстового)
        upair_t m_pbbuffer_req;
        bool m_isavail = false;
        std::uint8_t m_attemptcounter = 3;
        rdyHandlerPtr_t m_handlerptr;
    };
    std::map<ticket_t, context_t> m_contextpool;
};

ServerProcessor::ServerProcessor(bool _binpack,
                                 const char* _reqvalfname,
                                 const char* _respfname) :
    m_binarypack(_binpack)
{
    const char * FUNCTION = __FUNCTION__;
    try
    {
        if (m_binarypack)
        {
        }

            //Читаем из файла текстовый пакет для валидации запроса
            //  Читаем эталон запроса
            std::ifstream ifstm_req;
            ifstm_req.open(_reqvalfname);
            if (!ifstm_req.is_open())
            {
                throw "error";
            }
            // get length of file:
            ifstm_req.seekg (0, std::ios::end);
            auto size_req = ifstm_req.tellg();
            ifstm_req.seekg (0, std::ios::beg);
            m_pbbuffer_req_valid.second = size_req;
            char * ptr_req = (size_req > 0)?new char[size_req]:nullptr;
            ifstm_req.read(ptr_req, m_pbbuffer_req_valid.second);
            m_pbbuffer_req_valid.first.reset(ptr_req);
            DBGOUT2("s: valid request was red from file:", size_req)



            // Читаем даннные для ответа
            //Читаем из файла текстовый пакет для валидации
            std::ifstream ifstm_rep;
            ifstm_rep.open(_respfname);
            if (!ifstm_rep.is_open())
            {
                throw "error";
            }
            // get length of file:
            ifstm_rep.seekg (0, std::ios::end);
            std::size_t size_rep = ifstm_rep.tellg();
            ifstm_rep.seekg (0, std::ios::beg);
            char * ptr_rep = (size_rep > 0)?new char[size_rep]:nullptr;
            m_pbbufferbin_rep.first.reset(ptr_rep);
            ifstm_rep.read(ptr_rep, size_rep);
            ifstm_rep.close();
            m_pbbufferbin_rep.second = size_rep;
            DBGOUT2("s: reply was red from file:", size_rep)

            auto size_busy = strlen(simple_reply_busy);
            char * ptr_busy = (size_busy > 0)?new char[size_busy]:nullptr;
            m_pcbuffer_busy.first.reset(ptr_busy);
            memcpy(ptr_busy, simple_reply_busy, size_busy);
            m_pcbuffer_busy.second = size_busy;
    }
    catch (...)
    {
        errspace::show_errmsg(FUNCTION);
        throw;
    }
}


// Отдать экземляр ответа
void ServerProcessor::get_input_data(spair_t& _data,
                                     const ticket_t& _tk)
{
    std::unique_lock<std::mutex> lock(m_procmtx);
    {
        auto ptr = m_pbbufferbin_rep.first.get();

        upair_t buffer_rep;
        cpconv_t adapter(buffer_rep);
        adapter = m_pbbufferbin_rep;
        ptr = buffer_rep.first.get();
        _data.first = std::move(buffer_rep.first);
        _data.second = buffer_rep.second;
        ptr = m_pbbufferbin_rep.first.get();
        assert(ptr != nullptr);
        //std::ostream reply_stream(m_pbbuffer_rep.get());
        //reply_stream << simple_reply_good;
        //char * ptr = new char[m_pbbuffer_req_valid.second];
    }
    if (m_contextpool[_tk].m_handlerptr)
    {
        (*m_contextpool[_tk].m_handlerptr.get())();
    }
}

void ServerProcessor::get_busy_data(cpconv_t _data)
{
    std::unique_lock<std::mutex> lock(m_procmtx);
    _data = m_pcbuffer_busy;
    assert(m_pcbuffer_busy.first.get() != nullptr);
}

bool ServerProcessor::process_output_data(mvconv_t _data,
                                          ticket_t &_tk,
                                          abstcomplptr_t _compl,
                                          rdyHandlerPtr_t& _hnd)
{
    std::unique_lock<std::mutex> lock(m_procmtx);
    m_contextpool[_tk].m_pbbuffer_req = _data.extractPair();
    m_contextpool[_tk].m_attemptcounter = 5;
    bool result = compare_resp(_tk);
    if (result) m_successes++;
        else m_fails++;

    DBGOUT2("s: compare result(is reply good):", result)
    return true;
}

eCompl_t ServerProcessor::getComplType()
{
    std::unique_lock<std::mutex> lock(m_procmtx);
    if (m_binarypack)
    {
        return eCompl_t::bin;
    }
    return eCompl_t::none;
}

void ServerProcessor::setReadyHandler(const rdyHandlerPtr_t& _hnd,
                                      const ticket_t& _tk)
{
    m_contextpool[_tk].m_handlerptr = _hnd;
}

void ServerProcessor::debug(ClientProcessor * _proc)
{
    //
}

bool ServerProcessor::compare_resp(const ticket_t& _tk)
{
    std::size_t curr_size = m_contextpool[_tk].m_pbbuffer_req.second;
    std::size_t size_val = m_pbbuffer_req_valid.second;
    DBGOUT4("s: (compare_resp) size_val:", size_val, "size1:", curr_size)
    if (curr_size == 0) return false;
    auto ptr_req = m_contextpool[_tk].m_pbbuffer_req.first.get();
    std::string_view sv_val( m_pbbuffer_req_valid.first.get(),
                         m_pbbuffer_req_valid.second);
    std::string_view sv2(ptr_req, curr_size );
    //sv2.resize(sv.size() + 1, 0);
    std::string_view::iterator it_start = sv2.begin();
    std::string_view::iterator it_end = sv2.end();
    std::string_view::iterator it_start_val = sv_val.begin();
    std::string_view::iterator it_end_val = sv_val.end();

    bool result = false;
    //std::cerr << sv << std::endl;
    //std::cerr << sv2 << std::endl;
    if (curr_size <= m_pbbuffer_req_valid.second)
    {
        result =  std::equal(it_start, it_end, it_start_val);
    }
    else
    {
        result =  std::equal(it_start_val, it_end_val, it_start);
    }
    return  result;
}

bool ServerProcessor::isAvailable(const ticket_t& _tk)
{
    std::unique_lock<std::mutex> lock(m_procmtx);
    bool result = false;
    auto& refcounter  = m_contextpool[_tk].m_attemptcounter;
    if (!refcounter)
        {   refcounter = 5; result = true;   }
    else
        { refcounter -= 1;   }
    m_contextpool[_tk].m_isavail = result;
    return result;
}

//==========================================================================
//==========================================================================

class ClientProcessor: public IProcessor
{
    bool m_binarypack = false;
public:
    ClientProcessor();
    virtual void get_input_data(cpconv_t _data, const ticket_t& _tk) override final;
    virtual bool process_output_data(mvconv_t _data,
                                     const ticket_t& _tk) override final
    { }
    virtual bool process_output_data(mvconv_t _data,
                                const ticket_t& _tk,
             std::shared_ptr<ICompleter_t> _compl) override final;
    virtual bool isAvailable(const ticket_t& _tk);
    virtual eCompl_t getComplType() override
    {
        if (m_binarypack)
        {
            return eCompl_t::compound;
        }
        return eCompl_t::none;
    }
    bool compare_resp(const ticket_t& _tk,
                      const char * _ptr,
                      std::size_t _size);
    bool compare_busy(const ticket_t& _tk,
                      const char * _ptr,
                      std::size_t _size);
public:
    // Буффер для хранения экземпляра запроса
    upair_t m_pcbuffer_req;
    // Буффер для отправки запроса
    bstmbuffptr_t m_pbbuffer_req;
    // Буффер для хранения экземляра правильного ответа
    upair_t m_pcbuffer_resp_valid;
    friend class ServerProcessor;
    void do_compose(const std::string& server);
    void do_compose_bin();
    std::size_t m_size_valid;
    std::size_t m_successes = 0;
    std::size_t m_fails = 0;
    struct context_t
    {
        // Буффер для чтения ответа
        upair_t m_pcbuffer_resp;
        bool m_isavailble = false;
        context_t() = default;
        context_t(context_t&& _ctx)
        {
            m_pcbuffer_resp.first
                    = std::move(_ctx.m_pcbuffer_resp.first);
            m_pcbuffer_resp.second = _ctx.m_pcbuffer_resp.second;
            m_isavailble = _ctx.m_isavailble;
        }
        context_t& operator=(context_t&& _ctx)
        {
            m_pcbuffer_resp.first
                    = std::move(_ctx.m_pcbuffer_resp.first);
            m_pcbuffer_resp.second = _ctx.m_pcbuffer_resp.second;
            m_isavailble = _ctx.m_isavailble;
        }
    };
    std::map<ticket_t, context_t> m_contextpool;
};


ClientProcessor::ClientProcessor()
{
    const char * FUNCTION = __FUNCTION__;
    try
    {
        m_binarypack = true;
        //Читаем из файла ответ для валидации
        std::ifstream ifstm;
        ifstm.open(fullname_bin);
        assert(ifstm.is_open());
        // get length of file:
        ifstm.seekg (0, std::ios::end);
        auto size = ifstm.tellg();
        m_pcbuffer_resp_valid.second = size;
        ifstm.seekg (0, std::ios::beg);
        char * ptr = (size > 0)?(new char[size]):nullptr;
        ifstm.read(ptr, m_pcbuffer_resp_valid.second);
        m_pcbuffer_resp_valid.first.reset(ptr);
        ifstm.close();
        std::string tmpstr = "c: valid response was red from file:" +
                std::to_string(m_pcbuffer_resp_valid.second);
        DBGOUT(tmpstr)

        //Читаем экземляр запроса
        std::ifstream ifstm2;
        ifstm2.open(fullname_bin);
        assert(ifstm2.is_open());
        // get length of file:
        ifstm2.seekg(0, std::ios::end);
        std::size_t size2 = ifstm2.tellg();
        ifstm2.seekg(0, std::ios::beg);
        char * ptr2 = (size2 > 0)?(new char[size2]):nullptr;
        ifstm2.read(ptr2, size2);
        m_pcbuffer_req.first.reset(ptr2);
        m_pcbuffer_req.second = size2;
        ifstm2.close();
        std::string tmpstr2 = "c: request was read from file:" +
                std::to_string(m_pcbuffer_req.second);
        DBGOUT(tmpstr2)
    }
    catch (...)
    {
        errspace::show_errmsg(FUNCTION);
        throw;
    }
}

void ClientProcessor::get_input_data(cpconv_t _data,
                                     const ticket_t& _tk)
{
    const char * FUNCTION = __FUNCTION__;
    try
    {
        _data = m_pcbuffer_req;
    }
    catch (...)
    {
        errspace::show_errmsg(FUNCTION);
        throw;
    }
}

bool ClientProcessor::process_output_data(mvconv_t _data,
                                          const ticket_t& _tk,
                                          abstcomplptr_t _compl)
{
    const char * FUNCTION = __FUNCTION__;
    try
    {
        bool result = false;
        bstmbuffptr_t pbbuffer_resp = _data.extractBufPtr();

        DBGOUT2("process_output_data size=", pbbuffer_resp.get()->size());
        mvconv_t adapter = std::move(pbbuffer_resp);
        if (m_contextpool.find(_tk) == m_contextpool.end())
        {
            context_t ctx;
            ctx.m_isavailble = false;
            ctx.m_pcbuffer_resp = adapter.extractPair();
            std::pair<ticket_t, context_t> pr;
            pr.first = _tk;
            pr.second = std::move(ctx);
            m_contextpool.insert(std::move(pr));
        }
        else
        {
            m_contextpool[_tk].m_pcbuffer_resp = adapter.extractPair();
        }

        using cc = compound_completer_t;
        cc * pcompl = dynamic_cast<cc *>(_compl.get());
        if (!pcompl) return result;
        auto buffptr = m_contextpool[_tk].m_pcbuffer_resp.first.get();
        auto offset = pcompl->getOffset();
        auto size = m_contextpool[_tk].m_pcbuffer_resp.second - offset;
        if (offset > 0)
        {
            DBGOUT("c: compound packet ====================")
        }
        if (pcompl->getLastPacketType() == compound_completer_t::ePackType_t::text)
        {
            result = compare_busy(_tk, buffptr + offset, size);
            if (result)
            {
                DBGOUT("c: compare result: busy")
                m_contextpool[_tk].m_isavailble = false;
                return true;
            }
        }
        else
        {
            result = compare_resp(_tk, buffptr + offset, size);
        }
        m_contextpool[_tk].m_isavailble = result;
        if (result) m_successes++;
            else m_fails++;
        DBGOUT2("c: compare result(is reply good):", result);
        return result;
    }
    catch (...)
    {
        errspace::show_errmsg(FUNCTION);
        throw;
    }
}

bool ClientProcessor::compare_resp(const ticket_t& _tk,
                                   const char * _ptr,
                                   std::size_t _size)
{
    std::size_t curr_size = _size;//m_contextpool[_tk].m_pcbuffer_resp.second;
    std::string s = "c: (compare_resp) size_val:" +
            std::to_string(m_pcbuffer_resp_valid.second);
    s += "size1:" + std::to_string(curr_size);
    DBGOUT(s)
    if (curr_size == 0) return false;
    std::string_view sv_val(m_pcbuffer_resp_valid.first.get(),
                            m_pcbuffer_resp_valid.second);
    auto ptr = _ptr;//m_contextpool[_tk].m_pcbuffer_resp.first.get();
    std::string_view sv(ptr, curr_size );
    std::string_view::iterator it_start = sv.begin();
    std::string_view::iterator it_end = sv.end();
    std::string_view::iterator it_start_val = sv_val.begin();
    std::string_view::iterator it_end_val = sv_val.end();
    bool result = false;
    if (curr_size <= m_pcbuffer_resp_valid.second)
    {
        result =  std::equal(it_start, it_end, it_start_val);
    }
    else
    {
        result =  std::equal(it_start_val, it_end_val, it_start);
    }
    return  result;

}

bool ClientProcessor::compare_busy(const ticket_t& _tk,
                                   const char * _ptr,
                                   std::size_t _size)
{
    bool result = false;
    const auto val_size = static_strlen(simple_reply_busy);
    std::size_t curr_size = _size;//m_contextpool[_tk].m_pcbuffer_resp.second;
    std::string s = "c: (compare_busy) size_val:" +
            std::to_string(val_size);
    s += "size1:" + std::to_string(curr_size);
    DBGOUT(s)
    if (curr_size == 0) return result;
    std::string_view sv_val(simple_reply_busy);
    auto ptr = _ptr;//m_contextpool[_tk].m_pcbuffer_resp.first.get();
    std::string_view sv(ptr, curr_size );
    std::string_view::iterator it_start = sv.begin();
    std::string_view::iterator it_end = sv.end();
    std::string_view::iterator it_start_val = sv_val.begin();
    std::string_view::iterator it_end_val = sv_val.end();
    if (curr_size <= strlen(simple_reply_busy))
    {
        result =  std::equal(it_start, it_end, it_start_val);
    }
    else
    {
        result =  std::equal(it_start_val, it_end_val, it_start);
    }
    return  result;
}

bool ClientProcessor::isAvailable(const ticket_t& _tk)
{
    return m_contextpool[_tk].m_isavailble;
}

#endif // MOCK_PROCESSOR_H
