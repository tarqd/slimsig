#include <iostream>
#include <array>
#include <bandit/bandit.h>
#include <slimsig/signal.h>
#include <slimsig/signals_from_this.h>
#include <slimsig/detail/slot_id.h>
using namespace bandit;
namespace ss = slimsig;
using ss::signal_t;
using connection = typename signal_t<void()>::connection;
bool function_slot_triggered = false;
bool static_slot_triggered = false;
bool functor_slot_triggered = false;
void function_slot()
{
    function_slot_triggered = true;
}
struct class_test {
    static void static_slot() { static_slot_triggered = true; }
    bool bound_slot_triggered = false;
    void bound_slot() { bound_slot_triggered = true; }
    void operator()() { functor_slot_triggered = true; }
};

struct copy_test {
    copy_test(unsigned& copies, unsigned& moves)
        : copy_count(copies)
        , move_count(moves)
    {
        copies = 0;
        moves = 0;
    };
    copy_test(const copy_test& other)
        : copy_count(other.copy_count)
        , move_count(other.move_count)
    {
        ++copy_count;
    };
    copy_test(copy_test&& other)
        : copy_count(other.copy_count)
        , move_count(other.move_count)
    {
        ++move_count;
    };
    copy_test& operator=(const copy_test& other)
    {
        copy_count = other.copy_count;
        move_count = other.move_count;
        ++copy_count;
        return *this;
    };
    copy_test& operator=(copy_test&& other)
    {
        copy_count = other.copy_count;
        move_count = other.move_count;
        ++move_count;
        return *this;
    };

private:
    unsigned& copy_count;
    unsigned& move_count;
};

