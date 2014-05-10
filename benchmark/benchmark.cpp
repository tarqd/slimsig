#include <slimsig/slimsig.h>
#include <memory>
#include <vector>
#include <iostream>
#include <chrono>
long long count = 0;
void foo(int i) {
  count += 1;
}
int main(int argc, char* argv[]) {
  std::cout << "Slimmer Signals benchmark...\n";
  slimsig::signal<void(int)> signal;
  auto start = std::chrono::high_resolution_clock::now();
  for (unsigned i = 0; i < 100000; i++) {
      signal.connect(&foo);
  }
  auto diff = std::chrono::high_resolution_clock::now() - start;
  std::cout << "Connecting Time: " << std::chrono::duration_cast<std::chrono::milliseconds>(diff).count() << "ms\n";
  std::cout << "Slot Count: " << signal.slot_count() << "\n";
  start = std::chrono::high_resolution_clock::now();
  for (unsigned i = 0; i < 10000; i++){
    signal.emit(1);
  }
  diff = std::chrono::high_resolution_clock::now() - start;
  std::cout << "Emit Time: " << std::chrono::duration_cast<std::chrono::milliseconds>(diff).count() << "ms\n";
  std::cout << "Emit Count: " << count << "\n";
}
