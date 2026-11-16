#pragma once
#include "../parlay/internal/get_time.h"

#ifndef PBBS_BENCHMARK_NAME
#define PBBS_BENCHMARK_NAME "benchmark"
#endif

#if defined(PARLAY_OPENCILK) && defined(CILK_STATS) && CILK_STATS
#include <cilk/sched_stats.h>
#include <memory>
#include <system_error>

namespace pbbs_stats {
using snapshot = std::unique_ptr<__cilkrts_sched_stats_snapshot,
                                decltype(&__cilkrts_sched_stats_free_snapshot)>;

inline snapshot capture() {
  __cilkrts_sched_stats_snapshot* value = nullptr;
  int status = __cilkrts_sched_stats_snapshot_and_reset(&value);
  if (status) throw std::system_error(status, std::generic_category(), "Cheetah snapshot/reset");
  return snapshot(value, __cilkrts_sched_stats_free_snapshot);
}

inline void record(const snapshot& value, const char* name, int iteration) {
  std::string id = std::string(name) + "/iteration/" + std::to_string(iteration);
  int status = __cilkrts_sched_stats_record_snapshots(value.get(), id.c_str());
  if (status) throw std::system_error(status, std::generic_category(), "Cheetah snapshot recording");
}
}
#endif

namespace pbbs_time_detail {
// A spawning closure must return from its Cilk region before capture/reset.
template<class F>
__attribute__((noinline)) void invoke(F& f) { f(); }

// Match the existing timer's output, but print only after stopping/capturing.
inline void report(double elapsed) {
  std::ios::fmtflags flags = std::cout.flags();
  std::cout.precision(4);
  std::cout << std::fixed << "Parlay time: " << elapsed << std::endl;
  std::cout.flags(flags);
}
}

template<class F, class G, class H>
void time_loop(int rounds, double delay, F initf, G runf, H endf,
               const char* name = PBBS_BENCHMARK_NAME) {
  (void)name; // used only when scheduling snapshots are enabled
  parlay::internal::timer t;
  // run for delay seconds to "warm things up"
  // will skip if delay is zero
  while (t.total_time() < delay) {
    pbbs_time_detail::invoke(initf);
    pbbs_time_detail::invoke(runf);
    pbbs_time_detail::invoke(endf);
  }
  t.reset();
  for (int i=0; i < rounds; i++) {
    pbbs_time_detail::invoke(initf);
#if defined(PARLAY_OPENCILK) && defined(CILK_STATS) && CILK_STATS
    pbbs_stats::capture(); // discard setup, warmup and the preceding gap
#endif
    t.start();
    pbbs_time_detail::invoke(runf);
    double elapsed = t.stop();
#if defined(PARLAY_OPENCILK) && defined(CILK_STATS) && CILK_STATS
    pbbs_stats::record(pbbs_stats::capture(), name, i + 1);
#endif
    pbbs_time_detail::report(elapsed);
    pbbs_time_detail::invoke(endf);
  }
}
