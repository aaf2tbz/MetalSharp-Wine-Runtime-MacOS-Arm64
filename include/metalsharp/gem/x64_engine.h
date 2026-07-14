// SPDX-License-Identifier: Apache-2.0
#ifndef METALSHARP_GEM_X64_ENGINE_H
#define METALSHARP_GEM_X64_ENGINE_H
#include "metalsharp/gem/context.h"
#include "metalsharp/gem/memory.h"
#include <stdbool.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
#define GEM_X64_DEFAULT_HOST_RETURN_SENTINEL UINT64_C(0xfffffffffffffff0)
#define GEM_X64_JIT_CACHE_CAPACITY_BYTES UINT64_C(32505856)
struct gem_x64_runtime;
enum gem_x64_engine_mode { GEM_X64_ENGINE_INTERPRETER = 0, GEM_X64_ENGINE_JIT = 1 };
enum gem_x64_host_architecture {
    GEM_X64_HOST_UNKNOWN = 0,
    GEM_X64_HOST_X86_64 = 1,
    GEM_X64_HOST_AARCH64 = 2
};
enum gem_x64_memory_access {
    GEM_X64_ACCESS_NONE = 0,
    GEM_X64_ACCESS_FETCH = 1,
    GEM_X64_ACCESS_READ = 2,
    GEM_X64_ACCESS_WRITE = 3
};
struct gem_x64_runtime_config {
    uint64_t host_return_sentinel, max_budget;
    enum gem_x64_engine_mode engine_mode;
    uint32_t reserved;
    uint64_t max_jit_cache_bytes;
};
struct gem_x64_jit_info {
    enum gem_x64_engine_mode engine_mode;
    enum gem_x64_host_architecture host_architecture;
    uint64_t cache_capacity_bytes, paths_generated, paths_executed, invalidations;
};
struct gem_x64_stop_info {
    enum gem_stop_reason reason;
    uint64_t instructions_retired, fault_address;
    enum gem_x64_memory_access access;
    uint32_t memory_error, engine_status;
};
#if defined(__cplusplus)
static_assert(sizeof(struct gem_x64_stop_info) == 40U, "gem_x64_stop_info ABI changed");
#else
_Static_assert(sizeof(struct gem_x64_stop_info) == 40U, "gem_x64_stop_info ABI changed");
#endif
/* Runtime and associated memory are thread-confined. Calls made while a run is
 * active fail closed; GEM remains the sole canonical owner. */
struct gem_x64_runtime *gem_x64_runtime_create(struct gem_memory *,
                                               const struct gem_x64_runtime_config *);
void gem_x64_runtime_destroy(struct gem_x64_runtime *);
enum gem_stop_reason gem_x64_runtime_run(struct gem_x64_runtime *, struct gem_thread_context *,
                                         uint64_t);
bool gem_x64_runtime_last_stop_info(const struct gem_x64_runtime *, struct gem_x64_stop_info *);
/* Reports whether the most recently retired instruction was decoded and
 * executed by Blink's CALL handler. This execution-owned identity is reset by
 * each run and is false for faults or runs retiring no instruction. */
bool gem_x64_runtime_last_instruction_was_call(const struct gem_x64_runtime *);
/* Execution-owned Blink decoder identity for the most recently retired RET. */
bool gem_x64_runtime_last_instruction_was_ret(const struct gem_x64_runtime *);
/* Invalidates translated code and Blink's instruction cache for the supplied
 * canonical, non-empty guest range. Invalid ranges fail closed as a no-op. */
void gem_x64_runtime_invalidate_code(struct gem_x64_runtime *, uint64_t, uint64_t);
bool gem_x64_runtime_jit_info(const struct gem_x64_runtime *, struct gem_x64_jit_info *);
const char *gem_x64_runtime_engine_name(const struct gem_x64_runtime *);
const char *gem_x64_runtime_engine_version(const struct gem_x64_runtime *);
const char *gem_x64_runtime_engine_license(const struct gem_x64_runtime *);
const char *gem_x64_runtime_engine_provenance(const struct gem_x64_runtime *);
#ifdef __cplusplus
}
#endif
#endif
