// SPDX-License-Identifier: Apache-2.0
#include "metalsharp/gem/arm64ec_target.h"
#include "metalsharp/gem/context.h"
#include "metalsharp/gem/hybrid_runtime.h"
#include "metalsharp/gem/pe_arm64x_loader.h"

#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace {
constexpr std::uint64_t kStackBase = UINT64_C(0x0000000400000000);
constexpr std::uint64_t kStackSize = UINT64_C(0x10000);
constexpr std::uint64_t kWriteCopyStackAlias = UINT64_C(0x0000000500000000);
constexpr std::uint64_t kChecker = UINT64_C(0xfffffffffffffd00);
constexpr std::uint64_t kDispatchCall = UINT64_C(0xfffffffffffffc00);
constexpr std::uint64_t kDispatchRet = UINT64_C(0xfffffffffffffb00);
constexpr std::uint64_t kX64Return = UINT64_C(0xfffffffffffffa00);
constexpr std::uint64_t kHostReturn = UINT64_C(0xfffffffffffff900);

std::vector<std::uint8_t> read_binary(const char *path) {
    std::ifstream input(path, std::ios::binary);
    assert(input);
    input.seekg(0, std::ios::end);
    const auto size = input.tellg();
    assert(size > 0 && size <= 64 * 1024 * 1024);
    input.seekg(0, std::ios::beg);
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(size));
    assert(input.read(reinterpret_cast<char *>(bytes.data()), size));
    return bytes;
}

std::map<std::string, std::uint32_t> read_map(const char *path) {
    std::ifstream input(path);
    std::string header;
    std::map<std::string, std::uint32_t> result;
    assert(input && std::getline(input, header) && header == "MSWR_ARM64EC_ENTRY_MAP_V3");
    std::string name;
    std::string value;
    while (input >> name >> value) {
        assert(value.size() == 8U && result.find(name) == result.end());
        std::size_t consumed = 0U;
        const auto parsed = std::stoul(value, &consumed, 16);
        assert(consumed == value.size() && parsed <= UINT32_MAX);
        result.emplace(name, static_cast<std::uint32_t>(parsed));
    }
    assert(input.eof());
    const std::array<const char *, 15> required = {
        "integer",      "floating",         "aggregate",      "variadic",
        "roundtrip",    "finish",           "direct",         "callbackResume",
        "tailTransfer", "boundedNested",    "armCallback",    "armNestedCallback",
        "checkerSlot",  "dispatchCallSlot", "dispatchRetSlot"};
    assert(result.size() == required.size());
    for (const auto *key : required)
        assert(result.find(key) != result.end());
    return result;
}

struct Harness {
    Harness() = default;
    Harness(const Harness &) = delete;
    Harness &operator=(const Harness &) = delete;
    Harness(Harness &&other) noexcept
        : memory(other.memory), materialized(other.materialized), map(other.map),
          caller(other.caller), finish(other.finish) {
        other.memory = nullptr;
        other.materialized = nullptr;
        other.map = nullptr;
    }

    gem_memory *memory{};
    gem_pe_arm64x_materialized_image *materialized{};
    gem_arm64ec_target_map *map{};
    std::uint64_t caller{};
    std::uint64_t finish{};

    ~Harness() {
        gem_arm64ec_target_map_destroy(map);
        gem_pe_arm64x_materialized_image_destroy(materialized);
        gem_memory_destroy(memory);
    }
};

