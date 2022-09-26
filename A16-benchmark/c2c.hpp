//
//  c2c.hpp
//  A16-benchmark
//
//  Created by yiwei yang on 9/24/22.
//

#ifndef c2c_hpp
#define c2c_hpp

#include <stdio.h>
// https://developer.apple.com/forums/thread/44002
#include <sched.h>
#include <stdlib.h>
#include <unistd.h>

#include <atomic>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <mach/thread_act.h>
#include <mach/thread_policy.h>
#include <map>
#include <sys/sysctl.h>
#include <sys/types.h>
#include <thread>
#include <vector>

void pinThread(int cpu);

class C2C {
public:
  std::string getC2C();
};

#endif /* c2c_hpp */
