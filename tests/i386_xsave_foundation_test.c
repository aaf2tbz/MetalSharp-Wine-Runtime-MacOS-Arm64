// SPDX-License-Identifier: Apache-2.0
/* ADR 0013 W4 gate: protected-mode VEX decode, XGETBV, standard XSAVE/XRSTOR,
 * fail-closed XSAVEOPT, #GP mapping, and restartable cross-page operands. */
#include "blink/gem_embed.h"
#include "i386_engine_internal.h"
#include "metalsharp/gem/i386_memory.h"

#include <assert.h>
#include <string.h>

#define CODE UINT32_C(0x00400000)
#define DATA UINT32_C(0x00500000)
#define STACK UINT32_C(0x00600000)
#define XSTATE_SIZE 832U

static const uint8_t xgetbv[] = {0x0fU, 0x01U, 0xd0U};
static const uint8_t xsetbv[] = {0x0fU, 0x01U, 0xd1U};
static const uint8_t xsave_esi[] = {0x0fU, 0xaeU, 0x26U};
static const uint8_t xrstor_esi[] = {0x0fU, 0xaeU, 0x2eU};
static const uint8_t xsaveopt_esi[] = {0x0fU, 0xaeU, 0x36U};
static const uint8_t vzeroupper[] = {0xc5U, 0xf8U, 0x77U};

static void put32(uint8_t *p, uint32_t value) {
    p[0] = (uint8_t)value;
    p[1] = (uint8_t)(value >> 8);
    p[2] = (uint8_t)(value >> 16);
    p[3] = (uint8_t)(value >> 24);
}

static void put64(uint8_t *p, uint64_t value) {
    put32(p, (uint32_t)value);
    put32(p + 4, (uint32_t)(value >> 32));
}

static uint64_t get64(const uint8_t *p) {
    return (uint64_t)p[0] | (uint64_t)p[1] << 8 | (uint64_t)p[2] << 16 | (uint64_t)p[3] << 24 |
           (uint64_t)p[4] << 32 | (uint64_t)p[5] << 40 | (uint64_t)p[6] << 48 |
           (uint64_t)p[7] << 56;
}

static struct gem_memory *make_memory(const uint8_t *code, size_t code_size,
                                      uint64_t committed_data) {
    struct gem_memory *memory = gem_memory_create();
    uint32_t address = CODE;
    assert(memory != NULL);
    assert(gem_i386_memory_reserve(memory, &address, GEM_GUEST_PAGE_SIZE) == GEM_MEMORY_OK);
    assert(gem_i386_memory_commit(memory, CODE, GEM_GUEST_PAGE_SIZE, GEM_PAGE_EXECUTE_READWRITE) ==
           GEM_MEMORY_OK);
    assert(gem_i386_memory_write(memory, CODE, code, code_size) == GEM_MEMORY_OK);
    address = DATA;
    assert(gem_i386_memory_reserve(memory, &address, 2U * GEM_GUEST_PAGE_SIZE) == GEM_MEMORY_OK);
    assert(gem_i386_memory_commit(memory, DATA, committed_data, GEM_PAGE_READWRITE) ==
           GEM_MEMORY_OK);
    address = STACK;
    assert(gem_i386_memory_reserve(memory, &address, GEM_GUEST_PAGE_SIZE) == GEM_MEMORY_OK);
    assert(gem_i386_memory_commit(memory, STACK, GEM_GUEST_PAGE_SIZE, GEM_PAGE_READWRITE) ==
           GEM_MEMORY_OK);
    return memory;
}

static struct gem_i386_runtime *make_runtime(struct gem_memory *memory,
                                             enum gem_i386_engine_mode mode, size_t code_size) {
    struct gem_i386_runtime_config config;
    memset(&config, 0, sizeof(config));
    config.engine_mode = mode;
    config.host_return_sentinel = CODE + (uint32_t)code_size;
    config.max_budget = 1U;
    return gem_i386_runtime_create(memory, &config);
}

static void initialize(struct gem_i386_context *context, uint32_t operand, uint32_t mask) {
    gem_i386_context_initialize(context, UINT32_C(0x7ffde000));
    context->eip = CODE;
    context->gpr[GEM_I386_ESP] = STACK + GEM_GUEST_PAGE_SIZE - 16U;
    context->gpr[GEM_I386_ESI] = operand;
    context->gpr[GEM_I386_EAX] = mask;
    context->xcr0 = GEM_I386_XCR0_SUPPORTED;
}

