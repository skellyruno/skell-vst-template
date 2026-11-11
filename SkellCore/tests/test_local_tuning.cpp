#ifdef NDEBUG
#undef NDEBUG
#endif
#include <SkellCore/LocalTuningBus.h>
#include <cassert>
#include <cstdint>
#include <iostream>
#if defined(_WIN32)
#include <string>
#include <vector>
#endif

using namespace skellcore::world;
int main(int argc, char** argv) {
    // Regression: public Windows headers must not macro-rewrite ordinary
    // identifiers used by host/plugin DSP code.
    const int near = 2, far = 3;
    assert(near + far == 5);
    constexpr auto max = LocalTuningBus::timestampMask;
    assert(LocalTuningBus::fresh(100, 100));
    assert(LocalTuningBus::fresh(100, 50));
    assert(!LocalTuningBus::fresh(100, 49));
    assert(LocalTuningBus::fresh(2, max)); // wraps every ~311 days
    assert(!LocalTuningBus::fresh(2, max-55));

#if defined(_WIN32)
    // Parent and child intentionally run in separate processes, proving IPC
    // works between VST3s even when hosts isolate their plugin processes.
    if (argc == 3 && std::string(argv[1]) == "reader") {
        std::wstring name;
        for (const char* p=argv[2];*p;++p) name.push_back(wchar_t(*p));
        LocalTuningBus reader(name.c_str());
        assert(reader.open());
        MusicalTuning received;
        std::uint32_t version=0;
        assert(reader.latest(received,version));
        assert(received.root == 4 && received.mode == 1 &&
               received.a4MilliHz == 432500 &&
               received.mask == diatonicMask(4,1));
        assert(version != 0);
        return 0;
    }

    const auto name = std::wstring(L"Local\\SkellCore.Tuning.Tests.") +
                      std::to_wstring(GetCurrentProcessId());
    LocalTuningBus sender(name.c_str()), receiver(name.c_str());
    assert(sender.open());
    assert(receiver.open());
    MusicalTuning tuning;
    std::uint32_t version=0;
    assert(!receiver.latest(tuning,version));
    const auto original=makeMusicalTuning(4,1,432.5);
    assert(sender.publish(original));
    assert(receiver.latest(tuning,version));
    assert(tuning.root == 4 && tuning.mode == 1 &&
           tuning.a4MilliHz == 432500);
    const auto oldVersion=version;

    wchar_t exe[MAX_PATH]{};
    assert(GetModuleFileNameW(nullptr,exe,MAX_PATH)>0);
    const auto command=std::wstring(L"\"")+exe+L"\" reader "+name;
    std::vector<wchar_t> mutableCommand(command.begin(),command.end());
    mutableCommand.push_back(0);
    STARTUPINFOW start{};
    start.cb=sizeof(start);
    PROCESS_INFORMATION process{};
    assert(CreateProcessW(nullptr,mutableCommand.data(),nullptr,nullptr,
                          FALSE,0,nullptr,nullptr,&start,&process));
    assert(WaitForSingleObject(process.hProcess,5000)==WAIT_OBJECT_0);
    DWORD exitCode=1;
    assert(GetExitCodeProcess(process.hProcess,&exitCode));
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    assert(exitCode==0);

    // Multiple publishers: an older publisher must never clear a newer
    // packet when its plugin is removed from a DAW track.
    LocalTuningBus other(name.c_str());
    assert(other.open());
    assert(other.publish(makeMusicalTuning(7,0,440.0)));
    sender.withdraw();
    assert(receiver.latest(tuning,version));
    assert(tuning.root==7 && tuning.mode==0);
    assert(version!=0 && LocalTuningBus::fresh(LocalTuningBus::tick(),version));
    other.withdraw();
    assert(!receiver.latest(tuning,version));
    assert(!sender.publish({-1,-1,0,440000}));
    sender.close();
    receiver.close();
    other.close();
    (void)oldVersion;
#else
    LocalTuningBus unsupported;
    assert(!unsupported.open());
    (void)argc; (void)argv;
#endif
    std::cout << "PASS: hubless tuning bus, cross-process and lifetime checks\n";
}
