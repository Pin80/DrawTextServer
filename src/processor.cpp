/** @file
 *  @brief         Модуль интерфейса обработчика данных
 *  @author unnamed
 *  @date   created 2020
 *  @date   modified 2020
 *  @version 1.0
 */
#include "../include/processor.h"

std::size_t compound_completer_t::do_completer(const error_code& _err,
                               std::size_t _sz,
                               std::size_t _id)
{
    const char * FUNCTION = __FUNCTION__;
    try
    {
        if (m_laststate == eCmplState::finished) return 0;
        if (_err)
        {
            LOG << str_sum("cmpl: error io_service for id:", _id);
            LOG << _err.message();
            m_iserror = true;
            return 0;
        }
        if (transfer_counter++ > max_transfers)
        {
            LOG << str_sum("cmpl: error, too many transfers", _id);
            m_iserror = true;
            return 0;
        }
    #if BUILDTYPE == BUILD_DEBUG
        if (transfer_counter > 5)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        }
    #endif
        if (_sz == 0) return default_max_transfer_size;
        m_complcounter = _sz;
        DBGOUT6("cmpl: code:", _id, "transf:", _sz, "total:", m_complcounter)
        while(1)
        {
            switch (m_laststate)
            {
                case eCmplState::noSign:
                {
                    m_type = ePackType_t::bin;
                    if ( complStatus(m_complcounter) < eCmplState::noLng )
                    {
                        DBGOUT("cmpl: counter < sig_size")
                        WATCH(1, m_laststate)
                        return default_max_transfer_size;
                    }
                    m_laststate = eCmplState::noLng;
                    // Проверка сигнатуры
                    fill_field(m_startcurrptr, m_sign_field);
                    auto cmpresult = memcmp(&signat, &m_sign_field, sig_size);
                    if (cmpresult != 0)
                    {
                        DBGOUT2("cmpl: bad signature ", (unsigned long)this)
                        m_laststate = eCmplState::unRec;
                        continue;

                    }
                    [[fallthrough]];
                }
                case eCmplState::noLng:
                {
                    if (complStatus(m_complcounter) < eCmplState::noDataCrc)
                    {
                        WATCH(2, m_laststate)
                        DBGOUT("cmpl: counter < header_size");
                        return default_max_transfer_size;
                    }
                    // Определение длинны поля данных и проверка того
                    // что она не привышает за размер пакета.
                    fill_field(m_startcurrptr + length_ofs, m_lng_field);
                    m_lng_value = get_length();
                    if (m_lng_value > max_valid_size)
                    {
                        m_iserror = true;
                        m_laststate = eCmplState::finished;
                        DBGOUT("cmpl: length > max_valid_size")
                        return 0;
                    }
                    m_laststate = eCmplState::noDataCrc;
                    [[fallthrough]];
                }
                case eCmplState::noDataCrc:
                {
                    if (complStatus(m_complcounter) < eCmplState::finished)
                    {
                        WATCH(3, m_laststate)
                        DBGOUT("cmpl: counter < m_length+header+crc")
                        return default_max_transfer_size;
                    }
                    // Проверка контрольной суммы
                    std::size_t crc_ofs = header_size + m_lng_value;
                    fill_field(m_startcurrptr + crc_ofs, m_crc_field);
                    m_crc_estimated = evaluate_crc(m_startcurrptr + header_size,
                                                   m_lng_value);
                    m_crc_value = get_crc();
                    m_iserror = (m_crc_value != m_crc_estimated);
                    DBGOUT2("cmpl: estimated crc = ", m_crc_estimated);
                    DBGOUT2("cmpl: written crc:", m_crc_value)
                    DBGOUT2("cmpl: compare:" , !m_iserror)
                    if (m_iserror)
                    {
                        return 0;
                    }
                    auto pack_offset = m_startcurrptr - m_startfirstptr;
                    if (pack_offset + crc_ofs + crc_size < m_complcounter)
                    {
                        m_startcurrptr += crc_ofs + crc_size;
                        m_laststate = eCmplState::noSign;
                        return default_max_transfer_size;
                    }
                    m_laststate = eCmplState::finished;
                    return 0;
                }
                case eCmplState::unRec:
                {
                    m_type = ePackType_t::text;
                    unsigned pack_offset = m_startcurrptr - m_startfirstptr;
                    if ((m_complcounter - pack_offset) < m_http_lng)
                    {
                        DBGOUT("cmpl: counter < m_http_term")
                        return default_max_transfer_size;
                    }
                    std::string_view sv_valid(m_http_term);
                    std::string_view sv_buff((char *)(m_startcurrptr),
                                             m_complcounter);
                    auto it2 = std::search(sv_buff.begin(),
                                sv_buff.end(),
                                sv_valid.begin(),
                                sv_valid.end());

                    if (it2 == sv_buff.end())
                    {
                        DBGOUT("cmpl: not found terminator")
                        return default_max_transfer_size;
                    }
                    auto dist = std::distance(sv_buff.begin(), it2);
                    dist += m_http_lng;
                    DBGOUT2("cmpl: found terminator", dist)
                    if (dist  == (m_complcounter - pack_offset))
                    {
                        m_laststate = eCmplState::finished;
                        return 0;
                    }
                    DBGOUT("cmpl: goto eCmplState::noSign")
                    m_startcurrptr += dist;
                    m_laststate = eCmplState::noSign;
                    continue;
                }
                case eCmplState::finished: { return 0; }
                default:
                {
                    assert("internal error");
                    throw std::exception();
                }
            }
        }
        DBGOUT("cmpl: completed !!!")
        return 0;
    } catch (...)
    {
        errspace::show_errmsg(FUNCTION);
        throw;
    }
}

