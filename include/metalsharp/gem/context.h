// SPDX-License-Identifier: Apache-2.0
#ifndef METALSHARP_GEM_CONTEXT_H
#define METALSHARP_GEM_CONTEXT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define GEM_CONTEXT_LAYOUT_VERSION UINT32_C(1)
#define GEM_CONTEXT_SERIALIZATION_VERSION UINT32_C(1)
#define GEM_THREAD_CONTEXT_SIZE_V1 UINT32_C(720)
#define GEM_THREAD_CONTEXT_ALIGNMENT_V1 UINT32_C(8)
#define GEM_THREAD_CONTEXT_EXPECTED_SIZE GEM_THREAD_CONTEXT_SIZE_V1

/*
 * The serialization version identifies the future portable, field-wise,
 * little-endian context encoding. Raw struct dumps are not a supported wire,
 * fixture, trace, or saved-state format.
 */

enum gem_isa {
    GEM_ISA_INVALID = 0,
    GEM_ISA_ARM64EC = 1,
    GEM_ISA_X64 = 2,
};

enum gem_stop_reason {
    GEM_STOP_NONE = 0,
    GEM_STOP_SYSCALL = 1,
    GEM_STOP_ARCH_TRANSITION = 2,
    GEM_STOP_HOST_RETURN = 3,
    GEM_STOP_MEMORY_FAULT = 4,
    GEM_STOP_WINDOWS_EXCEPTION = 5,
    GEM_STOP_ASYNC_REQUEST = 6,
    GEM_STOP_UNSUPPORTED_INSTRUCTION = 7,
    GEM_STOP_BUDGET_EXPIRED = 8,
    GEM_STOP_INVARIANT_VIOLATION = 9,
};

struct gem_u128 {
    uint64_t lo;
    uint64_t hi;
};

struct gem_thread_context {
    uint32_t layout_version;
    uint32_t context_size;
    uint64_t x[31];
    uint64_t sp;
    uint64_t pc;
    uint32_t nzcv;
    uint32_t fpcr;
    uint32_t fpsr;
    uint32_t reserved0;
    struct gem_u128 v[16];

    uint64_t x64_rflags;
    uint32_t x64_mxcsr;
    uint16_t x64_fcw;
    uint16_t x64_fsw;
    struct gem_u128 x87[8];

    uint64_t teb;
    uint64_t original_x64_sp;
    uint64_t transition_cookie;
    uint32_t isa;
    uint32_t stop_reason;
};

#if defined(__cplusplus)
#define GEM_CONTEXT_STATIC_ASSERT(condition, message) static_assert((condition), message)
#define GEM_CONTEXT_ALIGNOF(type) alignof(type)
#else
#define GEM_CONTEXT_STATIC_ASSERT(condition, message) _Static_assert((condition), message)
#define GEM_CONTEXT_ALIGNOF(type) _Alignof(type)
#endif

GEM_CONTEXT_STATIC_ASSERT(sizeof(uint8_t) == 1, "uint8_t must be 1 byte");
GEM_CONTEXT_STATIC_ASSERT(sizeof(uint16_t) == 2, "uint16_t must be 2 bytes");
GEM_CONTEXT_STATIC_ASSERT(sizeof(uint32_t) == 4, "uint32_t must be 4 bytes");
GEM_CONTEXT_STATIC_ASSERT(sizeof(uint64_t) == 8, "uint64_t must be 8 bytes");
GEM_CONTEXT_STATIC_ASSERT(sizeof(void *) == 8, "GEM context ABI requires a 64-bit host ABI");
GEM_CONTEXT_STATIC_ASSERT(GEM_CONTEXT_LAYOUT_VERSION == 1, "unexpected GEM context layout version");
GEM_CONTEXT_STATIC_ASSERT(GEM_CONTEXT_SERIALIZATION_VERSION == 1,
                          "unexpected GEM context serialization version");
GEM_CONTEXT_STATIC_ASSERT(sizeof(struct gem_u128) == 16, "gem_u128 must remain 16 bytes");
GEM_CONTEXT_STATIC_ASSERT(GEM_CONTEXT_ALIGNOF(struct gem_u128) == 8,
                          "gem_u128 alignment must remain 8 bytes");
GEM_CONTEXT_STATIC_ASSERT(sizeof(struct gem_thread_context) == GEM_THREAD_CONTEXT_SIZE_V1,
                          "gem_thread_context v1 size changed");
GEM_CONTEXT_STATIC_ASSERT(GEM_CONTEXT_ALIGNOF(struct gem_thread_context) ==
                              GEM_THREAD_CONTEXT_ALIGNMENT_V1,
                          "gem_thread_context v1 alignment changed");
GEM_CONTEXT_STATIC_ASSERT(offsetof(struct gem_thread_context, layout_version) == 0,
                          "layout_version offset changed");
GEM_CONTEXT_STATIC_ASSERT(offsetof(struct gem_thread_context, context_size) == 4,
                          "context_size offset changed");
