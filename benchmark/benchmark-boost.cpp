#include <boost/signals2.hpp>
#include <iostream>
#include <chrono>
long long count = 0;
void foo(int i) {
  count += 1;
}
int main(int argc, char* argv[]) {
  std::cout << "Boost::Signals2 benchmark...\n";
  using sig = typename boost::signals2::signal_type<void(int), boost::signals2::keywords::mutex_type<boost::signals2::dummy_mutex>>::type;
  sig signal;
  auto start = std::chrono::high_resolution_clock::now();
  for (unsigned i = 0; i < 100000; i++) {
      signal.connect(&foo);
  }
  std::cout << "Slot Count: " << signal.num_slots() << "\n";
  auto diff = std::chrono::high_resolution_clock::now() - start;
  std::cout << "Connecting Time: " << std::chrono::duration_cast<std::chrono::milliseconds>(diff).count() << "ms\n";
  start = std::chrono::high_resolution_clock::now();
  for (unsigned i = 0; i < 10000; i++){
    signal(1);
  }
  diff = std::chrono::high_resolution_clock::now() - start;
  std::cout << "Emit Time: " << std::chrono::duration_cast<std::chrono::milliseconds>(diff).count() << "ms\n";
  std::cout << "Emit Count: " << count << "\n";
}