static void expect_protection(const uint8_t *code, size_t code_size, uint32_t operand,
                              uint32_t ecx) {
    struct gem_memory *memory = make_memory(code, code_size, GEM_GUEST_PAGE_SIZE);
    struct gem_i386_runtime *runtime = make_runtime(memory, GEM_I386_ENGINE_INTERPRETER, code_size);
    struct gem_i386_context context;
    struct gem_i386_stop_info stop;
    assert(runtime != NULL);
    initialize(&context, operand, 7U);
    context.gpr[GEM_I386_ECX] = ecx;
    assert(gem_i386_runtime_run(runtime, &context, 1U) == GEM_STOP_WINDOWS_EXCEPTION);
    assert(gem_i386_runtime_last_stop_info(runtime, &stop));
    assert(stop.engine_status == GEM_I386_EXCEPTION_GENERAL_PROTECTION);
    gem_i386_runtime_destroy(runtime);
    gem_memory_destroy(memory);
}

static void exercise_xgetbv(enum gem_i386_engine_mode mode) {
    struct gem_memory *memory = make_memory(xgetbv, sizeof(xgetbv), GEM_GUEST_PAGE_SIZE);
    struct gem_i386_runtime *runtime = make_runtime(memory, mode, sizeof(xgetbv));
    struct gem_i386_context context;
    assert(runtime != NULL);
    initialize(&context, DATA, 0U);
    assert(gem_i386_runtime_run(runtime, &context, 1U) == GEM_STOP_HOST_RETURN);
    assert(context.gpr[GEM_I386_EAX] == 7U && context.gpr[GEM_I386_EDX] == 0U);
    gem_i386_runtime_destroy(runtime);
    gem_memory_destroy(memory);
}

static void exercise_xsave(enum gem_i386_engine_mode mode) {
    struct gem_memory *memory = make_memory(xsave_esi, sizeof(xsave_esi), GEM_GUEST_PAGE_SIZE);
    struct gem_i386_runtime *runtime = make_runtime(memory, mode, sizeof(xsave_esi));
    struct gem_i386_context context;
    uint8_t area[XSTATE_SIZE];
    uint32_t i;
    assert(runtime != NULL);
    initialize(&context, DATA, 7U);
    context.mxcsr = UINT32_C(0x1fa0);
    for (i = 0U; i < 8U; ++i) {
        context.xmm[i].lo = UINT64_C(0x1010101000000000) + i;
        context.xmm[i].hi = UINT64_C(0x2020202000000000) + i;
        context.ymm_upper[i].lo = UINT64_C(0xa0a0a0a000000000) + i;
        context.ymm_upper[i].hi = UINT64_C(0xb0b0b0b000000000) + i;
    }
    assert(gem_i386_runtime_run(runtime, &context, 1U) == GEM_STOP_HOST_RETURN);
    assert(gem_i386_memory_read(memory, DATA, area, sizeof(area)) == GEM_MEMORY_OK);
    assert(get64(area + 512) == 7U);
    assert(get64(area + 160) == UINT64_C(0x1010101000000000));
    assert(get64(area + 576) == UINT64_C(0xa0a0a0a000000000));
    gem_i386_runtime_destroy(runtime);
    gem_memory_destroy(memory);
}

static void exercise_xrstor(enum gem_i386_engine_mode mode) {
    struct gem_memory *memory = make_memory(xrstor_esi, sizeof(xrstor_esi), GEM_GUEST_PAGE_SIZE);
    struct gem_i386_runtime *runtime = make_runtime(memory, mode, sizeof(xrstor_esi));
    struct gem_i386_context context;
    uint8_t area[XSTATE_SIZE];
    uint32_t i;
    assert(runtime != NULL);
    memset(area, 0, sizeof(area));
    put32(area + 24, UINT32_C(0x1fa0));
    put64(area + 512, 6U);
    for (i = 0U; i < 8U; ++i) {
        put64(area + 160U + i * 16U, UINT64_C(0x3030303000000000) + i);
        put64(area + 168U + i * 16U, UINT64_C(0x4040404000000000) + i);
        put64(area + 576U + i * 16U, UINT64_C(0xc0c0c0c000000000) + i);
        put64(area + 584U + i * 16U, UINT64_C(0xd0d0d0d000000000) + i);
    }
    assert(gem_i386_memory_write(memory, DATA, area, sizeof(area)) == GEM_MEMORY_OK);
    initialize(&context, DATA, 6U);
    assert(gem_i386_runtime_run(runtime, &context, 1U) == GEM_STOP_HOST_RETURN);
    assert(context.mxcsr == UINT32_C(0x1fa0));
    assert(context.xmm[7].lo == UINT64_C(0x3030303000000007));
    assert(context.xmm[7].hi == UINT64_C(0x4040404000000007));
    assert(context.ymm_upper[7].lo == UINT64_C(0xc0c0c0c000000007));
    assert(context.ymm_upper[7].hi == UINT64_C(0xd0d0d0d000000007));
    gem_i386_runtime_destroy(runtime);
    gem_memory_destroy(memory);
}

