/** @file
 *  @brief         Модуль интерфейса обработчика картинок
 *  @author unnamed
 *  @date   created 2020
 *  @date   modified 2020
 *  @version 1.0
 */
#include "../include/image_sproc.h"

static const char * simple_reply_bad =
        "FAIL\r\n\r\n";
static const char * simple_reply_busy =
        "BUSY\r\n\r\n";


servimgProcessor_t::servimgProcessor_t(siprocessor_t&_config) :
    m_reqfreeidx_fifo(_config.m_queuesize),
    m_request_queue(_config.m_queuesize),
    m_queuesize(_config.m_queuesize)
{
    const char * FUNCTION = __FUNCTION__;
    try
    { 
        m_request_stock.resize(m_queuesize);
        m_response_stock.resize(m_queuesize);
        m_rep_timeout.resize(m_queuesize);
        for (std::uint8_t i = 0; i < m_queuesize; i++)
        {
            m_rep_timeout[i]._a = 0;
            m_reqfreeidx_fifo.bounded_push(i);
        }
        auto size = strlen(simple_reply_busy);
        char * ptr = (size > 0)?new char[size]:nullptr;
        m_pbbuffer_busy.first.reset(ptr);
        memcpy(ptr, simple_reply_busy, size);
        m_pbbuffer_busy.second = size;
        auto func = &servimgProcessor_t::Process;
        mngThread_t* pthread = new managedThread_t{func, this};
        m_pthread.reset(pthread);
        m_pthread->StartLoop();
        LOG << "s: server processor is running";
        using std::chrono::milliseconds;
        const auto max_attempts = 30;
        const auto delay_quant = 100;
        auto nattempts = 0;
        while(!m_isstarted)
        {
            std::this_thread::sleep_for(milliseconds(delay_quant));
            if (nattempts++ > max_attempts) break;
        }
        if (!m_isstarted)
            { throw std::exception(); }
    }
    catch (...)
    {
        errspace::show_errmsg(FUNCTION);
        throw;
    }
}

// Отдать экземляр ответа
void servimgProcessor_t::get_input_data(spair_t& _data,
                                     const ticket_t& _tk)
{
    bool result = false;
    auto rep_stock_idx = _tk.getProcVal();
    assert(rep_stock_idx < m_queuesize);
    result = (m_rep_timeout[rep_stock_idx]._a > 0);
    if (!result)
    {
        return;
    }
    auto& ref = m_response_stock[rep_stock_idx]->m_pbbuffer_req;
    if (ref.first)
    {
        _data.first = std::move(ref.first);
        _data.second = ref.second;
    }
    else
    {
        auto size = strlen(simple_reply_bad);
        auto ptr = (size > 0)?new char[size]:nullptr;
        _data.first.reset(ptr);
        memcpy(ptr, simple_reply_bad, size);
        _data.second = size;
    }
    m_response_stock[rep_stock_idx].reset();
    m_reqfreeidx_fifo.bounded_push(rep_stock_idx);
}

bool servimgProcessor_t::isAvailable(const ticket_t& _tk)
{
    std::uint8_t rep_stock_idx = _tk.getProcVal();
    assert(rep_stock_idx < QUEUESIZE);
    int timeout = m_rep_timeout[rep_stock_idx]._a;
    return timeout > 0;
}

void servimgProcessor_t::get_busy_data(cpconv_t _data)
{
    std::unique_lock<std::mutex> lock(m_procmtx);
    _data = m_pbbuffer_busy;
}

bool servimgProcessor_t::process_output_data(mvconv_t _data,
                                             ticket_t& _tk,
                                             abstcomplptr_t _compl,
                                             rdyHandlerPtr_t &_hnd)
{
    auto pair = _data.extractPair();
    bool result = compare_resp(pair);
    if (result)
    {
        std::uint8_t number;
        result = m_reqfreeidx_fifo.pop(number);
        if (result)
        {
            auto pctx = new context_t;
            m_request_stock[number].reset(pctx);
            pctx->m_pbbuffer_req = std::move(pair);
            pctx->m_handlerptr = _hnd;
            _tk.setProcVal(number);
            auto tk = _tk;
            m_rep_timeout[number]._a = 0;
            m_request_queue.bounded_push(tk);
        }
    }
    DBGOUT2("s: compare result(is reply good):", result);
    return result;
}


