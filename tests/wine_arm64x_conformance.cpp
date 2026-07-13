// SPDX-License-Identifier: Apache-2.0
#include "metalsharp/gem/pe_arm64x.h"
#include "metalsharp/gem/wine_bridge.h"

#include <mach/mach.h>
#include <mach/mach_vm.h>
#include <sys/mman.h>
#include <unistd.h>

#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <vector>

#ifndef MAP_ANONYMOUS
#define MAP_ANONYMOUS MAP_ANON
#endif

namespace {
constexpr std::uint64_t kPreferredBase = UINT64_C(0x180000000);
constexpr std::uint64_t kChecker = UINT64_C(0xfffffffffffffd00);
constexpr std::uint64_t kDispatchCall = UINT64_C(0xfffffffffffffc00);
constexpr std::uint64_t kDispatchRet = UINT64_C(0xfffffffffffffb00);
constexpr std::uint64_t kHostReturn = UINT64_C(0xfffffffffffffff0);
constexpr std::uint64_t kTeb = UINT64_C(0x700000010000);
constexpr std::uint32_t kDirectRva = UINT32_C(0x5070);
constexpr std::uint32_t kNestedRva = UINT32_C(0x5040);
constexpr std::uint32_t kCheckerSlotRva = UINT32_C(0x7020);
constexpr std::uint32_t kDirectCheckerSlotRva = UINT32_C(0x7010);
constexpr std::uint32_t kDispatchCallSlotRva = UINT32_C(0x7000);
constexpr std::uint32_t kDispatchRetSlotRva = UINT32_C(0x7008);
constexpr std::uint32_t kMovX2X30 = UINT32_C(0xaa1e03e2);
constexpr std::uint32_t kBlrX1 = UINT32_C(0xd63f0020);
constexpr std::uint32_t kMovX30X2 = UINT32_C(0xaa0203fe);
constexpr std::uint32_t kRet = UINT32_C(0xd65f03c0);

std::vector<std::uint8_t> read_file(const char *path) {
    std::ifstream stream(path, std::ios::binary);
    assert(stream);
    return {std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
}

void write_u64(std::uint8_t *base, std::uint32_t rva, std::uint64_t value) {
    std::memcpy(base + rva, &value, sizeof(value));
}

std::uint16_t read_u16(const std::uint8_t *address) {
    std::uint16_t value;
    std::memcpy(&value, address, sizeof(value));
    return value;
}

std::uint32_t read_u32(const std::uint8_t *address) {
    std::uint32_t value;
    std::memcpy(&value, address, sizeof(value));
    return value;
}

void relocate_image(std::uint8_t *image, std::uint32_t image_size, std::uint64_t loaded_base) {
    const auto pe_offset = read_u32(image + UINT32_C(0x3c));
    const auto optional = pe_offset + UINT32_C(24);
    const auto relocation_directory = optional + UINT32_C(112) + UINT32_C(5 * 8);
    const auto relocation_rva = read_u32(image + relocation_directory);
    const auto relocation_size = read_u32(image + relocation_directory + sizeof(std::uint32_t));
    std::uint32_t offset = 0U;
    assert(relocation_rva < image_size && relocation_size <= image_size - relocation_rva);
    while (offset < relocation_size) {
        const auto page_rva = read_u32(image + relocation_rva + offset);
        const auto block_size = read_u32(image + relocation_rva + offset + sizeof(std::uint32_t));
        assert(block_size >= 8U && block_size <= relocation_size - offset &&
               (block_size & 1U) == 0U);
        for (std::uint32_t entry_offset = 8U; entry_offset < block_size;
             entry_offset += sizeof(std::uint16_t)) {
            const auto entry = read_u16(image + relocation_rva + offset + entry_offset);
            const auto type = static_cast<std::uint32_t>(entry >> 12U);
            const auto target_rva = page_rva + (entry & UINT16_C(0x0fff));
            if (type == 0U)
                continue;
            assert(type == 10U && target_rva <= image_size - sizeof(std::uint64_t));
            std::uint64_t value;
            std::memcpy(&value, image + target_rva, sizeof(value));
            value += loaded_base - kPreferredBase;
            std::memcpy(image + target_rva, &value, sizeof(value));
        }
        offset += block_size;
    }
    assert(offset == relocation_size);
}

void initialize_context(gem_thread_context &context, std::uint64_t native_pc, std::uint64_t target,
                        std::uint64_t stack) {
    gem_context_initialize(&context, kTeb, GEM_ISA_ARM64EC);
    context.pc = native_pc;
    context.sp = stack;
    context.x[0] = 10U;
    context.x[1] = target;
    context.x[30] = kHostReturn;
}
} // namespace

int main(int argc, char **argv) {
    gem_pe_arm64x_image *metadata = nullptr;
    gem_pe_arm64x_summary summary{};
    gem_wine_process *process = nullptr;
    gem_wine_thread *thread = nullptr;
    const auto file = read_file(argc == 2 ? argv[1] : "");
    assert(gem_pe_arm64x_parse(file.data(), file.size(), nullptr, &metadata) == GEM_PE_OK);
    assert(gem_pe_arm64x_get_summary(metadata, &summary) == GEM_PE_OK);
    assert(summary.image_base == kPreferredBase && summary.size_of_image != 0U);

    mach_vm_address_t image_address = 0U;
    assert(mach_vm_allocate(mach_task_self(), &image_address, summary.size_of_image,
                            VM_FLAGS_ANYWHERE) == KERN_SUCCESS);
    auto *image = reinterpret_cast<std::uint8_t *>(static_cast<std::uintptr_t>(image_address));
    std::memset(image, 0, summary.size_of_image);
    assert(summary.size_of_headers <= file.size());
    std::memcpy(image, file.data(), summary.size_of_headers);
    for (std::size_t index = 0; index < gem_pe_arm64x_section_count(metadata); ++index) {
        gem_pe_arm64x_section section{};
        std::size_t offset = 0U;
        assert(gem_pe_arm64x_get_section(metadata, index, &section) == GEM_PE_OK);
        if (section.raw_size == 0U)
            continue;
        assert(gem_pe_arm64x_rva_to_file_offset(metadata, section.virtual_address, section.raw_size,
                                                &offset) == GEM_PE_OK);
        assert(offset <= file.size() && section.raw_size <= file.size() - offset);
        std::memcpy(image + section.virtual_address, file.data() + offset, section.raw_size);
    }
    relocate_image(image, summary.size_of_image, image_address);
    write_u64(image, kCheckerSlotRva, kChecker);
    write_u64(image, kDirectCheckerSlotRva, kChecker);
    write_u64(image, kDispatchCallSlotRva, kDispatchCall);
    write_u64(image, kDispatchRetSlotRva, kDispatchRet);

    const long host_page_long = sysconf(_SC_PAGESIZE);
    assert(host_page_long > 0);
    const auto host_page = static_cast<std::size_t>(host_page_long);
    auto *native = static_cast<std::uint8_t *>(
        mmap(nullptr, host_page, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
    auto *stack = static_cast<std::uint8_t *>(
        mmap(nullptr, host_page, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
    assert(native != MAP_FAILED && stack != MAP_FAILED);
    std::memcpy(native, &kMovX2X30, sizeof(kMovX2X30));
    std::memcpy(native + 4U, &kBlrX1, sizeof(kBlrX1));
    std::memcpy(native + 8U, &kMovX30X2, sizeof(kMovX30X2));
    std::memcpy(native + 12U, &kRet, sizeof(kRet));

    gem_wine_process_config process_config{};
    process_config.version = GEM_WINE_PROCESS_CONFIG_VERSION;
    process_config.struct_size = sizeof(process_config);
    process_config.segment_instruction_budget = 10000U;
    process_config.total_instruction_budget = 100000U;
    process_config.max_boundary_callbacks = 64U;
    process_config.host_return_sentinel = kHostReturn;
    assert(gem_wine_process_create(&process_config, &process) == GEM_WINE_OK);
    assert(gem_wine_process_reserve(process, image_address, summary.size_of_image) == GEM_WINE_OK);
    assert(gem_wine_process_commit_identity(process, image_address, image, summary.size_of_image,
                                            GEM_WINE_PAGE_EXECUTE_READWRITE) == GEM_WINE_OK);
    assert(gem_wine_process_reserve(process, reinterpret_cast<std::uintptr_t>(native), host_page) ==
           GEM_WINE_OK);
    assert(gem_wine_process_commit_identity(process, reinterpret_cast<std::uintptr_t>(native),
                                            native, host_page,
                                            GEM_WINE_PAGE_EXECUTE_READ) == GEM_WINE_OK);
    assert(gem_wine_process_reserve(process, reinterpret_cast<std::uintptr_t>(stack), host_page) ==
           GEM_WINE_OK);
    assert(gem_wine_process_commit_identity(process, reinterpret_cast<std::uintptr_t>(stack), stack,
                                            host_page, GEM_WINE_PAGE_READWRITE) == GEM_WINE_OK);

    gem_wine_arm64x_config arm64x{};
    arm64x.version = GEM_WINE_ARM64X_CONFIG_VERSION;
    arm64x.struct_size = sizeof(arm64x);
    arm64x.loaded_base = image_address;
    arm64x.image_size = summary.size_of_image;
    arm64x.checker_helper = kChecker;
    arm64x.dispatch_call_helper = kDispatchCall;
    arm64x.dispatch_ret_helper = kDispatchRet;
    assert(gem_wine_process_register_arm64x_mapped(process, &arm64x) == GEM_WINE_OK);
    assert(gem_wine_process_register_arm64x_mapped(process, &arm64x) == GEM_WINE_OK);

    gem_wine_thread_config thread_config{};
    thread_config.version = GEM_WINE_THREAD_CONFIG_VERSION;
    thread_config.struct_size = sizeof(thread_config);
    thread_config.teb = kTeb;
    assert(gem_wine_thread_create(process, &thread_config, &thread) == GEM_WINE_OK);

    struct Case {
        std::uint32_t rva;
        std::uint64_t expected;
    };
    for (const Case test : {Case{kDirectRva, 47U}, Case{kNestedRva, 85U}}) {
        gem_thread_context input{};
        gem_thread_context output{};
        gem_wine_run_result result{};
        initialize_context(input, reinterpret_cast<std::uintptr_t>(native),
                           image_address + test.rva,
                           reinterpret_cast<std::uintptr_t>(stack) + host_page - UINT64_C(0x100));
        const auto status = gem_wine_thread_run(thread, &input, &output, &result);
        if (status != GEM_WINE_OK)
            std::fprintf(stderr,
                         "bridge arm64x rva=%x status=%u outcome=%u stop=%u event=%u pc=%llx "
                         "fault=%llx engine=%u retired=%llu\n",
                         test.rva, static_cast<unsigned>(status), result.outcome,
                         result.stop_reason, result.last_event,
                         static_cast<unsigned long long>(output.pc),
                         static_cast<unsigned long long>(result.stop.fault_address),
                         result.stop.engine_status,
                         static_cast<unsigned long long>(result.instructions_retired));
        assert(status == GEM_WINE_OK);
        assert(result.outcome == GEM_WINE_RUN_COMPLETE && result.boundary_callbacks == 0U &&
               result.instructions_retired != 0U && output.pc == kHostReturn &&
               output.isa == GEM_ISA_ARM64EC && output.transition_cookie == 0U &&
               output.x[0] == test.expected);
    }

    assert(gem_wine_thread_destroy(thread) == GEM_WINE_OK);
    assert(gem_wine_process_destroy(process) == GEM_WINE_OK);
    gem_pe_arm64x_image_destroy(metadata);
    assert(munmap(native, host_page) == 0 && munmap(stack, host_page) == 0);
    assert(mach_vm_deallocate(mach_task_self(), image_address, summary.size_of_image) ==
           KERN_SUCCESS);
    return 0;
}
