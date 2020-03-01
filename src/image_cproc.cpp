/** @file
 *  @brief         Модуль интерфейса обработчика клиента
 *  @author unnamed
 *  @date   created 2020
 *  @date   modified 2020
 *  @version 1.0
 */
#include "../include/image_cproc.h"

static const char * simple_reply_bad =
        "FAIL\r\n\r\n";
static const char * simple_reply_busy =
        "BUSY\r\n\r\n";


ClientImgProcessor::ClientImgProcessor(ciprocessor_t& _config):
    m_outname(_config.m_oname),
    m_compare(_config.m_compare)
{
    const char * FUNCTION = __FUNCTION__;
    try
    {   
        //Читаем экземляр запроса
        std::ifstream ifstm_req;
        ifstm_req.open(_config.m_fname);
        if (!ifstm_req.is_open())
            { throw std::runtime_error("file not found"); }
        ifstm_req.seekg(0, std::ios::end);
        std::size_t size_img = ifstm_req.tellg();
        ifstm_req.seekg(0, std::ios::beg);
        char * ptr2 = (size_img > 0)?(new char[size_img]):nullptr;
        ifstm_req.read(ptr2, size_img);
        upair_t pair;
        pair.first.reset(ptr2);
        pair.second = size_img;
        ifstm_req.close();
        [[maybe_unused]] bool result = create_packet(pair);
        assert(result);
        DBGOUT2("c: request was read from file:", size_img)

        //Читаем из файла ответ для валидации
        std::ifstream ifstm_val;
        ifstm_val.open(_config.m_vname);
        if (!ifstm_val.is_open())
            { throw std::runtime_error("file not found"); }
        ifstm_val.seekg (0, std::ios::end);
        size_img = ifstm_val.tellg();
        ifstm_val.seekg (0, std::ios::beg);
        char * ptr = (size_img > 0)?(new char[size_img]):nullptr;
        m_pcbuffer_resp_valid.first.reset(ptr);
        m_pcbuffer_resp_valid.second = size_img;
        ifstm_val.read(ptr, size_img);
        ifstm_val.close();
        DBGOUT2("c: valid response was red from file:", size_img)
    }
    catch (...)
    {
        errspace::show_errmsg(FUNCTION);
        throw;
    }
}

bool ClientImgProcessor::create_packet(upair_t &_fpair)
{
    compndpack_t pk_creator;
    m_pcbuffer_req = pk_creator.make_packet(_fpair,
                                          "HeLLo World!!!");
    bool result = (m_pcbuffer_req.first.get() != nullptr);
    return result;
}

void ClientImgProcessor::save_result(const std::string &_fname,
                                     const Tpackstruct &_ps)
{
    std::ofstream ostm;
    ostm.open(_fname.c_str());
    if (ostm.is_open())
    {
        const simplechar_t* ptr = valconv_t(_ps->m_data.get());
        const auto& size = _ps->m_imgsize;
        ostm.write(ptr, size);
    }
}

void ClientImgProcessor::get_input_data(cpconv_t _data,
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

bool ClientImgProcessor::process_output_data(mvconv_t _data,
                                          const ticket_t& _tk,
                                          abstcomplptr_t _compl)
{
    const char * FUNCTION = __FUNCTION__;
    try
    {
        bool result = false;
        if (m_contextpool.find(_tk) == m_contextpool.end())
        {
            std::pair<ticket_t, context_t> pr;
            pr.first = _tk;
            context_t ctx;
            ctx.m_isavailble = false;
            ctx.m_pcbuffer_resp = _data.extractPair();
            pr.second = std::move(ctx);
            m_contextpool.insert(std::move(pr));
        }
        else
        {
            m_contextpool[_tk].m_pcbuffer_resp.first.reset();
            m_contextpool[_tk].m_pcbuffer_resp = _data.extractPair();
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
            result = compare_bad(_tk, buffptr + offset, size);
            if (result)
            {
                DBGOUT("c: compare result: bad")
                m_contextpool[_tk].m_isavailble = true;
                return true;
            }
        }
        else
        {
            compndpack_t frompack2;
            auto& ref = m_contextpool[_tk].m_pcbuffer_resp;
            auto unpack = frompack2.parse_packet(ref);
            result = unpack.m_current.has_value();
            if (!result)
            {
                unpack.m_next.reset();
                unpack.m_current.reset();
                return result;
            }
            auto uc = std::any_cast<Tpackstruct>(unpack.m_current);

            result = compare_resp(_tk,
                                  valconv_t(uc->m_data.get()),
                                  uc->m_imgsize);
            if (result)
            {

                save_result(m_outname, uc);
            }
            else
            {
                DBGOUT("====== no result ++++++++++++++++++")
            }
        }
        m_contextpool[_tk].m_isavailble = result;
        DBGOUT2("c: compare result(is reply good):", result);
        return result;
    }
    catch (...)
    {
        errspace::show_errmsg(FUNCTION);
        throw;
    }
}

bool ClientImgProcessor::compare_resp(const ticket_t& _tk,
                                      const char * _ptr,
                                      std::size_t _size)
{
   std::size_t curr_size = _size;
   std::size_t val_size = m_pcbuffer_resp_valid.second;
   DBGOUT4("c: (compare_resp) size_val:", val_size, "size_net:", curr_size)
   if ((curr_size == 0) || (val_size == 0)) return false;
   assert( _ptr != nullptr);
   std::string_view sv_val(m_pcbuffer_resp_valid.first.get(), val_size);
   auto ptr = _ptr;
   std::string_view sv(ptr, curr_size );
   std::string_view::iterator it_start = sv.begin();
   std::string_view::iterator it_end = sv.end();
   std::string_view::iterator it_start_val = sv_val.begin();
   std::string_view::iterator it_end_val = sv_val.end();
   bool result = false;
   if (curr_size <= val_size)
   {
       result =  std::equal(it_start, it_end, it_start_val);
   }
   else
   {
       result =  std::equal(it_start_val, it_end_val, it_start);
   }
   return  result;
}

bool ClientImgProcessor::compare_busy(const ticket_t& _tk,
                                      const char * _ptr,
                                      std::size_t _size)
{
    return compare_str(_tk, _ptr, _size, simple_reply_busy);
}

bool ClientImgProcessor::compare_bad(const ticket_t &_tk,
                                     const char *_ptr,
                                     std::size_t _size)
{
   return compare_str(_tk, _ptr, _size, simple_reply_bad);
}

bool ClientImgProcessor::compare_str(const ticket_t &_tk,
                                     const char *_ptr,
                                     std::size_t _size,
                                     const char *_str)
{
    bool result = false;
    const auto val_size = static_strlen(_str);
    std::size_t curr_size = _size;
    DBGOUT4("c: (compare_resp) size_val:", val_size, "size_net:", curr_size)
    if (curr_size == 0) return result;
    std::string_view sv_val(_str);
    auto ptr = _ptr;
    std::string_view sv(ptr, curr_size );
    std::string_view::iterator it_start = sv.begin();
    std::string_view::iterator it_end = sv.end();
    std::string_view::iterator it_start_val = sv_val.begin();
    std::string_view::iterator it_end_val = sv_val.end();
    if (curr_size <= val_size)
    {
        result =  std::equal(it_start, it_end, it_start_val);
    }
    else
    {
        result =  std::equal(it_start_val, it_end_val, it_start);
    }
    return  result;

}

bool ClientImgProcessor::isAvailable(const ticket_t& _tk)
{
    if (m_contextpool[_tk].m_isavailble)
    {
        m_contextpool.erase(_tk);
        return true;
    }
    return false;
}