void servimgProcessor_t::Process(IThread *_th)
{
    const char * FUNCTION = __FUNCTION__;
    using namespace boost::asio::ip;
    using namespace boost::asio;
    try
    {
        ticket_t tmptk;
        bool result = false;
        LOG << "s: processor thread loop is started";
        m_isstarted = true;
        auto isRunning = _th->isRunning();
        while(isRunning)
        {
            isRunning = _th->isRunning();
            if (_th)
            {
                _th->feedwd();
            }
            // is new request ?
            result = !m_request_queue.empty();
            if (!result)
            {
                continue;
            }
            // extract new request
            result = m_request_queue.pop(tmptk);
            if (!result)
            {
                continue;
            }
            auto req_stock_idx = tmptk.getProcVal();
            std::uint8_t rep_stock_idx = req_stock_idx;
            // get stock idx

            upair_t procpair;
            procpair = std::move(m_request_stock[req_stock_idx]->m_pbbuffer_req);
            auto response = drawText(procpair);
            auto handler = m_request_stock[req_stock_idx]->m_handlerptr;
            m_request_stock[req_stock_idx].reset();
            auto pctx = new context_t;
            pctx->m_pbbuffer_req = std::move(response);
            m_response_stock[rep_stock_idx].reset(pctx);
            // decrease timeout
            for (auto i = 0; i < m_queuesize; i++)
            {
                int old_val = m_rep_timeout[i]._a.load();
                auto new_val = old_val? old_val--: old_val;
                m_rep_timeout[i]._a.compare_exchange_weak(old_val, new_val);
            }
            m_rep_timeout[rep_stock_idx]._a = REPTIMEOUT;
            if (handler)
            {
                (*handler.get())();
            }
            isRunning = _th->isRunning();
        }
        LOG << "s: processor thread loop is stopped";
        m_isstarted = false;
    } catch (...)
    {
        errspace::show_errmsg(FUNCTION);
        throw;
    }
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


upair_t servimgProcessor_t::drawText(upair_t & _packet)
{
    typedef std::unique_ptr<value_t> TCairoBuffPtr;
    using Tpackstruct = std::shared_ptr<compndpack_t::unpacked_tComb>;
    bool result = false;
    compndpack_t frompack;

    auto unpack = frompack.parse_packet(_packet);
    result = unpack.m_current.has_value();
    if (!result)
    {
        unpack.m_next.reset();
        unpack.m_current.reset();
        upair_t presult;
        return presult;
    }
    auto uc = std::any_cast<Tpackstruct>(unpack.m_current);
    unpack.m_next.reset();
    unpack.m_current.reset();
    auto closure_ptr_read = std::make_unique<TCairoCl>();
    closure_ptr_read->m_ptr = std::move(uc->m_data);
    std::string imgstr = uc->m_str;
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
        cairo_show_text (cr, imgstr.c_str());
        DBGOUT("text is:")
        DBGOUT(imgstr)

        auto closure_ptr_write = std::make_unique<TCairoCl>();

        cairo_surface_write_to_png_stream(surf_src,
                                          &write_to_pngbuff,
                                          closure_ptr_write.get());
        result = (cairo_surface_status(surf_src) == CAIRO_STATUS_SUCCESS);
        cairo_destroy(cr);
        cairo_debug_reset_static_data();
        FcFini();
        char * ptr = valconv_t(closure_ptr_write->m_ptr.get());
        closure_ptr_write->m_ptr.release();
        pair2.first.reset(ptr);
        pair2.second = closure_ptr_write->buffsize;
        closure_ptr_write.reset();
    }
    cairo_surface_destroy(surf_src);
    compndpack_t topack2;
    auto packet2 = topack2.make_packet(pair2,"");

    return packet2;
}

eCompl_t servimgProcessor_t::getComplType()
{
    std::unique_lock<std::mutex> lock(m_procmtx);
    return eCompl_t::bin;
}

void servimgProcessor_t::setReadyHandler(const rdyHandlerPtr_t& _hnd,
                                      const ticket_t& _tk)
{
    m_contextpool[_tk].m_handlerptr = _hnd;
}

bool servimgProcessor_t::compare_resp(upair_t& _pair)
{
    std::size_t curr_size = _pair.second;
    std::size_t val_size = m_pbbuffer_req_valid.second;
    DBGOUT4("s: (compare_resp) size_val:", val_size, "size_net:", curr_size)

    if (curr_size == 0) return false;
    auto ptr = _pair.first.get();
    std::string_view sv_val( m_pbbuffer_req_valid.first.get(), val_size);
    std::string_view sv2(ptr, curr_size );
    std::string_view::iterator it_start = sv2.begin();
    std::string_view::iterator it_end = sv2.end();
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

//==========================================================================
//==========================================================================
