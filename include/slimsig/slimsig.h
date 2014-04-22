/**
 * Slimmer Signals
 * Copyright (c) 2014 Christopher Tarquini
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 *  THE SOFTWARE.
 */

/**
 * Slimmer Signals
 * Inspired by Boost::Signals2 and ssig
 * 
 * Main Differences between this library and all the other signal libraries:
 * - Light-weight: No unessarry virtual calls besides the ones inherit with std::function 
 * - Uses vectors as the underlying storage
 *    The theory is that you'll be emitting signals far more than you'll be adding slots 
 *    so using vectors will improve performance greatly by playing nice with the CPUs cache and taking
 *    advantage of SIMD
 * - Supports adding/removing slots while the signal is running (despite it being a vector)
 * - Removing slots should still be fast as it uses std::remove_if to iterate/execute slots
 *    Meaning that if there are disconnected slots they are removed quickly after iteration
 * - Custom allocators for more performance tweaking
 * 
 * While still being light-weight it still supports:
 * - connections/scoped_connections
 * - connection.disconnect()/connection.connected() can be called after the signal stops existing 
 *    There's a small overhead because of the use of a shared_ptr for each slot, however, my average use-case
 *    Involves adding 1-2 slots per signal so the overhead is neglible, especially if you use a custom allocator such as boost:pool
 *
 *  To keep it simple I left out support for slots that return values, though it shouldn't be too hard to implement.
 *  I've also left thread safety as something to be handled by higher level libraries
 *  Much in the spirit of other STL containers. My reasoning is that even with thread safety sort of baked in
 *  The user would still be responsible making sure slots don't do anything funny if they are executed on different threads
 *  All the mechanics for this would complicate the library and really not solve much of anything while also slowing down 
 *  the library for applications where signals/slots are only used from a single thread
 * 
 *  Plus it makes things like adding slots to multiple signals really slow because you have to lock each and every time
 *  You'd be better off using your own syncronization methods to setup all your singals at once and releasing your lock after
 *  you're done
 */
#ifndef slimsignals_h
#define slimsignals_h

#include <slimsig/detail/signal_base.h>

namespace slimsig {
  template <class Handler, class Allocator = std::allocator<std::function<Handler>>>
  class signal : private detail::signal_base<Allocator, Handler> {
    template <class F>
    struct function_traits;
    template<class R, class... Args>
    struct function_traits<R(*)(Args...)> : function_traits<R(Args...)> {};
    template<class R, class...Args>
    struct function_traits<R(Args...)> {
      using return_type = R;
      template <class T>
      struct as_member_of {
        using type = return_type (T::*)(Args...);
      };
      template <class T>
      using as_member_of_t = typename as_member_of<T>::type;
    };

    
  public:
    using base = detail::signal_base<Allocator, Handler>;
    using typename base::return_type;
    using typename base::function_type;
    using typename base::allocator_type;
    using typename base::slot_type;
    using typename base::slot_list;
    using typename base::list_allocator_type;
    using typename base::const_slot_reference;
    using typename base::connection_type;
    
    // allocator constructor
    using base::base;
    
    // default constructor
    signal() : signal(allocator_type()) {};

    using base::emit;
    using base::connect;
    using base::connect_once;
    using base::disconnect_all;
    using base::slot_count;
    using base::get_allocator;
    using base::compact;
    using base::empty;
    /*
    struct tracked_slot {
      using result_type = typename function_type::result_type;
      std::vector<std::weak_ptr<void>> tracked;
      template<class R>
      inline auto default_value() -> typename std::enable_if<!std::is_void<R>::value, R>::type {
        return R{};
      }
      template<class R>
      inline auto default_value() -> typename std::enable_if<std::is_void<R>::value, R>::type {
        return;
      }
      
      tracked_slot(std::vector<std::weak_ptr<void>> objs,
                   function_type fn, function_type& ctx = dummy()) : tracked(std::move(objs)), fn(std::move(fn)), context(ctx) {
        locked.reserve(tracked.size());
      };
      tracked_slot(const tracked_slot&) = default;
      tracked_slot(tracked_slot&&) = default;
      tracked_slot& operator=(const tracked_slot&) = default;
      tracked_slot& operator=(tracked_slot&&) = default;
      template <class... BindArguments>
      result_type operator()(BindArguments&&... args) {
        if (context) {
          for (auto i = tracked.begin(), end = tracked.end(); i != end; i++) {
            auto& obj = *i;
            if (!obj.expired()) {
              locked.emplace_back(obj);
            } else {
              context = nullptr;
              break;
            }
          }
          if (context) {
            return fn(std::forward<BindArguments>(args)...);
          }
        } else {
          return default_value<result_type>();
        }
        
      }
      std::vector<std::shared_ptr<void>> locked;
      function_type& context;
      function_type fn;
      static function_type& dummy() {
        static function_type empty{nullptr};
        return empty;
      }
    };
    
    connection_type connect(const function_type& slot, std::vector<std::weak_ptr<void>> tracked_objects) {
      auto& container = !this->is_running ? this->slots : this->pending_slots;
      container.emplace_back(std::allocate_shared<function_type>(allocator, std::allocator_arg, allocator,
                                                                 tracked_slot {tracked_objects, slot}));
      auto& binding = *container.back();
      binding.template target<tracked_slot>()->context = binding;
      return connection_type { std::weak_ptr<function_type> { container.back() } };
    }*/


  };
  template <class Handler, class Allocator = std::allocator<std::function<Handler>>>
  using signal_t = signal<Handler, Allocator>;
}
#endif
