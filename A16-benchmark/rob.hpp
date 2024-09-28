//
//  rob.hpp
//  A16-benchmark
//
//  Created by yiwei yang on 9/24/22.
//

#ifndef rob_hpp
#define rob_hpp

#include <stdio.h>
#include <string>
#include <dlfcn.h>
#include <pthread.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <assert.h>
#include <libkern/OSCacheControl.h>
// https://gist.github.com/dougallj/5bafb113492047c865c0c8cfbc930155
extern "C"{
#define KPERF_LIST                                                             \
/*  ret, name, params */                                                     \
F(int, kpc_get_counting, void)                                               \
F(int, kpc_force_all_ctrs_set, int)                                          \
F(int, kpc_set_counting, uint32_t)                                           \
F(int, kpc_set_thread_counting, uint32_t)                                    \
F(int, kpc_set_config, uint32_t, void *)                                     \
F(int, kpc_get_config, uint32_t, void *)                                     \
F(int, kpc_set_period, uint32_t, void *)                                     \
F(int, kpc_get_period, uint32_t, void *)                                     \
F(uint32_t, kpc_get_counter_count, uint32_t)                                 \
F(uint32_t, kpc_get_config_count, uint32_t)                                  \
F(int, kperf_sample_get, int *)                                              \
F(int, kpc_get_thread_counters, int, unsigned int, void *)

#define F(ret, name, ...)                                                      \
typedef ret name##proc(__VA_ARGS__);                                         \
static name##proc *name;
KPERF_LIST
#undef F

#define CFGWORD_EL0A32EN_MASK (0x10000)
#define CFGWORD_EL0A64EN_MASK (0x20000)
#define CFGWORD_EL1EN_MASK (0x40000)
#define CFGWORD_EL3EN_MASK (0x80000)
#define CFGWORD_ALLMODES_MASK (0xf0000)

#define CPMU_NONE 0
#define CPMU_CORE_CYCLE 0x02
#define CPMU_INST_A64 0x8c
#define CPMU_INST_BRANCH 0x8d
#define CPMU_SYNC_DC_LOAD_MISS 0xbf
#define CPMU_SYNC_DC_STORE_MISS 0xc0
#define CPMU_SYNC_DTLB_MISS 0xc1
#define CPMU_SYNC_ST_HIT_YNGR_LD 0xc4
#define CPMU_SYNC_BR_ANY_MISP 0xcb
#define CPMU_FED_IC_MISS_DEM 0xd3
#define CPMU_FED_ITLB_MISS 0xd4

#define KPC_CLASS_FIXED (0)
#define KPC_CLASS_CONFIGURABLE (1)
#define KPC_CLASS_POWER (2)
#define KPC_CLASS_RAWPMU (3)
#define KPC_CLASS_FIXED_MASK (1u << KPC_CLASS_FIXED)
#define KPC_CLASS_CONFIGURABLE_MASK (1u << KPC_CLASS_CONFIGURABLE)
#define KPC_CLASS_POWER_MASK (1u << KPC_CLASS_POWER)
#define KPC_CLASS_RAWPMU_MASK (1u << KPC_CLASS_RAWPMU)

#define COUNTERS_COUNT 10
#define CONFIG_COUNT 8
#define KPC_MASK (KPC_CLASS_CONFIGURABLE_MASK | KPC_CLASS_FIXED_MASK)
#define ROB_LATENCY_BOUND 100.
}
class ROB{
public:
    std::string getROB();
};

#endif /* rob_hpp */
