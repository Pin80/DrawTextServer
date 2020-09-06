/** @file
 *  @brief         Заголовочный файл утилит сервера
 *  @author unnamed
 *  @date   created 2020
 *  @date   modified 2020
 *  @version 1.0
 */
#ifndef UTILITY_H
#define UTILITY_H
#include "../include/common.h"

class noncopyable_t // and nonmoveable
{
    noncopyable_t(const noncopyable_t&) = delete;
    noncopyable_t& operator=(const noncopyable_t&) = delete;
public:
    noncopyable_t() = default;
};

#define WATCHPOINT_NUMBER 10
template <int _N>
struct watchpoint_t
{
    static std::any m_var;
    watchpoint_t<_N - 1> m_next;
    std::any& operator()(int idx)
    {
        if (idx == _N) return m_var;
            else return m_next(idx);
    }
};

template <> struct watchpoint_t<0>
{
    static std::any m_var;
    std::any& operator()(int idx = 0)
    {
        return m_var;
    }
};
#if BUILDTYPE == BUILD_DEBUG
extern watchpoint_t<WATCHPOINT_NUMBER> wplist;
#define WATCH(N, VAR) wplist(N) = VAR;
#define COMPARE(N, VAR) (watchpoint_t<N>::m_var.has_value())?\
    std::any_cast<decltype(VAR)>(watchpoint_t<N>::m_var) == VAR:false;
#else
#define WATCH(N, VAR)
#define COMPARE(N, VAR) true;
#endif
// PASTE THIS MACCRO INTO MAIN.CPP FOR WATCHPOINT USING
#define DEFINEWATCHPOINT template <int _N> std::any watchpoint_t<_N>::m_var; \
    template struct watchpoint_t<WATCHPOINT_NUMBER>; \
    std::any watchpoint_t<0>::m_var; \
    extern "C++" { watchpoint_t<WATCHPOINT_NUMBER> wplist; }
/*
template <int _N> std::any watchpoint_t<_N>::m_var;
template struct watchpoint_t<WATCHPOINT_NUMBER>;
std::any watchpoint_t<0>::m_var;
extern "C++" { watchpoint_t<WATCHPOINT_NUMBER> wplist; }
*/

namespace errspace
{
    // when NOLOGSUPPORT commneted,
    // it should be overloaded non-template function in error.h
    template<typename T = void>
    inline void show_errmsg(const char * _msg) { assert(false); }
}

using simplechar_t = char;
using charuptr_t = std::unique_ptr<simplechar_t[]>;
using charsptr_t = std::shared_ptr<simplechar_t[]>;
using upair_t = std::pair<charuptr_t, std::size_t>;
using spair_t = std::pair<charsptr_t, std::size_t>;
using bstmbuff_t = boost::asio::streambuf;
using bnownbuff_t = boost::asio::mutable_buffers_1;

/** @class mvconv_t
 *  @brief Класс адаптера наподобие std::any
 */
class cpconv_t
{
    cpconv_t(cpconv_t&&) = delete;
    cpconv_t& operator=(const cpconv_t&) = delete;
    cpconv_t& operator=(cpconv_t&&) = delete;
public:
    enum eType {eNone, echaruptr_t, ebstmbuffptr_t};
    using bnownbuffptr_t = std::unique_ptr<bnownbuff_t>;
    using bstmbuffptr_t = std::unique_ptr<bstmbuff_t>;
private:
    upair_t* m_pcdest = nullptr;
    bstmbuffptr_t* m_pbdest = nullptr;
    eType m_type = eNone;
public:
    eType getType() const { return m_type; }
    cpconv_t(upair_t& _cptr)
    {
        m_pcdest = &_cptr;
        m_type = echaruptr_t;
    }
    cpconv_t(bstmbuffptr_t& _ppbuf)
    {
        m_pbdest = &_ppbuf;
        m_type = ebstmbuffptr_t;
    }
    cpconv_t& operator=(const upair_t& _ptr)
    {
        if (m_type != echaruptr_t)
        {
            if ((*m_pbdest) && (_ptr.first))
            {
                auto rptr_copy = _ptr.first.get();
                std::ostream stream(m_pbdest->get());
                stream.write(rptr_copy, _ptr.second);
            }
        }
        else
        {
            if ((m_pcdest) && (_ptr.first))
            {
                simplechar_t* rptr = (_ptr.second != 0)?
                            new simplechar_t[_ptr.second]:nullptr;
                m_pcdest->first.reset(rptr);
                memcpy(rptr, _ptr.first.get(), _ptr.second);
                m_pcdest->second = _ptr.second;
            }
        }
        return *this;
    }
    cpconv_t& operator=(const bstmbuffptr_t& _ptr)
    {
        if (m_type != ebstmbuffptr_t)
        {
            if (( m_pcdest) && (_ptr))
            {
                using iterator = std::istreambuf_iterator<char>;
                const auto size = _ptr.get()->size();
                simplechar_t * newbuf = (size != 0)?
                            new simplechar_t[size]:nullptr;
                m_pcdest->first.reset(newbuf);
                auto it_dest_start = newbuf;
                iterator it_src_start(_ptr.get());
                iterator it_src_end;
                std::copy(it_src_start, it_src_end, it_dest_start );
                m_pcdest->second = size;
            }
        }
        else
        {
            if ((*m_pbdest) && (_ptr))
            {
                bstmbuff_t& target = *m_pbdest->get();
                bstmbuff_t& source = *_ptr.get();
                if (target.size() > 0)
                {
                    target.consume(target.size());
                }
                auto size =  source.size();
                std::size_t bytes_copied = buffer_copy(
                  target.prepare(size), // target's output sequence
                  source.data());                // source's input sequence
                // Explicitly move target's output sequence to its input sequence.
                target.commit(bytes_copied);
            }
        }
        return *this;
    }
};


