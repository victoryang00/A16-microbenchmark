//
//  rob.cpp
//  A16-benchmark
//
//  Created by yiwei yang on 9/24/22.
//

#include "rob.hpp"

uint64_t g_counters[COUNTERS_COUNT];
uint64_t g_config[COUNTERS_COUNT];
static int its = 8192;
static int outer_its = 64;
static int unroll = 1; // TODO
const char *delim = "\t";


static void configure_rdtsc() {
    if (kpc_set_config(KPC_MASK, g_config)) {
        printf("kpc_set_config failed\n");
        return;
    }
    
    if (kpc_force_all_ctrs_set(1)) {
        printf("kpc_force_all_ctrs_set failed\n");
        return;
    }
    
    if (kpc_set_counting(KPC_MASK)) {
        printf("kpc_set_counting failed\n");
        return;
    }
    
    if (kpc_set_thread_counting(KPC_MASK)) {
        printf("kpc_set_thread_counting failed\n");
        return;
    }
}

static void init_rdtsc() {
    void *kperf = dlopen(
                         "/System/Library/PrivateFrameworks/kperf.framework/Versions/A/kperf",
                         RTLD_LAZY);
    if (!kperf) {
        printf("kperf = %p\n", kperf);
        return;
    }
#define F(ret, name, ...)                                                      \
name = (name##proc *)(dlsym(kperf, #name));                                  \
if (!name) {                                                                 \
printf("%s = %p\n", #name, (void *)name);                                  \
return;                                                                    \
}
    KPERF_LIST
#undef F
    
    // TODO: KPC_CLASS_RAWPMU_MASK
    
    if (kpc_get_counter_count(KPC_MASK) != COUNTERS_COUNT) {
        printf("wrong fixed counters count\n");
        return;
    }
    
    if (kpc_get_config_count(KPC_MASK) != CONFIG_COUNT) {
        printf("wrong fixed config count\n");
        return;
    }
    
    // Not all counters can count all things:
    
    // CPMU_CORE_CYCLE           {0-7}
    // CPMU_FED_IC_MISS_DEM      {0-7}
    // CPMU_FED_ITLB_MISS        {0-7}
    
    // CPMU_INST_BRANCH          {3, 4, 5}
    // CPMU_SYNC_DC_LOAD_MISS    {3, 4, 5}
    // CPMU_SYNC_DC_STORE_MISS   {3, 4, 5}
    // CPMU_SYNC_DTLB_MISS       {3, 4, 5}
    // CPMU_SYNC_BR_ANY_MISP     {3, 4, 5}
    // CPMU_SYNC_ST_HIT_YNGR_LD  {3, 4, 5}
    // CPMU_INST_A64             {5}
    
    // using "CFGWORD_ALLMODES_MASK" is much noisier
    g_config[0] = CPMU_CORE_CYCLE | CFGWORD_EL0A64EN_MASK;
    // configs[3] = CPMU_SYNC_DC_LOAD_MISS | CFGWORD_EL0A64EN_MASK;
    // configs[4] = CPMU_SYNC_DTLB_MISS | CFGWORD_EL0A64EN_MASK;
    // configs[5] = CPMU_INST_A64 | CFGWORD_EL0A64EN_MASK;
    
    configure_rdtsc();
}

static unsigned long long int rdtsc() {
    if (kpc_get_thread_counters(0, COUNTERS_COUNT, g_counters)) {
        printf("kpc_get_thread_counters failed\n");
        return 1;
    }
    return g_counters[2];
}

static void shuffle(int *array, size_t n) {
    if (n > 1) {
        size_t i;
        for (i = 0; i < n - 1; i++) {
            size_t j = i + rand() / (RAND_MAX / (n - i) + 1);
            int t = array[j];
            array[j] = array[i];
            array[i] = t;
        }
    }
}

static void init_dbufs(uint64_t **out_data1, uint64_t **out_data2) {
    // Initialize two 256MB data buffers, with the same linked-list
    // of offsets.
    size_t size = 256 * 1024 * 1024;
    size_t cache_line_size = 64;
    size_t count = size / cache_line_size;
    size_t stride = cache_line_size / sizeof(void *);
    int *numbers = ((int*)malloc(count * sizeof(int)));
    for (int i = 0; i < count; i++) {
        numbers[i] = i;
    }
    shuffle(numbers, count);
    
    uint64_t *data1 = ((uint64_t*)calloc(size, 1));
    uint64_t *data2 = (uint64_t *)((char *)calloc(size + 64, 1) + 64);
    int next = numbers[count - 1];
    for (int i = 0; i < count; i++) {
        int n = numbers[i];
        data1[stride * n] = next * stride;
        data2[stride * n] = next * stride;
        next = n;
    }
    
    *out_data1 = data1;
    *out_data2 = data2;
    free(numbers);
}

static int add_prep(uint32_t *ibuf, int instr_type) {
    int o = 0;
    
    // free as much of the prf as possible
    switch (instr_type) {
        case 4: // gpr prf size
            for (int i = 5; i < 31; i++)
                ibuf[o++] = 0xd2800000 | i; // mov xi, #0
            break;
            
        case 5: // simd/fp prf size
            for (int i = 0; i < 32; i++)
                ibuf[o++] = 0x4ea11c20 | i; // mov.16b vi, v1
            break;
    }
    
    return o;
}
static int add_filler(uint32_t *ibuf, int instr_type, int j) {
    int o = 0;
    
    // "spike" is used to mean the first icount where the minimum time had clearly
    // jumped up
    
    switch (instr_type) {
        case 0: // OOO window maximum size (firestorm spike at 2295, icestorm spike at 415)
            ibuf[o++] = 0xd503201f; // nop
            break;
        case 1: // maximum in flight renames (firestorm spike at 623, icestorm spike at 111)
            ibuf[o++] = 0xd2800005; // mov x5, 0
            break;
        case 2: // load buffer size (firestorm spike at 129, icestorm spike at 29 (?))
            ibuf[o++] = 0xf9400045; // ldr x5, [x2]
            break;
        case 3: // store buffer size (firestorm spike at 108, icestorm spike at 36 (?))
            ibuf[o++] = 0xf9000445; // str x5, [x2, #8]
            break;
        case 4: // gpr prf size (firestorm spike at 380, icestorm spike at 79)
            ibuf[o++] = 0x8b0b0165; // add x5, x11, x11
            break;
        case 5: // simd/fp prf size (firestorm spike at 434, icestorm spike at 87)
            ibuf[o++] = 0x4e228420; // add v0.16b, v1.16b, v2.16b
            break;
        case 6: // scheduler (rs) size (firestorm spike at 158, icestorm spike at 34)
            ibuf[o++] = 0x8b010005; // add x5, x0, x1 (depends on pending load)
            break;
        case 7: // untaken branches (firestorm spike at 144, icestorm spike at 32)
            if (j == 0)
                ibuf[o++] = 0xeb0500bf; // cmp    x5, x5
            ibuf[o++] = 0x54000781;   // b.ne    .+0xF0
            break;
        case 8: // confused attempt to get a reoder buffer size (firestorm spike at 853)
            if (j == 0) {
                ibuf[o++] = 0xeb0500bf; // cmp    x5, x5
            } else if (j - 1 < 100) {
                ibuf[o++] = 0xf9000445; // str x5, [x2, #8]
            } else if (j - 1 - 100 < 130) {
                ibuf[o++] = 0x54000781; // b.ne    .+0xF0
            } else {
                ibuf[o++] = 0xd2800005; // mov x5, 0
            }
            break;
        case 9: // calls in flight (firestorm spike at 30, icestorm spike at 11)
            ibuf[o++] = 0x94000002; // bl +8
            ibuf[o++] = 0x14000002; // b  +8
            ibuf[o++] = 0xd65f03c0; // ret
            break;
        case 10: // uncond branch (firestorm spike at 88, icestorm spike at 32)
            ibuf[o++] = 0x14000001; // b  +4
            break;
        case 11: // taken branch (firestorm spike at 88, icestorm spike at 32)
            if (j == 0)
                ibuf[o++] = 0xeb0500bf; // cmp x5, x5
            ibuf[o++] = 0x54000020; // b.eq .+4
            break;
        case 12: // not-taken compare+branch (firestorm spike at 129)
            ibuf[o++] = 0xeb0500bf; // cmp x5, x5
            ibuf[o++] = 0x54000021; // b.ne .+4
            break;
        case 13: // taken compare+branch (firestorm spike at 88)
            ibuf[o++] = 0xeb0500bf; // cmp  x5, x5
            ibuf[o++] = 0x54000020; // b.eq .+4
            break;
    }
    
    return o;
}

void make_routine(uint32_t *ibuf, int icount, int instr_type) {
//    pthread_jit_write_protect_np(0);
    int o = 0;
    
    // prologue
    if (instr_type == 5) {
        ibuf[o++] = 0x6dbb3bef; // stp    d15, d14, [sp, #-80]!
        ibuf[o++] = 0x6d0133ed; // stp    d13, d12, [sp, #16]
        ibuf[o++] = 0x6d022beb; // stp    d11, d10, [sp, #32]
        ibuf[o++] = 0x6d0323e9; // stp    d9, d8, [sp, #48]
        ibuf[o++] = 0xa9047bfd; // stp    x29, x30, [sp, #64]
    } else {
        ibuf[o++] = 0xa9b87bfd; // stp    x29, x30, [sp, #-128]!
        ibuf[o++] = 0xa9016ffc; // stp    x28, x27, [sp, #16]
        ibuf[o++] = 0xa90267fa; // stp    x26, x25, [sp, #32]
        ibuf[o++] = 0xa9035ff8; // stp    x24, x23, [sp, #48]
        ibuf[o++] = 0xa90457f6; // stp    x22, x21, [sp, #64]
        ibuf[o++] = 0xa9054ff4; // stp    x20, x19, [sp, #80]
        ibuf[o++] = 0xa90647f2; // stp    x18, x17, [sp, #96]
        ibuf[o++] = 0xa9073ff0; // stp    x16, x15, [sp, #112]
    }
    
    // next, next, data1, data2, its
    // x0 = offset into data1
    // x1 = offset into data2
    // x2 = data1
    // x3 = data2
    // x4 = its
    
    o += add_prep(ibuf + o, instr_type);
    
    int start = o;
    int load_count = 1;
    for (int i = 0; i < load_count; i++)
        ibuf[o++] = 0xf8607840; // ldr    x0, [x2, x0, lsl #3]
    
    for (int j = 0; j < icount; j++) {
        o += add_filler(ibuf + o, instr_type, j);
    }
    
    for (int i = 0; i < load_count; i++)
        ibuf[o++] = 0xf8617861; // ldr    x1, [x3, x1, lsl #3]
    
    for (int j = 0; j < icount; j++) {
        // o += add_filler(ibuf+o, instr_type, j);
    }
    
    // lfence mode?
    ibuf[o++] = 0xD5033B9F; // DSB ISH
    ibuf[o++] = 0xD5033FDF; // ISB
    
    // loop back to top
    ibuf[o++] = 0x71000484; // subs    w4, w4, #1
    int off = start - o;
    assert(off < 0 && off > -0x40000);
    ibuf[o++] = 0x54000001 | ((off & 0x7ffff) << 5); // b.ne
    
    // epilogue
    if (instr_type == 5) {
        ibuf[o++] = 0xa9447bfd; // ldp    x29, x30, [sp, #64]
        ibuf[o++] = 0x6d4323e9; // ldp    d9, d8, [sp, #48]
        ibuf[o++] = 0x6d422beb; // ldp    d11, d10, [sp, #32]
        ibuf[o++] = 0x6d4133ed; // ldp    d13, d12, [sp, #16]
        ibuf[o++] = 0x6cc53bef; // ldp    d15, d14, [sp], #80
        ibuf[o++] = 0xd65f03c0; // ret
    } else {
        ibuf[o++] = 0xa9473ff0; // ldp    x16, x15, [sp, #112]
        ibuf[o++] = 0xa94647f2; // ldp    x18, x17, [sp, #96]
        ibuf[o++] = 0xa9454ff4; // ldp    x20, x19, [sp, #80]
        ibuf[o++] = 0xa94457f6; // ldp    x22, x21, [sp, #64]
        ibuf[o++] = 0xa9435ff8; // ldp    x24, x23, [sp, #48]
        ibuf[o++] = 0xa94267fa; // ldp    x26, x25, [sp, #32]
        ibuf[o++] = 0xa9416ffc; // ldp    x28, x27, [sp, #16]
        ibuf[o++] = 0xa8c87bfd; // ldp    x29, x30, [sp], #128
        ibuf[o++] = 0xd65f03c0; // ret
    }
    
//    pthread_jit_write_protect_np(1);
    sys_icache_invalidate(ibuf, o * 4);
}

std::string HelloWorld::sayHello(){
    int test_high_perf_cores = 1;
    int instr_type = 1;
    int start_icount = 600;
    int stop_icount = 700;
    int stride_icount = 1;
    std::string result;
    
    // TODO: can we force this to run on the fast cores?
    // counters seemingly fail to update if we initialise
    // them, then switch cores, although the fixed thread
    // counters don't have this problem.
    
    // QOS_CLASS_BACKGROUND does seem to pin it to the slow
    // cores though.
    if (test_high_perf_cores) {
        pthread_set_qos_class_self_np(QOS_CLASS_USER_INTERACTIVE, 0);
    } else {
        pthread_set_qos_class_self_np(QOS_CLASS_BACKGROUND, 0);
    }
    
    init_rdtsc();
    uint64_t *data1, *data2;
    init_dbufs(&data1, &data2);
    
    void *mapping = mmap(NULL, 0x400000, PROT_READ | PROT_WRITE | PROT_EXEC,
                         MAP_ANON | MAP_PRIVATE | MAP_JIT, -1, 0);
    uint32_t *ibuf = (uint32_t *)mapping;
    
    uint64_t next = 0;
    
    for (int icount = start_icount; icount <= stop_icount; icount += stride_icount) {
        make_routine(ibuf, icount, instr_type);
        
        auto routine = ((uint64_t (*)(uint64_t, uint64_t, uint64_t *, uint64_t *, uint64_t))ibuf);
        
        uint64_t min_diff = 0x7fffffffffffffffLL;
        uint64_t max_diff = 0x0;
        uint64_t sum_diff = 0;
        
        next = routine(next, next, data1, data2, its);
        
        for (int i = 0; i < outer_its; i++) {
            
            // in case we were on the wrong core earlier
            configure_rdtsc();
            
            long long start = rdtsc();
            next = routine(next, next, data1, data2, its);
            long long stop = rdtsc();
            
            uint64_t cycles = stop - start;
            
            sum_diff += cycles;
            if (min_diff > cycles) {
                min_diff = cycles;
            }
            if (max_diff < cycles) {
                max_diff = cycles;
            }
        }
        
       printf("%d%s%.2f%s%.2f%s%.2f\n", icount, delim,
           0.5 * min_diff / its / unroll, delim,
           0.5 * sum_diff / its / unroll / outer_its, delim,
           0.5 * max_diff / its / unroll);
    }
    return result;
}
