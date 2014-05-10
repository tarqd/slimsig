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
  enum SignalState {
    Default = 0,
    Dirty = 1,
    DisconnectAll = 1 << 1
  };
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
    allocator(alloc),
    m_self(std::make_shared<signal_holder>(this)),
    m_depth(0),
    m_last_depth(0),
    pending(1),
    m_size(0),
    last_id(),
    m_dirty(false),
    m_disconnect_all(false) {
      assert(m_self->signal == this);
           //m_self = std::make_shared<signal_base**>(this);
    };
  
  signal_base(size_t capacity, const allocator_type& alloc = allocator_type{})
  : signal_base(alloc) {
    //m_slots->active.reserve(capacity);
  }
  signal_base(const signal_base&) = delete;
  signal_base(signal_base&& other) {
    using std::swap;
    swap(pending, other.pending);
    swap(m_size, other.m_size);
    swap(m_depth, other.m_depth);
    swap(m_dirty, other.m_dirty);
    swap(m_disconnect_all, other.m_disconnect_all);
    swap(last_id, other.last_id);
    swap(allocator, other.allocator);
    swap(m_self, other.m_self);
    m_self->signal = this;
    other.m_self->signal = &other;
  }
  signal_base& operator=(signal_base&& other) {
    using std::swap;
    swap(pending, other.pending);
    swap(m_size, other.m_size);
    swap(m_depth, other.m_depth);
    swap(m_dirty, other.m_dirty);
    swap(m_disconnect_all, other.m_disconnect_all);
    swap(last_id, other.last_id);
    swap(allocator, other.allocator);
    swap(m_self, other.m_self);
    m_self->signal = this;
    other.m_self->signal = &other;
    return *this;
  }
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
        signal.m_last_depth = depth > 0 ? depth - 1 : 0;
        // if we completed iteration (depth = 0) collapse all the levels into the head list
        if (depth == 0) {
          auto& pending = signal.pending;
          auto head = pending.begin();
          // optimization: if the head is dirty remove its disconnected slots first
          if (signal.m_dirty) {
            head->erase(remove_if(head->begin(), head->end(), &is_disconnected), head->end());
          }
          // optimization: since we know the total connected slots, reserve memory for them ahead of time
          head->reserve(signal.m_size);
          // there is always at least 1 list so begin() + 1 is okay
          for_each(pending.begin()+1, pending.end(), [&] (slot_list_reference slots) {
            for (auto& slot : slots) {
                if (slot) head->emplace_back(move(slot));
            }
          });
          // clear out pending lists
          pending.resize(1);
        }
        
      }
    };
  // use R and Args... for better autocompletion
  inline void emit(Args... args) {
    using std::for_each;
    using std::remove_if;
    using std::move;
    using std::make_move_iterator;
    using std::forward;
    // scope guard
    emit_scope scope { *this };
    
    auto& last_list = pending.back();
    for (auto& slots : pending) {
      bool is_last = &slots == &last_list;
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
    }
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
    pending.resize(1);
    pending.back().clear();
    m_size = 0;
    m_dirty = false;
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
    for (auto& queue : pending) {
      if (queue.size() > 0 && queue.front() <= index) {
        for (auto& slot : queue) {
          if (slot.m_slot_id == index) {
            return slot.connected();
          }
        }
        break;
      }
    }
    return false;
  }
  
  inline void disconnect(slot_id index) {
    for (auto& queue : pending) {
      if (queue.size() > 0 && queue.front() <= index && queue.back() >= index) {
        for (auto& slot : queue) {
          if (slot.m_slot_id == index) {
            slot.disconnect();
            m_size--;
            // special case if we are removing from the first list
            if (&queue == &pending.front()) {
              m_dirty = true;
            }
            return;
          }
        }
        break;
      }
    }
  }
  ~signal_base() {
    m_self->signal = nullptr;
  }
  template <class FN, class TP, class Alloc>
  friend class signal;
private:
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
    if (m_depth > 0 && m_last_depth < m_depth) {
     pending.emplace_back();
     m_last_depth = m_depth;
    }
    return ++last_id;
  }
  template <class... SlotArgs>
  [[gnu::always_inline]]
  inline void emplace(SlotArgs&&... args) {
    pending.back().emplace_back(std::forward<SlotArgs>(args)...);
    m_size++;
  }
  allocator_type allocator;
  std::vector<std::vector<slot>> pending;
  std::shared_ptr<signal_holder> m_self;
  slot_id last_id;
  std::size_t m_size;
  unsigned m_depth;
  unsigned m_last_depth;
  bool m_dirty;
  bool m_disconnect_all;
};
}

#endif
