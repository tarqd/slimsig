//
//  slot_id.h
//  slimsig
//
//  Created by Christopher Tarquini on 2/20/15.
//
//

#ifndef slimsig_slot_id_h
#define slimsig_slot_id_h
#include <algorithm>
#include <stdint.h>
#include <utility>
#include <iostream>
#include <limits>
namespace slimsig {
namespace detail {
    class slot_id {
    public:
        constexpr slot_id()
            : m_high(0)
            , m_low(0){};
        constexpr slot_id(const slot_id&) = default;
        slot_id(slot_id&&) = default;
        constexpr slot_id(std::uint_fast64_t high, std::uint_fast64_t low)
            : m_high(high)
            , m_low(low){};
        constexpr slot_id(std::uint_fast64_t low)
            : slot_id(0, low){};
        friend class std::numeric_limits<slot_id>;
        [[gnu::always_inline]] inline slot_id& operator=(const slot_id&) = default;
        [[gnu::always_inline]] inline slot_id& operator=(slot_id&& other) = default;

        [[gnu::always_inline]] constexpr inline bool operator==(const slot_id& other) const
        {
            return m_high == other.m_high && m_low == other.m_low;
        };
        [[gnu::always_inline]] constexpr inline bool operator!=(const slot_id& other) const
        {
            return !(*this == other);
        };
        [[gnu::always_inline]] constexpr inline bool operator<(const slot_id& other) const
        {
            return m_high == other.m_high ? m_low < other.m_low : m_high < other.m_high;
        };
        [[gnu::always_inline]] constexpr inline bool operator<=(const slot_id& other) const
        {
            return (*this < other) || (*this == other);
        };
        [[gnu::always_inline]] constexpr inline bool operator>(const slot_id& other) const
        {
            return m_high == other.m_high ? m_low > other.m_low : m_high > other.m_high;
        };
        [[gnu::always_inline]] constexpr inline bool operator>=(const slot_id& other) const
        {
            return (*this > other) || (*this == other);
        };
        [[gnu::always_inline]] inline slot_id& operator++()
        {
            auto previous_lower = m_low++;
            m_high += (m_low < previous_lower);
            return *this;
        };
        [[gnu::always_inline]] inline slot_id operator++(int)
        {
            slot_id value(*this);
            ++(*this);
            return value;
        };
        [[gnu::always_inline]] inline slot_id& operator--()
        {
            auto previous_lower = m_low--;
            m_high -= (m_low > previous_lower);
            return *this;
        };
        [[gnu::always_inline]] inline slot_id operator--(int)
        {
            slot_id value(*this);
            ++(*this);
            return value;
        };
        friend std::ostream& operator<<(std::ostream& stream, const slot_id& rhs);

    private:
        uint_fast64_t m_high, m_low;
    };

    std::ostream& operator<<(std::ostream& stream, const slot_id& rhs)
    {
        struct stream_guard {
            stream_guard(std::ostream& stream)
                : format_holder(nullptr)
                , target_stream(stream)
            {
                format_holder.copyfmt(target_stream);
            };
            ~stream_guard() { target_stream.copyfmt(format_holder); }
            std::ostream format_holder;
            std::ostream& target_stream;
        } guard{stream};
        stream << std::hex << rhs.m_high << " " << rhs.m_low << std::endl;
        return stream;
    }
}
};

namespace std {
template <>
class numeric_limits<slimsig::detail::slot_id> : public numeric_limits<uint_fast64_t> {
private:
    using base = numeric_limits<uint_fast64_t>;
    using slot_id = slimsig::detail::slot_id;

public:
    static constexpr int digits = base::digits * 2;
    static constexpr int digits10 = base::digits10 * 2;
    static constexpr int max_digits10 = base::max_digits10 * 2;
    static constexpr slot_id min() { return {base::min(), base::min()}; };
    static constexpr slot_id lowest() { return min(); };
    static constexpr slot_id max() { return {base::max(), base::max()}; };
};
};

#endif
