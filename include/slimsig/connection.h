//
//  connection.h
//  slimsig
//
//  Created by Christopher Tarquini on 4/21/14.
//
//

#ifndef slimsig_connection_h
#define slimsig_connection_h
#include <memory>
#include <functional>
#include <algorithm>
namespace slimsig {
template <class Signal>
class connection;
namespace detail {
template <class Allocator, class F>
class signal_base;

template <class F>
class slot;

template <class SlotListT>
struct slot_store : std::enable_shared_from_this<slot_store<SlotListT>> {
  using slot_list = SlotListT;
  using allocator_type = typename SlotListT::allocator_type;
  slot_store(const allocator_type& alloc) :
    active(alloc),
    pending(alloc) {};
  
  slot_list active;
  slot_list pending;
  
};

template <class R, class... Args>
class slot<R(Args...)> {
public:
  using function_type = std::function<R(Args...)>;
  slot(function_type fn, unsigned long long slot_id) : m_fn(std::move(fn)), m_slot_id(slot_id) {};
  bool operator==(const slot& other) {
    return m_slot_id == other.m_slot_id;
  }
  bool operator <(const slot& other) {
    return m_slot_id < other.m_slot_id;
  }
  bool operator >(const slot& other) {
    return m_slot_id > other.m_slot_id;
  }
  inline operator bool() {
   return bool(m_fn);
  }
  bool connected() {
    return bool(m_fn);
  }
  void disconnect() {
    m_fn = nullptr;
  }
  R operator() (Args... args) {
    return m_fn(std::forward<Args>(args)...);
  }
  template <class A, class F>
  friend class signal_base;
  template <class Signal>
  friend class slimsig::connection;
private:
  inline function_type& slot_function() {
    return m_fn;
  }
  template <class T>
  inline T* slot_target() {
    return m_fn.template target<T>();
  }
  function_type m_fn;
  unsigned long long m_slot_id;
};
} // detail
  
  template <class Signal>
  class connection {
  using slot_type = typename Signal::slot_type;
  using slot_list = typename Signal::slot_list;
  using slot_list_iterator = typename Signal::slot_list::iterator;
  using slot_storage = typename Signal::slot_storage_type;
  connection(std::weak_ptr<slot_storage> slots, const slot_type& slot) : m_slots(std::move(slots)), m_slot_id(slot.m_slot_id) {};
  public:
    connection() {}; // empty connection
    connection(const connection& other) : m_slots(other.m_slots), m_slot_id(other.m_slot_id) {};
    connection(connection&& other) : m_slots(std::move(other.m_slots)), m_slot_id(other.m_slot_id) {};
    
    connection& operator=(connection&& rhs) {
      this->swap(rhs);
      return *this;
    }
    connection& operator=(const connection& rhs) {
      m_slot_id = rhs.m_slot_id;
      m_slots = rhs.m_slots;
      return *this;
    }
    
    void swap(connection& other) {
      using std::swap;
      swap(m_slots, other.m_slots);
      swap(m_slot_id, other.m_slot_id);
    }
    
    [[gnu::always_inline]]
    inline bool connected() {
      auto slot = get_slot();
      return slot && slot->connected();
    }
    [[gnu::always_inline]]
    inline void disconnect() {
      auto slot = get_slot();
      if (slot) {
        slot->disconnect();
      }
    }
    template <class Allocator, class F>
    friend class detail::signal_base;
    
  private:
    slot_type* get_slot() {
      auto ptr = m_slots.lock();
      if (!ptr) return nullptr;
      auto slot = get_slot(ptr->active);
      return slot ? slot : get_slot(ptr->pending);
    }
    inline slot_type* get_slot(slot_list& slots) {
      auto begin = slots.begin();
      auto end = slots.end();
      begin = get_slot(begin, end);
      return begin != end ? &*begin : nullptr;
    }
    slot_list_iterator get_slot(slot_list_iterator begin, slot_list_iterator end) {
      begin = std::lower_bound(begin, end, m_slot_id, [] (const slot_type& slot, unsigned long long slot_id)
      {
        return slot.m_slot_id < slot_id;
      });
      while (begin != end) {
        auto& slot = *begin;
        if (slot.m_slot_id == m_slot_id) break;
        else if (slot.m_slot_id > m_slot_id) {
          begin = end;
          break;
        }
      }
      return begin;
    }
    static slot_type* find(std::shared_ptr<slot_storage> store, unsigned long long slot_id) {
      if (store == nullptr) return nullptr;
      auto& slots = *store;
      auto is_match = [slot_id] (const slot_type& slot) {
        return slot.m_slot_id == slot_id;
      };
      auto begin = slots.active.begin();
      auto end = slots.active.end();
      begin = std::find_if(begin, end, is_match);
      if (begin != end) return &*begin;
      begin = slots.pending.begin();
      end = slots.pending.end();
      begin = std::find_if(begin, end, is_match);
      return begin != end ? &*begin : nullptr;
    }
    unsigned long long m_slot_id;
    std::weak_ptr<slot_storage> m_slots;
  };
  
  template <class connection>
  class scoped_connection {
  public:
    scoped_connection() : m_connection() {};
    scoped_connection(const connection& target) : m_connection(target) {};
    scoped_connection(const scoped_connection&) = delete;
    scoped_connection(scoped_connection&& other) : m_connection(other.m_connection) {};
    
    scoped_connection& operator=(const connection& rhs) {
      m_connection = rhs;
    };
    scoped_connection& operator=(scoped_connection&& rhs) {
      this->swap(rhs);
      return *this;
    };
    void swap(scoped_connection& other) {
      m_connection.swap(other.m_connection);
    };
    connection release() {
      connection ret{};
      m_connection.swap(ret);
      return ret;
    }
    ~scoped_connection() {
      m_connection.disconnect();
    }
  private:
    connection m_connection;
  };
  
  template <class connection>
  scoped_connection<connection> make_scoped_connection(connection&& target) {
    return scoped_connection<connection>(std::forward<connection>(target));
  };
 
}

#endif
