/** @file
 *  @brief         Заголовочный файл модуля комбинированого пакета
 *  @author unnamed
 *  @date   created 10.01.2020
 *  @date   modified 10.01.2020
 *  @version 1.0 (alpha)
 */

#ifndef COMB_PACKET_H
#define COMB_PACKET_H
#include "../include/packet.h"

struct compndpack_t : public binpacket_t
{
    struct unpacked_tComb
    {
        std::uint32_t m_imgsize = 0;
        std::unique_ptr<value_t[]> m_data;
        std::string m_str;
    };
    // Единый размер полей ширины, высоты страйда
    static constexpr auto common_size = sizeof (unpacked_tComb::m_imgsize);
    // Максимальный габарит изображения
    static constexpr auto max_img_size = 1024*512;
    // Минимальный габарит изображения
    static constexpr auto min_img_size = 16;
    // Размер поля размера
    static constexpr auto imgfld_size = common_size;
    // Смещение поля
    static constexpr auto imgfld_ofs = header_size;
    // Размер субзаголовка
    static constexpr auto sub_header_size = imgfld_size;
    // Порядок байт
    static constexpr value_t comm_order[common_size] = {3, 2, 1, 0};
    // Поле размера изображения
    value_t m_imgsize_field[imgfld_size];
    //
    constexpr std::size_t get_img_size() const;
    //
    void set_img_width(const std::size_t _val);
    //
    upair_t make_packet(upair_t & _data,
                         const std::string& _txt);
    //
    virtual bool validate_packet(cvalconv_t _buff, bool _callinh  = true) override;
    //
    virtual parsedata_t parse_packet(const upair_t& _data) override;
    //
    virtual ~compndpack_t() = default;
};

#endif // COMB_PACKET_H
