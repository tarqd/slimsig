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
#include <functional>
#include <iterator>
#include <atomic>
#include <memory>
#include <algorithm>
  
namespace slimsig {
  template <class Handler, class Allocator>
  class signal;
  
  template <class Signal>
  class connection {
  public:
    using signal_type = Signal;
    using slot_type = typename signal_type::slot_type;
    using function_type = typename signal_type::function_type;
    connection() {}; // empty connection
    connection(const connection& other) : m_slot(other.m_slot) {};
    connection(connection&& other) : m_slot(std::move(other.m_slot)) {};
    connection(const std::weak_ptr<function_type>& slot) : m_slot(slot) {};
    
    connection& operator=(connection&& rhs) {
      this->swap(rhs);
      return *this;
    }
    connection& operator=(const connection& rhs) {
      m_slot = rhs.m_slot;
      return *this;
    }
    
    void swap(connection& other) {
      using std::swap;
      swap(m_slot, other.m_slot);
    }
    
    [[gnu::always_inline]]
    inline bool connected() {
      auto slot = m_slot.lock();
      return slot && bool(*slot);
    }
    [[gnu::always_inline]]
    inline void disconnect() {
      auto slot = m_slot.lock();
      if (slot) {
        *slot = nullptr;
        m_slot.reset();
      }
    }
    
    
  private:
    std::weak_ptr<function_type> m_slot;
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
  
  template <class Handler, class Allocator = std::allocator<std::function<Handler>>>
  class signal {
  public:
    using function_type = std::function<Handler>;
    using allocator_type = Allocator;
    using slot_type = std::shared_ptr<function_type>;
    using slot_list = std::vector<slot_type, typename std::allocator_traits<allocator_type>::template rebind_traits<slot_type>::allocator_type>;
    using list_allocator_type = typename slot_list::allocator_type;
    using const_slot_reference = typename std::add_const<typename std::add_lvalue_reference<slot_type>::type>::type;
    using slot_iterator = typename slot_list::iterator;
    using connection_type = connection<signal>;
    
    // allocator constructor
    signal(const allocator_type& alloc)
      : is_running(false),
      slots { list_allocator_type(alloc) },
      pending_slots { list_allocator_type(alloc) } {};
    
    // default constructor
    signal() : signal(allocator_type()) {};
    template <class... Arguments>
    void emit(Arguments&&... args)
    {
      is_running = true;
      // runs the slot if connected, otherwise return true and queue it for deletion
      auto is_disconnected = [&] (const_slot_reference slot)
      {
        auto& fn = *slot;
        if (fn) { fn(std::forward<Arguments>(args)...); return false;}
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
        slots.insert(slots.cend(), std::make_move_iterator(pending_slots.begin()), std::make_move_iterator(pending_slots.end()));
        pending_slots.clear();
      }
      is_running = false;
    }
    connection_type connect(const function_type& slot) {
      auto& container = !is_running ? slots : pending_slots;
      container.emplace_back(std::allocate_shared<function_type>(allocator, std::allocator_arg, allocator, slot));
      return connection_type { std::weak_ptr<function_type>( container.back()) };
    }
    void disconnect_all() {
      slots.clear();
      pending_slots.clear();
    }
    const allocator_type& get_allocator() const {
      return allocator;
    }
    inline bool empty() const {
      return slots.size() + pending_slots.size() == 0;
    }
    inline std::size_t slot_count() const {
      return slots.size() + pending_slots.size();
    }
  private:
    allocator_type allocator;
    bool is_running = false;
    slot_list slots;
    slot_list pending_slots;
  };
}
#endif