Harness make_harness(const std::vector<std::uint8_t> &bytes,
                     const std::map<std::string, std::uint32_t> &entries) {
    Harness harness;
    harness.memory = gem_memory_create();
    assert(harness.memory);
    const std::array<gem_pe_arm64x_binding, 3> bindings{{
        {entries.at("checkerSlot"), kChecker},
        {entries.at("dispatchCallSlot"), kDispatchCall},
        {entries.at("dispatchRetSlot"), kDispatchRet},
    }};
    gem_pe_arm64x_materialize_options options{};
    options.version = GEM_PE_ARM64X_MATERIALIZE_OPTIONS_VERSION;
    options.image_base = UINT64_C(0x180000000);
    options.bindings = bindings.data();
    options.binding_count = bindings.size();
    const auto materialize_status = gem_pe_arm64x_materialize_preferred(
        harness.memory, bytes.data(), bytes.size(), &options, &harness.materialized);
    if (materialize_status != GEM_PE_MATERIALIZE_OK)
        std::fprintf(stderr, "materialize failed: %s\n",
                     gem_pe_materialize_status_name(materialize_status));
    assert(materialize_status == GEM_PE_MATERIALIZE_OK);
    const auto *metadata = gem_pe_arm64x_materialized_metadata(harness.materialized);
    assert(metadata);
    assert(gem_arm64ec_target_map_create(metadata, options.image_base, &harness.map) ==
           GEM_ARM64EC_TARGET_OK);
    gem_arm64ec_target_result target{};
    assert(gem_arm64ec_target_resolve(harness.map, options.image_base + entries.at("roundtrip"),
                                      &target) == GEM_ARM64EC_TARGET_OK);
    assert(target.kind == GEM_ARM64EC_TARGET_ARM64EC && target.redirection_hops == 1U);
    harness.caller = target.resolved_va;
    assert(gem_arm64ec_target_resolve(harness.map, options.image_base + entries.at("finish"),
                                      &target) == GEM_ARM64EC_TARGET_OK);
    assert(target.kind == GEM_ARM64EC_TARGET_ARM64EC && target.redirection_hops == 1U);
    harness.finish = target.resolved_va;
    std::uint64_t stack = kStackBase;
    assert(gem_memory_reserve(harness.memory, &stack, kStackSize) == GEM_MEMORY_OK);
    assert(gem_memory_commit(harness.memory, stack, kStackSize, GEM_PAGE_READWRITE) ==
           GEM_MEMORY_OK);
    return harness;
}

gem_hybrid_runtime_config runtime_config() {
    gem_hybrid_runtime_config config{};
    config.version = GEM_HYBRID_RUNTIME_CONFIG_VERSION;
    config.loaded_base = UINT64_C(0x180000000);
    config.checker_helper = kChecker;
    config.dispatch_call_helper = kDispatchCall;
    config.dispatch_ret_helper = kDispatchRet;
    config.x64_return_sentinel = kX64Return;
    config.host_return_sentinel = kHostReturn;
    config.max_budget = 10000U;
    return config;
}

gem_thread_context initial_context(std::uint64_t caller) {
    gem_thread_context context{};
    gem_context_initialize(&context, UINT64_C(0x70000000), GEM_ISA_ARM64EC);
    context.pc = caller;
    context.sp = kStackBase + kStackSize - UINT64_C(0x100);
    context.x[30] = kHostReturn;
    context.x[0] = 12U;
    for (unsigned index = 0; index < 16U; ++index) {
        context.v[index].lo = UINT64_C(0x1000000000000000) + index;
        context.v[index].hi = UINT64_C(0x2000000000000000) + index;
    }
    for (unsigned index = 0; index < 8U; ++index) {
        context.x87[index].lo = UINT64_C(0x3000000000000000) + index;
        context.x87[index].hi = UINT64_C(0x4000000000000000) + index;
    }
    return context;
}
/* Authentic ARM64X first-boundary transitions captured against the validated
 * issue14-5 evidence build. The expected stop RVA per entry prevents the
 * evidence from drifting across rebuilds; any future deviation fails the probe
 * closed. */
struct PhaseBPath {
    const char *entry_name;
    std::uint32_t expected_stop_rva;
};
constexpr std::array<PhaseBPath, 4> kPhaseBPaths = {{
    {"direct", UINT32_C(0x4080)},
    {"callbackResume", UINT32_C(0x4020)},
    {"tailTransfer", UINT32_C(0x4090)},
    {"boundedNested", UINT32_C(0x4060)},
}};
constexpr std::uint64_t kPhaseBLoadedBase = UINT64_C(0x180000000);
constexpr std::uint64_t kPhaseBExpectedRetired = UINT64_C(17);

const char *arm64ec_target_kind_label(enum gem_arm64ec_target_kind kind) {
    switch (kind) {
    case GEM_ARM64EC_TARGET_ARM64:
        return "arm64";
    case GEM_ARM64EC_TARGET_ARM64EC:
        return "arm64ec";
    case GEM_ARM64EC_TARGET_X64_BOUNDARY:
        return "x64-boundary";
    }
    return "invalid";
}

