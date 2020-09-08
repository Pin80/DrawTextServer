/** @file
 *  @brief         Заголовочный файл модуля интерфейса обработчика данных
 *  @author unnamed
 *  @date   created 2020
 *  @date   modified 2020
 *  @version 1.0
 */
#ifndef PROCESSOR_H
#define PROCESSOR_H
#include "../include/common.h"
#include "../include/error.h"
#include "../include/logger.h"
#include "../include/utility.h"
#include "../include/packet.h"

enum class eCompl_t {none, http, bin, compound, debug};

class binpack_compleater_t;
class http_compleater_t;

/** @class ICompleter_t
 *  @brief Класс интерфейса комплитера
 */
class ICompleter_t : public boost::noncopyable
{
protected:
    virtual ~ICompleter_t() = default;
public:
    // Тип ошибки
    using error_code = boost::system::error_code;
    // Ожидаемый размер следующей передачи по умолчанию
    enum { default_max_transfer_size = 1024*1024*1024 };
    virtual std::size_t do_completer(const error_code& err,
                                   std::size_t _sz,
                                   std::size_t _id = 0) = 0;
    virtual bool isError() const noexcept = 0;
};

class complwrapper_t;

/** @class binpack_completer_t
 *  @brief Класс комплитера бинарного пакета
 */
class binpack_completer_t: public ICompleter_t,
                         public binpacket_t
{
public:
    /// Комплитер
    virtual std::size_t do_completer(const error_code& _err,
                                     std::size_t _sz,
                                     std::size_t _id) override;
    /// Получить значение флага ошибки
    virtual bool isError() const noexcept override { return  m_iserror; }
    /// Получить размер переданных данных
    auto data_length() const noexcept {return  m_complcounter; }
protected:
    friend class complwrapper_t;
    /// Конструктор класса
    binpack_completer_t(upair_t& _buff): m_buff(_buff)
    {
        const char * FUNCTION = __FUNCTION__;
        try
        {
            m_buffer_size = m_buff.second;
            m_startfirstptr = valconv_t(m_buff.first.get());
            m_startcurrptr = m_startfirstptr;
        }
        catch (...)
        {
            errspace::show_errmsg(FUNCTION);
            throw;
        }
    }
protected:
    /// Максимальное число передач (для завершения при сбое)
    static constexpr auto max_transfers = 50;
    /// Последние состояние парсинга буффера
    eCmplState m_laststate = eCmplState::noSign;
    /// Текущий счётчик передач
    std::size_t transfer_counter = 0;
    /// Размер последний передачи
    std::size_t m_complcounter = 0;
    /// Флаг ошибки
    bool m_iserror = false;
    /// Ссылка на буффер
    upair_t& m_buff;
    /// Размер буффера
    std::size_t m_buffer_size = 0;
    /// Размер буффера
    std::size_t m_bufferusing = 0;
    /// Указатель на начало последнего декодируемого пакета
    value_t* m_startcurrptr = nullptr;
    /// Указатель на начало первого декодируемого пакета
    value_t* m_startfirstptr = nullptr;
    /// Результаты парсинга бинарного пакета:
    std::size_t m_lng_value;
    std::uint16_t m_crc_value;
    std::uint16_t m_crc_estimated;
};

/** @class binpack_completer_t
 *  @brief Класс комплитера составного пакета
 */
class compound_completer_t: public binpack_completer_t
{
public:
    enum class ePackType_t {bin, text};
    /// Комплитер
    virtual std::size_t do_completer(const error_code& _err,
                                     std::size_t _sz,
                                     std::size_t _id) override final;
    ePackType_t getLastPacketType() const noexcept
    { return m_type; }
    auto getOffset() const
    { return m_startcurrptr - m_startfirstptr; }
protected:
    friend class complwrapper_t;
    /// Конструктор класса
    compound_completer_t(upair_t& _buff): binpack_completer_t(_buff)
    {     }
private:
    /// Терминатор http пакета
    static constexpr const char* m_http_term = "\r\n\r\n";
    /// Длинна терминатора http пакета
    static constexpr auto m_http_lng = static_strlen(m_http_term);
    /// Тип последнего пакета
    ePackType_t m_type = ePackType_t::bin;
};

