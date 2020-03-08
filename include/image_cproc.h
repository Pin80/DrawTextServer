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

/** @struct ciprocessor_t
 *  @brief Класс конфигурации обработчика ответов
 */
struct ciprocessor_t
{
    /// флаг активации сравнения
    bool m_compare = false;
    /// имя входного файла
    std::string m_fname;
    /// имя валидационного файла
    std::string m_vname;
    /// имя выходного файла
    std::string m_oname;
};

/** @class clientImgProcessor_t
 *  @brief Класс обработчика ответов
 */
class clientImgProcessor_t: public IProcessor_t
{
public:
    using Tpackstruct = std::shared_ptr<compndpack_t::unpacked_tComb>;
    /** @struct context_t
     */
    struct context_t
    {
        /// Буффер для чтения ответа от сервера
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
    clientImgProcessor_t(ciprocessor_t &_config);
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
    void reset_succcnt()
    { m_nsucces = 0;   }
    std::uint16_t get_succcnt() const noexcept
    { return m_nsucces; }
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
    void do_compose(const std::string& server);
    void do_compose_bin();
private:
    /// Буффер для хранения экземпляра запроса
    upair_t m_pcbuffer_req;
    /// Буффер для отправки запроса
    bstmbuffptr_t m_pbbuffer_req;
    /// Буффер для хранения экземляра правильного ответа
    upair_t m_pcbuffer_resp_valid;
    /// Пул контекстов выполнения для каждого соединения
    std::map<ticket_t, context_t> m_contextpool;
    /// Имя файла для сохранения полученной картнки
    std::string m_outname;
    /// Флаг активации сравнения
    bool m_compare = false;
    /// Количество успешно-обработанных картинок
    std::uint16_t m_nsucces = 0;
};


#endif // IMAGE_CPROC_H