GEM_CONTEXT_STATIC_ASSERT(offsetof(struct gem_thread_context, x) == 8, "x offset changed");
GEM_CONTEXT_STATIC_ASSERT(offsetof(struct gem_thread_context, sp) == 256, "sp offset changed");
GEM_CONTEXT_STATIC_ASSERT(offsetof(struct gem_thread_context, pc) == 264, "pc offset changed");
GEM_CONTEXT_STATIC_ASSERT(offsetof(struct gem_thread_context, nzcv) == 272, "nzcv offset changed");
GEM_CONTEXT_STATIC_ASSERT(offsetof(struct gem_thread_context, fpcr) == 276, "fpcr offset changed");
GEM_CONTEXT_STATIC_ASSERT(offsetof(struct gem_thread_context, fpsr) == 280, "fpsr offset changed");
GEM_CONTEXT_STATIC_ASSERT(offsetof(struct gem_thread_context, reserved0) == 284,
                          "reserved0 offset changed");
GEM_CONTEXT_STATIC_ASSERT(offsetof(struct gem_thread_context, v) == 288, "v offset changed");
GEM_CONTEXT_STATIC_ASSERT(offsetof(struct gem_thread_context, x64_rflags) == 544,
                          "x64_rflags offset changed");
GEM_CONTEXT_STATIC_ASSERT(offsetof(struct gem_thread_context, x64_mxcsr) == 552,
                          "x64_mxcsr offset changed");
GEM_CONTEXT_STATIC_ASSERT(offsetof(struct gem_thread_context, x64_fcw) == 556,
                          "x64_fcw offset changed");
GEM_CONTEXT_STATIC_ASSERT(offsetof(struct gem_thread_context, x64_fsw) == 558,
                          "x64_fsw offset changed");
GEM_CONTEXT_STATIC_ASSERT(offsetof(struct gem_thread_context, x87) == 560, "x87 offset changed");
GEM_CONTEXT_STATIC_ASSERT(offsetof(struct gem_thread_context, teb) == 688, "teb offset changed");
GEM_CONTEXT_STATIC_ASSERT(offsetof(struct gem_thread_context, original_x64_sp) == 696,
                          "original_x64_sp offset changed");
GEM_CONTEXT_STATIC_ASSERT(offsetof(struct gem_thread_context, transition_cookie) == 704,
                          "transition_cookie offset changed");
GEM_CONTEXT_STATIC_ASSERT(offsetof(struct gem_thread_context, isa) == 712, "isa offset changed");
GEM_CONTEXT_STATIC_ASSERT(offsetof(struct gem_thread_context, stop_reason) == 716,
                          "stop_reason offset changed");
GEM_CONTEXT_STATIC_ASSERT(GEM_ISA_INVALID == 0, "GEM_ISA_INVALID value changed");
GEM_CONTEXT_STATIC_ASSERT(GEM_ISA_ARM64EC == 1, "GEM_ISA_ARM64EC value changed");
GEM_CONTEXT_STATIC_ASSERT(GEM_ISA_X64 == 2, "GEM_ISA_X64 value changed");
GEM_CONTEXT_STATIC_ASSERT(GEM_STOP_NONE == 0, "GEM_STOP_NONE value changed");
GEM_CONTEXT_STATIC_ASSERT(GEM_STOP_SYSCALL == 1, "GEM_STOP_SYSCALL value changed");
GEM_CONTEXT_STATIC_ASSERT(GEM_STOP_ARCH_TRANSITION == 2, "GEM_STOP_ARCH_TRANSITION value changed");
GEM_CONTEXT_STATIC_ASSERT(GEM_STOP_HOST_RETURN == 3, "GEM_STOP_HOST_RETURN value changed");
GEM_CONTEXT_STATIC_ASSERT(GEM_STOP_MEMORY_FAULT == 4, "GEM_STOP_MEMORY_FAULT value changed");
GEM_CONTEXT_STATIC_ASSERT(GEM_STOP_WINDOWS_EXCEPTION == 5,
                          "GEM_STOP_WINDOWS_EXCEPTION value changed");
GEM_CONTEXT_STATIC_ASSERT(GEM_STOP_ASYNC_REQUEST == 6, "GEM_STOP_ASYNC_REQUEST value changed");
GEM_CONTEXT_STATIC_ASSERT(GEM_STOP_UNSUPPORTED_INSTRUCTION == 7,
                          "GEM_STOP_UNSUPPORTED_INSTRUCTION value changed");
GEM_CONTEXT_STATIC_ASSERT(GEM_STOP_BUDGET_EXPIRED == 8, "GEM_STOP_BUDGET_EXPIRED value changed");
GEM_CONTEXT_STATIC_ASSERT(GEM_STOP_INVARIANT_VIOLATION == 9,
                          "GEM_STOP_INVARIANT_VIOLATION value changed");

#undef GEM_CONTEXT_ALIGNOF
#undef GEM_CONTEXT_STATIC_ASSERT

void gem_context_initialize(struct gem_thread_context *context, uint64_t teb, enum gem_isa isa);
bool gem_context_is_valid(const struct gem_thread_context *context);
const char *gem_stop_reason_name(enum gem_stop_reason reason);

#ifdef __cplusplus
}
#endif

#endif
