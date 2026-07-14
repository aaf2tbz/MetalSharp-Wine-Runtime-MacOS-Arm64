// SPDX-License-Identifier: Apache-2.0
#include "x64_engine_internal.h"
#include <stdlib.h>
#include <string.h>
struct gem_x64_runtime *gem_x64_runtime_create(struct gem_memory *m,
                                               const struct gem_x64_runtime_config *c) {
    struct gem_x64_runtime *r;
    if (!m)
        return 0;
    r = calloc(1, sizeof(*r));
    if (!r)
        return 0;
    r->memory = m;
    atomic_init(&r->async_stop_requested, false);
    if (c)
        r->config = *c;
    if (r->config.reserved ||
        (r->config.engine_mode != GEM_X64_ENGINE_INTERPRETER &&
         r->config.engine_mode != GEM_X64_ENGINE_JIT) ||
        (r->config.engine_mode == GEM_X64_ENGINE_INTERPRETER &&
         r->config.max_jit_cache_bytes != 0) ||
        (r->config.engine_mode == GEM_X64_ENGINE_JIT &&
         r->config.max_jit_cache_bytes != GEM_X64_JIT_CACHE_CAPACITY_BYTES)) {
        free(r);
        return 0;
    }
    if (!r->config.host_return_sentinel)
        r->config.host_return_sentinel = GEM_X64_DEFAULT_HOST_RETURN_SENTINEL;
    if (!gem_x64_blink_create(r)) {
        free(r);
        return 0;
    }
    return r;
}
void gem_x64_runtime_destroy(struct gem_x64_runtime *r) {
    if (r) {
        gem_x64_blink_destroy(r);
        free(r);
    }
}
enum gem_stop_reason gem_x64_runtime_run(struct gem_x64_runtime *r, struct gem_thread_context *c,
                                         uint64_t budget) {
    struct gem_x64_context in, out;
    enum gem_stop_reason reason = GEM_STOP_BUDGET_EXPIRED;
    uint64_t retired = 0;
    if (!r)
        return GEM_STOP_INVARIANT_VIOLATION;
    memset(&r->last_stop, 0, sizeof(r->last_stop));
    r->last_instruction_was_call = false;
    r->last_instruction_was_ret = false;
    if (r->running || !gem_context_x64_materialize(c, &in) ||
        (r->config.max_budget && budget > r->config.max_budget)) {
        r->last_stop.reason = GEM_STOP_INVARIANT_VIOLATION;
        return GEM_STOP_INVARIANT_VIOLATION;
    }
    if (!budget) {
        c->stop_reason = GEM_STOP_BUDGET_EXPIRED;
        r->last_stop.reason = GEM_STOP_BUDGET_EXPIRED;
        return GEM_STOP_BUDGET_EXPIRED;
    }
    r->running = true;
    while (retired < budget) {
        uint32_t one = 0;
        if (atomic_exchange_explicit(&r->async_stop_requested, false, memory_order_acq_rel)) {
            reason = GEM_STOP_ASYNC_REQUEST;
            break;
        }
        r->transaction = gem_memory_transaction_begin(r->memory);
        if (!r->transaction) {
            reason = GEM_STOP_INVARIANT_VIOLATION;
            break;
        }
        reason = gem_x64_blink_step(r, &in, &out, &one);
        gem_memory_transaction_end(r->transaction);
        r->transaction = 0;
        if (reason != GEM_STOP_NONE)
            break;
        if (one != 1 || !gem_context_x64_commit(&out, c)) {
            reason = GEM_STOP_INVARIANT_VIOLATION;
            break;
        }
        in = out;
        ++retired;
        if (c->pc == r->config.host_return_sentinel) {
            reason = GEM_STOP_HOST_RETURN;
            break;
        }
    }
    r->running = false;
    if (reason == GEM_STOP_NONE)
        reason = GEM_STOP_BUDGET_EXPIRED;
    r->last_stop.reason = reason;
    r->last_stop.instructions_retired = retired;
    if (reason != GEM_STOP_INVARIANT_VIOLATION)
        c->stop_reason = reason;
    return reason;
}
bool gem_x64_runtime_last_stop_info(const struct gem_x64_runtime *r, struct gem_x64_stop_info *o) {
    if (!r || !o)
        return false;
    *o = r->last_stop;
    return true;
}
bool gem_x64_runtime_last_instruction_was_call(const struct gem_x64_runtime *r) {
    return r != NULL && r->last_stop.instructions_retired != 0U && r->last_instruction_was_call;
}
bool gem_x64_runtime_last_instruction_was_ret(const struct gem_x64_runtime *r) {
    return r != NULL && r->last_stop.instructions_retired != 0U && r->last_instruction_was_ret;
}
void gem_x64_runtime_invalidate_code(struct gem_x64_runtime *r, uint64_t a, uint64_t s) {
    if (r && !r->running)
        (void)gem_x64_blink_invalidate_code(r, a, s);
}
void gem_x64_runtime_request_async_stop(struct gem_x64_runtime *r) {
    if (r)
        atomic_store_explicit(&r->async_stop_requested, true, memory_order_release);
}
bool gem_x64_runtime_jit_info(const struct gem_x64_runtime *r, struct gem_x64_jit_info *o) {
    return gem_x64_blink_jit_info(r, o);
}
const char *gem_x64_runtime_engine_name(const struct gem_x64_runtime *r) {
    if (!r)
        return "unavailable";
    return r->config.engine_mode == GEM_X64_ENGINE_JIT ? "Blink JIT" : "Blink interpreter";
}
const char *gem_x64_runtime_engine_version(const struct gem_x64_runtime *r) {
    return r ? gem_x64_blink_version() : "unavailable";
}
const char *gem_x64_runtime_engine_license(const struct gem_x64_runtime *r) {
    return r ? "ISC" : "unavailable";
}
const char *gem_x64_runtime_engine_provenance(const struct gem_x64_runtime *r) {
    return r ? "jart/blink@f006a4fc6f9b8de9272504fdff0dbbe5ce5dc580;explicit-interpreter-or-"
               "single-instruction-jit;bounded-cache;process-serialized;patch-sha256="
               "2e4be43984e2eef5d2b52186cbedd18384004727d362c5f6fa6fe7d64d71fd6a,"
               "560648510b5d3b5e0feb5e9462fb1534d652422f15cf69e14662a276fd96d396"
             : "unavailable";
}
