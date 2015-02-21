//
//  connection.h
//  slimsig
//
//  Created by Christopher Tarquini on 4/21/14.
//
//

#ifndef slimsig_connection_h
#define slimsig_connection_h

#include <slimsig/detail/slot.h>
#include <functional>
#include <algorithm>
#include <limits>

namespace slimsig {
template <class Signal>
class connection;

template <class ThreadPolicy, class Allocator, class F>
class basic_signal;
// detail

template <class Signal>
class connection {
    using slot = typename Signal::slot;
    using slot_type = slot;
    using slot_list_iterator = typename Signal::slot_list::iterator;
    using slot_storage = typename Signal::slot_list;
    using slot_id = typename slot::slot_id;
    connection(const slot_type& slot)
        : connection(slot.m_slot_id){};
    connection(slot_id slot_id)
        : m_slot_id(slot_id){};

public:
    connection(){}; // empty connection
    connection(const connection& other)
        : m_slot_id(other.m_slot_id){};
    connection(connection&& other)
        : m_slot_id(other.m_slot_id){};

    connection& operator=(connection&& rhs)
    {
        this->swap(rhs);
        return *this;
    }
    connection& operator=(const connection& rhs)
    {
        m_slot_id = rhs.m_slot_id;
        return *this;
    }

    void swap(connection& other)
    {
        using std::swap;
        swap(m_slot_id, other.m_slot_id);
    }

    template <class ThreadPolicy, class Allocator, class F>
    friend class basic_signal;
    template <class T, class IDGenerator, class FlagType, class Allocator>
    friend class slot_list;

private:
    slot_id m_slot_id;
};

template <class Signal>
class scoped_connection : public Signal::connection {
public:
    using signal = Signal;
    using connection = typename signal::connection;
    scoped_connection() = default;
    scoped_connection(signal& signal, const connection& target)
        : connection(target)
        , m_signal(&signal){};
    scoped_connection(const scoped_connection&) = delete;
    scoped_connection(scoped_connection&& other)
        : connection(other)
        , m_signal(other.m_signal){};

    scoped_connection& operator=(const connection& rhs)
    {
        if (m_signal)
            m_signal->disconnect(*this);
        connection::operator=(rhs);
        return *this;
    };
    scoped_connection& operator=(scoped_connection&& rhs)
    {
        this->swap(rhs);
        return *this;
    };
    void swap(scoped_connection& other)
    {
        this->swap(other);
    };
    connection release()
    {
        connection ret{};
        this->swap(ret);
        return ret;
    }
    ~scoped_connection()
    {
        if (m_signal)
            m_signal->disconnect(*this);
    }

private:
    signal* m_signal;
};

template <class Signal>
scoped_connection<Signal> make_scoped_connection(Signal& signal, typename Signal::connection& target)
{
    return scoped_connection<Signal>(signal, target);
};
template <class Signal>
scoped_connection<Signal> make_scoped_connection(Signal& signal, typename Signal::connection&& target)
{
    return scoped_connection<Signal>(signal, target);
};
}

#endif
