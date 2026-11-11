#pragma once
// SkellWorld optional localhost link. UI/worker-thread ONLY.
// Never create, start, stop or publish from processBlock().
#include <SkellCore/World.h>
#include <SkellCore/WorldMusical.h>
#include <SkellCore/WorldTuning.h>
#include <juce_core/juce_core.h>
#include <atomic>
#include <array>
#include <string>
#include <algorithm>

namespace skellcore::world {
class WorldLink final : private juce::Thread {
public:
    WorldLink(std::string productId, std::string displayName)
        : juce::Thread("SkellWorld local link"), product_(std::move(productId)),
          display_(std::move(displayName)),
          instance_(juce::Uuid().toString().toStdString()) {}
    ~WorldLink() override { leave(); }

    // Explicit opt-in from the JUCE message thread. Never start at plugin load.
    bool join() {
        if (isThreadRunning()) return false;
        requested_.store(true, std::memory_order_release);
        online_.store(false, std::memory_order_release);
        peers_.store(0, std::memory_order_release);
        pending_.resetWhenStopped();
        if (!startThread()) {
            requested_.store(false, std::memory_order_release);
            return false;
        }
        return true;
    }
    void leave() {
        requested_.store(false, std::memory_order_release);
        signalThreadShouldExit();
        stopThread(3000); // 200ms read poll + bounded reconnect sleep
        online_.store(false, std::memory_order_release);
        peers_.store(0, std::memory_order_release);
    }
    bool requested() const noexcept { return requested_.load(std::memory_order_acquire); }
    bool online() const noexcept { return online_.load(std::memory_order_acquire); }
    int peerCount() const noexcept { return peers_.load(std::memory_order_acquire); }
    // UI timer only: informational key, valid only while connected and the
    // sending peer is registered as skell.key. Callers must explicitly opt in.
    bool latestMusicalKey(MusicalKey& key, std::uint64_t& version) const noexcept {
        if(!online()) return false;
        const auto word=lastKey_.load(std::memory_order_acquire);
        const int encoded=int(word & 255u);
        key=unpackMusicalKey(encoded);
        version=word>>8;
        return key.valid() && version!=0;
    }
    // UI thread: the entire tuning snapshot is one event. Send a legacy key
    // announcement too, for consumers not yet updated to musical.tuning.
    bool publishMusicalTuning(MusicalTuning tuning) noexcept {
        if (!tuning.valid()) return false;
        const auto payload=formatMusicalTuning(tuning);
        const bool sent=publish("musical.tuning",payload.data());
        if (sent) publishMusicalKey({tuning.root,tuning.mode});
        return sent;
    }
    // UI polling only. A 64-bit atomic preserves key, scale and A4 consistency.
    bool latestMusicalTuning(MusicalTuning& tuning, std::uint32_t& version) const noexcept {
        if (!online()) return false;
        return unpackMusicalTuning(lastTuning_.load(std::memory_order_acquire),tuning,version);
    }
    bool publishMusicalKey(MusicalKey key) noexcept {
        if(!key.valid()) return false;
        const auto payload=formatMusicalKey(key);
        return publish("musical.key",payload.data());
    }

