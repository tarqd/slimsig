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
namespace slimsig {
  namespace detail {
  template <class Allocator, class F>
  class signal_base;
  }
  template <class Handler>
  class connection {
  public:
    using function_type = Handler;
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
    
    template <class Allocator, class F>
    friend class detail::signal_base;
    
  private:
    inline std::shared_ptr<function_type> slot() {
      return m_slot.lock();
    }
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
 
}

#endif
