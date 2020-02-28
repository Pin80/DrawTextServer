/** @file
 *  @brief         Заголовочный файл модуля интерфейса обработчика картинок
 *  @author unnamed
 *  @date   created 2020
 *  @date   modified 2020
 *  @version 1.0
 */
#ifndef IMAGE_SPROC_H
#define IMAGE_SPROC_H
#include "../include/common.h"
#include "../include/server.h"
#include "../include/client.h"
#include "../include/error.h"
#include "../include/logger.h"
#include "../include/watchdog.h"
#include "../include/processor.h"
#include "../include/comb_packet.h"

/** @class servimgProcessor_t
 *  @brief Класс обработчика картинок
 */
class servimgProcessor_t: public ISMP
{
public:
    servimgProcessor_t();
    virtual void get_input_data(spair_t &_data,
                                const ticket_t& _tk) override final;
    virtual bool process_output_data(mvconv_t _data,
                                ticket_t& _tk,
                                abstcomplptr_t _compl,
                                rdyHandlerPtr_t& _hnd) override final;
    virtual bool process_output_data(mvconv_t _data,
                                     const ticket_t& _tk) override final
    {}
    virtual eCompl_t getComplType() override;
    virtual void setReadyHandler(const rdyHandlerPtr_t& _hnd,
                                 const ticket_t& _tk) override final;
    bool compare_resp(upair_t& _pair);
    virtual bool isAvailable(const ticket_t& _tk) override final;
    virtual void get_busy_data(cpconv_t _data) override final;
private:
    /** @class context_t
     */
    struct context_t
    {
        /// Буффер для хранения экземпляра запроса
        upair_t m_pbbuffer_req;
        /// Указатель на обработчик сигнала готовности ответа
        rdyHandlerPtr_t m_handlerptr;
    };
    using threadfunc_t = void (servimgProcessor_t::*)(IThread *);
    using mngThread_t = managedThread_t<threadfunc_t, servimgProcessor_t, false>;
    using mngthreadPtr_t = std::unique_ptr<mngThread_t>;
    using fixed_trait = boost::lockfree::fixed_sized<true>;
    using lfreeQueue_t = typename boost::lockfree::queue<ticket_t, fixed_trait>;
    using lfreeIdxStock_t = typename
                            boost::lockfree::queue<std::uint8_t, fixed_trait>;
    void Process(IThread* _th);
    upair_t drawText(upair_t& _packet);
    /// Размер очереди
    static constexpr auto QUEUESIZE = 10;
    /// Сколько будет хранится ответ
    static constexpr auto REPTIMEOUT = 5;
    std::array<std::unique_ptr<context_t>, QUEUESIZE> m_request_stock;
    std::array<std::unique_ptr<context_t>, QUEUESIZE> m_response_stock;
    std::array<std::atomic_int, QUEUESIZE> m_rep_timeout;
    lfreeIdxStock_t m_reqfreeidx_fifo;
    lfreeQueue_t m_request_queue;
    /// Буффер для хранения текстового ответа
    bstmbuffptr_t m_pbbuffer_rep;
    /// Эталон правильного запроса
    upair_t m_pbbuffer_req_valid;
    /// Буффер хранения ответа "занят"
    upair_t m_pbbuffer_busy;
    /// Буффер хранения ответа "ошибка"
    upair_t m_pbbuffer_error;
    /// Пул контекстов
    std::map<ticket_t, context_t> m_contextpool;
    /// Временный мьютекс. потом статнет ненужным
    std::mutex m_procmtx;
    /// УКазтель на поток
    mngthreadPtr_t m_pthread;
    /// Флаг запуска обработчика запросов
    std::atomic_bool m_isstarted = false;
};

#endif // IMAGE_SPROC_H