go_bandit([] {
  describe("signal", [] {
    signal_t<void()> signal;
    before_each([&] {
      signal = ss::signal_t<void()>{};
    });
    it("should trigger basic function slots", [&] {
      signal.connect(&function_slot);
      signal.emit();
      AssertThat(function_slot_triggered, Equals(true));
    });

    it("should trigger static method slots", [&] {
      signal.connect(&class_test::static_slot);
      signal.emit();
      AssertThat(static_slot_triggered, Equals(true));
    });
    it("should trigger bound member function slots", [&] {
      class_test obj;
      signal.connect(std::bind(&class_test::bound_slot, &obj));
      signal.emit();
      AssertThat(obj.bound_slot_triggered, Equals(true));
    });
    it("should trigger functor slots", [&] {
      class_test obj;
      signal.connect(obj);
      signal.emit();
      AssertThat(functor_slot_triggered, Equals(true));
    });
    it("should trigger lambda slots", [&] {
      bool fired = false;
      signal.connect([&] { fired = true; });
      signal.emit();
      AssertThat(fired, Equals(true));
    });
    describe("#emit()", [&] {
      it("should should not perfectly forward r-value references", [&] {
        ss::signal_t<void(std::string)> signal;
        std::string str("hello world");
        signal.connect([](std::string str) {
          AssertThat(str, Equals(std::string("hello world")));
        });
        signal.connect([](std::string str) {
          AssertThat(str, Equals(std::string("hello world")));
        });
        signal.emit(std::move(str));
      });
      it("should not copy references", [&] {
        ss::signal_t<void(std::string&)> signal;
        std::string str("hello world");
        signal.connect([](std::string& str) {
          AssertThat(str, Equals(std::string("hello world")));
          str = "hola mundo";
        });
        signal.connect([](std::string& str) {
          AssertThat(str, Equals(std::string("hola mundo")));
        });
        signal.emit(str);
      });
      it("should be re-entrant", [&] {  // see test.js
        unsigned count{0};
        signal.connect([&] {
          ++count;
          if (count == 1) {
            signal.connect_once([&] { ++count; });
            signal.emit();
          };
        });
        signal.emit();
        AssertThat(count, Equals(3u))
      });
      it("should not aknowledge changes to the slot list created after emit is "
         "called",
         [&] {
           connection conn1, conn2, conn3;
           unsigned count1{0}, count2{0}, count3{0};
           /*
             First: fires 1,2,3;
             Second: fires 1,2
             Third: fires 1
           */
           conn1 = signal.connect([&] {
             ++count1;
             if (count1 == 1) {
               signal.disconnect(conn3);
             };
             if (count1 == 2) {
               signal.disconnect(conn2);
             };
             if (count1 <= 2) {
               signal.emit();
             };
           });
           // should twice (first/second iteration)
           conn2 = signal.connect([&] {
             ++count2;
             if (count2 == 1) {
             };
           });
           conn3 = signal.connect([&] { ++count3; });
           signal.emit();
           AssertThat(signal.connected(conn1), Equals(true));
           AssertThat(signal.connected(conn2), Equals(false));
           AssertThat(signal.connected(conn3), Equals(false));
           AssertThat(count1, Equals(3u));
           AssertThat(count2, Equals(2u));
           AssertThat(count3, Equals(1u));
         });
      it("should optimize last calls", [] {
        ss::signal_t<void(copy_test)> signal;
        unsigned copies1 = 0u, copies2 = 0u, moves1 = 0u, moves2 = 0u;
        copy_test test1{copies1, moves1}, test2{copies2, moves2};
        signal.connect([](copy_test test) {});
        signal.emit(test1);
        signal.connect([](copy_test test) {});
        signal.emit(test2);
        // minus the first copy from emit() since that only happens once no
        // matter how many
        // slots we have connected, we are counting how many copies happen due
        // to extra slots
        AssertThat((copies1 - 1u) * 2u, IsLessThan(copies2));
        AssertThat(moves1 * 2u, IsGreaterThan(moves2));
      });

    });
    describe("#slot_count()", [&] {
      it("should return the slot count", [&] {
        signal.connect([] {});
        AssertThat(signal.slot_count(), Equals(1u));
      });
      it("should return the correct count when adding slots during iteration",
         [&] {
           signal.connect([&] {
             signal.connect([] {});
             AssertThat(signal.slot_count(), Equals(2u));
           });
           signal.emit();
           AssertThat(signal.slot_count(), Equals(2u));
         });
    });
    describe("#connect_once()", [&] {
      it("it should fire once", [&] {
        unsigned count = 0;
        signal.connect_once([&] { count++; });
        signal.emit();
        AssertThat(count, Equals(1u));
        signal.emit();
        AssertThat(count, Equals(1u));

      });
    });
    describe("#connect(signal)", [&] {
      it("should emit with the signal until shared_ptr falls out of scope",
         [&] {
           unsigned count = 0;
           connection con;
           {
             auto target = std::make_shared<signal_t<void()> >();
             con = target->connect([&] { ++count; });
             signal.connect(target);
             signal.emit();
           }
           signal.emit();
           AssertThat(count, Equals(1u));
           AssertThat(signal.empty(), IsTrue());
           AssertThat(signal.connected(con), IsFalse());
         });
    });
    describe("#disconnect_all()", [&] {
      it("should remove all slots", [&] {
        auto conn1 = signal.connect([] {});
        auto conn2 = signal.connect([] {});
        signal.disconnect_all();
        AssertThat(signal.slot_count(), Equals(0u));
        AssertThat(signal.connected(conn1), Equals(false));
        AssertThat(signal.connected(conn2), Equals(false));
        AssertThat(signal.empty(), Equals(true));
      });
      it("should remove all slots while iterating", [&] {
        // should still fire each slot once
        // matches node.js event emitter behavior
        std::pair<unsigned, connection> res1, res2;
        res1.second = signal.connect([&] {
          res1.first++;
          signal.disconnect_all();
          AssertThat(signal.connected(res1.second), Equals(false));
          AssertThat(signal.connected(res2.second), Equals(false));
        });

        res2.second = signal.connect([&] { res2.first++; });
        signal.emit();

        AssertThat(signal.slot_count(), Equals(0u));
        AssertThat(signal.connected(res1.second), Equals(false));
        AssertThat(signal.connected(res2.second), Equals(false));
        AssertThat(res1.first, Equals(1u));
        AssertThat(res2.first, Equals(1u));

      });
      it("should remove all slots while iterating, without removing new slots",
         [&] {
           std::pair<unsigned, connection> res1, res2, res3;
           res1.second = signal.connect([&] {
             res1.first++;
             signal.disconnect_all();
             res3.second = signal.connect([&] { res3.first++; });
           });

           res2.second = signal.connect([&] { res2.first++; });
           signal.emit();
           signal.emit();
           AssertThat(signal.slot_count(), Equals(1u));
           AssertThat(res1.first, Equals(1u));
           AssertThat(res2.first, Equals(1u));
           AssertThat(res2.first, Equals(1u));
         });

      it("should support disconnect_all while iterating, followed by "
         "connect/emit",
         [&] {
           std::pair<unsigned, connection> res1, res2;
           res1.second = signal.connect([&] {
             res1.first++;
             signal.disconnect_all();
             res2.second = signal.connect([&] { res2.first++; });
             signal.emit();
           });
           signal.emit();
           AssertThat(res1.first, Equals(1u));
           AssertThat(res2.first, Equals(1u));
           AssertThat(signal.connected(res1.second), Equals(false));
           AssertThat(signal.connected(res2.second), Equals(true));
           AssertThat(signal.slot_count(), Equals(1u));
         });
    });

  });
  /*
  describe("tracking", [] {
    ss::signal<void()> signal;
    before_each([&] { signal = ss::signal<void()>{}; });
    it("should disconnect slots when tracked objects are destroyed", [&] {
      struct foo{};
      bool called = false;
      auto tracked = std::make_shared<foo>();
      signal.connect([&] {
        called = true;
      }, {tracked});
      tracked.reset();
      signal.emit();
      AssertThat(called, Equals(false));
      signal.compact();
      AssertThat(signal.slot_count(), Equals(0));
    });
  });*/
  describe("connection", [] {
    ss::signal_t<void()> signal;
    before_each([&] { signal = ss::signal_t<void()>{}; });
    describe("#connected()", [&] {
      it("should return whether or not the slot is connected", [&] {
        auto connection = signal.connect([] {});
        AssertThat(signal.connected(connection), Equals(true));
        signal.disconnect_all();
        AssertThat(signal.connected(connection), Equals(false));
      });
    });
    describe("#disconnect", [&] {
      it("should disconnect the slot", [&] {
        bool fired = false;
        auto connection = signal.connect([&] { fired = true; });
        signal.disconnect(connection);
        signal.emit();
        AssertThat(fired, Equals(false));
        AssertThat(signal.connected(connection), Equals(false));
        AssertThat(signal.slot_count(), Equals(0u));
      });
      it("should not throw if already disconnected", [&] {
        auto connection = signal.connect([] {});
        signal.disconnect(connection);
        signal.connected(connection);
        AssertThat(signal.connected(connection), Equals(false));
        AssertThat(signal.slot_count(), Equals(0u));
      });
    });
    it("should be consistent across copies", [&] {
      auto conn1 = signal.connect([] {});
      auto conn2 = conn1;
      signal.disconnect(conn1);
      AssertThat(signal.connected(conn1), Equals(signal.connected(conn2)));
      AssertThat(signal.slot_count(), Equals(0u));
    });
    it("should not affect slot lifetime", [&] {
      bool fired = false;
      auto fn = [&] { fired = true; };
      { auto connection = signal.connect(fn); }
      signal.emit();
      AssertThat(fired, Equals(true));
    });
    it("should still be valid if the signal is destroyed", [&] {
      using connection_type = ss::signal_t<void()>::connection;
      connection_type connection;
      {
        ss::signal_t<void()> scoped_signal{};
        connection = scoped_signal.connect([] {});
      }
      AssertThat(signal.connected(connection), Equals(false));
    });

  });
  describe("scoped_connection", [] {
    ss::signal_t<void()> signal;
    before_each([&] { signal = ss::signal_t<void()>(); });
    it("should disconnect the connection after leaving the scope", [&] {
      bool fired = false;
      {
        auto scoped = make_scoped_connection(signal, signal.connect([&] { fired = true; }));
      }
      signal.emit();
      AssertThat(fired, Equals(false));
      AssertThat(signal.empty(), Equals(true));
    });
    it("should update state of underlying connection", [&] {
      auto connection = signal.connect([] {});
      { auto scoped = make_scoped_connection(signal, connection); }
      signal.emit();
      AssertThat(signal.connected(connection), Equals(false));
    });
  });
});
int main(int argc, char* argv[])
{
    bandit::run(argc, argv);
}
