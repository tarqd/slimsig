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
namespace slimsig { namespace detail {

template <class Allocator, class F>
class signal_base;

template<class Allocator, class R, class... Args>
class signal_base<Allocator, R(Args...)>
{

public:
  using return_type = R;
  using function_type = std::function<R(Args...)>;
  using allocator_type = Allocator;
  using slot_type = slot<R(Args...)>;
  using slot_list = std::vector<slot_type, typename std::allocator_traits<allocator_type>::template rebind_traits<slot_type>::allocator_type>;
  using slot_storage_type = slot_store<slot_list>;
  using list_allocator_type = typename slot_list::allocator_type;
  using const_slot_reference = typename std::add_const<typename std::add_lvalue_reference<slot_type>::type>::type;
  using slot_iterator = typename slot_list::iterator;
  using connection_type = connection<signal_base>;
  using extended_function = std::function<R(connection_type& conn, Args...)>;

private:
  unsigned long long m_last_id = 0;
  inline unsigned long long next_id() {
    return m_last_id++;
  };
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
  m_slots(std::allocate_shared<slot_storage_type>(alloc, list_allocator_type(alloc))),
  is_running(false) {};
  
  signal_base(size_t capacity, const allocator_type& alloc = allocator_type{})
  : signal_base(alloc) {
    m_slots->active.reserve(capacity);
  }
  
  // use R and Args&&.. for better autocompletion
  inline R emit(Args&&... args) {
    // runs the slot if connected, otherwise return true and queue it for deletion
    auto is_disconnected = [&] (const_slot_reference slot)
    {
      if (slot) { slot(std::forward<Args>(args)...); return false;}
      else return true;
    };
    auto& slots = *m_slots;
    
    auto begin = slots.active.begin();
    auto end = slots.active.end();
    // sane implementations only move elements if the predicate returns true
    // as if calling find_if to find the first matching slot, then shifting the rest of the values
    // so the matched elements end up at the end of the array
    begin = std::remove_if(begin, end, is_disconnected);
    if (begin != end) slots.active.erase(begin, end);
    if (slots.pending.size() > 0) {
      slots.active.insert(slots.active.cend(),
                   std::make_move_iterator(slots.pending.begin()),
                   std::make_move_iterator(slots.pending.end()));
      slots.pending.clear();
    }
    is_running = false;
  }
  
  inline connection_type connect(function_type slot)
  {
    auto& container = !is_running ? m_slots->active : m_slots->pending;
    container.emplace_back(std::move(slot), next_id());
    return connection_type { m_slots, container.back() };
  };
  
  
  connection_type connect_once(function_type slot)  {
    struct fire_once {
      fire_once (function_type func) : fn(std::move(func)) {}
      function_type fn;
      connection_type connection;
      R operator() (Args&&... args) {
        fn(std::forward<Args>(args)...);
        connection.disconnect();
      }
    };
    auto& container = queue();
    container.emplace_back(fire_once{std::move(slot)}, next_id());
    auto& slot_handler = container.back();
    connection_type connection { m_slots, slot_handler };
    slot_handler.template slot_target<fire_once>()->connection = connection;
    return connection;
  }
  
  void disconnect_all() {
    auto& slots = *m_slots;
    slots.active.clear();
    slots.pending.clear();
  }
  const allocator_type& get_allocator() const {
    return allocator;
  }
  inline bool empty() const {
    auto& slots = *m_slots;
    return slots.active.size() == 0 && slots.pending.size() == 0;
  }
  inline std::size_t slot_count() const {
    auto& slots = *m_slots;
    return slots.active.size() + slots.pending.size();
  }
  inline void compact() {
    auto& slots = *m_slots;
    auto& active = slots.active;
    auto& pending = slots.pending;
    auto begin = active.begin();
    auto end = active.end();
    begin = std::remove_if(begin, end, &is_disconnected);
    slots.erase(begin, end);
    begin = pending.begin();
    end = pending.end();
    begin = std::remove_if(begin, end, &is_disconnected);
    pending.erase(begin, end);
  }
protected:
  allocator_type allocator;
  inline slot_list& queue() {
    return !is_running ? m_slots->active : m_slots->pending;
  }
  std::shared_ptr<slot_storage_type> m_slots;
  bool is_running;
private:
  static inline bool is_disconnected(const_slot_reference slot) {
    return slot && bool(*slot);
  }
};
}}

#endif
