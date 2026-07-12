// SPDX-License-Identifier: Apache-2.0
#include "x64_engine_internal.h"
#include <stdlib.h>
#include <string.h>
static bool materialize(const struct gem_thread_context *s, struct gem_x64_context *d) {
    if (!gem_context_is_valid(s) || s->isa != GEM_ISA_X64 || !d)
        return false;
    memset(d, 0, sizeof(*d));
    d->gpr[0] = s->x[8];
    d->gpr[1] = s->x[0];
    d->gpr[2] = s->x[1];
    d->gpr[3] = s->x[27];
    d->gpr[4] = s->sp;
    d->gpr[5] = s->x[29];
    d->gpr[6] = s->x[25];
    d->gpr[7] = s->x[26];
    d->gpr[8] = s->x[2];
    d->gpr[9] = s->x[3];
    d->gpr[10] = s->x[4];
    d->gpr[11] = s->x[5];
    d->gpr[12] = s->x[19];
    d->gpr[13] = s->x[20];
    d->gpr[14] = s->x[21];
    d->gpr[15] = s->x[22];
    d->rip = s->pc;
    d->rflags = s->x64_rflags;
    d->mxcsr = s->x64_mxcsr;
    d->fcw = s->x64_fcw;
    d->fsw = s->x64_fsw;
    memcpy(d->xmm, s->v, sizeof(d->xmm));
    memcpy(d->x87, s->x87, sizeof(d->x87));
    d->teb = s->teb;
    return true;
}
static bool commit_state(const struct gem_x64_context *s, struct gem_thread_context *d) {
    struct gem_thread_context n = *d;
    if (!s || s->teb != d->teb)
        return false;
    n.x[8] = s->gpr[0];
    n.x[0] = s->gpr[1];
    n.x[1] = s->gpr[2];
    n.x[27] = s->gpr[3];
    n.sp = s->gpr[4];
    n.x[29] = s->gpr[5];
    n.x[25] = s->gpr[6];
    n.x[26] = s->gpr[7];
    n.x[2] = s->gpr[8];
    n.x[3] = s->gpr[9];
    n.x[4] = s->gpr[10];
    n.x[5] = s->gpr[11];
    n.x[19] = s->gpr[12];
    n.x[20] = s->gpr[13];
    n.x[21] = s->gpr[14];
    n.x[22] = s->gpr[15];
    n.pc = s->rip;
    n.x64_rflags = s->rflags;
    n.x64_mxcsr = s->mxcsr;
    n.x64_fcw = s->fcw;
    n.x64_fsw = s->fsw;
    memcpy(n.v, s->xmm, sizeof(n.v));
    memcpy(n.x87, s->x87, sizeof(n.x87));
    n.x[18] = n.teb;
    if (!gem_context_is_valid(&n) || n.isa != GEM_ISA_X64)
        return false;
    *d = n;
    return true;
}
struct gem_x64_runtime *gem_x64_runtime_create(struct gem_memory *m,
                                               const struct gem_x64_runtime_config *c) {
    struct gem_x64_runtime *r;
    if (!m)
        return 0;
    r = calloc(1, sizeof(*r));
    if (!r)
        return 0;
    r->memory = m;
    if (c)
        r->config = *c;
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
    if (r->running || !materialize(c, &in) ||
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
        if (one != 1 || !commit_state(&out, c)) {
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
void gem_x64_runtime_invalidate_code(struct gem_x64_runtime *r, uint64_t a, uint64_t s) {
    (void)r;
    (void)a;
    (void)s;
}
const char *gem_x64_runtime_engine_name(const struct gem_x64_runtime *r) {
    return r ? "Blink interpreter" : "unavailable";
}
const char *gem_x64_runtime_engine_version(const struct gem_x64_runtime *r) {
    return r ? gem_x64_blink_version() : "unavailable";
}
const char *gem_x64_runtime_engine_license(const struct gem_x64_runtime *r) {
    return r ? "ISC" : "unavailable";
}
const char *gem_x64_runtime_engine_provenance(const struct gem_x64_runtime *r) {
    return r ? "jart/blink@f006a4fc6f9b8de9272504fdff0dbbe5ce5dc580;real-interpreter;"
               "DISABLE_JIT;patch-sha256="
               "5db0ef0f144fe0df014496fe521e0640659f7dd44cfbb3e79defa7fb503551a6"
             : "unavailable";
}
