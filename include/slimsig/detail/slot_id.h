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
namespace slimsig { namespace detail {
  class slot_id {
  public:
    slot_id() : m_value({0, 0}) {};
    slot_id(const slot_id&) = default;
    slot_id(slot_id&&) = default;
    slot_id(std::pair<std::uint_fast64_t, std::uint_fast64_t> pair) : m_value(std::move(pair)) {};
    
    [[gnu::always_inline]]
    inline slot_id& operator=(const slot_id&) = default;
    [[gnu::always_inline]]
    inline slot_id& operator=(slot_id&& other) = default;
    
    [[gnu::always_inline]]
    inline bool operator==(const slot_id& other) {
      return m_value == other.m_value;
    };
    [[gnu::always_inline]]
    inline bool operator!=(const slot_id& other) const {
      return m_value != other.m_value;
    };
    [[gnu::always_inline]]
    inline bool operator<(const slot_id& other) const {
      return m_value < other.m_value;
    };
    [[gnu::always_inline]]
    inline bool operator<=(const slot_id& other) const {
      return m_value <= other.m_value;
    };
    [[gnu::always_inline]]
    inline bool operator>(const slot_id& other) const {
      return m_value > other.m_value;
    };
    [[gnu::always_inline]]
    inline bool operator>=(const slot_id& other) const {
      return m_value >= other.m_value;
    };
    [[gnu::always_inline]]
    inline slot_id& operator++() {
      auto previous_lower = m_value.second++;
      m_value.first += (m_value.second < previous_lower);
      return *this;
    };
    [[gnu::always_inline]]
    inline slot_id operator++(int) {
      slot_id value(*this);
      ++(*this);
      return value;
    };
    [[gnu::always_inline]]
    inline slot_id& operator--() {
      auto previous_lower = m_value.second--;
      m_value.first -= (m_value.second > previous_lower);
      return *this;
    };
    [[gnu::always_inline]]
    inline slot_id operator--(int) {
      slot_id value(*this);
      ++*this;
      return value;
    };
  friend std::ostream & operator<<(std::ostream & stream, const slot_id& rhs);
  private:
    std::pair<std::uint_fast64_t, std::uint_fast64_t> m_value;
  };

std::ostream & operator<<(std::ostream & stream, const slot_id& rhs){
    struct stream_guard {
      stream_guard (std::ostream& stream) : format_holder(nullptr), target_stream(stream) {
        format_holder.copyfmt(target_stream);
      };
      ~stream_guard() { target_stream.copyfmt(format_holder); }
      std::ostream format_holder;
      std::ostream& target_stream;
    } guard {stream};
    stream << std::hex << rhs.m_value.first << " " << rhs.m_value.second << std::endl;
    return stream;
}
}};

#endif