std::size_t binpack_completer_t::do_completer(const error_code& _err,
                                 std::size_t _sz,
                                 std::size_t _id)
{
    const char * FUNCTION = __FUNCTION__;
    try
    {
        if (m_laststate == eCmplState::finished) return 0;
        if (_err)
        {
            LOG << str_sum("cmpl: io_service error id:", _id);
            DBGOUT(_err.message())
            m_iserror = true;
            return 0;
        }
        if (transfer_counter++ > max_transfers)
        {
            LOG << str_sum("cmpl: error, too many transfers", _id);
            m_iserror = true;
            return 0;
        }
    #if BUILDTYPE == BUILD_DEBUG
        if (transfer_counter > 5)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        }
    #endif
        if (_sz == 0) return default_max_transfer_size;
        m_complcounter = _sz;
        DBGOUT6("cmpl: code:", _id, "transf:", _sz, "total:", m_complcounter)

        switch (m_laststate)
        {
            case eCmplState::noSign:
            {
                if ( complStatus(m_complcounter) < eCmplState::noLng )
                {
                    DBGOUT("cmpl: counter < sig_size")
                    WATCH(1, m_laststate)
                    return default_max_transfer_size;
                }
                m_laststate = eCmplState::noLng;
                fill_field(m_startcurrptr, m_sign_field);
                auto cmpresult = memcmp(&signat, &m_sign_field, sig_size);
                if (cmpresult != 0)
                {
                    DBGOUT("cmpl: bad signature")
                    m_iserror = true;
                    m_laststate = eCmplState::finished;
                    return 0;
                }
                [[fallthrough]];
            }
            case eCmplState::noLng:
            {
                if ( complStatus(m_complcounter) < eCmplState::noDataCrc )
                {
                    WATCH(2, m_laststate)
                    DBGOUT("cmpl: counter < header_size");
                    return default_max_transfer_size;
                }
                // Определение длинны поля данных и проверка того
                // что она не привышает за размер пакета.
                fill_field(m_startcurrptr + length_ofs, m_lng_field);
                m_lng_value = get_length();
                if (m_lng_value > max_valid_size)
                {
                    m_iserror = true;
                    m_laststate = eCmplState::finished;
                    DBGOUT2("cmpl: length > max_valid_size:", m_lng_value)
                    return 0;
                }
                m_laststate = eCmplState::noDataCrc;
                [[fallthrough]];
            }
            case eCmplState::noDataCrc:
            {
                if ( complStatus(m_complcounter) < eCmplState::finished )
                {
                    WATCH(3, m_laststate)
                    DBGOUT2("cmpl: counter < m_length+header+crc:", m_lng_value)
                    return default_max_transfer_size;
                }
                std::size_t crc_ofs = header_size + m_lng_value;
                fill_field(m_startcurrptr + crc_ofs, m_crc_field);
                m_crc_estimated = evaluate_crc(m_startcurrptr + header_size,
                                               m_lng_value);
                m_crc_value = get_crc();
                m_iserror = (m_crc_value != m_crc_estimated);
                DBGOUT2("cmpl: estimated crc = ", m_crc_estimated);
                DBGOUT2("cmpl: written crc:", m_crc_value)
                DBGOUT2("cmpl: compare:" , !m_iserror)
                if (m_iserror)
                {
                    return 0;
                }
                auto pack_ofset = m_startcurrptr - m_startfirstptr;
                if (pack_ofset + crc_ofs + crc_size < m_complcounter)
                {
                    m_startcurrptr += crc_ofs + crc_size;
                    m_laststate = eCmplState::noSign;
                    return default_max_transfer_size;
                }
                m_laststate = eCmplState::finished;
                return 0;
            }
            case eCmplState::finished: { return 0; }
            default:
            {
                assert("internal error");
                throw std::exception();
            }
        }
        DBGOUT("cmpl: completed !!!")
        return 0;

    } catch (...)
    {
        errspace::show_errmsg(FUNCTION);
        throw;
    }
}

