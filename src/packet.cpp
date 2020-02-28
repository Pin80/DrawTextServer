/** @file
 *  @brief         Модуля класса для пакета
 *  @author unnamed
 *  @date   created 10.01.2020
 *  @date   modified 10.01.2020
 *  @version 1.0 (alpha)
 */
#include "../include/packet.h"

std::size_t binpacket_t::get_length() const noexcept
{
    return get_value<std::size_t, length_size>(m_lng_field, length_order);
}

void binpacket_t::set_length(const std::size_t _val) noexcept
{
    set_value<std::size_t, length_size>(_val, m_lng_field, length_order);
}

std::uint16_t binpacket_t::get_crc() const noexcept
{
    return get_value<std::uint16_t, crc_size>(m_crc_field, crc_order);
}

void binpacket_t::set_crc(const std::uint16_t _val) noexcept
{
    set_value<std::uint16_t, crc_size>(_val, m_crc_field, crc_order);
}

binpacket_t::crc_t::value_type binpacket_t::evaluate_crc(const void * _ptr,
                                                    std::size_t _sz)
{
    crc_ccitt1.reset(0);
    crc_ccitt1.process_bytes(_ptr, _sz);
    return  crc_ccitt1.checksum();
}

upair_t binpacket_t::make_packet(const upair_t& _data)
{
    const upair_t& pair = _data;
    auto size = pair.second + header_size + crc_size;
    simplechar_t* ptr = new simplechar_t[size];
    upair_t newpair;
    newpair.first.reset(ptr);
    newpair.second = size;
    memcpy(ptr + header_size, pair.first.get(), pair.second);
    set_length(pair.second);
    memcpy(ptr, signat, sig_size);
    memcpy(ptr + length_ofs, m_lng_field, length_size);
    auto crc = evaluate_crc(pair.first.get(), pair.second);
    set_crc(crc);
    memcpy(ptr + header_size + pair.second, m_crc_field, crc_size);
    return upair_t(std::move(newpair));
}

bool binpacket_t::validate_packet(cvalconv_t _buff,
                              [[maybe_unused]] bool _callinh)
{
    // Проверка сигнатуры
    const value_t * ptr = _buff;
    fill_field(ptr, m_sign_field);
    auto cmpresult = memcmp(&signat, &m_sign_field, sig_size);
    if (cmpresult != 0)
    {
        DBGOUT("cmpl: bad signature")
        return false;
    }
    // Определение длинны поля данных и проверка того
    // что она не привышает за размер пакета.
    fill_field(ptr + length_ofs, m_lng_field);
    auto lng_value = get_length();
    if (lng_value > max_valid_size)
    {
        DBGOUT("cmpl: length > max_valid_size")
        return false;
    }
    // Проверка контрольной суммы
    std::size_t m_crc_ofs = header_size + lng_value;
    fill_field(ptr + m_crc_ofs, m_crc_field);
    auto crc_estimated = evaluate_crc(ptr + header_size, lng_value);
    auto crc_value = get_crc();
    DBGOUT2("cmpl: estimated crc = ", crc_estimated);
    DBGOUT2("cmpl: written crc:", crc_value)
    DBGOUT2("cmpl: compare:" , (crc_value == crc_estimated))
            return (crc_value == crc_estimated);
}

binpacket_t::parsedata_t binpacket_t::parse_packet(const upair_t &_data)
{
    parsedata_t pd;
    auto up = std::make_shared<unpacked_t>();
    bool result = false;
    result = binpacket_t::validate_packet(_data.first.get());
    if (!result)
    {
        return pd;
    }
    up->m_crc = get_crc();
    up->m_length = get_length();
    auto ptr = new value_t[up->m_length];
    up->m_data.reset(ptr);

    memcpy(ptr, _data.first.get(), up->m_length);
    pd.m_current = up;
    return pd;
}
