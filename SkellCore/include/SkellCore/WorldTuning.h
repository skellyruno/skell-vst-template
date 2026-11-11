#pragma once
// SkellWorld SW1 tuning snapshot. One informational event contains tonic,
// major/minor mode, absolute 12-note scale mask and reference A4 millihertz.
#include <array>
#include <cstdint>
#include <cstdio>
#include <string_view>
#include <cmath>

namespace skellcore::world {
struct MusicalTuning {
    int root = -1;
    int mode = -1;                       // 0 major, 1 natural minor
    std::uint16_t mask = 0;              // absolute C..B pitch classes
    std::uint32_t a4MilliHz = 440000;    // 432100 = A4 432.1Hz
    constexpr bool valid() const noexcept {
        return root >= 0 && root < 12 && mode >= 0 && mode <= 1 &&
               mask > 0 && mask <= 4095 && (mask & (1u << root)) != 0 &&
               a4MilliHz >= 400000 && a4MilliHz <= 480000;
    }
    double a4Hz() const noexcept { return static_cast<double>(a4MilliHz) / 1000.0; }
};
inline constexpr std::uint16_t diatonicMask(int root, int mode) noexcept {
    if (root < 0 || root >= 12 || mode < 0 || mode > 1) return 0;
    constexpr std::array<int,7> major {0,2,4,5,7,9,11};
    constexpr std::array<int,7> minor {0,2,3,5,7,8,10};
    std::uint16_t mask = 0;
    for (const int n : (mode == 0 ? major : minor))
        mask |= std::uint16_t(1u << ((root + n) % 12));
    return mask;
}
inline MusicalTuning makeMusicalTuning(int root, int mode, double a4Hz) noexcept {
    MusicalTuning value;
    if (!std::isfinite(a4Hz) || a4Hz < 400.0 || a4Hz > 480.0) return value;
    value.root = root;
    value.mode = mode;
    value.mask = diatonicMask(root,mode);
    value.a4MilliHz = static_cast<std::uint32_t>(std::lround(a4Hz * 1000.0));
    return value;
}
inline std::array<char,40> formatMusicalTuning(const MusicalTuning& value) noexcept {
    std::array<char,40> output{};
    if (value.valid())
        std::snprintf(output.data(),output.size(),"%d,%d,%u,%u",
                      value.root,value.mode,static_cast<unsigned>(value.mask),
                      static_cast<unsigned>(value.a4MilliHz));
    return output;
}
inline bool parseMusicalTuning(std::string_view text, MusicalTuning& value) noexcept {
    std::array<std::uint32_t,4> fields{};
    unsigned field = 0;
    bool digit = false;
    for (const unsigned char c : text) {
        if (c == ',') {
            if (!digit || field == 3) return false;
            ++field;
            digit = false;
        } else if (c >= '0' && c <= '9') {
            if (fields[field] > (480000u - (c-'0')) / 10u) return false;
            fields[field] = fields[field]*10u + (c-'0');
            digit = true;
        } else return false;
    }
    if (field != 3 || !digit || fields[2] > 4095) return false;
    MusicalTuning parsed {static_cast<int>(fields[0]),static_cast<int>(fields[1]),
                          static_cast<std::uint16_t>(fields[2]),fields[3]};
    if (!parsed.valid()) return false;
    value = parsed;
    return true;
}
inline std::uint64_t packMusicalTuning(const MusicalTuning& tune, std::uint32_t version) noexcept {
    if (!tune.valid() || (version & 0x0fffffffu)==0) return 0;
    return (std::uint64_t(version & 0x0fffffffu) << 36) |
           (std::uint64_t(tune.a4MilliHz) << 17) |
           (std::uint64_t(tune.mask) << 5) |
           (std::uint64_t(tune.mode) << 4) |
           std::uint64_t(tune.root);
}
inline bool unpackMusicalTuning(std::uint64_t packet, MusicalTuning& tune,
                               std::uint32_t& version) noexcept {
    version = static_cast<std::uint32_t>(packet >> 36);
    const MusicalTuning parsed {int(packet & 15u),int((packet >> 4)&1u),
                               static_cast<std::uint16_t>((packet >> 5)&4095u),
                               static_cast<std::uint32_t>((packet >> 17)&524287u)};
    if (version == 0 || !parsed.valid()) return false;
    tune = parsed;
    return true;
}
} // namespace skellcore::world
