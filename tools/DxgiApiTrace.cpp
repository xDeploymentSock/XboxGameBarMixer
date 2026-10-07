// Passive, bounded DXGI API event collection. Never measures scanout or displayed FPS.
#include <windows.h>
#include <evntrace.h>
#include <evntcons.h>
#include <algorithm>
#include <array>
#include <cerrno>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>
#include <utility>

namespace {
constexpr GUID dxgi_provider{0xca11c036, 0x0102, 0x4a2d,
    {0xa6, 0xad, 0xf0, 0x3c, 0xfe, 0xd5, 0xd3, 0xc9}};
constexpr GUID owned_test_provider{0xe163da23, 0xcec7, 0x4d15,
    {0x8d, 0x42, 0x10, 0x2c, 0xa1, 0x31, 0x17, 0x7e}};
struct trace_properties {
    EVENT_TRACE_PROPERTIES value{};
    std::array<wchar_t, 256> name{};
    trace_properties() {
        value.Wnode.BufferSize = static_cast<ULONG>(sizeof(*this));
        value.Wnode.Flags = WNODE_FLAG_TRACED_GUID;
        value.Wnode.ClientContext = 1; // QueryPerformanceCounter timestamps.
        value.LoggerNameOffset = static_cast<ULONG>(offsetof(trace_properties, name));
        value.BufferSize = 64;
        value.MinimumBuffers = 16;
        value.MaximumBuffers = 64;
        value.LogFileMode = EVENT_TRACE_REAL_TIME_MODE;
        value.FlushTimer = 1;
    }
};
struct event_ids {
    BOOLEAN include{TRUE};
    UCHAR reserved{};
    USHORT count{2};
    USHORT ids[2]{42, 43};
};
static_assert(offsetof(event_ids, ids) == offsetof(EVENT_FILTER_EVENT_ID, Events));
struct row { DWORD pid{}, tid{}; USHORT id{}; LONGLONG qpc{}; };
struct collection {
    GUID provider{dxgi_provider};
    std::vector<DWORD> pids;
    std::vector<row> rows;
    bool overflow{};
    bool callback_failed{};
};
void WINAPI receive(EVENT_RECORD* event) noexcept {
    auto* data = static_cast<collection*>(event->UserContext);
    if (!data || !IsEqualGUID(event->EventHeader.ProviderId, data->provider)) { return; }
    const auto id = event->EventHeader.EventDescriptor.Id;
    if (id != 42 && id != 43) { return; }
    const auto pid = event->EventHeader.ProcessId;
    if (std::find(data->pids.begin(), data->pids.end(), pid) == data->pids.end()) { return; }
    if (data->rows.size() >= 100000) { data->overflow = true; return; }
    try {
        data->rows.push_back({pid, event->EventHeader.ThreadId, id,
                              event->EventHeader.TimeStamp.QuadPart});
    } catch (...) { data->callback_failed = true; }
}
void check(ULONG code, const char* operation) {
    if (code != ERROR_SUCCESS) {
        throw std::runtime_error(std::string(operation) + " failed: " + std::to_string(code));
    }
}
class session final {
public:
    session() = default;
    session(const session&) = delete;
    session& operator=(const session&) = delete;
    ~session() {
        if (active_) { (void)ControlTraceW(handle_, nullptr, &properties_.value, EVENT_TRACE_CONTROL_STOP); }
        if (consumer_ != INVALID_PROCESSTRACE_HANDLE) { (void)CloseTrace(consumer_); }
        if (worker_.joinable()) { worker_.join(); }
    }
    void start(collection& data) {
        const auto name = L"FuserDxgiApi-" + std::to_wstring(GetCurrentProcessId()) +
                          L"-" + std::to_wstring(GetTickCount64());
        check(StartTraceW(&handle_, name.c_str(), &properties_.value), "StartTraceW");
        active_ = true;
        provider_ = data.provider;
        EVENT_TRACE_LOGFILEW logfile{};
        logfile.LoggerName = properties_.name.data();
        logfile.ProcessTraceMode = PROCESS_TRACE_MODE_REAL_TIME | PROCESS_TRACE_MODE_EVENT_RECORD |
                                  PROCESS_TRACE_MODE_RAW_TIMESTAMP;
        logfile.EventRecordCallback = receive;
        logfile.Context = &data;
        consumer_ = OpenTraceW(&logfile);
        if (consumer_ == INVALID_PROCESSTRACE_HANDLE) { check(GetLastError(), "OpenTraceW"); }
        worker_ = std::thread([this] { process_result_ = ProcessTrace(&consumer_, 1, nullptr, nullptr); });
        event_ids ids{};
        std::array<EVENT_FILTER_DESCRIPTOR, 2> filters{};
        filters[0] = {reinterpret_cast<ULONGLONG>(data.pids.data()),
                      static_cast<ULONG>(data.pids.size() * sizeof(DWORD)), EVENT_FILTER_TYPE_PID};
        filters[1] = {reinterpret_cast<ULONGLONG>(&ids), static_cast<ULONG>(sizeof(ids)), EVENT_FILTER_TYPE_EVENT_ID};
        ENABLE_TRACE_PARAMETERS enable{};
        enable.Version = ENABLE_TRACE_PARAMETERS_VERSION_2;
        enable.EnableProperty = EVENT_ENABLE_PROPERTY_IGNORE_KEYWORD_0;
        enable.EnableFilterDesc = filters.data();
        enable.FilterDescCount = static_cast<ULONG>(filters.size());
        check(EnableTraceEx2(handle_, &data.provider, EVENT_CONTROL_CODE_ENABLE_PROVIDER,
                            TRACE_LEVEL_VERBOSE, 0x8000000000000002ULL, 0, 1000, &enable), "EnableTraceEx2");
    }
    const EVENT_TRACE_PROPERTIES& stop() {
        check(EnableTraceEx2(handle_, &provider_, EVENT_CONTROL_CODE_DISABLE_PROVIDER,
                            0, 0, 0, 1000, nullptr), "Disable provider");
        check(ControlTraceW(handle_, nullptr, &properties_.value, EVENT_TRACE_CONTROL_STOP), "StopTrace");
        active_ = false;
        worker_.join();
        if (process_result_ != ERROR_SUCCESS && process_result_ != ERROR_CANCELLED) {
            check(process_result_, "ProcessTrace");
        }
        return properties_.value;
    }
private:
    trace_properties properties_{};
    GUID provider_{};
    TRACEHANDLE handle_{};
    TRACEHANDLE consumer_{INVALID_PROCESSTRACE_HANDLE};
    std::thread worker_{};
    ULONG process_result_{ERROR_SUCCESS};
    bool active_{};
};
class owned_provider final {
public:
    owned_provider() { check(EventRegister(&owned_test_provider, nullptr, nullptr, &handle_), "EventRegister"); }
    owned_provider(const owned_provider&) = delete;
    owned_provider& operator=(const owned_provider&) = delete;
    ~owned_provider() { (void)EventUnregister(handle_); }
    void emit_pair() const {
        const EVENT_DESCRIPTOR start{42, 0, 0, TRACE_LEVEL_INFORMATION, 1, 9, 2};
        const EVENT_DESCRIPTOR stop{43, 0, 0, TRACE_LEVEL_INFORMATION, 2, 9, 2};
        check(EventWrite(handle_, &start, 0, nullptr), "EventWrite start");
        check(EventWrite(handle_, &stop, 0, nullptr), "EventWrite stop");
    }
private:
    REGHANDLE handle_{};
};
DWORD number(const wchar_t* input, DWORD low, DWORD high) {
    wchar_t* end{};
    errno = 0;
    const auto parsed = std::wcstoul(input, &end, 10);
    if (errno || end == input || *end || *input == L'-' || parsed < low || parsed > high) {
        throw std::runtime_error("Invalid numeric argument");
    }
    return static_cast<DWORD>(parsed);
}
class process_guard final {
public:
    explicit process_guard(DWORD pid) : handle_{OpenProcess(SYNCHRONIZE, FALSE, pid)} {
        if (!handle_) { throw std::runtime_error("A requested process is not accessible"); }
        if (!running()) {
            CloseHandle(handle_);
            handle_ = nullptr;
            throw std::runtime_error("A requested process is not running");
        }
    }
    process_guard(const process_guard&) = delete;
    process_guard& operator=(const process_guard&) = delete;
    process_guard(process_guard&& other) noexcept : handle_{std::exchange(other.handle_, nullptr)} {}
    process_guard& operator=(process_guard&&) = delete;
    ~process_guard() { if (handle_) { CloseHandle(handle_); } }
    bool running() const noexcept { return WaitForSingleObject(handle_, 0) == WAIT_TIMEOUT; }
private:
    HANDLE handle_{};
};
}
int wmain(int argc, wchar_t** argv) {
    try {
        const bool self_test = argc == 3 && std::wstring{argv[1]} == L"--self-test";
        if (!self_test && (argc < 4 || argc > 11)) {
            std::cerr << "Usage: fuser_dxgi_api_trace seconds output.csv pid [pid...]\n";
            return 2;
        }
        const auto seconds = self_test ? 1UL : number(argv[1], 1, 30);
        collection data{};
        std::vector<process_guard> processes;
        processes.reserve(8);
        data.rows.reserve(100000);
        std::optional<owned_provider> synthetic;
        if (self_test) {
            data.provider = owned_test_provider;
            data.pids.push_back(GetCurrentProcessId());
            processes.emplace_back(GetCurrentProcessId());
            synthetic.emplace();
        }
        for (int i = 3; i < argc; ++i) {
            const auto pid = number(argv[i], 1, std::numeric_limits<DWORD>::max());
            if (std::find(data.pids.begin(), data.pids.end(), pid) != data.pids.end()) {
                throw std::runtime_error("Duplicate process ID");
            }
            processes.emplace_back(pid);
            data.pids.push_back(pid);
        }
        std::ofstream csv{std::filesystem::path{argv[2]}, std::ios::out | std::ios::trunc};
        if (!csv) { throw std::runtime_error("Cannot open output CSV"); }
        LARGE_INTEGER frequency{}, began{}, ended{};
        if (!QueryPerformanceFrequency(&frequency)) { throw std::runtime_error("QPC frequency unavailable"); }
        session trace{};
        trace.start(data);
        QueryPerformanceCounter(&began);
        if (synthetic) {
            for (unsigned i = 0; i < 100; ++i) { synthetic->emit_pair(); }
        }
        std::this_thread::sleep_for(std::chrono::seconds{seconds});
        QueryPerformanceCounter(&ended);
        const auto& result = trace.stop();
        csv << "pid,thread_id,event_id,qpc,qpc_frequency\n";
        std::size_t window_events{};
        for (const auto& event : data.rows) {
            if (event.qpc >= began.QuadPart && event.qpc <= ended.QuadPart) {
                ++window_events;
                csv << event.pid << ',' << event.tid << ',' << event.id << ',' << event.qpc << ','
                    << frequency.QuadPart << '\n';
            }
        }
        csv.flush();
        if (!csv) { throw std::runtime_error("CSV write failed"); }
        std::cout << "duration_s=" << static_cast<double>(ended.QuadPart - began.QuadPart) / frequency.QuadPart
                  << " events_in_window=" << window_events << " events_lost=" << result.EventsLost
                  << " realtime_buffers_lost=" << result.RealTimeBuffersLost
                  << " log_buffers_lost=" << result.LogBuffersLost
                  << " capacity_exceeded=" << data.overflow << " callback_failed=" << data.callback_failed << '\n';
        bool valid = !data.overflow && !data.callback_failed && result.EventsLost == 0 &&
                     result.RealTimeBuffersLost == 0 && result.LogBuffersLost == 0;
        for (std::size_t index = 0; index < data.pids.size(); ++index) {
            const auto pid = data.pids[index];
            const auto starts = std::count_if(data.rows.begin(), data.rows.end(), [=](const row& event) {
                return event.pid == pid && event.id == 42 && event.qpc >= began.QuadPart && event.qpc <= ended.QuadPart;
            });
            const auto stops = std::count_if(data.rows.begin(), data.rows.end(), [=](const row& event) {
                return event.pid == pid && event.id == 43 && event.qpc >= began.QuadPart && event.qpc <= ended.QuadPart;
            });
            const bool alive = processes[index].running();
            std::cout << "pid=" << pid << " starts=" << starts << " stops=" << stops << " still_running=" << alive << '\n';
            valid = valid && starts > 0 && stops > 0 && alive;
            if (self_test) { valid = valid && starts == 100 && stops == 100; }
        }
        std::cout << "event_capture_valid=" << valid << " synthetic=" << self_test << " displayed_fps_measured=0\n";
        return valid ? 0 : 3;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
