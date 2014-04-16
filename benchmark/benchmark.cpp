#include <slimsig/slimsig.h>

int count = 0;
void foo(int i) {
  count += 1;
}
int main(int argc, char* argv[]) {
  slimsig::signal<void(int)> signal;
  for (unsigned i = 0; i < 100000; i++)
    signal.connect(&foo);
  for (unsigned i = 0; i < 10000; i++)
    signal.emit(1);
}