/** @class mvconv_t
 *  @brief Класс адаптера наподобие std::any
 */
class mvconv_t
{
    mvconv_t() = delete;
    mvconv_t(cpconv_t&&) = delete;
    mvconv_t& operator=(const cpconv_t&) = delete;
    mvconv_t& operator=(cpconv_t&&) = delete;
public:
    using bstmbuffptr_t = cpconv_t::bstmbuffptr_t;
    mvconv_t(upair_t&& _cptr)
    {
        m_cptr = std::move(_cptr.first);
        m_size = _cptr.second;
        _cptr.second = 0;
        type = cpconv_t::echaruptr_t;
    }
    mvconv_t(bstmbuffptr_t&& _pbufptr)
    {
        m_bufptr = std::move(_pbufptr);
        type = cpconv_t::ebstmbuffptr_t;
    }
    upair_t extractPair()
    { 
        if (type == cpconv_t::ebstmbuffptr_t)
        {
            using iterator = std::istreambuf_iterator<char>;
            const auto size = (m_bufptr)?m_bufptr.get()->size():0;
            upair_t pair;
            simplechar_t * newbuf = (size > 0)?new simplechar_t[size]:nullptr;
            pair.first.reset(newbuf);
            pair.second = size;
            if (newbuf)
            {
                auto it_dest_start = newbuf;
                iterator it_src_start(m_bufptr.get());
                iterator it_src_end;
                std::copy(it_src_start, it_src_end, it_dest_start );
            }
            m_bufptr.reset();
            return upair_t(std::move(pair));
        }
        else
        {
            return upair_t{std::move(m_cptr), m_size};
        }
    }
    bstmbuffptr_t extractBufPtr()
    {
        if (type == cpconv_t::echaruptr_t)
        {
            bstmbuffptr_t bufptr = std::make_unique<bstmbuff_t>();
            // std::ostream doesn't own buffer
            std::ostream stream(bufptr.get());
            if (m_cptr)
            {
                stream.write(m_cptr.get(), m_size);
            }
            m_cptr.reset();
            return bstmbuffptr_t{std::move(bufptr)};
        }
        else
        {
            return bstmbuffptr_t{std::move(m_bufptr)};
        }
    }
    cpconv_t::eType getType() const { return type;}
private:
    charuptr_t m_cptr;
    bstmbuffptr_t m_bufptr;
    std::size_t m_size = 0;
    cpconv_t::eType type = cpconv_t::eNone;
};

/** @class ticket_t
 *  @brief Класс уникального идентификатора соединения
 */