std::string hex_u64(std::uint64_t value, std::size_t width = 16) {
    std::ostringstream os;
    os << "0x" << std::uppercase << std::hex << std::setw(static_cast<int>(width))
       << std::setfill('0') << value;
    return os.str();
}

std::string hex_u64_quoted(std::uint64_t value, std::size_t width = 16) {
    std::ostringstream os;
    os << '"' << "0x" << std::uppercase << std::hex << std::setw(static_cast<int>(width))
       << std::setfill('0') << value << '"';
    return os.str();
}

std::string dec_u64(std::uint64_t value) {
    std::ostringstream os;
    os << std::dec << value;
    return os.str();
}

enum gem_arm64ec_boundary_action trace_boundary(void *, std::uint64_t pc, gem_thread_context *,
                                                gem_arm64ec_boundary_kind *kind) {
    if (pc == kChecker)
        *kind = GEM_ARM64EC_BOUNDARY_CHECK_ICALL;
    else if (pc == kDispatchCall)
        *kind = GEM_ARM64EC_BOUNDARY_DISPATCH_CALL;
    else if (pc == kDispatchRet)
        *kind = GEM_ARM64EC_BOUNDARY_DISPATCH_RETURN;
    else
        return GEM_ARM64EC_BOUNDARY_NOT_HANDLED;
    return GEM_ARM64EC_BOUNDARY_STOP;
}

std::uint64_t fnv1a(const std::uint8_t *data, std::size_t size) {
    std::uint64_t hash = UINT64_C(14695981039346656037);
    for (std::size_t i = 0; i < size; ++i) {
        hash ^= data[i];
        hash *= UINT64_C(1099511628211);
    }
    return hash;
}

