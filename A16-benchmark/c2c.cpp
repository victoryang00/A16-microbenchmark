//
//  c2c.cpp
//  A16-benchmark
//
//  Created by yiwei yang on 9/24/22.
//

#include "c2c.hpp"

void pinThread(int cpu){
  cpu_set_t set; // change to thread_policy_set(pthread_mach_thread_np(m_thread.native_handle()), THREAD_AFFINITY_POLICY, (thread_policy_t)&policy, 1);
  CPU_ZERO(&set);
  CPU_SET(cpu, &set);
  if (sched_setaffinity(0, sizeof(set), &set) == -1) {
    perror("sched_setaffinity");
    exit(1);
  }
}

std::string C2C::getC2C(){

    int nsamples = 1000;
    bool plot = false;

    int opt;
    
    cpu_set_t set;
    CPU_ZERO(&set);
    if (sched_getaffinity(0, sizeof(set), &set) == -1) {
      perror("sched_getaffinity");
      exit(1);
    }

    // enumerate available CPUs
    std::vector<int> cpus;
    for (int i = 0; i < CPU_SETSIZE; ++i) {
      if (CPU_ISSET(i, &set)) {
        cpus.push_back(i);
      }
    }

    std::map<std::pair<int, int>, std::chrono::nanoseconds> data;

    for (size_t i = 0; i < cpus.size(); ++i) {
      for (size_t j = i + 1; j < cpus.size(); ++j) {

        alignas(64) std::atomic<int> seq1 = {-1};
        alignas(64) std::atomic<int> seq2 = {-1};

        auto t = std::thread([&] {
          pinThread(cpus[i]);
          for (int m = 0; m < nsamples; ++m) {
            for (int n = 0; n < 100; ++n) {
              while (seq1.load(std::memory_order_acquire) != n)
                ;
              seq2.store(n, std::memory_order_release);
            }
          }
        });

        std::chrono::nanoseconds rtt = std::chrono::nanoseconds::max();

        pinThread(cpus[j]);
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

    std::cout << std::setw(4) << "CPU";
    for (size_t i = 0; i < cpus.size(); ++i) {
      std::cout << " " << std::setw(4) << cpus[i];
    }
    std::cout << std::endl;
    for (size_t i = 0; i < cpus.size(); ++i) {
      std::cout << std::setw(4) << cpus[i];
      for (size_t j = 0; j < cpus.size(); ++j) {
        std::cout << " " << std::setw(4) << data[{i, j}].count();
      }
      std::cout << std::endl;
    }

    return "C2C: uncaught";
}
