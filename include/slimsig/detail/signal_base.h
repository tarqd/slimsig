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
#include <cassert>
namespace slimsig {
struct multithread_policy {
  using slot_id = std::atomic<uint_least64_t>;
  using signal_mutex = std::recursive_mutex;
  using lock = std::unique_lock<signal_mutex>;
  using slot_mutex = std::recursive_mutex;
  using slot_lock = std::recursive_mutex;
  
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
  using signal_mutex = fake_mutex;
  using slot_mutex = fake_mutex;
  using slot_lock = fake_mutex;
};

template <class Handler, class ThreadPolicy, class Allocator>
class signal;

template <class ThreadPolicy, class Allocator, class Handler>
class signal_base;

template <class Handler, class ThreadPolicy, class Allocator>
void swap(signal<Handler, ThreadPolicy, Allocator>& lhs, signal<Handler, ThreadPolicy, Allocator> rhs);

template<class ThreadPolicy, class Allocator, class R, class... Args>
class signal_base<ThreadPolicy, Allocator, R(Args...)>
{
  struct emit_scope;
public:
  using return_type = R;
  using callback = std::function<R(Args...)>;
  using allocator_type = Allocator;
  using slot = basic_slot<R(Args...), typename ThreadPolicy::slot_id>;
  using slot_list = std::vector<std::vector<slot>>;
  using list_allocator_type = typename std::allocator_traits<Allocator>::template rebind_traits<slot_list>::allocator_type;
  using connection = connection<signal_base>;
  using extended_callback = std::function<R(connection& conn, Args...)>;
  using thread_policy = ThreadPolicy;
  using slot_id = typename thread_policy::slot_id;
  using slot_reference = typename slot_list::value_type::reference;
  using const_slot_reference = typename slot_list::value_type::const_reference;
private:
  using slot_list_reference = typename slot_list::reference;
  using const_slot_list_reference = typename slot_list::const_reference;
public:
  static constexpr std::size_t arity = sizeof...(Args);
  struct signal_holder {
    signal_holder(signal_base* p) : signal(p) {};
    signal_base* signal;
  };
  template <std::size_t N>
  struct argument
  {
    static_assert(N < arity, "error: invalid parameter index.");
    using type = typename std::tuple_element<N,std::tuple<Args...>>::type;
  };
  
  // allocator constructor
  signal_base(const allocator_type& alloc) :
    pending(0),
    m_self(nullptr),
    last_id(),
    m_size(0),
    m_depth(0),
    allocator(alloc),
    m_disconnect_all(false){};
  
  signal_base(size_t capacity, const allocator_type& alloc = allocator_type{})
  : signal_base(alloc) {};
  
  signal_base(signal_base&& other) {
    this->swap(other);
    
  }
  signal_base& operator=(signal_base&& other) {
    this->swap(other);
    return *this;
  }
  // no copy
  signal_base(const signal_base&) = delete;
  signal_base& operator=(const signal_base&) = delete;
  void swap(signal_base& rhs) {
    using std::swap;
    swap(pending, rhs.pending);
    swap(m_self, rhs.m_self);
    swap(last_id, rhs.last_id);
    swap(m_size, rhs.m_size);
    swap(m_depth, rhs.m_depth);
    swap(allocator, rhs.allocator);
    swap(m_disconnect_all, rhs.m_disconnect_all);
    if (m_self) m_self->signal = this;
    if (rhs.m_self) rhs.m_self->signal = &rhs;
  }
  template <class Container, class Callback>
  void each_until(const Container& container, typename Container::size_type begin, typename Container::size_type end, const Callback& fn)
  {
    using std::any_of;
    for(; begin != end; begin++) {
      if (fn(container[begin])) break;
    }
  }
  // use R and Args... for better autocompletion
  inline void emit(Args... args) {
    using std::for_each;
    using std::remove_if;
    using std::move;
    using std::make_move_iterator;
    using std::forward;
    using std::any_of;
    // scope guard
    emit_scope scope { *this };
    auto end = pending.size();
    if (end == 0) return;
    each_until(pending, 0, --end, [&] (const_slot_reference slot) {
      if (slot) (*slot)(args...);
      return m_disconnect_all;
    });
    if (!m_disconnect_all) {
      auto& slot = pending.back();
      if (slot) (*slot)(std::forward<Args&&>(args)...);
    }
    
    /*
    for (auto index = 0; index < pending_count; index++)  {
      bool is_last = &slots == &last_list;
      auto& slots = pending[index];
      auto& last_slot = slots.back();
      for (auto& slot : slots) {
        if (slot) {
          if (is_last && &last_slot == &slot) {
            (*slot)(forward<Args&&>(args)...);
          } else {
            (*slot)(args...);
          }
          if (m_disconnect_all) break;
        }
      }
      if (m_disconnect_all) break;
    }*/
  }
  
