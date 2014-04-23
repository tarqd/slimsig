#include <boost/signals2.hpp>
#include <iostream>
#include <chrono>
int count = 0;
void foo(int i) {
  count += 1;
}
int main(int argc, char* argv[]) {
  std::cout << "Boost::Signals2 benchmark...\n";
  boost::signals2::signal<void(int)> signal;
  auto start = std::chrono::high_resolution_clock::now();
  for (unsigned i = 0; i < 100000; i++) {
      signal.connect(&foo);
  }
  auto diff = std::chrono::high_resolution_clock::now() - start;
  std::cout << "Connecting Time: " << std::chrono::duration_cast<std::chrono::milliseconds>(diff).count() << "ms\n";
  start = std::chrono::high_resolution_clock::now();
  for (unsigned i = 0; i < 10000; i++){
    signal(1);
  }
  diff = std::chrono::high_resolution_clock::now() - start;
  std::cout << "Emit Time: " << std::chrono::duration_cast<std::chrono::milliseconds>(diff).count() << "ms\n";
}
