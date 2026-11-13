// SkellCore host-readiness regression: uses the ACTUAL Quickstart processor
// and editor, but does not pretend to be an FL Studio host test.
// Runs as an ordinary JUCE test executable: no external VST3 scanning here.
#include "../Source/PluginProcessor.h"
#include "../Source/PluginEditor.h"
#include <SkellCore/LevelBridge.h>
#include <JuceHeader.h>
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <limits>
#include <memory>
#include <vector>

namespace {
bool require(bool result, const char* why) {
    if (!result) std::fprintf(stderr, "SKELL HOST SMOKE FAILED: %s\n", why);
    return result;
}

bool setActualParameter(SkellAudioProcessor& processor,
                        const char* id, float value) {
    auto* parameter = processor.parameters.getParameter(id);
    if (parameter == nullptr) return false;
    parameter->beginChangeGesture();
    parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
    parameter->endChangeGesture();
    return true;
}

float readActualParameter(SkellAudioProcessor& processor, const char* id) {
    const auto* v = processor.parameters.getRawParameterValue(id);
    return v == nullptr ? std::numeric_limits<float>::quiet_NaN() : v->load();
}

bool processKnownSignal(SkellAudioProcessor& processor, int frames,
                        int blocks, float& lastSample) {
    if (frames <= 0 || blocks <= 0) return false;
    juce::AudioBuffer<float> audio(2, frames);
    juce::MidiBuffer midi;
    for (int block = 0; block < blocks; ++block) {
        for (int channel = 0; channel < 2; ++channel) {
            auto* dest = audio.getWritePointer(channel);
            for (int sample = 0; sample < frames; ++sample)
                dest[sample] = channel == 0 ? .2f : -.15f;
        }
        processor.processBlock(audio, midi);
        if (audio.hasBeenCleared()) return false; // should never mute test signal
        for (int channel = 0; channel < 2; ++channel)
            for (int sample = 0; sample < frames; ++sample)
                if (!std::isfinite(audio.getSample(channel, sample)))
                    return false;
        lastSample = audio.getSample(0, frames-1);
    }
    return true;
}

bool stateAndAutomation() {
    SkellAudioProcessor original;
    const auto stereo = original.getBusesLayout();
    auto mismatch = stereo;
    mismatch.outputBuses.set(0, juce::AudioChannelSet::mono());
    if (!require(original.isBusesLayoutSupported(stereo),
                 "stereo layout rejected")
        || !require(!original.isBusesLayoutSupported(mismatch),
                    "mismatched bus layout accepted")
        || !require(original.getLatencySamples() == 0, "unexpected reported latency"))
        return false;

    original.prepareToPlay(48000., 128);
    if (!require(setActualParameter(original, "gain", 1.625f),
                 "gain automation parameter not found")
        || !require(setActualParameter(original, "enabled", 1.f),
                    "enabled parameter not found")) return false;

    float output = 0.f;
    if (!require(processKnownSignal(original, 128, 16, output), "automated output invalid")
        || !require(std::abs(output-.325f) <= .002f,
                    "automated gain output wrong after smoothing")) return false;

    const auto signalPeak = original.takeOutputPeak();
    if (!require(std::isfinite(signalPeak) && signalPeak >= .32f && signalPeak <= 1.f,
                 "output peak bridge failed")
        || !require(original.takeOutputPeak() == 0.f,
                    "output peak bridge did not clear after read")) return false;

    juce::MemoryBlock saved;
    original.getStateInformation(saved);
    if (!require(saved.getSize() > 32, "host state serialization was empty"))
        return false;

    // Brand-new plugin instance must restore both original host parameters.
    SkellAudioProcessor restored;
    restored.setStateInformation(saved.getData(), static_cast<int>(saved.getSize()));
    if (!require(std::abs(readActualParameter(restored,"gain")-1.625f)<.002f,
                 "gain did not survive save/load")
        || !require(readActualParameter(restored,"enabled") >= .5f,
                    "enable state did not survive save/load")) return false;

    restored.prepareToPlay(96000., 257);
    if (!require(processKnownSignal(restored, 257, 16, output),
                 "restored state produced invalid audio")
        || !require(std::abs(output-.325f)<.002f,
                    "restored instance gain did not settle")) return false;

    // Actual enable/bypass control must smoothly restore dry unity signal.
    if (!require(setActualParameter(restored,"enabled",0.f),"disable automation failed")
        || !require(processKnownSignal(restored,257,20,output),
                    "bypassed output invalid")
        || !require(std::abs(output-.2f)<.002f,
                    "power off did not bypass to unity")) return false;

    juce::MemoryBlock bypassState;
    restored.getStateInformation(bypassState);
    SkellAudioProcessor bypassedReload;
    bypassedReload.setStateInformation(bypassState.getData(),
                                       static_cast<int>(bypassState.getSize()));
    if (!require(readActualParameter(bypassedReload,"enabled")<.5f,
                 "disabled state was not persisted")) return false;
    bypassedReload.prepareToPlay(44100.,64);
    if (!require(processKnownSignal(bypassedReload,64,12,output),
                 "disabled restored processing invalid")
        || !require(std::abs(output-.2f)<.002f,
                    "disabled restored signal was not dry")) return false;

    // Reject junk host state gracefully, retaining current configuration.
    const char junk[] = "not-a-valid-juce-xml-chunk";
    bypassedReload.setStateInformation(junk, int(sizeof(junk)));
    if (!require(readActualParameter(bypassedReload,"enabled")<.5f,
                 "invalid host state changed parameters")) return false;

    // Editor creation/destruction does not change the processor's audio state.
    for (int cycle = 0; cycle < 4; ++cycle) {
        std::unique_ptr<juce::AudioProcessorEditor> ui(bypassedReload.createEditor());
        if (!require(ui != nullptr && ui->getWidth()>0 && ui->getHeight()>0,
                     "editor open failed")) return false;
        ui->setSize(cycle%2 ? 840 : 390, cycle%2 ? 560 : 250);
        if (!require(ui->getWidth() == (cycle%2 ? 840 : 390),
                     "editor resize failed")) return false;
        ui.reset();
        if (!require(processKnownSignal(bypassedReload,64,2,output),
                     "audio failed after editor close")) return false;
    }

    // No leakage between unrelated instances.
    if (!require(std::abs(readActualParameter(original, "gain")-1.625f)<.002f,
                 "new instance changed original gain")) return false;
    original.releaseResources();
    restored.releaseResources();
    bypassedReload.releaseResources();
    std::puts("PASS: actual processor state, automation, bypass, editor lifecycle");
    return true;
}

bool multiInstanceBench() {
    constexpr int samples = 128;
    constexpr int instances = 8;
    constexpr int rounds = 600;
    std::array<std::unique_ptr<SkellAudioProcessor>, instances> processors;
    for (int i=0;i<instances;++i) {
        processors[i] = std::make_unique<SkellAudioProcessor>();
        if (!require(setActualParameter(*processors[i],"gain",1.f+.125f*float(i)),
                     "multi-instance gain automation missing")) return false;
        processors[i]->prepareToPlay(48000.,samples);
    }
    std::array<juce::AudioBuffer<float>,instances> buffers;
    for (auto& audio : buffers) audio.setSize(2,samples);
    juce::MidiBuffer midi;
    float checksum=0.f;
    const auto start=std::chrono::steady_clock::now();
    for (int round=0;round<rounds;++round) {
        for (int index=0;index<instances;++index) {
            auto& audio=buffers[index];
            audio.clear();
            for (int ch=0;ch<2;++ch) {
                auto* ptr=audio.getWritePointer(ch);
                for (int n=0;n<samples;++n)
                    ptr[n]=float(std::sin((round*samples+n)*.019))* .10f;
            }
            processors[index]->processBlock(audio,midi);
            for (int ch=0;ch<2;++ch) {
                for (int n=0;n<samples;++n)
                    if (!std::isfinite(audio.getSample(ch,n))) {
                        std::fprintf(stderr,"SKELL HOST SMOKE FAILED: NaN at instance %d\n",index);
                        return false;
                    }
            }
            checksum+=audio.getSample(0,samples-1);
            if ((round%90)==0 && index==3) {
                if (!require(setActualParameter(*processors[index],"enabled",
                          ((round/90)&1) ? 0.f : 1.f),"automated bypass failed"))
                    return false;
            }
        }
    }
    const auto elapsed=std::chrono::duration<double>(
        std::chrono::steady_clock::now()-start).count();
    // Simulated serial engine budget for all eight effects, 48kHz.
    const auto audioSeconds=double(rounds*samples)/48000.;
    std::printf("SkellCore 8-instance processor CPU baseline: %.3f ms total / %.1f s audio;"
                " ratio=%.5f, checksum=%.5f (non-gating)\n",
                elapsed*1000.,audioSeconds,elapsed/audioSeconds,checksum);
    for (auto& processor:processors) processor->releaseResources();
    return require(std::isfinite(checksum) && std::isfinite(elapsed) && elapsed>=0.,
                   "invalid CPU bench measurements");
}

// A serial processing matrix: measures *processBlock* CPU, excluding buffer
// preparation, host scheduling, GUI, ASIO and rendering. Informational only.
// Each instance receives the same known input and independent parameters.
bool sampleRateAndInstanceMatrix() {
    constexpr int rates[] { 44100, 48000, 96000 };
    constexpr int sizes[] { 64, 128, 512 };
    constexpr int counts[] { 1, 4, 8, 16 };
    constexpr int blocks = 96;
    bool success = true;
    std::puts("SKELL_MATRIX_CSV,rate,block,instances,process_ms,audio_seconds,serial_ratio,checksum");
    for (const int rate : rates) {
        for (const int blockSize : sizes) {
            for (const int count : counts) {
                std::vector<std::unique_ptr<SkellAudioProcessor>> processors;
                processors.reserve(static_cast<std::size_t>(count));
                std::vector<juce::AudioBuffer<float>> buffers;
                buffers.reserve(static_cast<std::size_t>(count));
                for (int instance = 0; instance < count; ++instance) {
                    auto processor = std::make_unique<SkellAudioProcessor>();
                    const float expectedGain = 0.90f + 0.06f * instance;
                    if (!require(setActualParameter(*processor, "gain", expectedGain),
                                 "matrix gain parameter missing")
                        || !require(setActualParameter(*processor, "enabled",
                                       instance % 4 != 0 ? 1.f : 0.f),
                                    "matrix enabled parameter missing"))
                        return false;
                    processor->prepareToPlay(rate, blockSize);
                    buffers.emplace_back(2, blockSize);
                    processors.push_back(std::move(processor));
                }
                juce::MidiBuffer midi;
                double elapsedMs = 0.0, checksum = 0.0;
                for (int block = 0; block < blocks; ++block) {
                    for (int instance = 0; instance < count; ++instance) {
                        auto& processor = *processors[static_cast<std::size_t>(instance)];
                        auto& audio = buffers[static_cast<std::size_t>(instance)];
                        // Test an actual automated bypass transition mid-stream.
                        if (block == 32 && (instance % 4) == 1
                            && !setActualParameter(processor, "enabled", 0.f))
                            return require(false, "matrix bypass automation failed");
                        const float l = 0.075f + .0001f * instance;
                        const float r = -0.06f - .0001f * instance;
                        for (int i = 0; i < blockSize; ++i) {
                            audio.setSample(0, i, l);
                            audio.setSample(1, i, r);
                        }
                        const auto start = std::chrono::steady_clock::now();
                        processor.processBlock(audio, midi);
                        const auto finish = std::chrono::steady_clock::now();
                        elapsedMs += std::chrono::duration<double, std::milli>(
                            finish - start).count();
                        const auto actualL = audio.getSample(0, blockSize-1);
                        const auto actualR = audio.getSample(1, blockSize-1);
                        checksum += actualL + actualR;
                        if (!std::isfinite(actualL) || !std::isfinite(actualR))
                            return require(false, "matrix non-finite output");
                        // The final blocks exceed the 15ms smoothing window
                        // even at 96kHz/64 samples, so gain/bypass must settle.
                        if (block == blocks - 1) {
                            const float actualGain = readActualParameter(processor,"gain");
                            const bool enabled = readActualParameter(processor,"enabled") >= .5f;
                            const float expected = enabled ? actualGain : 1.f;
                            if (!require(std::abs(actualL - l * expected) <= .0015f
                                         && std::abs(actualR - r * expected) <= .0015f,
                                         "matrix gain/bypass or instance isolation failed"))
                                return false;
                        }
                    }
                }
                const double audioSeconds = (double(blocks) * blockSize) / double(rate);
                const double serialRatio = elapsedMs / (audioSeconds * 1000.0);
                if (!require(std::isfinite(elapsedMs) && elapsedMs >= 0.
                             && std::isfinite(serialRatio)
                             && std::isfinite(checksum),
                             "matrix timing or checksum invalid"))
                    return false;
                std::printf("SKELL_MATRIX_CSV,%d,%d,%d,%.5f,%.5f,%.6f,%.7f\n",
                            rate, blockSize, count, elapsedMs, audioSeconds,
                            serialRatio, checksum);
                for (auto& p : processors) p->releaseResources();
            }
        }
    }

    // Mono is a supported host layout as well as stereo.
    SkellAudioProcessor monoProcessor;
    auto monoLayout = monoProcessor.getBusesLayout();
    monoLayout.inputBuses.set(0, juce::AudioChannelSet::mono());
    monoLayout.outputBuses.set(0, juce::AudioChannelSet::mono());
    if (!require(monoProcessor.setBusesLayout(monoLayout),
                 "mono layout refused")) return false;
    monoProcessor.prepareToPlay(48000., 128);
    juce::AudioBuffer<float> monoBuffer(1, 128);
    monoBuffer.clear();
    for (int i=0; i<128; ++i) monoBuffer.setSample(0, i, .125f);
    juce::MidiBuffer midi;
    monoProcessor.processBlock(monoBuffer, midi);
    if (!require(std::isfinite(monoBuffer.getSample(0,127))
                 && std::abs(monoBuffer.getSample(0,127)-.125f) <= .001f,
                 "mono processing invalid")) return false;
    monoProcessor.releaseResources();
    std::puts("PASS: 36 rate/block/instance cases and mono host layout");
    return success;
}
}
int main() {
    juce::ScopedJuceInitialiser_GUI initialiseJuce;
    if (!stateAndAutomation() || !multiInstanceBench() || !sampleRateAndInstanceMatrix()) return 1;
    std::puts("PASS: SkellCore Quickstart host readiness smoke");
    return 0;
}
