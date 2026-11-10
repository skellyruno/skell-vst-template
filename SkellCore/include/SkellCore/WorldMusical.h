#pragma once
// Typed, versioned musical-key payload carried as an INFORMATIONAL SW1 event.
// No external value becomes a parameter change without explicit user consent.
#include <array>
#include <string_view>
#include <cstdint>

namespace skellcore::world {
struct MusicalKey {
    int root = -1; // 0 = C, ... 11 = B
    int mode = -1; // 0 = major, 1 = natural minor
    constexpr bool valid() const noexcept {
        return root >= 0 && root < 12 && mode >= 0 && mode < 2;
    }
    constexpr int packed() const noexcept { return valid() ? 1 + root + mode*12 : 0; }
};
inline bool parseMusicalKey(std::string_view value, MusicalKey& out) noexcept {
    if (value.size()!=3 && value.size()!=4) return false;
    const auto dot=value.size()-2;
    if(value[dot]!='.' || value.back()<'0' || value.back()>'1') return false;
    int root=0;
    for(std::size_t i=0;i<dot;++i) {
        if(value[i]<'0'||value[i]>'9')return false;
        root=root*10+(value[i]-'0');
    }
    const MusicalKey parsed{root,value.back()-'0'};
    if(!parsed.valid())return false;
    out=parsed;
    return true;
}
inline std::array<char,5> formatMusicalKey(MusicalKey key) noexcept {
    std::array<char,5> text{};
    if(!key.valid()) return text;
    if(key.root>=10) {
        text[0]='1'; text[1]=char('0'+key.root-10);
        text[2]='.'; text[3]=char('0'+key.mode);
    } else {
        text[0]=char('0'+key.root); text[1]='.'; text[2]=char('0'+key.mode);
    }
    return text;
}
inline MusicalKey unpackMusicalKey(int packed) noexcept {
    if(packed<1 || packed>24)return {};
    const int value=packed-1;
    return {value%12,value/12};
}
} // namespace skellcore::world
