#include <slimsig/slimsig.h>
#include <memory>
#include <vector>
#include <iostream>
#include <chrono>
int count = 0;
void foo(int i) {
  count += 1;
}
int main(int argc, char* argv[]) {
  if (argc == 1) {
  std::cout << "Vector benchmark...\n";
    std::vector<std::function<void(int)>> vec;
    auto start = std::chrono::high_resolution_clock::now();
    for (unsigned i = 0; i < 100000; i++) {
      vec.push_back(std::function<void(int)>(&foo));
    }
    auto diff = std::chrono::high_resolution_clock::now() - start;
    std::cout << "Connecting Time: " << std::chrono::duration_cast<std::chrono::milliseconds>(diff).count() << "ms\n";
    start = std::chrono::high_resolution_clock::now();
    for (unsigned x = 0; x < 10000; x++) {
    for (unsigned i = 0; i < 100000; i++){
      vec[i](1);
    }
    }
    diff = std::chrono::high_resolution_clock::now() - start;
    std::cout << "Emit Time: " << std::chrono::duration_cast<std::chrono::milliseconds>(diff).count() << "ms\n";
    std::cout << "count: " << count;
    }
  else {
      std::cout << "Signal benchmark...\n";
    slimsig::signal<void(int)> signal;
    auto start = std::chrono::high_resolution_clock::now();
    for (unsigned i = 0; i < 100000; i++) {
      signal.connect(&foo);
    }
    auto diff = std::chrono::high_resolution_clock::now() - start;
    std::cout << "Connecting Time: " << std::chrono::duration_cast<std::chrono::milliseconds>(diff).count() << "ms\n";
    start = std::chrono::high_resolution_clock::now();
    for (unsigned i = 0; i < 10000; i++){
      signal.emit(1);
    }
    diff = std::chrono::high_resolution_clock::now() - start;
    std::cout << "Emit Time: " << std::chrono::duration_cast<std::chrono::milliseconds>(diff).count() << "ms\n";
    std::cout << "count: " << count;
  }
}
