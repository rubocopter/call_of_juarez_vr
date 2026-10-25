#include "runtime/graphics_settings.hpp"
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <winioctl.h>
#include <array>
#include <atomic>
#include <bit>
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <limits>
#include <string>
#include <thread>
#include <vector>

using namespace cojvr::runtime;
namespace {
unsigned checks = 0;
void Require(bool condition, const char* why) {
    ++checks;
    if (!condition) { std::cerr << "FAIL: " << why << '\n'; std::exit(1); }
}
constexpr std::string_view neutral =
    "version=1\nenabled=true\nsharpness=0\ncontrast=1\nsaturation=1\ngamma=1\nedge_smoothing=false\n";
std::string Changed(std::string_view key, std::string_view value) {
    std::string text(neutral);
    const auto start = text.find(std::string(key) + "=") + key.size() + 1;
    text.replace(start, text.find('\n', start) - start, value);
    return text;
}
bool Neutral(const GraphicsSettings& values) {
    return !values.enabled && values.sharpness == 0.0F && values.contrast == 1.0F &&
        values.saturation == 1.0F && values.gamma == 1.0F && !values.edge_smoothing;
}
void Invalid(std::string_view text) {
    const auto result = ParseGraphicsSettings(text);
    Require(result.status == GraphicsSettingsStatus::invalid, "malformed settings admitted");
    Require(result.error_code != 0, "invalid settings lack diagnostics");
    Require(Neutral(result.values), "invalid settings retained partial effects");
    Require(!GraphicsEffectsActive(result.values), "invalid settings enabled processing");
}
void Write(const std::filesystem::path& path, std::string_view text) {
    std::ofstream out(path, std::ios::binary);
    out.write(text.data(), static_cast<std::streamsize>(text.size()));
    out.close();
    Require(!out.fail(), "fixture file write failed");
}
struct Fixtures {
    std::filesystem::path root = std::filesystem::temp_directory_path() /
        ("cojvr-graphics-test-" + std::to_string(GetCurrentProcessId()) + "-" +
         std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    Fixtures() { std::filesystem::create_directory(root); }
    ~Fixtures() { std::error_code error; std::filesystem::remove_all(root, error); }
};
std::string Bytes(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}
bool Same(const GraphicsSettings& a, const GraphicsSettings& b) {
    return a.enabled == b.enabled && a.edge_smoothing == b.edge_smoothing &&
        std::bit_cast<std::uint32_t>(a.sharpness) == std::bit_cast<std::uint32_t>(b.sharpness) &&
        std::bit_cast<std::uint32_t>(a.contrast) == std::bit_cast<std::uint32_t>(b.contrast) &&
        std::bit_cast<std::uint32_t>(a.saturation) == std::bit_cast<std::uint32_t>(b.saturation) &&
        std::bit_cast<std::uint32_t>(a.gamma) == std::bit_cast<std::uint32_t>(b.gamma);
}
constexpr GraphicsSettings first{true, 0.123456789F, 1.23456789F, 1.87654321F, 0.7654321F, true};
constexpr GraphicsSettings second{false, 0.875F, 0.625F, 0.25F, 1.875F, false};
bool ReplacementDenied(std::uint32_t error) {
    // MoveFileEx can report access denied for a destination whose open reader
    // denies deletion; CreateFile read denial reports a sharing violation.
    return error == ERROR_ACCESS_DENIED || error == ERROR_SHARING_VIOLATION;
}

void Persistence() {
    Fixtures fixtures;
    const auto file = fixtures.root / L"nested" / L"graphics.ini";
    const auto saved = SaveGraphicsSettings(file, first);
    Require(saved.ok && saved.error_code == 0, "save did not create missing directory/file");
    auto loaded = LoadGraphicsSettings(file);
    Require(loaded.status == GraphicsSettingsStatus::loaded && Same(loaded.values, first),
        "version-1 save did not round-trip exact complete float tuple");
    Require(ParseGraphicsSettings(Bytes(file)).status == GraphicsSettingsStatus::loaded,
        "save did not produce strict seven-key schema");
    for (float value : {0.0F, -0.0F, std::numeric_limits<float>::denorm_min(),
        std::numeric_limits<float>::min(), 0.333333343F, 0.99999994F, 1.0F}) {
        auto values = first;
        values.sharpness = value;
        values.saturation = value;
        Require(SaveGraphicsSettings(file, values).ok, "finite float save rejected");
        Require(Same(LoadGraphicsSettings(file).values, values), "finite float bits lost on persistence");
    }
    Write(fixtures.root / L"settings.ini", "cinematic sentinel");
    Write(fixtures.root / L"resolution.json", "resolution sentinel");
    Write(std::filesystem::path(file.wstring() + L".tmp-foreign"), "foreign temporary");
    Require(SaveGraphicsSettings(file, second).ok && Same(LoadGraphicsSettings(file).values, second),
        "existing valid file not atomically replaced");
    Require(Bytes(fixtures.root / L"settings.ini") == "cinematic sentinel" &&
        Bytes(fixtures.root / L"resolution.json") == "resolution sentinel", "save changed unrelated preferences");
    const auto valid_bytes = Bytes(file);
    for (auto member : {&GraphicsSettings::sharpness, &GraphicsSettings::contrast,
        &GraphicsSettings::saturation, &GraphicsSettings::gamma}) {
        auto bad = first;
        bad.*member = std::numeric_limits<float>::quiet_NaN();
        const auto rejected = SaveGraphicsSettings(file, bad);
        Require(!rejected.ok && rejected.error_code == ERROR_INVALID_PARAMETER && Bytes(file) == valid_bytes,
            "invalid direct save modified existing bytes");
    }
    for (const auto& bad : {std::filesystem::path{}, fixtures.root,
        std::filesystem::path(std::wstring(L"bad\0path", 8))}) {
        const auto rejected = SaveGraphicsSettings(bad, first);
        Require(!rejected.ok && rejected.error_code != 0, "invalid save path admitted");
    }
    for (auto text : {"version=2\n", "", "version=1\ngamma=nan\n"}) {
        Write(file, text);
        const auto rejected = SaveGraphicsSettings(file, first);
        Require(!rejected.ok && rejected.error_code != 0 && Bytes(file) == text,
            "direct save destroyed malformed/future/empty existing file");
    }
    Write(file, neutral);
    const auto before_denial = Bytes(file);
    // Reads permitted, replacement forbidden: exercises temp write/flush/close,
    // failed rename and owned-temp cleanup rather than just read preflight.
    HANDLE lock = CreateFileW(file.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
    Require(lock != INVALID_HANDLE_VALUE, "save-denial fixture failed");
    const auto denied = SaveGraphicsSettings(file, first);
    CloseHandle(lock);
    Require(!denied.ok && ReplacementDenied(denied.error_code) && Bytes(file) == before_denial,
        "denied replacement damaged old bytes or lost native diagnostics");
    unsigned entries = 0;
    for ([[maybe_unused]] const auto& entry : std::filesystem::directory_iterator(file.parent_path())) ++entries;
    Require(entries == 2 && Bytes(std::filesystem::path(file.wstring() + L".tmp-foreign")) == "foreign temporary",
        "failed save leaked own temporary or removed foreign sibling");
    lock = CreateFileW(file.c_str(), GENERIC_READ, 0, nullptr, OPEN_EXISTING, 0, nullptr);
    Require(lock != INVALID_HANDLE_VALUE, "direct read-denial fixture failed");
    const auto unreadable = SaveGraphicsSettings(file, second);
    CloseHandle(lock);
    Require(!unreadable.ok && unreadable.error_code == ERROR_SHARING_VIOLATION && Bytes(file) == before_denial,
        "direct save overwrote unreadable file");
}

void Store() {
    Fixtures fixtures;
    const auto file = fixtures.root / L"new" / L"graphics.ini";
    GraphicsPreferenceStore store;
    const auto before = CurrentGraphicsPreferences();
    Require(store.Read().revision == 0 && Neutral(store.Read().values), "uninitialized store not neutral");
    auto result = store.Apply(first, 0);
    Require(!result.accepted && !result.saved && !result.conflict && result.error_code == ERROR_NOT_READY &&
        CurrentGraphicsPreferences().revision == before.revision && Same(CurrentGraphicsPreferences().values, before.values),
        "uninitialized apply wrote/published");
    auto initial = store.Initialize(file);
    Require(initial.revision == 1 && Neutral(initial.values) && initial.load_status == GraphicsSettingsStatus::missing &&
        initial.load_error != 0 && !initial.save_failed && !std::filesystem::exists(file),
        "missing initialization wrote file or lost initial diagnostics");
    Require(CurrentGraphicsPreferences().revision == 1 && Neutral(CurrentGraphicsPreferences().values),
        "initialization did not publish neutral values/revision");
    const auto other = fixtures.root / L"other.ini";
    Write(other, neutral);
    Require(store.Initialize(other).load_status == GraphicsSettingsStatus::missing && !std::filesystem::exists(file),
        "second initialization reloaded/replaced original path");
    result = store.Apply(first, initial.revision);
    Require(result.accepted && result.saved && !result.conflict && result.error_code == 0 &&
        result.snapshot.revision == 2 && Same(result.snapshot.values, first) && !result.snapshot.save_failed,
        "valid edit failed to advance revision/save exact tuple");
    Require(Same(LoadGraphicsSettings(file).values, first) && Bytes(other) == neutral,
        "first initialized path did not retain ownership");
    Require(CurrentGraphicsPreferences().revision == 2 && Same(CurrentGraphicsPreferences().values, first),
        "accepted edit not published coherently");
    Require(result.snapshot.load_status == GraphicsSettingsStatus::missing && result.snapshot.load_error == initial.load_error,
        "save rewrote startup load diagnostics");
    const auto saved_bytes = Bytes(file);
    const auto stale = store.Apply(second, 1);
    Require(!stale.accepted && !stale.saved && stale.conflict && stale.error_code == ERROR_REVISION_MISMATCH &&
        stale.snapshot.revision == 2 && Same(stale.snapshot.values, first) && Bytes(file) == saved_bytes,
        "stale editor overwrote newer edit or failed to return current tuple");
    auto bad = second; bad.gamma = 3.0F;
    result = store.Apply(bad, 2);
    Require(!result.accepted && !result.conflict && result.error_code == ERROR_INVALID_PARAMETER &&
        store.Read().revision == 2 && Bytes(file) == saved_bytes && Same(CurrentGraphicsPreferences().values, first),
        "invalid editor values partially committed or published");
    HANDLE lock = CreateFileW(file.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
    Require(lock != INVALID_HANDLE_VALUE, "store save-denial fixture failed");
    result = store.Apply(second, 2);
    CloseHandle(lock);
    Require(result.accepted && !result.saved && !result.conflict && ReplacementDenied(result.error_code) &&
        result.snapshot.revision == 3 && result.snapshot.save_failed && result.snapshot.save_error == result.error_code &&
        Same(result.snapshot.values, second) && Bytes(file) == saved_bytes,
        "save failure lost accepted session values/old bytes/native diagnostics");
    const auto published = CurrentGraphicsPreferences();
    Require(published.revision == 3 && Same(published.values, second), "failed save left session choice unpublished");
    Require(store.Initialize(other).revision == 3 && store.Read().save_failed,
        "reinitialization reset revision or save failure");
    result = store.Apply(second, 3);
    Require(result.accepted && result.saved && result.snapshot.revision == 4 && !result.snapshot.save_failed &&
        result.snapshot.save_error == 0 && Same(LoadGraphicsSettings(file).values, second),
        "unchanged-value retry did not persist/advance/clear error");
    Write(file, "future format");
    result = store.Apply(first, 4);
    Require(result.accepted && !result.saved && result.snapshot.save_failed && Bytes(file) == "future format" &&
        Same(CurrentGraphicsPreferences().values, first), "later external invalid file was overwritten by editor");
    GraphicsPreferenceStore loaded;
    Require(SaveGraphicsSettings(other, first).ok, "loaded store fixture failed");
    initial = loaded.Initialize(other);
    Require(initial.load_status == GraphicsSettingsStatus::loaded && initial.load_error == 0 &&
        Same(initial.values, first) && Same(CurrentGraphicsPreferences().values, first), "valid initial tuple not loaded/published");
}

void ProtectedStartup() {
    Fixtures fixtures;
    for (bool unreadable : {false, true}) {
        const auto file = fixtures.root / (unreadable ? L"unreadable.ini" : L"invalid.ini");
        Write(file, unreadable ? neutral : "version=2\n");
        const auto bytes = Bytes(file);
        HANDLE lock = INVALID_HANDLE_VALUE;
        if (unreadable) {
            lock = CreateFileW(file.c_str(), GENERIC_READ, 0, nullptr, OPEN_EXISTING, 0, nullptr);
            Require(lock != INVALID_HANDLE_VALUE, "startup read-denial fixture failed");
        }
        GraphicsPreferenceStore store;
        const auto initial = store.Initialize(file);
        if (unreadable) CloseHandle(lock);
        Require(initial.load_status == (unreadable ? GraphicsSettingsStatus::io_error : GraphicsSettingsStatus::invalid) &&
            initial.load_error != 0 && Neutral(initial.values) && !initial.save_failed && initial.revision == 1,
            "bad initial load did not retain neutral startup diagnostics");
        auto applied = store.Apply(first, 1);
        Require(applied.accepted && !applied.saved && !applied.conflict && applied.error_code == initial.load_error &&
            applied.snapshot.save_failed && Same(applied.snapshot.values, first) && Bytes(file) == bytes,
            "invalid/unreadable initial file not protected with session-only edits");
        Write(file, neutral); // External repair must not authorize this session's writer.
        applied = store.Apply(second, 2);
        Require(applied.accepted && !applied.saved && applied.snapshot.revision == 3 && Bytes(file) == neutral &&
            Same(CurrentGraphicsPreferences().values, second), "external repair removed startup protection");
        std::filesystem::remove(file);
        applied = store.Apply(first, 3);
        Require(applied.accepted && !applied.saved && !std::filesystem::exists(file) && applied.snapshot.revision == 4 &&
            store.Initialize(fixtures.root / L"different.ini").load_status == initial.load_status,
            "removal/reinitialization bypassed whole-session startup protection");
    }
    GraphicsPreferenceStore empty;
    const auto initial = empty.Initialize({});
    const auto applied = empty.Apply(first, initial.revision);
    Require(initial.load_status == GraphicsSettingsStatus::io_error && applied.accepted && !applied.saved &&
        applied.error_code == ERROR_INVALID_NAME, "invalid initial path lost session-only behavior");
}

void ConcurrentEditors() {
    Fixtures fixtures;
    const auto file = fixtures.root / L"graphics.ini";
    GraphicsPreferenceStore store;
    const auto initial = store.Initialize(file);
    std::atomic<bool> start{false};
    std::array<GraphicsPreferenceApplyResult, 2> results;
    std::thread a([&] { while (!start.load()) std::this_thread::yield(); results[0] = store.Apply(first, initial.revision); });
    std::thread b([&] { while (!start.load()) std::this_thread::yield(); results[1] = store.Apply(second, initial.revision); });
    start = true;
    a.join(); b.join();
    Require(results[0].accepted != results[1].accepted && results[0].conflict != results[1].conflict,
        "two same-revision editors both committed");
    const auto& winner = results[results[0].accepted ? 0 : 1];
    const auto& loser = results[results[0].accepted ? 1 : 0];
    Require(winner.saved && !loser.saved && loser.error_code == ERROR_REVISION_MISMATCH &&
        loser.snapshot.revision == 2 && Same(loser.snapshot.values, winner.snapshot.values) &&
        Same(LoadGraphicsSettings(file).values, winner.snapshot.values), "concurrent loser/disk disagrees with winner");
}

void PublicationAndIoOrdering() {
    Fixtures fixtures;
    const auto file = fixtures.root / L"graphics.ini";
    Write(file, neutral);
    GraphicsPreferenceStore store;
    const auto initial = store.Initialize(file);
    // A real NTFS oplock holds the save's destination open until we release it.
    // The renderer must see the new tuple while the editor is blocked in I/O.
    HANDLE oplock = CreateFileW(file.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        nullptr, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, nullptr);
    Require(oplock != INVALID_HANDLE_VALUE, "oplock open failed");
    OVERLAPPED overlapped{};
    overlapped.hEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    Require(overlapped.hEvent != nullptr, "oplock event creation failed");
    DWORD bytes = 0;
    const BOOL requested = DeviceIoControl(oplock, FSCTL_REQUEST_OPLOCK_LEVEL_1, nullptr, 0, nullptr, 0, &bytes, &overlapped);
    Require(!requested && GetLastError() == ERROR_IO_PENDING, "local fixture does not grant oplock");
    GraphicsPreferenceApplyResult first_result, next_result;
    std::thread editor([&] { first_result = store.Apply(first, initial.revision); });
    const bool blocked = WaitForSingleObject(overlapped.hEvent, 5000) == WAIT_OBJECT_0;
    HANDLE read_done = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    Require(read_done != nullptr, "publication event creation failed");
    GraphicsPreferenceSnapshot during_io;
    std::thread renderer([&] { during_io = CurrentGraphicsPreferences(); SetEvent(read_done); });
    const bool read_without_io = WaitForSingleObject(read_done, 1000) == WAIT_OBJECT_0;
    // A newer editor must wait for the first save; otherwise older bytes could
    // land last. Both threads use explicit expected revisions.
    std::thread next_editor([&] { next_result = store.Apply(second, 2); });
    CloseHandle(oplock);
    editor.join(); next_editor.join(); renderer.join();
    CloseHandle(read_done); CloseHandle(overlapped.hEvent);
    Require(blocked, "save never reached held filesystem boundary");
    Require(read_without_io && during_io.revision == 2 && Same(during_io.values, first),
        "publication waited on editor/file I/O or happened after save");
    Require(first_result.accepted && first_result.saved && first_result.snapshot.revision == 2 &&
        next_result.accepted && next_result.saved && next_result.snapshot.revision == 3 &&
        Same(LoadGraphicsSettings(file).values, second) && Same(CurrentGraphicsPreferences().values, second),
        "editor persistence/publication completed out of revision order");
}

void CoherentPublication() {
    Fixtures fixtures;
    const auto file = fixtures.root / L"protected.ini";
    Write(file, "unrecognized"); // Session-only edits stress publication without disk latency.
    GraphicsPreferenceStore store;
    const auto initial = store.Initialize(file);
    std::atomic<bool> start{false}, done{false}, mixed{false};
    std::atomic<unsigned> observations{0};
    std::array<std::thread, 4> readers;
    for (auto& reader : readers) reader = std::thread([&] {
        while (!start.load()) std::this_thread::yield();
        do {
            const auto current = CurrentGraphicsPreferences();
            const bool valid = current.revision == 1 ? Neutral(current.values) :
                Same(current.values, current.revision % 2 == 0 ? first : second);
            if (!valid) mixed = true;
            ++observations;
        } while (!done.load());
    });
    start = true;
    auto revision = initial.revision;
    for (unsigned i = 0; i < 2000; ++i) {
        const auto result = store.Apply(i % 2 == 0 ? first : second, revision);
        if (!result.accepted || result.saved || result.snapshot.revision != revision + 1) mixed = true;
        revision = result.snapshot.revision;
    }
    done = true;
    for (auto& reader : readers) reader.join();
    Require(observations > 0 && !mixed && CurrentGraphicsPreferences().revision == 2001,
        "renderer observed an unpublished mixed values/revision tuple");
    Require(Bytes(file) == "unrecognized", "publication stress modified protected bytes");
    // The process singleton is the same owner used by native editors/presenter.
    auto& shared = SharedGraphicsPreferences();
    const auto snapshot = shared.Initialize(fixtures.root / L"shared.ini");
    const auto applied = shared.Apply(first, snapshot.revision);
    Require(&shared == &SharedGraphicsPreferences() && applied.accepted && applied.saved &&
        CurrentGraphicsPreferences().revision == applied.snapshot.revision && Same(CurrentGraphicsPreferences().values, first),
        "shared editor and renderer publication do not agree");
}
}
int main(int argc,char** argv) {
    if(argc==2) {
        const auto sample=LoadGraphicsSettings(std::filesystem::path(argv[1]));
        Require(sample.status==GraphicsSettingsStatus::loaded,"shipped example is invalid");
        Require(!GraphicsEffectsActive(sample.values),"shipped example enables unvalidated effects");
        std::cout<<"Graphics example parses and remains disabled\n";
        return 0;
    }
    const auto parsed = ParseGraphicsSettings(neutral);
    Require(parsed.status == GraphicsSettingsStatus::loaded && parsed.error_code == 0,
        "complete neutral document did not load");
    Require(parsed.values.enabled && ValidGraphicsSettings(parsed.values), "valid enabled settings lost");
    Require(!GraphicsEffectsActive(parsed.values), "neutral settings did not bypass processing");
    Require(Neutral(GraphicsSettings{}), "defaults are not neutral and disabled");
    Require(ValidGraphicsSettings({}), "neutral defaults invalid");

    const auto rich = ParseGraphicsSettings(
        " ; experimental\r\n\t# local choice\r\n gamma = 1.25\r\n"
        "saturation=1.5\r\n contrast = 0.75\r\nsharpness=0.5\r\n"
        "edge_smoothing=true\r\nenabled=true\r\nversion=1");
    Require(rich.status == GraphicsSettingsStatus::loaded && rich.values.enabled &&
        rich.values.sharpness == 0.5F && rich.values.contrast == 0.75F &&
        rich.values.saturation == 1.5F && rich.values.gamma == 1.25F && rich.values.edge_smoothing,
        "CRLF/comments/reordered values did not round-trip");
    Require(GraphicsEffectsActive(rich.values), "enabled effects bypassed");
    auto disabled = rich.values; disabled.enabled = false;
    Require(ValidGraphicsSettings(disabled) && !GraphicsEffectsActive(disabled), "disabled effects active");

    for (const auto& entry : std::array<std::pair<std::string_view, std::string_view>, 9>{{
        {"sharpness", "1"}, {"contrast", "0.5"}, {"contrast", "1.5"},
        {"saturation", "0"}, {"saturation", "2"}, {"gamma", "0.5"},
        {"gamma", "2"}, {"edge_smoothing", "true"}, {"sharpness", "5e-1"}}}) {
        const auto result = ParseGraphicsSettings(Changed(entry.first, entry.second));
        Require(result.status == GraphicsSettingsStatus::loaded && GraphicsEffectsActive(result.values),
            "inclusive boundary or independent effect rejected");
    }
    const std::array<std::string_view, 7> keys{
        "version", "enabled", "sharpness", "contrast", "saturation", "gamma", "edge_smoothing"};
    for (auto key : keys) {
        std::string missing(neutral);
        const auto start = missing.find(std::string(key) + "=");
        missing.erase(start, missing.find('\n', start) - start + 1);
        Invalid(missing);
        const auto line_end = neutral.find('\n', start);
        Invalid(std::string(neutral) + std::string(neutral.substr(start, line_end - start)) + "\n");
        Invalid(Changed(key, ""));
    }
    for (auto key : {"sharpness", "contrast", "saturation", "gamma"}) {
        for (auto value : {"nan", "NaN", "inf", "-inf", "Infinity", "1e1000", "1e-1000",
            "1.0junk", "1,0", "0x1p0", "1 0", "1=0", "\"1\"", ".", "--1", "+-1"})
            Invalid(Changed(key, value));
    }
    for (auto key : {"enabled", "edge_smoothing"})
        for (auto value : {"True", "FALSE", "1", "0", "yes", "false # comment", "true;comment"})
            Invalid(Changed(key, value));
    for (const auto& entry : std::array<std::pair<std::string_view, std::string_view>, 11>{{
        {"version", "2"}, {"version", "1.0"}, {"version", "01"},
        {"sharpness", "-0.001"}, {"sharpness", "1.001"}, {"contrast", "0.499"},
        {"contrast", "1.501"}, {"saturation", "-0.001"}, {"saturation", "2.001"},
        {"gamma", "0.499"}, {"gamma", "2.001"}}}) Invalid(Changed(entry.first, entry.second));
    for (auto text : {"", "# comment only\n; comment\n", "[graphics]\n", "version 1\n"}) Invalid(text);
    Invalid(std::string(neutral) + "unknown=1\n");
    Invalid(std::string(neutral) + "Enabled=true\n");
    Invalid(Changed("gamma", "1\rgamma=2"));
    auto nul = std::string(neutral); nul.push_back('\0'); Invalid(nul);
    Invalid(std::string("\xEF\xBB\xBF") + std::string(neutral));
    auto bounded = std::string(neutral) + "#";
    bounded.resize(8192, 'x');
    Require(ParseGraphicsSettings(bounded).status == GraphicsSettingsStatus::loaded, "8192-byte document rejected");
    Invalid(bounded + "x");

    for (auto member : {&GraphicsSettings::sharpness, &GraphicsSettings::contrast,
        &GraphicsSettings::saturation, &GraphicsSettings::gamma}) {
        for (float value : {std::numeric_limits<float>::quiet_NaN(),
            std::numeric_limits<float>::infinity(), -std::numeric_limits<float>::infinity(), -1.0F, 3.0F}) {
            auto values = rich.values; values.*member = value;
            Require(!ValidGraphicsSettings(values) && !GraphicsEffectsActive(values), "invalid direct settings active");
            values.enabled = false;
            Require(!ValidGraphicsSettings(values), "disabled flag bypassed validation");
        }
    }

    Fixtures fixtures;
    const auto file = fixtures.root / L"gr\u00e1ficos.ini";
    auto loaded = LoadGraphicsSettings(file);
    Require(loaded.status == GraphicsSettingsStatus::missing && Neutral(loaded.values), "missing file not neutral");
    Require(!std::filesystem::exists(file), "read created missing config");
    Write(file, Changed("edge_smoothing", "true"));
    loaded = LoadGraphicsSettings(file);
    Require(loaded.status == GraphicsSettingsStatus::loaded && loaded.values.edge_smoothing &&
        GraphicsEffectsActive(loaded.values), "Unicode-path file did not load");
    const auto before_time = std::filesystem::last_write_time(file);
    Require(LoadGraphicsSettings(file).status == GraphicsSettingsStatus::loaded &&
        std::filesystem::last_write_time(file) == before_time, "load modified config");
    Write(file, Changed("gamma", "nan"));
    loaded = LoadGraphicsSettings(file);
    Require(loaded.status == GraphicsSettingsStatus::invalid && loaded.error_code != 0 && Neutral(loaded.values),
        "invalid file retained active settings");
    Write(file, bounded);
    Require(LoadGraphicsSettings(file).status == GraphicsSettingsStatus::loaded, "8192-byte file rejected");
    Write(file, bounded + "x");
    loaded = LoadGraphicsSettings(file);
    Require(loaded.status == GraphicsSettingsStatus::invalid && loaded.error_code != 0 && Neutral(loaded.values),
        "oversized file accepted");
    Write(file, "");
    Require(LoadGraphicsSettings(file).status == GraphicsSettingsStatus::invalid, "empty file treated as missing");
    for (const auto& path : {std::filesystem::path{}, fixtures.root}) {
        loaded = LoadGraphicsSettings(path);
        Require(loaded.status == GraphicsSettingsStatus::io_error && loaded.error_code != 0 && Neutral(loaded.values),
            "invalid path/directory lacks I/O diagnostics");
    }
    HANDLE locked = CreateFileW(file.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr);
    Require(locked != INVALID_HANDLE_VALUE, "read-denial fixture failed");
    loaded = LoadGraphicsSettings(file);
    CloseHandle(locked);
    Require(loaded.status == GraphicsSettingsStatus::io_error && loaded.error_code == ERROR_SHARING_VIOLATION &&
        Neutral(loaded.values), "unreadable config did not fail closed with native error");

    std::wstring local(32768, L'\0');
    const auto length = GetEnvironmentVariableW(L"LOCALAPPDATA", local.data(), static_cast<DWORD>(local.size()));
    if (length && length < local.size()) {
        local.resize(length);
        Require(DefaultGraphicsSettingsPath() == std::filesystem::path(local) / L"CoJVR" / L"CallOfJuarez" / L"graphics.ini",
            "default path differs from dedicated local graphics path");
    }
    Require(checks == 556, "legacy parser/load baseline changed");
    const auto legacy_checks = checks - 1;
    for(unsigned scale=0;scale<=4;++scale)for(unsigned horse=0;horse<=2;++horse) {
        auto text=Changed("version","3")+"reconstruction=dlaa\nrender_scale="+std::to_string(scale)+
            "\nhorse_comfort="+std::to_string(horse)+"\n";
        const auto legacy_loaded=ParseGraphicsSettings(text);
        Require(legacy_loaded.status==GraphicsSettingsStatus::loaded&&legacy_loaded.values.render_scale==(scale==4?0:scale)&&legacy_loaded.values.horse_comfort==horse,
            "v3 scale migration changed meaning or horse choice");
    }
    for(unsigned scale=0;scale<=5;++scale) {
        const auto tail="reconstruction=dlaa\nrender_scale="+std::to_string(scale)+"\nhorse_comfort=2\n";
        const auto text=Changed("version","4")+tail;
        const auto v4_loaded=ParseGraphicsSettings(text);
        Require(v4_loaded.status==GraphicsSettingsStatus::loaded&&v4_loaded.values.render_scale==scale&&
            v4_loaded.values.reconstruction==ReconstructionMode::dlaa&&v4_loaded.values.horse_comfort==2,
            "v4 complete ten-key scale tuple rejected");
        auto reordered=text.substr(text.find('\n')+1)+"version=4\n";
        Require(ParseGraphicsSettings(reordered).values.render_scale==scale&&
            ParseGraphicsSettings(reordered).status==GraphicsSettingsStatus::loaded,"v4 parser depends on version order");
        const auto scale_file=fixtures.root/("scale-"+std::to_string(scale)+".ini");
        Require(SaveGraphicsSettings(scale_file,v4_loaded.values).ok&&Bytes(scale_file).starts_with("version=4\n")&&
            LoadGraphicsSettings(scale_file).values.render_scale==scale,"v4 scale save/reload lost selection");
        for(const auto key : {"version","enabled","sharpness","contrast","saturation","gamma","edge_smoothing",
            "reconstruction","render_scale","horse_comfort"}) {
            auto missing=text;const auto start=missing.find(std::string(key)+"=");
            const auto end=missing.find('\n',start);
            const auto line=missing.substr(start,end-start+1);
            missing.erase(start,end-start+1);Invalid(missing);Invalid(text+line);
        }
    }
    for(const auto version : {"3","4"})for(const auto scale : {"-1","6","9","05","5.0","+5","5 #comment"})
        Invalid(Changed("version",version)+"reconstruction=native\nrender_scale="+scale+"\nhorse_comfort=0\n");
    Invalid(Changed("version","3")+"reconstruction=native\nrender_scale=5\nhorse_comfort=0\n");
    Invalid(Changed("version","5")+"reconstruction=native\nrender_scale=0\nhorse_comfort=0\n");
    const auto legacy_file=fixtures.root/L"legacy-manual.ini";
    const auto legacy=Changed("version","3")+"reconstruction=dlss_quality\nrender_scale=4\nhorse_comfort=2\n";
    Write(legacy_file,legacy);const auto legacy_time=std::filesystem::last_write_time(legacy_file);
    GraphicsPreferenceStore migrating;
    const auto migrated=migrating.Initialize(legacy_file);
    Require(migrated.load_status==GraphicsSettingsStatus::loaded&&migrated.values.render_scale==0&&
        migrated.values.horse_comfort==2&&migrated.values.reconstruction==ReconstructionMode::dlss_quality&&
        Bytes(legacy_file)==legacy&&std::filesystem::last_write_time(legacy_file)==legacy_time,
        "legacy Manual migration rewrote file on load or lost unrelated fields");
    Require(migrating.Apply(migrated.values,migrated.revision).saved&&Bytes(legacy_file).starts_with("version=4\n")&&
        LoadGraphicsSettings(legacy_file).values.render_scale==0,"accepted save failed to persist migrated Auto in v4");
    auto invalid_mode=GraphicsSettings{};invalid_mode.horse_comfort=3;
    Require(!ValidGraphicsSettings(invalid_mode),"unknown horse mode admitted");
    invalid_mode={};invalid_mode.render_scale=6;Require(!ValidGraphicsSettings(invalid_mode),"unknown scale admitted");
    PublishMountedMotion(false);Require(!CurrentMountedMotion(),"unpublished mounted motion active");
    PublishMountedMotion(true);Require(CurrentMountedMotion(),"positive mounted motion not published");
    PublishMountedMotion(false);Require(!CurrentMountedMotion(),"mount loss retained comfort lease");
    PublishMountedMotion(true);std::this_thread::sleep_for(std::chrono::milliseconds(170));
    Require(!CurrentMountedMotion(),"stale mounted motion retained comfort lease");
    Require(CurrentMountedMotionLeaseMs()==0,"expired lease diagnostic retained motion");
    PublishMountedMotion(true);
    const auto lease=CurrentMountedMotionLeaseMs();
    Require(lease>0&&lease<=150,"published lease diagnostic differs from processing gate");
    PublishMountedMotion(false);
    Require(CurrentMountedMotionLeaseMs()==0,"revocation diagnostic retained motion");
    const auto horse_file=fixtures.root/L"horse.ini";
    for(unsigned horse=0;horse<=2;++horse) {
        auto values=first;values.horse_comfort=static_cast<std::uint8_t>(horse);
        Require(SaveGraphicsSettings(horse_file,values).ok,"horse choice save failed");
        Require(LoadGraphicsSettings(horse_file).values.horse_comfort==horse,"saved horse choice lost on reload");
        GraphicsPreferenceStore store;
        const auto initial=store.Initialize(horse_file);
        Require(initial.values.horse_comfort==horse,"startup publication lost horse choice");
        values.horse_comfort=static_cast<std::uint8_t>((horse+1)%3);
        const auto edited=store.Apply(values,initial.revision);
        Require(edited.accepted&&edited.saved&&CurrentGraphicsPreferences().values.horse_comfort==values.horse_comfort,
            "accepted horse edit failed live publication/persistence");
    }
    Require(parsed.values.reconstruction==ReconstructionMode::native,"version-1 migration enabled reconstruction");
    for(const auto mode:{std::string_view("native"),std::string_view("dlaa"),std::string_view("dlss_quality")}) {
        auto text=Changed("version","2");text+="reconstruction=";text+=mode;text+='\n';
        const auto temporal=ParseGraphicsSettings(text);
        Require(temporal.status==GraphicsSettingsStatus::loaded,"version-2 reconstruction preference rejected");
        const auto expected=mode=="dlaa"?ReconstructionMode::dlaa:mode=="dlss_quality"?ReconstructionMode::dlss_quality:ReconstructionMode::native;
        Require(temporal.values.reconstruction==expected,"reconstruction selection changed");
        Require(!GraphicsEffectsActive(temporal.values),"requested temporal mode enabled spatial processing");
    }
    Invalid(std::string(neutral)+"reconstruction=dlaa\n");
    Invalid(Changed("version","2")+"reconstruction=unknown\n");
    Invalid(Changed("version","2")+"reconstruction=DLAA\n");

    Persistence();
    Store();
    ProtectedStartup();
    ConcurrentEditors();
    PublicationAndIoOrdering();
    CoherentPublication();
    std::cout << "Graphics settings: " << checks << " checks passed (" << legacy_checks
        << " legacy parser/load checks)\n";
}
