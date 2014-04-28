//
//  signal_base.h
//  slimsig
//
//  Created by Christopher Tarquini on 4/21/14.
//
//

#ifndef slimsig_signal_base_h
#define slimsig_signal_base_h
#include <slimsig/connection.h>
#include <functional>
#include <iterator>
#include <memory>
#include <vector>
#include <algorithm>
#include <atomic>
#include <mutex>
#include <cmath>
namespace slimsig {
struct multithread_policy {
  using slot_id = std::atomic<uint_least64_t>;
  using mutex = std::mutex;
  using lock = std::unique_lock<mutex>;
  using slot_mutex = std::mutex;
  using slot_lock = std::mutex;
  
};
// used in single threaded mode
// doesn't actually do any locking
// uses always inlined empty methods
// to make sure they are optimized out by the compiler
struct fake_mutex {
  [[gnu::always_inline]]
  inline void lock() {};
  [[gnu::always_inline]]
  inline void unlock() {};
  [[gnu::always_inline]]
  inline bool try_lock() { return true; };
};

struct singlethread_policy {
  using slot_id = std::uint_least64_t;
  using mutex = fake_mutex;
  using lock = std::unique_lock<fake_mutex>;
  using slot_mutex = fake_mutex;
  using slot_lock = fake_mutex;
};

template <class Handler, class ThreadPolicy, class Allocator>
class signal;

template <class ThreadPolicy, class Allocator, class F>
class signal_base;


template<class ThreadPolicy, class Allocator, class R, class... Args>
class signal_base<ThreadPolicy, Allocator, R(Args...)>
{
public:
  using return_type = R;
  using callback = std::function<R(Args...)>;
  using allocator_type = Allocator;
  using slot_list = slimsig::slot_list<R(Args...), typename ThreadPolicy::slot_id, bool>;
  using slot = typename slot_list::slot;
  using list_allocator_type = typename std::allocator_traits<Allocator>::template rebind_traits<slot_list>::allocator_type;
  using const_slot_reference = typename slot_list::const_reference;
  using connection = connection<signal_base>;
  using extended_callback = std::function<R(connection& conn, Args...)>;
  using thread_policy = ThreadPolicy;
  using slot_id = typename thread_policy::slot_id;

private:
public:
  static constexpr std::size_t arity = sizeof...(Args);
  
  template <std::size_t N>
  struct argument
  {
    static_assert(N < arity, "error: invalid parameter index.");
    using type = typename std::tuple_element<N,std::tuple<Args...>>::type;
  };
  
  // allocator constructor
  signal_base(const allocator_type& alloc) :
    allocator(alloc),
    m_slots(std::allocate_shared<slot_list>(alloc, list_allocator_type(alloc))),
    is_running(false),
    last_id() {};
  
  signal_base(size_t capacity, const allocator_type& alloc = allocator_type{})
  : signal_base(alloc) {
    m_slots->active.reserve(capacity);
  }
  
  // use R and Args&&.. for better autocompletion
  inline R emit(Args... args) {
    auto& slots = *m_slots;
    std::lock_guard<slot_list> guard(slots);
    // runs the slot if connected, otherwise return true and queue it for deletion
    auto last_slot = slots.back();
    auto is_disconnected = [&] (const_slot_reference slot)
    {
      if (slot) {
        if (slot == last_slot) {
          (*slot)(std::forward<Args>(args)...);
        } else {
          (*slot)(args...);
        }

        return false;
      }
      else return true;
    };
    
    
    auto begin = slots.begin();
    auto end = slots.end();
    // sane implementations only move elements if the predicate returns true
    // as if calling find_if to find the first matching slot, then shifting the rest of the values
    // so the matched elements end up at the end of the array
    begin = std::remove_if(begin, end, is_disconnected);
    slots.erase(begin, end);
  }
  
  inline connection connect(callback slot)
  {
    auto sid = m_slots->emplace(std::move(slot));
    return connection { m_slots, sid};
  };
  
  inline connection connect_extended(extended_callback slot)
  {
    struct extended_slot {
      callback fn;
      connection connection;
      R operator()(Args&&... args) {
        return fn(connection, std::forward<Args>(args)...);
      }
    };
    auto& container = queue();
    auto slot_id = ++last_id;
    container.emplace_back(extended_slot
    {
      std::move(slot), { m_slots, slot_id }
    }, slot_id);
    return connection { m_slots, slot_id };
  }
  
  template <class TP, class Alloc>
  inline connection connect(std::shared_ptr<signal<R(Args...), TP, Alloc>> signal) {
    using signal_type = typename decltype(signal)::element_type;
    
    struct signal_slot {
      std::weak_ptr<signal_type> handle;
      connection connection;
      R operator()(Args&&... args) {
        auto signal = handle.lock();
        if (signal) {
          return signal->emit(std::forward<Args>(args)...);
        } else {
          connection.disconnect();
          return detail::default_value<R>();
        }
      }
    };
    
    auto slot_id = m_slots->emplace(signal_slot { signal, {m_slots,0}});
    return connection { m_slots,  slot_id};
  }
  
  connection connect_once(callback slot)  {
    struct fire_once {
      callback fn;
      connection connection;
      R operator() (Args&&... args) {
        connection.disconnect();
        return fn(std::forward<Args>(args)...);
      }
    };
    auto slot_id = m_slots->template emplace_extended<fire_once>(std::move(slot));
    return connection { m_slots, slot_id };
  }
  void disconnect_all() {
    m_slots->clear();
  }
  const allocator_type& get_allocator() const {
    return allocator;
  }
  inline bool empty() const {
    return m_slots->empty();
  }
  inline std::size_t slot_count() const {
    return m_slots->total_size();
  }
  inline void compact() {
    auto begin = std::remove_if(m_slots->begin(), m_slots->end(), &is_disconnected);
    m_slots.erase(begin, m_slots->end());
  }
  template <class FN, class TP, class Alloc>
  friend class signal;
private:
  static inline bool is_disconnected(const_slot_reference slot) {
    return slot && bool(*slot);
  }
  allocator_type allocator;
  inline slot_list& queue() {
    return !is_running ? m_slots->active : m_slots->pending;
  }
  std::shared_ptr<slot_list> m_slots;
  bool is_running;
  slot_id last_id;
};
}

#endif
