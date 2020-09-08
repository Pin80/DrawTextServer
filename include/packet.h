/** @file
 *  @brief         Заголовочный файл модуля пакета
 *  @author unnamed
 *  @date   created 10.01.2020
 *  @date   modified 10.01.2020
 *  @version 1.0 (alpha)
 */
#ifndef PACKET_H
#define PACKET_H
#include "../include/common.h"
#include "../include/utility.h"

/** @struct binpacket_t
 *  @brief Класс простого пакета
 */
struct binpacket_t : boost::noncopyable
{
    // incoptable with python
    // boost::crc_16_type crc_result;
    /// Тип данных для crc
    using crc_t = boost::crc_basic<16>;
    /// Тип данных результата парсинга пакета
    struct parsedata_t
    {
        std::any m_current;
        std::any m_next;
    };
    /// Тип данных результата парсинга пакета для m_current, m_next
    struct unpacked_t
    {
        std::uint32_t m_length = 0;
        crc_t::value_type m_crc = 0;
        std::unique_ptr<value_t[]> m_data;
    };
    /// Тип состояния прогресса заполнения
    enum class eCmplState {noSign, noLng, noDataCrc, finished, unRec};
    /// Сигнатура пакета
    static constexpr value_t signat[] = {'P','A','C','K'};
    /// Размер поля сигнатуры
    static constexpr auto sig_size = sizeof (signat);
    /// Размер поля длинны данных
    static constexpr auto length_size = sizeof(unpacked_t::m_length);
    /// Размер заголовка пакета
    static constexpr auto header_size = sig_size + length_size;
    /// Размер поля crc
    static constexpr auto crc_size = sizeof( unpacked_t::m_crc);
    /// Смещение поля длинны
    static constexpr auto length_ofs = sig_size;
    /// Разрядность байт поля длинны
    static constexpr value_t length_order[] = {3, 2, 1, 0};
    /// Разрядность байт поля контрольной суммы
    static constexpr value_t crc_order[] = {0, 1};
    /// Максимально допустимый размер поля данных
    static constexpr auto max_valid_size = 2'000'000;
    /// Поле сигнатуры
    value_t m_sign_field[sig_size] = {};
    /// Поле длинны
    value_t m_lng_field[length_size] = {};
    /// Поле контрольной суммы
    value_t m_crc_field[crc_size] = {};
    /// Тип контрольной суммы
    crc_t  crc_ccitt1{ 0x1021, 0xFFFF, 0, false, false };
    /// Шаблонный метод получения значения и какого либо поля пакета
    template< class T, auto size >
    constexpr T get_value(const value_t (&_field)[size],
                          const value_t (&_order)[size]) const noexcept
    {
        static_assert((std::is_integral<decltype(size)>::value) && (size > 0));
        static_assert((std::is_integral<T>::value));
        constexpr auto nbits = 8;
        T tmp_value = 0;
        for (decltype(size) i = 0; i < size; i++)
        {
            tmp_value += _field[i] * (0x01 << (_order[i] * nbits));
        }
        return tmp_value;
    }
    /// Шаблонный метод установки значения какого либо поля пакета
    template< class T, auto size>
    void set_value(const T _val,
                   value_t (&_field)[size],
                   const value_t (_order)[size]) noexcept
    {
        static_assert((std::is_integral<decltype(size)>::value) && (size > 0));
        static_assert((std::is_integral<T>::value));
        constexpr auto nbits = 8;
        for (decltype(size) i = 0; i < size; i++)
        {
            _field[i] = (_val >> (_order[i] * nbits)) & 0xFF;
        }
    }
    /// Шаблонный метод копирования значения какого либо поля пакета
    template< auto size>
    void fill_field(const value_t * _ptr, value_t (&_field)[size])
    {
        memcpy(&_field, _ptr, size);
    }
    /// Метод получения длинны пакета
    std::size_t get_length() const noexcept;
    /// Метод установки длинны пакета
    void set_length(const std::size_t _val) noexcept;
    /// Метод получения crc
    std::uint16_t get_crc() const noexcept;
    /// Метод установки crc
    void set_crc(const std::uint16_t _val) noexcept;
    /// Вернуть состояние автомата парсинга комплитера по смещению
    constexpr eCmplState complStatus(std::size_t _transf) const noexcept
    {
        if (_transf < sig_size)
            { return eCmplState::noSign;  }
        else
        {
            if (_transf < header_size)
                { return eCmplState::noLng; }
            else
            {
                if (_transf < header_size + get_length() + crc_size)
                    { return eCmplState::noDataCrc; }
                else
                    { return eCmplState::finished; }
            }
        }
    }
    /// Вычислить crc массива данных начиная с _ptr
    crc_t::value_type evaluate_crc(const void * _ptr, std::size_t _sz);
    /// сконструировать пакет с данными _data
    upair_t make_packet(const upair_t& _data);
    /// Проверить блок данных на соответствие структуры пакета
    /// и заполнить поля пакета
    virtual bool validate_packet(cvalconv_t _buff, bool _callinh = true);
    /// Сранить статусы парсинга пакетов
    friend constexpr bool operator < (eCmplState _st1, eCmplState _st2)
    {
        using type = std::underlying_type<binpacket_t::eCmplState>::type;
        auto x1 =  static_cast< type > (_st1);
        auto x2 =  static_cast< type > (_st2);
        return x1 < x2;
    }
    ///  Произвести парсинг пакета
    virtual parsedata_t parse_packet(const upair_t& _data);
};


#endif // PACKET_H
