#include "dump.h"
#include "frontend/frontend.h"
#include "game_code.h"
#include "kernel/kernel.h"
#include "loader.h"
#include "log.h"
#include "memory.h"

#include <SDL3/SDL.h>
#ifdef _WIN32
#include <windows.h>
#include <bcrypt.h>
#else
#include <openssl/sha.h>
#endif

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string_view>
#include <thread>

namespace game_code {
const RecompEntry *functions() { return recomp_functions; }
size_t function_count() { return recomp_function_count; }
}

extern "C" void recomp_restore_guest_context(Ctx *ctx, uint32_t target) {
    for (auto *frame = ctx->unwind_frame; frame; frame = frame->previous) {
        if (frame->sp == ctx->r[13] && frame->return_pc == ctx->r[14]) {
            recomp_tail_indirect(ctx, target);
            ctx->unwind_frame = frame;
            longjmp(frame->resume, 1);
        }
    }
    fatal("Guest exception restores PC %08x, LR %08x, SP %08x, r0 %08x without a matching frame",
          target, ctx->r[14], ctx->r[13], ctx->r[0]);
}

namespace {
constexpr uint64_t title_id = 0x0004000000162f00;
constexpr const char *partition = "CHIBI-EUR.cxi";
constexpr std::array<uint8_t, 32> code_hash = {
    0xa8, 0x2c, 0x32, 0xe5, 0x09, 0x3a, 0xbb, 0x0f, 0x2f, 0x59, 0xe9, 0xbf, 0x81, 0x81, 0x92, 0x57,
    0xfd, 0xce, 0x9d, 0x34, 0x08, 0x8d, 0xcc, 0x7f, 0x73, 0x49, 0xa7, 0x21, 0xdb, 0x58, 0x79, 0xc5,
};

#ifdef _WIN32
LONG WINAPI report_access_violation(EXCEPTION_POINTERS *exception) {
    const auto &record = *exception->ExceptionRecord;
    if (record.ExceptionCode != EXCEPTION_ACCESS_VIOLATION || record.NumberParameters < 2)
        return EXCEPTION_CONTINUE_SEARCH;
    const auto address = reinterpret_cast<const void *>(record.ExceptionInformation[1]);
    const auto pc = reinterpret_cast<uintptr_t>(record.ExceptionAddress);
    const auto base = reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
    const auto op = record.ExceptionInformation[0];
    LOG("Windows access violation at host PC %p (image base %p), %s %p",
        reinterpret_cast<void *>(pc), reinterpret_cast<void *>(base),
        op == 0 ? "reading" : op == 1 ? "writing" : "executing", address);
    uint32_t guest = 0;
    if (recomp_mem && mem::guest_address(address, &guest)) LOG("Guest memory address %08x", guest);
    print_trace();
    std::fflush(stderr);
    if (std::getenv("CHIBI_NO_DIALOG")) std::_Exit(1);
    return EXCEPTION_CONTINUE_SEARCH;
}
#endif

GameImage load(const char *path) {
    if (peek_program_id(path) != title_id)
        fatal("Expected a decrypted Chibi-Robo! Zip Lash European image (CTR-P-BXLP): %s", path);
    open_dump(path);
    const auto &code = dump().code();
    if (code.size() != 0x4c7000) fatal("Unexpected cartridge code size");
    std::array<uint8_t, 32> hash{};
#ifdef _WIN32
    const auto status = BCryptHash(BCRYPT_SHA256_ALG_HANDLE, nullptr, 0, const_cast<PUCHAR>(code.data()),
                                   static_cast<ULONG>(code.size()), hash.data(), static_cast<ULONG>(hash.size()));
    const bool hashed = status >= 0;
#else
    const bool hashed = SHA256(code.data(), code.size(), hash.data()) != nullptr;
#endif
    if (!hashed || hash != code_hash) fatal("The cartridge code does not match the European revision built here");
    mem::init();
    return load_game();
}

int verify_startup(const char *path) {
    const GameImage image = load(path);
    // The first BL in the cartridge entry point invokes this ARM routine.
    constexpr uint32_t address = 0x00100024;
    constexpr uint32_t start = 0x005c6dec;
    constexpr uint32_t end = 0x005fa5e8;
    if (image.data_end != start || image.bss_size != end - start)
        fatal("The cartridge has a different BSS layout");
    const auto first = game_code::functions();
    const auto last = first + game_code::function_count();
    const auto entry = std::lower_bound(first, last, address,
        [](const RecompEntry &item, uint32_t target) { return item.addr < target; });
    if (entry == last || entry->addr != address) fatal("The recompiled BSS initialization routine is missing");

    std::fill(mem::ptr(start), mem::ptr(end), uint8_t{0xa5});
    *mem::ptr(start - 1) = 0x4b;
    *mem::ptr(end) = 0x37;
    Ctx registers{};
    entry->fn(&registers);
    recomp_follow_tail(&registers);
    if (registers.r[0] != end || registers.r[1] != end ||
        *mem::ptr(start - 1) != 0x4b || *mem::ptr(end) != 0x37 ||
        !std::all_of(mem::ptr(start), mem::ptr(end), [](uint8_t value) { return value == 0; }))
        fatal("The recompiled startup routine did not reproduce the BSS clear");
    std::printf("Original startup function 0x%08x cleared %u bytes and preserved both boundary bytes\n",
                address, end - start);
    return 0;
}

void play(const char *path) {
    const auto image = load(path);
    kernel::init(image);
    if (std::getenv("CHIBI_TRACE_BOOT")) {
        std::thread([] {
            for (int sample = 1; sample <= 3; ++sample) {
                std::this_thread::sleep_for(std::chrono::seconds(5));
                LOG("Boot trace at %d seconds", sample * 5);
                kernel::dump_threads(stderr);
            }
        }).detach();
    }
    kernel::run(image);
}
}

int main(int argc, char **argv) {
#ifdef _WIN32
    SetUnhandledExceptionFilter(report_access_violation);
#endif
    log_cfg::init();
    if (argc == 2 && std::string_view(argv[1]) == "--verify-gamepad") return frontend::verify_gamepad();
    if (argc == 2 && std::string_view(argv[1]) == "--poll-gamepad") return frontend::poll_gamepad();
    if (argc == 3 && std::string_view(argv[1]) == "--verify-startup") return verify_startup(argv[2]);
    if (argc > 2 || (argc == 2 && std::string_view(argv[1]) == "--help")) {
        std::puts("ChibiRoboZipLash [path to decrypted CHIBI-EUR.cxi]");
        return argc > 2 ? 2 : 0;
    }
    if (!std::getenv("CHIBI_NO_DIALOG")) {
        fatal_hook = [](const char *message) {
            SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Chibi-Robo! Zip Lash", message, nullptr);
        };
    }
    const char *base = SDL_GetBasePath();
    const std::filesystem::path executable = base ? base : ".";
    auto image = executable / partition;
    if (!std::filesystem::is_regular_file(image)) image = executable / ".." / "extracted" / partition;
    if (!std::filesystem::is_regular_file(image)) image = executable / ".." / ".." / "extracted" / partition;
    if (argc == 2) image = argv[1];
    if (!std::filesystem::is_regular_file(image)) fatal("Cartridge image not found: %s", image.string().c_str());
    frontend::run(play, image.string().c_str());
}
