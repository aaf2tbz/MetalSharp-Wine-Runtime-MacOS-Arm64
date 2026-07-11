# SPDX-License-Identifier: Apache-2.0
# Dynarmic 6.7.0's native ARM64 callback trampolines assume that a 16-byte
# aggregate returned by a C++ member function is returned in x0/x1. MSVC's
# Windows ARM64 ABI instead passes a result-buffer pointer in x1 and shifts the
# first explicit member-function argument to x2. Adapt only the three Vector
# read trampolines; scalar and Vector write callbacks already match MSVC's ABI.

if(NOT DEFINED DYNARMIC_SOURCE_DIR)
    message(FATAL_ERROR "DYNARMIC_SOURCE_DIR is required")
endif()

set(source "${DYNARMIC_SOURCE_DIR}/src/dynarmic/backend/arm64/a64_address_space.cpp")
file(READ "${source}" contents)

set(normal_old [=[    code.LDR(X0, l_this);
    code.LDR(Xscratch0, l_addr);
    code.BLR(Xscratch0);
    code.FMOV(D0, X0);
    code.FMOV(V0.D()[1], X1);
    ABI_PopRegisters(code, (1ull << 29) | (1ull << 30), 0);
]=])
set(normal_new [=[    code.LDR(X0, l_this);
#if defined(_WIN32) && defined(_MSC_VER)
    code.MOV(X2, X1);
    code.SUB(SP, SP, 16);
    code.MOV(X1, SP);
#endif
    code.LDR(Xscratch0, l_addr);
    code.BLR(Xscratch0);
#if defined(_WIN32) && defined(_MSC_VER)
    code.LDP(X0, X1, SP, 0);
    code.ADD(SP, SP, 16);
#else
    code.FMOV(D0, X0);
    code.FMOV(V0.D()[1], X1);
#endif
    ABI_PopRegisters(code, (1ull << 29) | (1ull << 30), 0);
]=])
set(wrapped_old [=[    code.LDR(X0, l_this);
    code.MOV(X1, Xscratch0);
    code.LDR(Xscratch0, l_addr);
    code.BLR(Xscratch0);
    code.FMOV(D0, X0);
    code.FMOV(V0.D()[1], X1);
    ABI_PopRegisters(code, save_regs, 0);
]=])
set(wrapped_new [=[    code.LDR(X0, l_this);
#if defined(_WIN32) && defined(_MSC_VER)
    code.MOV(X2, Xscratch0);
    code.SUB(SP, SP, 16);
    code.MOV(X1, SP);
#else
    code.MOV(X1, Xscratch0);
#endif
    code.LDR(Xscratch0, l_addr);
    code.BLR(Xscratch0);
#if defined(_WIN32) && defined(_MSC_VER)
    code.LDP(X0, X1, SP, 0);
    code.ADD(SP, SP, 16);
#else
    code.FMOV(D0, X0);
    code.FMOV(V0.D()[1], X1);
#endif
    ABI_PopRegisters(code, save_regs, 0);
]=])

string(FIND "${contents}" "${normal_old}" normal_offset)
string(FIND "${contents}" "${wrapped_old}" wrapped_offset)
string(FIND "${contents}" "${normal_new}" normal_new_offset)
string(FIND "${contents}" "${wrapped_new}" wrapped_new_offset)
if(normal_offset EQUAL -1 AND wrapped_offset EQUAL -1 AND
   NOT normal_new_offset EQUAL -1 AND NOT wrapped_new_offset EQUAL -1)
    return()
endif()
if(normal_offset EQUAL -1 OR wrapped_offset EQUAL -1 OR
   NOT normal_new_offset EQUAL -1 OR NOT wrapped_new_offset EQUAL -1)
    message(FATAL_ERROR "pinned Dynarmic ARM64 callback trampoline text changed or is partially patched")
endif()

string(REPLACE "${normal_old}" "${normal_new}" contents "${contents}")
string(REPLACE "${wrapped_old}" "${wrapped_new}" contents "${contents}")
string(FIND "${contents}" "${normal_old}" remaining_normal_offset)
string(FIND "${contents}" "${wrapped_old}" remaining_wrapped_offset)
if(NOT remaining_normal_offset EQUAL -1 OR NOT remaining_wrapped_offset EQUAL -1)
    message(FATAL_ERROR "failed to patch all Dynarmic ARM64 Vector read trampolines")
endif()
file(WRITE "${source}" "${contents}")