std::size_t http_completer_t::do_completer(const error_code& _err,
                                 std::size_t _sz,
                                 std::size_t _id)
{
    const char * FUNCTION = __FUNCTION__;
    try
    {
        if (_err)
        {
            DBGOUT("cmpl: error")
            m_iserror = true;
            return 0;
        }
        if (_sz == 0) return default_max_transfer_size;
        m_chunks++;
        m_complcounter = _sz;
        DBGOUT2("cmpl: compleater:", _sz)
        DBGOUT2("cmpl: total:", m_complcounter)
        std::string_view sv_valid(m_http_term);
        std::size_t size = m_buff.second;
        char* ptr = m_buff.first.get();
        std::string_view sv_buff(ptr ,size);

        auto it2 = std::search(sv_buff.begin(),
                    sv_buff.end(),
                    sv_valid.begin(),
                    sv_valid.end());

        if (it2 != sv_buff.end())
        {
            DBGOUT("cmpl: found terminator")
            return 0;
        }
        else
        {
            DBGOUT("cmpl: not found terminator")
            return default_max_transfer_size;
        }

    } catch (...)
    {
        errspace::show_errmsg(FUNCTION);
        throw;
    }
}

complwrapper_t::complwrapper_t(eCompl_t _type, upair_t& _buff)
{
    const char * FUNCTION = __FUNCTION__;
    try
    {

        switch(_type)
        {
            case eCompl_t::none: break;
            case eCompl_t::bin:
            {
                m_id = 0;
                auto ptr = new binpack_completer_t(_buff);
                m_pimpl.reset(ptr);
                break;
            }
            case eCompl_t::debug:
            {
                m_id = 1;
                auto ptr = new binpack_completer_t(_buff);
                m_pimpl.reset(ptr);
                break;
            }
            case eCompl_t::http:
            {
                auto ptr = new http_completer_t(_buff);
                m_pimpl.reset(ptr);
                break;
            }
            case eCompl_t::compound:
            {
                m_id = 1;
                auto ptr = new compound_completer_t(_buff);
                m_pimpl.reset(ptr);
                break;
            }
            default: assert(false);
        }

    } catch (...)
    {
        errspace::show_errmsg(FUNCTION);
        throw;
    }
}