/** @class http_completer_t
 *  @brief Класс пакета с терминатором \\r\\n\\r\\n
 */
class http_completer_t: public ICompleter_t
{
public:
    virtual std::size_t do_completer(const error_code& _err,
                                     std::size_t _sz,
                                     std::size_t _id) override final;
    bool isError() const noexcept override { return m_iserror; }
protected:
    friend class complwrapper_t;
    http_completer_t(upair_t& _buff): m_buff(_buff) {}
private:
    static constexpr const char* m_http_term = "\r\n\r\n";
    std::size_t m_chunks = 0;
    std::size_t m_length;
    bool m_iserror = false;
    std::size_t m_complcounter = 0;
    std::size_t m_estimated = 0;
    upair_t& m_buff;
};

/** @class http_completer_t
 *  @brief Класс универсального комплитера (адаптер)
 */
class complwrapper_t
{
protected:
    using error_code = boost::system::error_code;
    std::shared_ptr<ICompleter_t>  m_pimpl;
    std::size_t m_id = 0;
public:
    typedef std::size_t result_type;
    complwrapper_t(eCompl_t _type, upair_t& _buff);
    template <typename Error>
    std::size_t operator()(const Error& _err,
                           std::size_t _bytes_transferred)
    {
        assert(m_pimpl);
        if (m_pimpl)
        {
            return m_pimpl->do_completer(_err, _bytes_transferred, m_id);
        }
        return 0;
    }
    bool isError() const
    { return  m_pimpl->isError(); }
    std::shared_ptr<ICompleter_t> getNativeCompl()
    { return m_pimpl; }
};

/** @class IProcessor_t
 *  @brief Класс интерфейса обработчика данных
 */
class IProcessor_t : boost::noncopyable
{
public:
    using bstmbuffptr_t = cpconv_t::bstmbuffptr_t;
    using abstcomplptr_t = std::shared_ptr<ICompleter_t>;
    virtual void get_input_data(cpconv_t _data,
                                const ticket_t& _tk) = 0;
    virtual bool process_output_data(mvconv_t _data,
                                const ticket_t& _tk) = 0;
    virtual bool process_output_data(mvconv_t _data,
                                const ticket_t& _tk,
                                abstcomplptr_t _compl) = 0;
    virtual bool isAvailable(const ticket_t& _tk) = 0;
    virtual eCompl_t getComplType()
    { return eCompl_t::none; }
    virtual ~IProcessor_t() = default;
};

/** @class IServerProcessor_t
 *  @brief Класс интерфейса обработчика запросов со стороны сервера
 */
class IServerProcessor_t : boost::noncopyable
{
public:
    using readyHandler_t = std::function<void ()>;
    using rdyHandlerPtr_t = std::shared_ptr<readyHandler_t>;
    using bstmbuffptr_t = cpconv_t::bstmbuffptr_t;
    using abstcomplptr_t = std::shared_ptr<ICompleter_t>;
    virtual void get_busy_data(cpconv_t _data) = 0;
    virtual void setReadyHandler(const rdyHandlerPtr_t& _hnd,
                                 const ticket_t& _tk) = 0;
    virtual void get_input_data(spair_t& _data,
                                const ticket_t& _tk) = 0;
    virtual bool process_output_data(mvconv_t _data,
                                const ticket_t& _tk) = 0;
    virtual bool process_output_data(mvconv_t _data,
                                ticket_t& _tk,
                                abstcomplptr_t _compl,
                                rdyHandlerPtr_t& _hnd) = 0;
    virtual bool isAvailable(const ticket_t& _tk) = 0;
    virtual eCompl_t getComplType()
    { return eCompl_t::none; }
    virtual ~IServerProcessor_t() = default;
};
using ISMP = IServerProcessor_t;


#endif // PROCESSOR_H