void write_phase_b_trace(const char *path, Harness &harness,
                         const std::map<std::string, std::uint32_t> &entries,
                         const gem_pe_arm64x_image *metadata) {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    assert(out);
    out << "{\n  \"schema\":\"mswr.phase-b.execution-evidence.v1\",\n  \"paths\":[\n";
    /* Hash the entire reserved guest stack region (a single fixed checked range)
     * rather than a window relative to context.sp, which may legitimately move
     * across runs and would otherwise sample different pages. */
    std::vector<std::uint8_t> stack_before(kStackSize);
    std::vector<std::uint8_t> stack_after(kStackSize);
    for (std::size_t i = 0; i < kPhaseBPaths.size(); ++i) {
        const auto *entry_name = kPhaseBPaths[i].entry_name;
        const auto expected_stop_rva = kPhaseBPaths[i].expected_stop_rva;
        const auto expected_stop_va = kPhaseBLoadedBase + expected_stop_rva;

        gem_arm64ec_target_result target{};
        const auto requested_va = kPhaseBLoadedBase + entries.at(entry_name);
        assert(gem_arm64ec_target_resolve(harness.map, requested_va, &target) ==
               GEM_ARM64EC_TARGET_OK);
        /* Preserve requested and resolved classifications distinctly so a future
         * misclassification of either side fails loudly rather than silently
         * passing through the other. */
        gem_pe_arm64x_rva_info requested_info{};
        gem_pe_arm64x_rva_info resolved_info{};
        assert(gem_pe_arm64x_classify_rva(metadata, target.requested_rva, &requested_info) ==
               GEM_PE_OK);
        assert(gem_pe_arm64x_classify_rva(metadata, target.resolved_rva, &resolved_info) ==
               GEM_PE_OK);
        assert(target.kind == GEM_ARM64EC_TARGET_ARM64EC);
        assert(arm64ec_target_kind_label(target.kind) ==
               std::string(gem_pe_rva_class_name(resolved_info.classification)));
        const auto resolved_va = target.resolved_va;

        gem_arm64ec_runtime_config config{};
        config.host_return_sentinel = kHostReturn;
        config.max_budget = 10000U;
        auto *runtime = gem_arm64ec_runtime_create(harness.memory, &config);
        assert(runtime && gem_arm64ec_runtime_attach_arm64x(runtime, metadata, kPhaseBLoadedBase));
        assert(gem_arm64ec_runtime_set_boundary_broker(runtime, trace_boundary, nullptr));
        auto context = initial_context(resolved_va);
        const auto initial_sp = context.sp;
        assert(gem_memory_read(harness.memory, kStackBase, stack_before.data(), kStackSize) ==
               GEM_MEMORY_OK);
        const auto reason = gem_arm64ec_runtime_run(runtime, &context, 10000U);
        gem_arm64ec_stop_info stop{};
        assert(gem_arm64ec_runtime_last_stop_info(runtime, &stop));
        assert(stop.reason == reason && context.x[18] == context.teb &&
               gem_context_is_valid(&context));
        const auto final_sp = context.sp;
        assert(gem_memory_read(harness.memory, kStackBase, stack_after.data(), kStackSize) ==
               GEM_MEMORY_OK);

        /* Drift-detecting assertions against the authentic evidence. Each first
         * ARM64X architecture transition must land at the recorded x64 PC,
         * retire exactly 17 instructions, and the broker must stop before the
         * engine commits any x64 fetch. */
        assert(reason == GEM_STOP_ARCH_TRANSITION);
        const auto stop_va = context.pc;
        const auto stop_rva = static_cast<std::uint32_t>(stop_va - kPhaseBLoadedBase);
        gem_pe_arm64x_rva_info stop_info{};
        assert(gem_pe_arm64x_classify_rva(metadata, stop_rva, &stop_info) == GEM_PE_OK);
        assert(stop_info.classification == GEM_PE_RVA_X64);
        assert(stop_va == expected_stop_va);
        assert(stop_rva == expected_stop_rva);
        assert(stop.instructions_retired == kPhaseBExpectedRetired);

        /* Deterministic, allocation-free-of-fixed-sizes JSON via std::ostringstream.
         * Field order is fixed; numeric bases and widths are fixed. */
        std::ostringstream os;
        os << "    {\"path\":\"" << entry_name << '"';
        os << ",\"requested\":" << hex_u64_quoted(requested_va);
        os << ",\"requestedClassification\":\""
           << gem_pe_rva_class_name(requested_info.classification) << '"';
        os << ",\"resolved\":" << hex_u64_quoted(resolved_va);
        os << ",\"resolvedClassification\":\""
           << gem_pe_rva_class_name(resolved_info.classification) << '"';
        os << ",\"stop\":" << hex_u64_quoted(stop_va);
        os << ",\"stopRva\":\"" << hex_u64(stop_rva, 8) << '"';
        os << ",\"stopClassification\":\"" << gem_pe_rva_class_name(stop_info.classification)
           << '"';
        os << ",\"expectedStop\":" << hex_u64_quoted(expected_stop_va);
        os << ",\"expectedStopRva\":\"" << hex_u64(expected_stop_rva, 8) << '"';
        os << ",\"reason\":\"" << gem_stop_reason_name(reason) << '"';
        os << ",\"pc\":" << hex_u64_quoted(context.pc);
        os << ",\"sp\":" << hex_u64_quoted(context.sp);
        os << ",\"lr\":" << hex_u64_quoted(context.x[30]);
        os << ",\"x0\":" << hex_u64_quoted(context.x[0]);
        os << ",\"x8\":" << hex_u64_quoted(context.x[8]);
        os << ",\"x9\":" << hex_u64_quoted(context.x[9]);
        os << ",\"x10\":" << hex_u64_quoted(context.x[10]);
        os << ",\"x11\":" << hex_u64_quoted(context.x[11]);
        os << ",\"x18\":" << hex_u64_quoted(context.x[18]);
        os << ",\"teb\":" << hex_u64_quoted(context.teb);
        os << ",\"retired\":" << dec_u64(stop.instructions_retired);
        os << ",\"faultAddress\":" << hex_u64_quoted(stop.fault_address);
        os << ",\"access\":" << static_cast<unsigned>(stop.access);
        os << ",\"memoryError\":" << stop.memory_error;
        os << ",\"engineStatus\":" << stop.engine_status;
        os << ",\"stackBase\":" << hex_u64_quoted(kStackBase);
        os << ",\"stackSize\":" << kStackSize;
        os << ",\"stackHashBefore\":\"" << hex_u64(fnv1a(stack_before.data(), kStackSize)) << '"';
        os << ",\"stackHashAfter\":\"" << hex_u64(fnv1a(stack_after.data(), kStackSize)) << '"';
        os << ",\"initialSp\":" << hex_u64_quoted(initial_sp);
        os << ",\"finalSp\":" << hex_u64_quoted(final_sp);
        os << '}';
        if (i + 1 != kPhaseBPaths.size())
            os << ',';
        os << '\n';
        out << os.str();
        assert(out.good());
        gem_arm64ec_runtime_destroy(runtime);
    }
    out << "  ]\n}\n";
    assert(out.good());
}