  inline connection connect(callback slot)
  {
    auto sid = prepare_connection();
    emplace(sid, std::move(slot));
    return { m_self, sid };
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
    return create_connection<extended_slot>(std::move(slot));
  }
  
  template <class TP, class Alloc>
  inline connection connect(std::shared_ptr<signal<R(Args...), TP, Alloc>> signal) {
    using signal_type = slimsig::signal<R(Args...), TP, Alloc>;
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
    return create_connection<signal_slot>(std::move(signal));
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
    return create_connection<fire_once>(std::move(slot));
  }
  

  void disconnect_all() {
    // cancel any iteration that's happening
    if (m_depth > 0) m_disconnect_all = true;
    pending.clear();
    m_size = 0;
  }
  const allocator_type& get_allocator() const {
    return allocator;
  }
  inline bool empty() const {
    return m_size == 0;
  }
  inline std::size_t slot_count() const {
    return m_size;
  }
  inline bool connected(slot_id index) {
    using std::find_if;
    using std::lower_bound;
    auto end = pending.cend();
    auto slot = lower_bound(pending.cbegin(), end, index, [] (const_slot_reference slot, const slot_id& index) {
      return slot < index;
    });
    if (slot != end && slot->m_slot_id == index) return slot->connected();
    return false;
  }
  
  inline void disconnect(slot_id index) {
    using std::find_if;
    using std::lower_bound;
    auto end = pending.end();
    auto slot = lower_bound(pending.begin(), end, index, [] (slot_reference slot, const slot_id& index) {
      return slot < index;
    });
    if (slot != end && slot->m_slot_id == index) {
      if (slot->connected()) {
       slot->disconnect();
       m_size -= 1;
      }
      
    }
  }
  ~signal_base() {
    if (m_self) m_self->signal = nullptr;
  }
  template <class FN, class TP, class Alloc>
  friend class signal;
  template <class F, class T, class A>
  friend void slimsig::swap(signal<F, T, A>&, signal<F, T, A>&);
private:
  struct emit_scope{
    signal_base& signal;
    emit_scope(signal_base& context) : signal(context) {
      signal.m_depth++;
    }
    ~emit_scope() {
      using std::move;
      using std::for_each;
      using std::remove_if;
      auto depth = --signal.m_depth;
      // if we completed iteration (depth = 0) collapse all the levels into the head list
      if (depth == 0) {
        auto m_size = signal.m_size;
        auto& pending = signal.pending;
        if (m_size != pending.size()) {
          pending.erase(remove_if(pending.begin(), pending.end(), &is_disconnected), pending.end());
        }
        signal.m_disconnect_all = false;
        assert(m_size == pending.size());
      }
      
    }
  };
  static bool is_disconnected(const_slot_reference slot) {
    return !bool(slot);
  }
  template<class C, class T>
  [[gnu::always_inline]]
  inline connection create_connection(T&& slot) {
    auto sid = prepare_connection();
    emplace(sid, C { std::move(slot), {m_self, sid} });
    return connection { m_self, sid };
  }
  
  [[gnu::always_inline]]
  inline slot_id prepare_connection() {
    // lazy initialize to put off heap allocations if the user
    // has not connected a slot
    if (!m_self) m_self = std::make_shared<signal_holder>(this);
    return ++last_id;
  }
  template <class... SlotArgs>
  [[gnu::always_inline]]
  inline void emplace(SlotArgs&&... args) {
    pending.emplace_back(std::forward<SlotArgs>(args)...);
    m_size++;
  }
  std::vector<slot> pending;
  std::shared_ptr<signal_holder> m_self;
  slot_id last_id;
  std::size_t m_size;
  unsigned m_depth;
  allocator_type allocator;
  bool m_disconnect_all;
};

  template <class Handler, class ThreadPolicy, class Allocator>
  void swap(signal<Handler, ThreadPolicy, Allocator>& lhs, signal<Handler, ThreadPolicy, Allocator>& rhs) {
    using std::swap;
    lhs.swap(rhs);
  }
}

#endif