// Сама по себе уникальность может требоваться только
// при использовании потокобезопасных хештаблиц, как способ
// взаимодействия между объектом-сервером и объектом-обработчиком
// В варианте с boost::lockfree уникальность не требуется
class ticket_t
{
public:
    /// Тип поля ключа
    typedef std::uint8_t type[16];
    /// Конструктор класса
    /// Можно создать по умолчанию (без уникальности)
    ticket_t() = default;
    /// Конструктор класса
    /// для уникальности идентификатора в _obj можно поместить this
    ticket_t(void * _obj)
    {
      unsigned long tmp_val = reinterpret_cast<unsigned long>(_obj);
      memset(m_val, 0, sizeof(type));
      memcpy(m_val + 8, m_sold, sizeof(m_sold));
      memcpy(m_val, &tmp_val,sizeof (tmp_val));
    }
    bool operator < (const ticket_t& _t) const
    {
        return std::lexicographical_compare(std::begin(m_val),
                                          std::end(m_val),
                                          std::begin(_t.m_val),
                                          std::end(_t.m_val));
    }
    bool operator == (const ticket_t& _t) const
    {
      return std::equal(std::begin(m_val),
                          std::end(m_val),
                          std::begin(_t.m_val));
    }
    std::uint8_t getProcVal() const { return m_procval; }
    void setProcVal(std::uint8_t _val) { m_procval = _val; }
private:
  static constexpr const std::uint8_t m_sold[8] =
                            {0x55, 0xAA, 1, 7, 13, 2, 101, 250};
  std::uint8_t m_procval = 0;
  std::uint8_t m_val[16];
};

constexpr auto static_strlen(const char * _str ) noexcept
{   return std::char_traits<char>::length(_str); }

using value_t = unsigned char;

class cvalconv_t
{
public:
    constexpr cvalconv_t(const value_t * _ptr) noexcept :
        m_ptr(_ptr) {}
    cvalconv_t(const char * _ptr) noexcept :
        m_ptr(reinterpret_cast<const unsigned char *>(_ptr)) {}
    operator const value_t *() const noexcept {return m_ptr; }
    operator const char *() const noexcept
    { return reinterpret_cast<const char *>(m_ptr); }
    operator const void *() const noexcept
    { return m_ptr; }
    cvalconv_t& operator+(std::size_t _ofs) noexcept
    {   m_ptr += _ofs;   return *this;  }
    // this is not safe ptr:
    // virtual ~valconv_t() { delete m_ptr; }
private:
    const value_t * m_ptr;
};


class valconv_t
{
public:
    constexpr valconv_t(value_t * _ptr) noexcept :
        m_ptr(_ptr) {}
    valconv_t(char * _ptr) noexcept :
        m_ptr(reinterpret_cast<unsigned char *>(_ptr)) {}

    operator value_t *() const noexcept {return m_ptr; }
    operator char *() const noexcept
    { return reinterpret_cast<char *>(m_ptr); }
    operator void *() const noexcept
    { return m_ptr; }
    valconv_t& operator+(std::size_t _ofs) noexcept
    {   m_ptr += _ofs;   return *this;  }
    // this is not safe ptr:
    // virtual ~valconv_t() { delete m_ptr; }
private:
    value_t * m_ptr;
};

#if defined(__GNUC__) || defined(__MINGW32__)
    #define ALWAYS_INLINE   inline __attribute__((__always_inline__))
#else
    #define ALWAYS_INLINE   __forceinline
#endif

constexpr size_t CACHELINE_SIZE = 64;

ALWAYS_INLINE static void CpuRelax() noexcept
{
#if defined(__GNUC__) || defined(__MINGW32__)
    asm("pause");
#else
    _mm_pause();
#endif
}

class ticketspinlock_t
{
public:
    ticketspinlock_t() = default;
    ticketspinlock_t(const ticketspinlock_t& _tk)
    {
        ServingTicketNo = _tk.ServingTicketNo.load();
        NextTicketNo = _tk.NextTicketNo.load();
    }
    ALWAYS_INLINE void lock() noexcept
    {
        const auto myTicketNo = NextTicketNo.fetch_add(1, std::memory_order_relaxed);

        while (ServingTicketNo.load(std::memory_order_acquire) != myTicketNo)
            CpuRelax();
    }

    ALWAYS_INLINE void unlock() noexcept
    {
        const auto newNo = ServingTicketNo.load(std::memory_order_relaxed) + 1;
        ServingTicketNo.store(newNo, std::memory_order_release);
    }

private:
    alignas(CACHELINE_SIZE) std::atomic_size_t ServingTicketNo = {0};
    alignas(CACHELINE_SIZE) std::atomic_size_t NextTicketNo = {0};
};

template <typename T>
struct atomwrapper
{
  std::atomic<T> _a;

  atomwrapper()
    :_a()
  {}

  atomwrapper(const std::atomic<T> &a)
    :_a(a.load())
  {}

  atomwrapper(const atomwrapper &other)
    :_a(other._a.load())
  {}

  atomwrapper &operator=(const atomwrapper &other)
  {
    _a.store(other._a.load());
  }
};



#endif // UTILITY_H
