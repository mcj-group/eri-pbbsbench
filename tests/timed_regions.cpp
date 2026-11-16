#include "../common/time_loop.h"
#include <chrono>
#include <thread>
#include <vector>
#include <cstdlib>
#if defined(PARLAY_OPENCILK)
#include <cilk/cilk.h>
#endif

int main(int argc, char** argv) {
  const bool warmup = argc < 2 || std::string(argv[1]) != "no-warmup";
  std::vector<long long> measured;
  int calls = 0;
  time_loop(3, warmup ? 0.001 : 0.0,
    [] { std::this_thread::sleep_for(std::chrono::milliseconds(200)); },
    [&] {
      auto start = std::chrono::steady_clock::now();
#if defined(PARLAY_OPENCILK)
      // Deliberately spawn directly in the closure: invoke() must keep its
      // Cilk region from extending into the snapshot calls in time_loop().
      cilk_spawn std::this_thread::sleep_for(std::chrono::milliseconds(20));
#endif
      std::this_thread::sleep_for(std::chrono::milliseconds(20));
#if defined(PARLAY_OPENCILK)
      cilk_sync;
#endif
      auto elapsed = std::chrono::steady_clock::now() - start;
      if (!warmup || calls > 0)
        measured.push_back(std::chrono::duration_cast<std::chrono::nanoseconds>(elapsed).count());
      ++calls;
    },
    [] { std::this_thread::sleep_for(std::chrono::milliseconds(200)); });
  if (calls != 3 + int(warmup) || measured.size() != 3) std::abort();
  for (auto ns : measured) std::cout << "inner_ns: " << ns << '\n';
}
