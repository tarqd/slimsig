#include <iostream>
#include <array>
#include <bandit/bandit.h>
#include <slimsig/slimsig.h>

using namespace bandit;
namespace ss = slimsig;
using ss::signal_t;
using connection = typename signal_t<void()>::connection;
bool function_slot_triggered = false;
bool static_slot_triggered = false;
bool functor_slot_triggered = false;
void function_slot() { function_slot_triggered = true; }
struct class_test {
  static void static_slot() { static_slot_triggered = true; }
  bool bound_slot_triggered = false;
  void bound_slot() { bound_slot_triggered = true; }
  void operator() () { functor_slot_triggered = true; }
};

go_bandit([]
{
  describe("signal", []
  {
    signal_t<void()> signal;
    before_each([&]
    {
      signal = ss::signal<void()>{};
    });
    
    it("should trigger basic function slots", [&]
    {
      signal.connect(&function_slot);
      signal.emit();
      AssertThat(function_slot_triggered, Equals(true));
    });
    
    it("should trigger static method slots", [&]
    {
      signal.connect(&class_test::static_slot);
      signal.emit();
      AssertThat(static_slot_triggered, Equals(true));
    });
    it("should trigger bound member function slots", [&]
    {
      class_test obj;
      signal.connect(std::bind(&class_test::bound_slot, &obj));
      signal.emit();
      AssertThat(obj.bound_slot_triggered, Equals(true));
    });
    it("should trigger functor slots", [&]
    {
      class_test obj;
      signal.connect(obj);
      signal.emit();
      AssertThat(functor_slot_triggered, Equals(true));
    });
    it("should trigger lambda slots", [&] {
      bool fired = false;
      signal.connect([&] { fired = true;});
      signal.emit();
      AssertThat(fired, Equals(true));
    });
    describe("#emit()", [&] {
      it("should should not perfectly forward r-value references", [&] {
        ss::signal<void(std::string)> signal;
        std::string str("hello world");
        signal.connect([] (std::string str) {
          AssertThat(str, Equals(std::string("hello world")));
        });
        signal.connect([] (std::string str){
          AssertThat(str, Equals(std::string("hello world")));
        });
        signal.emit(std::move(str));
      });
      it("should not copy references", [&] {
        ss::signal<void(std::string&)> signal;
        std::string str("hello world");
        signal.connect([] (std::string& str) {
          AssertThat(str, Equals(std::string("hello world")));
          str = "hola mundo";
        });
        signal.connect([] (std::string& str){
          AssertThat(str, Equals(std::string("hola mundo")));
        });
        signal.emit(str);
      });
      it("should be re-entrant", [&] { // see test.js
        unsigned count{0};
        signal.connect([&]{
          ++count;
          if (count == 1) {
            signal.connect_once([&]{
              ++count;
            });
            signal.emit();
          };
        });
        signal.emit();
        AssertThat(count, Equals(3u))
      });
      
    });
    describe("#slot_count()", [&] {
      it("should return the slot count", [&]
      {
        signal.connect([]{});
        AssertThat(signal.slot_count(), Equals(1u));
      });
      it("should return the correct count when adding slots during iteration", [&]
      {
        signal.connect([&] {
          signal.connect([]{});
          AssertThat(signal.slot_count(), Equals(2u));
        });
        signal.emit();
        AssertThat(signal.slot_count(), Equals(2u));
      });
    });
    describe("#connect_once()", [&]{
      it("it should fire once", [&] {
        unsigned count = 0;
        signal.connect_once([&] {
          count++;
        });
        signal.emit();
        AssertThat(count, Equals(1u));
        signal.emit();
        AssertThat(count, Equals(1u));
        
      });
    });
    describe("#disconnect_all()", [&]
    {
    /*
      it("should remove all slots", [&]
      {
        auto conn1 = signal.connect([]{});
        auto conn2 = signal.connect([]{});
        signal.disconnect_all();
        AssertThat(signal.slot_count(), Equals(0u));
        AssertThat(conn1.connected(), Equals(false));
        AssertThat(conn2.connected(), Equals(false));
        AssertThat(signal.empty(), Equals(true));
      });*/
      it("should remove all slots while iterating", [&]
      {
        // should still fire each slot once
        // matches node.js event emitter behavior
        std::pair<unsigned, connection> res1, res2;
        res1.second = signal.connect([&] {
          res1.first++;
          signal.disconnect_all();
        });
        
        res2.second = signal.connect([&] {
          res2.first++;
        });
        signal.emit();
        
        AssertThat(signal.slot_count(), Equals(0u));
        AssertThat(res1.second.connected(), Equals(false));
        AssertThat(res2.second.connected(), Equals(false));
        AssertThat(res1.first, Equals(1));
        AssertThat(res2.first, Equals(1));
        
      });
      it("should remove all slots while iterating, without removing new slots", [&]
      {
        std::pair<unsigned, connection> res1, res2, res3;
        res1.second = signal.connect([&] {
          res1.first++;
          signal.disconnect_all();
          res3.second = signal.connect([&] {
            res3.first++;
          });
        });
        
        res2.second = signal.connect([&]{
          res2.first++;
        });
        signal.emit();
        signal.emit();
        AssertThat(signal.slot_count(), Equals(1u));
        AssertThat(res1.first, Equals(1u));
        AssertThat(res2.first, Equals(1u));
        AssertThat(res2.first, Equals(1u));
      });
      
      it("should support disconnect_all while iterating, followed by connect/emit", [&] {
        std::pair<unsigned, connection> res1, res2;
        res1.second = signal.connect([&] {
          res1.first++;
          signal.disconnect_all();
          res2.second = signal.connect([&] {
            res2.first++;
          });
          signal.emit();
        });
        signal.emit();
        AssertThat(res1.first, Equals(1u));
        AssertThat(res2.first, Equals(1u));
        AssertThat(res1.second.connected(), Equals(false));
        AssertThat(res2.second.connected(), Equals(true));
        AssertThat(signal.slot_count(), Equals(1));
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
    ss::signal<void()> signal;
    before_each([&] { signal = ss::signal<void()>{}; });
    describe("#connected()", [&]
    {
      it("should return whether or not the slot is connected", [&]
      {
        auto connection = signal.connect([]{});
        AssertThat(connection.connected(), Equals(true));
        signal.disconnect_all();
        AssertThat(connection.connected(), Equals(false));
      });
    });
    describe("#disconnect", [&]{
      it("should disconnect the slot",[&]
      {
        bool fired = false;
        auto connection = signal.connect([&] { fired = true; });
        connection.disconnect();
        signal.emit();
        AssertThat(fired, Equals(false));
        AssertThat(connection.connected(), Equals(false));
        AssertThat(signal.slot_count(), Equals(0u));
      });
      it("should not throw if already disconnected", [&]
      {
        auto connection = signal.connect([]{});
        connection.disconnect();
        connection.disconnect();
        AssertThat(connection.connected(), Equals(false));
        AssertThat(signal.slot_count(), Equals(0u));
      });
    });
    it("should be consistent across copies", [&]
    {
      auto conn1 = signal.connect([]{});
      auto conn2 = conn1;
      conn1.disconnect();
      AssertThat(conn1.connected(), Equals(conn2.connected()));
      AssertThat(signal.slot_count(), Equals(0u));
    });
    it("should not affect slot lifetime", [&]
    {
      bool fired = false;
      auto fn = [&] { fired = true; };
      {
        auto connection = signal.connect(fn);
      }
      signal.emit();
      AssertThat(fired, Equals(true));
    });
    it("should still be valid if the signal is destroyed", [&]
    {
      using connection_type = ss::signal<void()>::connection;
      connection_type connection;
      {
        ss::signal<void()> scoped_signal{};
        connection = scoped_signal.connect([]{});
      }
      AssertThat(connection.connected(), Equals(false));
    });
    
  });
  describe("scoped_connection", []{
    ss::signal<void()> signal;
    before_each([&] { signal = ss::signal<void()>(); });
    it("should disconnect the connection after leaving the scope", [&]
    {
      bool fired = false;
      {
        auto scoped = make_scoped_connection(signal.connect([&]{ fired = true ;}));
      }
      signal.emit();
      AssertThat(fired, Equals(false));
      AssertThat(signal.empty(), Equals(true));
    });
    it("should update state of underlying connection", [&]
    {
      auto connection = signal.connect([]{});
      {
        auto scoped = make_scoped_connection(connection);
      }
      signal.emit();
      AssertThat(connection.connected(), Equals(false));
    });
  });
});
int main(int argc, char* argv[]) {
  bandit::run(argc, argv);
}