    // UI-thread event publishing only; informational data, never remote controls.
    bool publish(std::string_view topic, std::string_view value) noexcept {
        if (!online()) return false;
        Frame frame;
        return make(frame, Kind::Event, instance_, topic, value) && pending_.push(frame);
    }

private:
    bool send(juce::StreamingSocket& socket, const Frame& frame) {
        std::array<char,maxLine> line{};
        std::size_t length=0;
        if (!encode(frame,line,length)) return false;
        std::size_t sent=0;
        while (sent < length && !threadShouldExit()) {
            const int written=socket.write(line.data()+sent,static_cast<int>(length-sent));
            if (written <= 0) return false;
            sent += static_cast<std::size_t>(written);
        }
        return sent == length;
    }
    void onFrame(const Frame& frame) noexcept {
        if (frame.kind == Kind::Welcome && text(frame.instance) == "hub" &&
            text(frame.value) == "connected") {
            online_.store(true, std::memory_order_release);
        } else if (frame.kind == Kind::Peer) {
            if (text(frame.value) == "online") {
                // Remember sender product IDs; unauthenticated local events
                // must not drive key/scale controls by default.
                bool present=false;
                for (const auto& item : peerInfo_) {
                    if (item.kind==Kind::Peer && text(item.instance)==text(frame.instance))
                        present=true;
                }
                if (!present) for (auto& item : peerInfo_) {
                    if(item.kind==Kind::Invalid) { item=frame; break; }
                }
                const auto current=peers_.load(std::memory_order_relaxed);
                if (current < 32) peers_.store(current+1,std::memory_order_release);
            } else if (text(frame.value) == "offline") {
                for(auto& item : peerInfo_) {
                    if(item.kind==Kind::Peer && text(item.instance)==text(frame.instance))
                        item=Frame{};
                }
                if(text(frame.instance)==text(lastKeySource_)) {
                    lastKey_.store(0,std::memory_order_release);
                    lastKeySource_.fill(0);
                }
                if(text(frame.instance)==text(lastTuningSource_)) {
                    lastTuning_.store(0,std::memory_order_release);
                    lastTuningSource_.fill(0);
                }
                const auto current=peers_.load(std::memory_order_relaxed);
                if (current > 0) peers_.store(current-1,std::memory_order_release);
            }
        } else if (frame.kind==Kind::Event && text(frame.topic)=="musical.tuning") {
            bool isKeySource=false;
            for (const auto& item : peerInfo_) {
                if(item.kind==Kind::Peer && text(item.instance)==text(frame.instance) &&
                   text(item.topic)=="skell.key") {isKeySource=true;break;}
            }
            MusicalTuning tuning;
            if(isKeySource && parseMusicalTuning(text(frame.value),tuning)) {
                lastTuningSource_=frame.instance;
                const auto counter=++tuningSequence_;
                lastTuning_.store(packMusicalTuning(tuning,counter),std::memory_order_release);
            }
        } else if (frame.kind==Kind::Event && text(frame.topic)=="musical.key") {
            // The broker reports source identity separately in PEER frames.
            // Only the registered SkellKey product may suggest musical keys.
            bool isKeySource=false;
            for (const auto& item : peerInfo_) {
                if(item.kind==Kind::Peer && text(item.instance)==text(frame.instance) &&
                   text(item.topic)=="skell.key") {isKeySource=true;break;}
            }
            MusicalKey key;
            if(isKeySource && parseMusicalKey(text(frame.value),key)) {
                lastKeySource_=frame.instance;
                const auto counter=++keySequence_;
                lastKey_.store((counter<<8)|static_cast<std::uint64_t>(key.packed()),
                               std::memory_order_release);
            }
        }
    }
    void run() override {
        // Once the user opts in, a hub restart should not strand a plugin.
        // Retry slowly; never run retries on the audio or message thread.
        while (!threadShouldExit()) {
            online_.store(false,std::memory_order_release);
            peers_.store(0,std::memory_order_release);
            lastKey_.store(0,std::memory_order_release);
            lastKeySource_.fill(0);
            lastTuning_.store(0,std::memory_order_release);
            lastTuningSource_.fill(0);
            for(auto& item:peerInfo_) item=Frame{};
            juce::StreamingSocket socket;
            if (socket.connect("127.0.0.1",defaultPort,500)) {
                Frame hello;
                if (make(hello,Kind::Hello,instance_,product_,display_) &&
                    send(socket,hello)) {
                    std::array<char,maxLine> line{};
                    std::size_t used=0;
                    bool validSession=true;
                    while (validSession && !threadShouldExit()) {
                        Frame outbound;
                        while (pending_.pop(outbound)) {
                            if (!send(socket,outbound)) { validSession=false; break; }
                        }
                        if (!validSession || threadShouldExit()) break;
                        const int ready=socket.waitUntilReady(true,200);
                        if (ready < 0) break;
                        if (ready == 0) continue;
                        std::array<char,256> bytes{};
                        const int n=socket.read(bytes.data(),static_cast<int>(bytes.size()),false);
                        if (n <= 0) break;
                        for (int i=0;i<n;++i) {
                            if (used >= maxLine-1) { validSession=false; break; }
                            line[used++]=bytes[static_cast<std::size_t>(i)];
                            if (line[used-1]=='\n') {
                                Frame incoming;
                                if (!decode({line.data(),used},incoming)) {
                                    validSession=false;
                                    break;
                                }
                                onFrame(incoming);
                                used=0;
                            }
                        }
                    }
                }
                socket.close();
            }
            online_.store(false,std::memory_order_release);
            peers_.store(0,std::memory_order_release);
            lastKey_.store(0,std::memory_order_release);
            lastTuning_.store(0,std::memory_order_release);
            // Do not replay stale theme/preset suggestions after reconnect.
            Frame obsolete;
            while (pending_.pop(obsolete)) {}
            if (!threadShouldExit()) juce::Thread::sleep(1200);
        }
    }
    // Peer registry and source identity are only touched on this worker.
    std::array<Frame,32> peerInfo_{};
    std::array<char,48> lastKeySource_{};
    std::uint64_t keySequence_=0;
    std::uint32_t tuningSequence_=0;
    std::array<char,48> lastTuningSource_{};
    std::atomic<std::uint64_t> lastTuning_{0};
    std::atomic<std::uint64_t> lastKey_{0};
    Mailbox<32> pending_; // one UI producer, one worker consumer
    const std::string product_;
    const std::string display_;
    const std::string instance_;
    std::atomic<bool> requested_{false},online_{false};
    std::atomic<int> peers_{0};
};
} // namespace skellcore::world
