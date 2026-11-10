#ifdef NDEBUG
#undef NDEBUG
#endif
#include <SkellCore/WorldMusical.h>
#include <SkellCore/World.h>
#include <cassert>
#include <array>
#include <iostream>
#include <string_view>
using namespace skellcore::world;
int main() {
    for(int root=0;root<12;++root)for(int mode=0;mode<2;++mode) {
        MusicalKey key{root,mode},parsed;
        const auto formatted=formatMusicalKey(key);
        assert(parseMusicalKey(formatted.data(),parsed));
        assert(parsed.root==root && parsed.mode==mode);
        assert(unpackMusicalKey(key.packed()).root==root);
        assert(unpackMusicalKey(key.packed()).mode==mode);
        Frame event;
        assert(make(event,Kind::Event,"instance-001","musical.key",formatted.data()));
        std::array<char,maxLine> wire{};
        std::size_t length=0;
        assert(encode(event,wire,length));
        Frame decoded;
        assert(decode({wire.data(),length},decoded));
        MusicalKey fromWire;
        assert(parseMusicalKey(text(decoded.value),fromWire));
        assert(fromWire.packed()==key.packed());
    }
    for(auto invalid:{"","-1.0","12.0","0.2","00.0","1.00","1.0x","1/0","1.1\n","1.1|EVIL","abc"}) {
        MusicalKey k;assert(!parseMusicalKey(invalid,k));
    }
    assert(!unpackMusicalKey(0).valid());
    assert(!unpackMusicalKey(25).valid());
    assert(!formatMusicalKey({12,0})[0]);
    std::cout<<"SkellWorld musical-key codec passed\n";
}
