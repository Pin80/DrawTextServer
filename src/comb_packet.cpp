/** @file
 *  @brief         Модуля класса для комбинированого пакета
 *  @author unnamed
 *  @date   created 10.01.2020
 *  @date   modified 10.01.2020
 *  @version 1.0 (alpha)
 */
#include "../include/comb_packet.h"

constexpr std::size_t compndpack_t::get_img_size() const
{
    return get_value<std::size_t, imgfld_size>(m_imgsize_field, comm_order);
}

void compndpack_t::set_img_width(const std::size_t _val)
{
    set_value<std::size_t, imgfld_size>(_val, m_imgsize_field, comm_order);
}

upair_t compndpack_t::make_packet(upair_t & _data,
                     const std::string& _txt)
{
    upair_t& pair = _data;
    upair_t newpair;
    auto datasize = sub_header_size + pair.second + _txt.size() + 1;
    auto totalsize = header_size + datasize + crc_size;
    simplechar_t* ptr = new simplechar_t[ totalsize ];
    newpair.first.reset(ptr);
    newpair.second = totalsize;
    memcpy(ptr, signat, sig_size);
    set_length(datasize);
    memcpy(ptr + length_ofs, m_lng_field, length_size);

    set_img_width(pair.second);
    memcpy(ptr + imgfld_ofs, m_imgsize_field, imgfld_size);

    memcpy(ptr + header_size + sub_header_size,
           pair.first.get(),
           pair.second);

    memcpy(ptr + header_size + sub_header_size + pair.second,
           _txt.c_str(),
           _txt.size() + 1);

    auto crc = evaluate_crc(ptr + header_size, datasize);
    set_crc(crc);
    memcpy(ptr + header_size + datasize, m_crc_field, crc_size);
    return newpair;
}

bool compndpack_t::validate_packet(cvalconv_t _buff,
                                      bool _callinh)
{
    const value_t * ptr = _buff;
    bool result = (_callinh)?binpacket_t::validate_packet(ptr):false;
    if ((!result) && (_callinh))
    {
        DBGOUT("cmpl: bad packet")
        return false;
    }
    // Проверка ширины
    fill_field(ptr + imgfld_ofs, m_imgsize_field);
    auto size = get_img_size();
    result = ((size <= max_img_size) && (size > min_img_size));
    if (!result)
    {
        DBGOUT("cmpl: bad size")
        return false;
    }
    return result;
}

binpacket_t::parsedata_t compndpack_t::parse_packet(const upair_t &_data)
{
    parsedata_t pd;
    auto up = std::make_shared<unpacked_tComb>();
    bool result = false;

    pd.m_next = binpacket_t::parse_packet(_data);
    result = pd.m_next.has_value();
    if (!result)
    {
        return pd;
    }

    result = validate_packet(_data.first.get(), false);
    if (!result)
    {
        return pd;
    }

    auto imgsize = get_img_size();
    value_t * ptr = new value_t[imgsize];
    up->m_data.reset(ptr);
    up->m_imgsize = imgsize;
    auto srcptr = _data.first.get() + imgfld_ofs + imgfld_size;
    memcpy(ptr, srcptr, imgsize);
    auto strsize = get_length() - imgsize - sub_header_size;
    srcptr += imgsize;
    up->m_str.resize(strsize);
    auto sptr = const_cast<char *>(up->m_str.c_str());
    memcpy(sptr, srcptr, strsize);
    pd.m_current = up;
    return pd;
}
