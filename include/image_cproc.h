/** @file
 *  @brief         Заголовочный файл модуля интерфейса обработчика данных
 *  @author unnamed
 *  @date   created 2020
 *  @date   modified 2020
 *  @version 1.0
 */
#ifndef IMAGE_CPROC_H
#define IMAGE_CPROC_H
#include "../include/common.h"
#include "../include/server.h"
#include "../include/client.h"
#include "../include/error.h"
#include "../include/logger.h"
#include "../include/watchdog.h"
#include "../include/processor.h"
#include "../include/comb_packet.h"

class ClientImgProcessor: public IProcessor
{
public:
    using Tpackstruct = std::shared_ptr<compndpack_t::unpacked_tComb>;
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
    ClientImgProcessor(const std::string& _fname,
                       const std::string& _vname,
                       const std::string& _oname);
    virtual void get_input_data(cpconv_t _data,
                                const ticket_t& _tk) override final;
    virtual bool process_output_data(mvconv_t _data,
                                     const ticket_t& _tk,
                                     abstcomplptr_t _compl) override final;
    virtual bool process_output_data(mvconv_t _data,
                                     const ticket_t& _tk) override final
    {}
    virtual bool isAvailable(const ticket_t& _tk);
    virtual eCompl_t getComplType() override
    {
        return eCompl_t::compound;
    }
protected:
    bool compare_resp(const ticket_t& _tk,
                      const char * _ptr,
                      std::size_t _size);
    bool compare_busy(const ticket_t& _tk,
                      const char * _ptr,
                      std::size_t _size);
    bool compare_bad(const ticket_t& _tk,
                      const char * _ptr,
                      std::size_t _size);
    bool compare_str(const ticket_t& _tk,
                      const char * _ptr,
                      std::size_t _size,
                       const char* _str);
    bool create_packet(upair_t& _fpair);
    void save_result(const std::string& _fname,
                     const Tpackstruct &_pair);
    // Буффер для хранения экземпляра запроса
    upair_t m_pcbuffer_req;
    // Буффер для отправки запроса
    bstmbuffptr_t m_pbbuffer_req;
    // Буффер для хранения экземляра правильного ответа
    upair_t m_pcbuffer_resp_valid;
    void do_compose(const std::string& server);
    void do_compose_bin();
    std::size_t m_size_valid;
    std::map<ticket_t, context_t> m_contextpool;
    std::string m_outname;
};


#endif // IMAGE_CPROC_H