void assert_stop(gem_hybrid_runtime *runtime, gem_stop_reason reason,
                 gem_hybrid_stop_source source) {
    gem_hybrid_stop_info info{};
    assert(gem_hybrid_runtime_last_stop_info(runtime, &info));
    assert(info.reason == reason && info.source == source);
    if (source == GEM_HYBRID_STOP_SOURCE_ARM64EC)
        assert(info.arm64ec.reason == reason);
    if (source == GEM_HYBRID_STOP_SOURCE_X64)
        assert(info.x64.reason == reason);
}
} // namespace

int main(int argc, char **argv) {
    assert(argc == 3 || argc == 4);
    const auto bytes = read_binary(argv[1]);
    const auto entries = read_map(argv[2]);
    auto harness = make_harness(bytes, entries);
    const auto config = runtime_config();
    const auto *metadata = gem_pe_arm64x_materialized_metadata(harness.materialized);
    gem_thread_context expected_context{};
    std::array<std::uint8_t, GEM_GUEST_PAGE_SIZE> expected_stack{};

    for (unsigned iteration = 0; iteration < 100U; ++iteration) {
        gem_hybrid_runtime *runtime = gem_hybrid_runtime_create(harness.memory, metadata, &config);
        assert(runtime);
        auto context = initial_context(harness.caller);
        const auto initial = context;
        gem_hybrid_roundtrip_stats stats{};
        const auto reason = gem_hybrid_runtime_run_integer_roundtrip(
            runtime, &context, harness.caller, harness.finish, 10000U, &stats);
        if (reason != GEM_STOP_HOST_RETURN)
            std::fprintf(
                stderr,
                "roundtrip failed: %s pc=%llx sp=%llx x0=%llx x8=%llx x9=%llx x18=%llx teb=%llx "
                "arm=%llu x64=%llu checker=%llu call=%llu x2a=%llu desc=%llu ret=%llu "
                "depth=%u\n",
                gem_stop_reason_name(reason), static_cast<unsigned long long>(context.pc),
                static_cast<unsigned long long>(context.sp),
                static_cast<unsigned long long>(context.x[0]),
                static_cast<unsigned long long>(context.x[8]),
                static_cast<unsigned long long>(context.x[9]),
                static_cast<unsigned long long>(context.x[18]),
                static_cast<unsigned long long>(context.teb),
                static_cast<unsigned long long>(stats.arm64ec_instructions_retired),
                static_cast<unsigned long long>(stats.x64_instructions_retired),
                static_cast<unsigned long long>(stats.checker_boundaries),
                static_cast<unsigned long long>(stats.dispatch_call_boundaries),
                static_cast<unsigned long long>(stats.x64_to_arm64ec_boundaries),
                static_cast<unsigned long long>(stats.descriptor_resolutions),
                static_cast<unsigned long long>(stats.dispatch_ret_boundaries),
                stats.final_frame_depth);
        assert(reason == GEM_STOP_HOST_RETURN);
        assert_stop(runtime, reason, GEM_HYBRID_STOP_SOURCE_BROKER);
        assert(context.x[0] == 30U && context.pc == kHostReturn && context.x[18] == context.teb &&
               context.transition_cookie == 0U && context.isa == GEM_ISA_ARM64EC);
        assert(std::memcmp(&context.v[6], &initial.v[6], 10U * sizeof(context.v[0])) == 0);
        assert(std::memcmp(context.x87, initial.x87, sizeof(context.x87)) == 0);
        assert(stats.x64_instructions_retired == 4U && stats.checker_boundaries == 1U &&
               stats.dispatch_call_boundaries == 1U && stats.x64_to_arm64ec_boundaries == 1U &&
               stats.descriptor_resolutions == 1U && stats.dispatch_ret_boundaries == 1U &&
               stats.frame_pushes == 1U && stats.frame_pops == 1U &&
               stats.maximum_frame_depth == 1U && stats.final_frame_depth == 0U);
        std::array<std::uint8_t, GEM_GUEST_PAGE_SIZE> current_stack{};
        assert(gem_memory_read(harness.memory, kStackBase + kStackSize - GEM_GUEST_PAGE_SIZE,
                               current_stack.data(), current_stack.size()) == GEM_MEMORY_OK);
        if (iteration == 0U) {
            expected_context = context;
            expected_stack = current_stack;
        } else {
            assert(std::memcmp(&context, &expected_context, sizeof(context)) == 0);
            assert(current_stack == expected_stack);
        }
        gem_hybrid_runtime_destroy(runtime);
    }

    {
        gem_hybrid_runtime *runtime = gem_hybrid_runtime_create(harness.memory, metadata, &config);
        auto context = initial_context(harness.caller);
        context.transition_cookie = 7U;
        assert(runtime && gem_hybrid_runtime_run_integer_roundtrip(
                              runtime, &context, harness.caller, harness.finish, 10000U, nullptr) ==
                              GEM_STOP_INVARIANT_VIOLATION);
        assert_stop(runtime, GEM_STOP_INVARIANT_VIOLATION, GEM_HYBRID_STOP_SOURCE_BROKER);
        gem_hybrid_runtime_destroy(runtime);
    }
    {
        gem_hybrid_runtime *runtime = gem_hybrid_runtime_create(harness.memory, metadata, &config);
        auto context = initial_context(harness.caller);
        assert(runtime && gem_hybrid_runtime_run_integer_roundtrip(
                              runtime, &context, harness.caller, harness.finish, 1U, nullptr) ==
                              GEM_STOP_BUDGET_EXPIRED);
        assert_stop(runtime, GEM_STOP_BUDGET_EXPIRED, GEM_HYBRID_STOP_SOURCE_ARM64EC);
        gem_hybrid_runtime_destroy(runtime);
    }
    {
        gem_hybrid_runtime *runtime = gem_hybrid_runtime_create(harness.memory, metadata, &config);
        bool reached_post_frame_budget = false;
        assert(runtime);
        for (std::uint64_t budget = 2U; budget < 512U && !reached_post_frame_budget; ++budget) {
            auto context = initial_context(harness.caller);
            const auto initial = context;
            gem_hybrid_roundtrip_stats stats{};
            const auto reason = gem_hybrid_runtime_run_integer_roundtrip(
                runtime, &context, harness.caller, harness.finish, budget, &stats);
            if (reason == GEM_STOP_BUDGET_EXPIRED && stats.frame_pushes == 1U) {
                const auto stop = context.stop_reason;
                context.stop_reason = GEM_STOP_NONE;
                assert(std::memcmp(&context, &initial, sizeof(context)) == 0);
                assert(stop == GEM_STOP_BUDGET_EXPIRED && context.transition_cookie == 0U &&
                       stats.final_frame_depth == 0U);
                gem_hybrid_stop_info info{};
                assert(gem_hybrid_runtime_last_stop_info(runtime, &info));
                assert(info.reason == reason);
                reached_post_frame_budget = true;
            }
        }
        assert(reached_post_frame_budget);
        auto retry = initial_context(harness.caller);
        assert(gem_hybrid_runtime_run_integer_roundtrip(runtime, &retry, harness.caller,
                                                        harness.finish, 10000U,
                                                        nullptr) == GEM_STOP_HOST_RETURN);
        assert_stop(runtime, GEM_STOP_HOST_RETURN, GEM_HYBRID_STOP_SOURCE_BROKER);
        gem_hybrid_runtime_destroy(runtime);
    }
    {
        gem_hybrid_runtime *runtime = gem_hybrid_runtime_create(harness.memory, metadata, &config);
        auto context = initial_context(harness.caller);
        const auto initial = context;
        std::uint64_t stack_before = 0U;
        std::uint64_t stack_after = 0U;
        gem_hybrid_roundtrip_stats stats{};
        assert(gem_memory_read(harness.memory, context.sp - sizeof(stack_before), &stack_before,
                               sizeof(stack_before)) == GEM_MEMORY_OK);
        assert(runtime && gem_hybrid_runtime_run_integer_roundtrip(
                              runtime, &context, harness.caller, harness.caller, 10000U, &stats) ==
                              GEM_STOP_INVARIANT_VIOLATION);
        const auto stop = context.stop_reason;
        context.stop_reason = GEM_STOP_NONE;
        assert(std::memcmp(&context, &initial, sizeof(context)) == 0);
        assert(stop == GEM_STOP_INVARIANT_VIOLATION && stats.frame_pushes == 1U &&
               stats.final_frame_depth == 0U && context.transition_cookie == 0U);
        assert(gem_memory_read(harness.memory, context.sp - sizeof(stack_after), &stack_after,
                               sizeof(stack_after)) == GEM_MEMORY_OK &&
               stack_after == stack_before);
        assert(gem_hybrid_runtime_run_integer_roundtrip(runtime, &context, harness.caller,
                                                        harness.finish, 10000U,
                                                        nullptr) == GEM_STOP_HOST_RETURN);
        gem_hybrid_runtime_destroy(runtime);
    }
    {
        const auto source_page = kStackBase + kStackSize - GEM_GUEST_PAGE_SIZE;
        gem_hybrid_runtime *runtime = gem_hybrid_runtime_create(harness.memory, metadata, &config);
        auto context = initial_context(harness.caller);
        const std::uint64_t marker = UINT64_C(0x6a5b4c3d2e1f9081);
        std::uint64_t previous = 0U;
        std::uint64_t observed = 0U;
        assert(gem_memory_read(harness.memory, source_page, &previous, sizeof(previous)) ==
               GEM_MEMORY_OK);
        assert(runtime &&
               gem_memory_alias(harness.memory, kWriteCopyStackAlias, source_page,
                                GEM_GUEST_PAGE_SIZE, GEM_PAGE_WRITECOPY) == GEM_MEMORY_OK);
        context.sp = kWriteCopyStackAlias + GEM_GUEST_PAGE_SIZE - UINT64_C(0x100);
        assert(gem_hybrid_runtime_run_integer_roundtrip(runtime, &context, harness.caller,
                                                        harness.finish, 10000U,
                                                        nullptr) == GEM_STOP_INVARIANT_VIOLATION);
        assert(context.transition_cookie == 0U);
        assert(gem_memory_write(harness.memory, source_page, &marker, sizeof(marker)) ==
               GEM_MEMORY_OK);
        assert(gem_memory_read(harness.memory, kWriteCopyStackAlias, &observed, sizeof(observed)) ==
                   GEM_MEMORY_OK &&
               observed == marker);
        assert(gem_memory_write(harness.memory, source_page, &previous, sizeof(previous)) ==
               GEM_MEMORY_OK);
        context = initial_context(harness.caller);
        assert(gem_hybrid_runtime_run_integer_roundtrip(runtime, &context, harness.caller,
                                                        harness.finish, 10000U,
                                                        nullptr) == GEM_STOP_HOST_RETURN);
        assert(gem_memory_release(harness.memory, kWriteCopyStackAlias, GEM_GUEST_PAGE_SIZE) ==
               GEM_MEMORY_OK);
        gem_hybrid_runtime_destroy(runtime);
    }
    {
        gem_hybrid_runtime_config wrong = config;
        wrong.dispatch_call_helper--;
        gem_hybrid_runtime *runtime = gem_hybrid_runtime_create(harness.memory, metadata, &wrong);
        auto context = initial_context(harness.caller);
        assert(runtime && gem_hybrid_runtime_run_integer_roundtrip(
                              runtime, &context, harness.caller, harness.finish, 10000U, nullptr) ==
                              GEM_STOP_INVARIANT_VIOLATION);
        gem_hybrid_runtime_destroy(runtime);
    }

    if (argc == 4)
        write_phase_b_trace(argv[3], harness, entries, metadata);

    std::puts("authentic ARM64EC -> Blink -> ARM64EC integer round trip passed");
    return 0;
}
