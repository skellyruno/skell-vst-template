#pragma once
// Hub-free, same-Windows-session SKELL tuning IPC.
//
// The shared record is ONE 64-bit atomic word containing the complete Key,
// Scale, pitch-class mask and A4 reference. No sockets, installer, service,
// locks, allocations or DSP callbacks are involved in reading/writing it.
// Clients must open/publish/read/close from a JUCE UI/worker timer ONLY.
//
// Windows Local\ kernel-object namespace restricts the mapping to the
// interactive logon session. The creator's default Windows security
// descriptor restricts access according to its user token. This is a
// same-user informational bus, not an authentication/security boundary:
// other software running as that user could impersonate SkellKey.
//
// Publication carries a 28-bit monotonic 100ms timestamp (the existing
// MusicalTuning packet version field). Missing/crashed publishers expire
// after 5 seconds; normal shutdown clears ONLY its own final packet.
// If multiple SkellKey instances are active, last publisher wins.
#include <SkellCore/WorldTuning.h>
#include <cstdint>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
// Win32 still exposes legacy near/far macros in some toolchains. Do not
// contaminate JUCE/plugin translation units which use these as variable names.
#ifdef near
#undef near
#endif
#ifdef far
#undef far
#endif
#endif

namespace skellcore::world {
class LocalTuningBus final {
public:
    static constexpr std::uint32_t timestampMask = 0x0fffffffu;
    static constexpr std::uint32_t maxAgeTicks = 50u; // 5 seconds
#if defined(_WIN32)
    static constexpr const wchar_t* defaultName =
        L"Local\\SkellCore.MusicalTuning.v1";
    explicit LocalTuningBus(const wchar_t* name = defaultName) noexcept
        : name_(name) {}
#else
    LocalTuningBus() noexcept = default;
#endif
    ~LocalTuningBus() noexcept { close(); }
    LocalTuningBus(const LocalTuningBus&) = delete;
    LocalTuningBus& operator=(const LocalTuningBus&) = delete;

    static bool fresh(std::uint32_t now, std::uint32_t sent) noexcept {
        return sent != 0 && ((now - sent) & timestampMask) <= maxAgeTicks;
    }

#if defined(_WIN32)
    bool open() noexcept {
        if (mapping_ != nullptr) return true;
        HANDLE handle = CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr,
            PAGE_READWRITE, 0, sizeof(Record), name_);
        if (handle == nullptr) return false;
        auto* region = static_cast<Record*>(
            MapViewOfFile(handle, FILE_MAP_READ | FILE_MAP_WRITE,
                          0, 0, sizeof(Record)));
        if (region == nullptr) {
            CloseHandle(handle);
            return false;
        }
        handle_ = handle;
        mapping_ = region;
        return true;
    }
    bool available() const noexcept { return mapping_ != nullptr; }
    static std::uint32_t tick() noexcept {
        // Same monotonic clock for all processes in the same Windows session.
        const std::uint32_t t = std::uint32_t(
            (GetTickCount64() / 100ull) & timestampMask);
        return t == 0 ? 1u : t;
    }
    bool publish(MusicalTuning tuning) noexcept {
        if (!tuning.valid() || !open()) return false;
        const auto packet = packMusicalTuning(tuning, tick());
        if (packet == 0) return false;
        InterlockedExchange64(&mapping_->word, static_cast<LONG64>(packet));
        lastWritten_ = packet;
        return true;
    }
    bool latest(MusicalTuning& out, std::uint32_t& version) const noexcept {
        if (mapping_ == nullptr) return false;
        const auto packet = static_cast<std::uint64_t>(
            InterlockedCompareExchange64(&mapping_->word, 0, 0));
        if (!unpackMusicalTuning(packet, out, version)) return false;
        return fresh(tick(), version);
    }
    void withdraw() noexcept {
        if (mapping_ == nullptr || lastWritten_ == 0) return;
        // Never clear a newer packet belonging to another publisher.
        InterlockedCompareExchange64(&mapping_->word, 0,
                                     static_cast<LONG64>(lastWritten_));
        lastWritten_ = 0;
    }
    void close() noexcept {
        withdraw();
        if (mapping_ != nullptr) {
            UnmapViewOfFile(mapping_);
            mapping_ = nullptr;
        }
        if (handle_ != nullptr) {
            CloseHandle(handle_);
            handle_ = nullptr;
        }
    }
private:
    struct Record { alignas(8) volatile LONG64 word; };
    static_assert(sizeof(Record) == 8, "single atomic tuning word");
    const wchar_t* name_;
    HANDLE handle_ = nullptr;
    Record* mapping_ = nullptr;
    std::uint64_t lastWritten_ = 0;
#else
    // Existing optional localhost Hub continues to serve non-Windows hosts.
    bool open() noexcept { return false; }
    bool available() const noexcept { return false; }
    bool publish(MusicalTuning) noexcept { return false; }
    bool latest(MusicalTuning&, std::uint32_t&) const noexcept { return false; }
    void withdraw() noexcept {}
    void close() noexcept {}
#endif
};
} // namespace skellcore::world
