//
//  c2c.cpp
//  A16-benchmark
//
//  Created by yiwei yang on 9/24/22.
//  https://github.com/rigtorp/c2clat

#include "c2c.hpp"

typedef struct cpu_set {
  uint32_t count;
} cpu_set_t;

static inline void CPU_ZERO(cpu_set_t *cs) { cs->count = 0; }

static inline void CPU_SET(int num, cpu_set_t *cs) { cs->count |= (1 << num); }

static inline int CPU_ISSET(int num, cpu_set_t *cs) {
  return (cs->count & (1 << num));
}

void pin_thread(int cpu) {
  thread_affinity_policy policy;
  policy.affinity_tag = cpu;
  kern_return_t result =
      thread_policy_set(pthread_mach_thread_np(pthread_self()),
                        THREAD_AFFINITY_POLICY, (thread_policy_t)&policy, 1);
  //  if (result != KERN_SUCCESS) {
  //    perror(("thread_policy_set() failure: " +
  //    std::to_string(result)).c_str()); exit(1);
  //  }
}
int cpu_count() {
  int count;
  size_t size = sizeof(count);
  sysctlbyname("hw.ncpu", &count, &size, 0, 0);
  return count;
}

int sched_getaffinity(pid_t pid, size_t cpu_size, cpu_set_t *cpu_set) {
  int32_t core_count = 0;
  size_t len = sizeof(core_count);
  int ret = sysctlbyname("machdep.cpu.core_count", &core_count, &len, 0, 0);
  //  if (ret) {
  //    printf("error while get core count %d\n", ret);
  //    return -1;
  //  }
  cpu_set->count = 0;
  for (int i = 0; i < core_count; i++) {
    cpu_set->count |= (1 << i);
  }

  return 0;
}

std::string C2C::getC2C() {
  int nsamples = 1000;
  /// http://www.hybridkernel.com/2015/01/18/binding_threads_to_cores_osx.html
  cpu_set_t set;
  CPU_ZERO(&set);
  if (sched_getaffinity(0, sizeof(set), &set) == -1) {
    perror("sched_getaffinity");
    exit(1);
  }

  // enumerate available CPUs
  std::vector<int> cpus;
  for (int i = 0; i < cpu_count(); ++i) {
    if (CPU_ISSET(i, &set)) {
      cpus.push_back(i);
    }
  }

  std::map<std::pair<int, int>, std::chrono::nanoseconds> data;
  std::string res("");
  for (size_t i = 0; i < cpus.size(); ++i) {
    for (size_t j = i + 1; j < cpus.size(); ++j) {

      alignas(64) std::atomic<int> seq1 = {-1};
      alignas(64) std::atomic<int> seq2 = {-1};

      auto t = std::thread([&] {
        pin_thread(cpus[i]);
        for (int m = 0; m < nsamples; ++m) {
          for (int n = 0; n < 100; ++n) {
            while (seq1.load(std::memory_order_acquire) != n)
              ;
            seq2.store(n, std::memory_order_release);
          }
        }
      });

      std::chrono::nanoseconds rtt = std::chrono::nanoseconds::max();

      pin_thread(cpus[j]);
      for (int m = 0; m < nsamples; ++m) {
        seq1 = seq2 = -1;
        auto ts1 = std::chrono::steady_clock::now();
        for (int n = 0; n < 100; ++n) {
          seq1.store(n, std::memory_order_release);
          while (seq2.load(std::memory_order_acquire) != n)
            ;
        }
        auto ts2 = std::chrono::steady_clock::now();
        rtt = std::min(rtt, ts2 - ts1);
      }

      t.join();

      data[{i, j}] = rtt / 2 / 100;
      data[{j, i}] = rtt / 2 / 100;
    }
  }

  res += "C2C: ";
  for (size_t i = 0; i < cpus.size(); ++i) {
    res += std::string(" ") + "\t" + std::to_string(cpus[i]);
  }
  res += "\n";

  for (size_t i = 0; i < cpus.size(); ++i) {
    res += "\t" + std::to_string(cpus[i]);
    for (size_t j = 0; j < cpus.size(); ++j) {
      res += std::string(" ") + "\t" + std::to_string(data[{i, j}].count());
    }
  }
  return res;
}