static void exercise_cross_page_retry(void) {
    const uint32_t operand = DATA + GEM_GUEST_PAGE_SIZE - 64U;
    struct gem_memory *memory = make_memory(xsave_esi, sizeof(xsave_esi), GEM_GUEST_PAGE_SIZE);
    struct gem_i386_runtime *runtime =
        make_runtime(memory, GEM_I386_ENGINE_INTERPRETER, sizeof(xsave_esi));
    struct gem_i386_context context;
    uint8_t first[64];
    assert(runtime != NULL);
    initialize(&context, operand, 7U);
    memset(first, 0xa5, sizeof(first));
    assert(gem_i386_memory_write(memory, operand, first, sizeof(first)) == GEM_MEMORY_OK);
    assert(gem_i386_runtime_run(runtime, &context, 1U) == GEM_STOP_MEMORY_FAULT);
    memset(first, 0, sizeof(first));
    assert(gem_i386_memory_read(memory, operand, first, sizeof(first)) == GEM_MEMORY_OK);
    assert(first[0] == 0xa5U && first[63] == 0xa5U);
    assert(gem_i386_memory_commit(memory, DATA + GEM_GUEST_PAGE_SIZE, GEM_GUEST_PAGE_SIZE,
                                  GEM_PAGE_READWRITE) == GEM_MEMORY_OK);
    assert(gem_i386_runtime_run(runtime, &context, 1U) == GEM_STOP_HOST_RETURN);
    gem_i386_runtime_destroy(runtime);
    gem_memory_destroy(memory);
}

static void exercise_vex_decode_gate(void) {
    struct gem_memory *memory = make_memory(vzeroupper, sizeof(vzeroupper), GEM_GUEST_PAGE_SIZE);
    struct gem_i386_runtime *runtime =
        make_runtime(memory, GEM_I386_ENGINE_INTERPRETER, sizeof(vzeroupper));
    struct gem_i386_context context;
    struct blink_gem_decode_attempt attempt;
    assert(runtime != NULL);
    initialize(&context, DATA, 0U);
    assert(gem_i386_runtime_run(runtime, &context, 1U) == GEM_STOP_UNSUPPORTED_INSTRUCTION);
    memset(&attempt, 0, sizeof(attempt));
    attempt.abi_version = BLINK_GEM_DECODE_ATTEMPT_ABI_VERSION;
    attempt.size = sizeof(attempt);
    assert(blink_gem_machine_decode_attempt_info(runtime->backend, &attempt));
    assert(attempt.valid && attempt.mopcode == UINT32_C(0x177));
    gem_i386_runtime_destroy(runtime);
    gem_memory_destroy(memory);
}

static void exercise_xsaveopt_gate(void) {
    struct gem_memory *memory =
        make_memory(xsaveopt_esi, sizeof(xsaveopt_esi), GEM_GUEST_PAGE_SIZE);
    struct gem_i386_runtime *runtime =
        make_runtime(memory, GEM_I386_ENGINE_INTERPRETER, sizeof(xsaveopt_esi));
    struct gem_i386_context context;
    assert(runtime != NULL);
    initialize(&context, DATA, 7U);
    assert(gem_i386_runtime_run(runtime, &context, 1U) == GEM_STOP_UNSUPPORTED_INSTRUCTION);
    gem_i386_runtime_destroy(runtime);
    gem_memory_destroy(memory);
}

int main(void) {
    exercise_xgetbv(GEM_I386_ENGINE_INTERPRETER);
    exercise_xgetbv(GEM_I386_ENGINE_JIT);
    exercise_xsave(GEM_I386_ENGINE_INTERPRETER);
    exercise_xsave(GEM_I386_ENGINE_JIT);
    exercise_xrstor(GEM_I386_ENGINE_INTERPRETER);
    exercise_xrstor(GEM_I386_ENGINE_JIT);
    exercise_cross_page_retry();
    exercise_vex_decode_gate();
    exercise_xsaveopt_gate();
    expect_protection(xsave_esi, sizeof(xsave_esi), DATA + 1U, 0U);
    expect_protection(xgetbv, sizeof(xgetbv), DATA, 1U);
    expect_protection(xsetbv, sizeof(xsetbv), DATA, 0U);
    return 0;
}
