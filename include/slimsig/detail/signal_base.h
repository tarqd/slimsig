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
  using slot_type = std::shared_ptr<function_type>;
  using slot_list = std::vector<slot_type, typename std::allocator_traits<allocator_type>::template rebind_traits<slot_type>::allocator_type>;
  using list_allocator_type = typename slot_list::allocator_type;
  using const_slot_reference = typename std::add_const<typename std::add_lvalue_reference<slot_type>::type>::type;
  using slot_iterator = typename slot_list::iterator;
  using connection_type = connection<function_type>;
  using extended_function = std::function<R(connection_type& conn, Args...)>;
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
  slots { list_allocator_type{allocator} },
  pending_slots{list_allocator_type{allocator}},
  is_running(false) {};
  
  // use R and Args... for better autocompletion
  R emit(Args... args) {
    is_running = true;
    // runs the slot if connected, otherwise return true and queue it for deletion
    auto is_disconnected = [&] (const_slot_reference slot)
    {
      auto& fn = *slot;
      // we don't use std::forward because that will cause problems with r-value references
      // for example if the signature is void(std::string) and the user emits
      // with an r-value reference, only the first slot will be able to access it
      // This results in extra copying but you can avoid that by accepting values by l-value references
      // instead of by value
      if (fn) {
        if (slot != slots.back()) {
          fn(args...);
        } else {
          // move arguments for last slot
          fn(std::forward<Args>(args)...);
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
    if (begin != end) slots.erase(begin, end);
    if (pending_slots.size() > 0) {
      slots.insert(slots.cend(),
                   std::make_move_iterator(pending_slots.begin()),
                   std::make_move_iterator(pending_slots.end()));
      pending_slots.clear();
    }
    is_running = false;
  }
  
  connection_type connect(std::function<R(Args...)> slot)
  {
    auto& container = !is_running ? slots : pending_slots;
    container.emplace_back(std::allocate_shared<function_type>(allocator, std::allocator_arg, allocator, std::move(slot)));
    return connection_type { std::weak_ptr<function_type>( container.back()) };
  }
  
  connection_type connect_extended(extended_function slot){
    struct extended_slot {
      extended_slot (function_type func) : fn(std::move(func)) {}
      function_type fn;
      connection_type connection;
      R operator() (Args&&... args) {
        fn(connection, std::forward<Args>(args)...);
      }
    };
    auto connection = connect(extended_slot{std::move(slot)});
    auto slot_ptr = connection.slot();
    (*slot_ptr).template target<extended_slot>()->connection = connection;
    return connection;
  }
  
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
    auto connection = connect(fire_once {slot});
    auto slot_ptr = connection.slot();
    (*slot_ptr).template target<fire_once>()->connection = connection;
    return connection;
  }
  void disconnect_all() {
    slots.clear();
    pending_slots.clear();
  }
  const allocator_type& get_allocator() const {
    return allocator;
  }
  inline bool empty() const {
    return this->slots.size() + this->pending_slots.size() == 0;
  }
  inline std::size_t slot_count() const {
    return this->slots.size() + pending_slots.size();
  }
  inline void compact() {
    auto begin = std::remove_if(slots.begin(), slots.end(), &is_disconnected);
    slots.erase(begin, slots.end());
    begin = std::remove_if(pending_slots.begin(), pending_slots.end(), &is_disconnected);
    slots.erase(begin, pending_slots.end());
  }
protected:
  allocator_type allocator;
  slot_list slots;
  slot_list pending_slots;
  bool is_running;
private:
  static inline bool is_disconnected(const_slot_reference slot) {
    return slot && bool(*slot);
  }
};
}}

#endif
