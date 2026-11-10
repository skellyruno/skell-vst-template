#pragma once
// SkellWorld optional localhost link. UI/worker-thread ONLY.
// Never create, start, stop or publish from processBlock().
#include <SkellCore/World.h>
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
        stopThread(3000); // worker socket reads have a 75ms poll limit
        online_.store(false, std::memory_order_release);
        peers_.store(0, std::memory_order_release);
    }
    bool requested() const noexcept { return requested_.load(std::memory_order_acquire); }
    bool online() const noexcept { return online_.load(std::memory_order_acquire); }
    int peerCount() const noexcept { return peers_.load(std::memory_order_acquire); }

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
        return socket.write(line.data(),static_cast<int>(length)) == static_cast<int>(length);
    }
    void onFrame(const Frame& frame) noexcept {
        if (frame.kind == Kind::Welcome && text(frame.instance) == "hub" &&
            text(frame.value) == "connected") {
            online_.store(true, std::memory_order_release);
        } else if (frame.kind == Kind::Peer) {
            if (text(frame.value) == "online") {
                const auto current=peers_.load(std::memory_order_relaxed);
                if (current < 32) peers_.store(current+1,std::memory_order_release);
            } else if (text(frame.value) == "offline") {
                const auto current=peers_.load(std::memory_order_relaxed);
                if (current > 0) peers_.store(current-1,std::memory_order_release);
            }
        }
    }
    void run() override {
        juce::StreamingSocket socket;
        if (!socket.connect("127.0.0.1",defaultPort,500)) return;
        Frame hello;
        if (!make(hello,Kind::Hello,instance_,product_,display_) || !send(socket,hello)) return;
        std::array<char,maxLine> line{};
        std::size_t used=0;
        while (!threadShouldExit()) {
            Frame outbound;
            while (pending_.pop(outbound))
                if (!send(socket,outbound)) { socket.close(); return; }
            const int ready=socket.waitUntilReady(true,75);
            if (ready < 0) break;
            if (ready == 0) continue;
            std::array<char,256> bytes{};
            const int n=socket.read(bytes.data(),static_cast<int>(bytes.size()),false);
            if (n <= 0) break;
            for (int i=0;i<n;++i) {
                const char c=bytes[static_cast<std::size_t>(i)];
                if (used >= maxLine-1) { socket.close(); return; }
                line[used++]=c;
                if (c=='\n') {
                    Frame inbound;
                    if (!decode({line.data(),used},inbound)) { socket.close(); return; }
                    onFrame(inbound);
                    used=0;
                }
            }
        }
        socket.close();
        online_.store(false,std::memory_order_release);
    }
    Mailbox<32> pending_; // one UI producer, one worker consumer
    const std::string product_;
    const std::string display_;
    const std::string instance_;
    std::atomic<bool> requested_{false},online_{false};
    std::atomic<int> peers_{0};
};
} // namespace skellcore::world
