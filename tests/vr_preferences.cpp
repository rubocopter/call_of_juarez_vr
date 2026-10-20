#include "runtime/vr_settings.hpp"
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <array>
#include <atomic>
#include <barrier>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <thread>

using namespace cojvr::runtime;
namespace {
void Require(bool value, const char* why) {
    if (!value) throw std::runtime_error(why);
}
void Write(const std::filesystem::path& file, std::string_view bytes) {
    std::filesystem::create_directories(file.parent_path());
    std::ofstream out(file, std::ios::binary);
    out << bytes;
    Require(out.good(), "fixture write failed");
}
std::string Bytes(const std::filesystem::path& file) {
    std::ifstream in(file, std::ios::binary);
    Require(in.good(), "fixture read failed");
    return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}
bool Equal(VrPreferences a, VrPreferences b) {
    return a.cinematics == b.cinematics && a.turning == b.turning;
}
void Published(VrPreferences expected) {
    Require(CurrentCinematicMode() == expected.cinematics && CurrentTurnMode() == expected.turning,
        "session choice was not published to atomic game-thread readers");
}
void Persisted(const std::filesystem::path& file, VrPreferences expected) {
    const auto loaded = LoadVrSettings(file);
    Require(loaded.status == SettingsLoadStatus::loaded && loaded.mode == expected.cinematics &&
        loaded.turning == expected.turning, "disk values differ from the accepted preference pair");
}
struct TemporaryDirectory {
    std::filesystem::path path = std::filesystem::temp_directory_path() /
        ("cojvr-preferences-" + std::to_string(GetCurrentProcessId()) + "-" +
            std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    TemporaryDirectory() { std::filesystem::create_directories(path); }
    ~TemporaryDirectory() { std::error_code ignored; std::filesystem::remove_all(path, ignored); }
};
struct ReadLock {
    HANDLE handle;
    explicit ReadLock(const std::filesystem::path& file, DWORD share = FILE_SHARE_READ) :
        handle(CreateFileW(file.c_str(), GENERIC_READ, share, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr)) {
        Require(handle != INVALID_HANDLE_VALUE, "destination lock fixture failed");
    }
    ~ReadLock() { CloseHandle(handle); }
};

// Catches accepting an incomplete/ambiguous schema or discarding the v1 cinema choice.
void StrictSchemas(const std::filesystem::path& root) {
    const auto file = root / "schema.ini";
    Write(file, "version=2\ncinematics=immersive\nturning=snap\n");
    auto loaded = LoadVrSettings(file);
    Require(loaded.status == SettingsLoadStatus::loaded && loaded.mode == CinematicsMode::immersive,
        "strict v2 cinema + turning preferences must load");
    Require(loaded.turning == TurnMode::snap, "v2 snap choice was discarded");
    Write(file, " # preference\r\n turning = smooth\r\ncinematics=theater\r\n version = 2 ");
    loaded = LoadVrSettings(file);
    Require(loaded.status == SettingsLoadStatus::loaded && loaded.mode == CinematicsMode::theater &&
        loaded.turning == TurnMode::smooth, "v2 key order/CRLF broke strict parsing");
    for (auto cinema : {CinematicsMode::theater, CinematicsMode::immersive}) {
        Require(SaveVrSettings(file, cinema).ok, "legacy v1 saver failed");
        Require(Bytes(file) == (cinema == CinematicsMode::theater ?
            "version=1\ncinematics=theater\n" : "version=1\ncinematics=immersive\n"),
            "legacy saver changed its v1 API semantics");
        loaded = LoadVrSettings(file);
        Require(loaded.status == SettingsLoadStatus::loaded && loaded.mode == cinema &&
            loaded.turning == TurnMode::smooth, "v1 must migrate cinema with smooth turning");
        for (auto turning : {TurnMode::smooth, TurnMode::snap}) {
            Require(SaveVrSettings(file, VrPreferences{cinema, turning}).ok, "v2 saver failed");
            Persisted(file, {cinema, turning});
            Require(Bytes(file).starts_with("version=2\n"), "new saver did not emit v2");
        }
    }
    for (std::string_view invalid : {
        "", "version=0\ncinematics=immersive\n", "version=3\ncinematics=immersive\nturning=snap\n",
        "version=02\ncinematics=immersive\nturning=snap\n", "cinematics=immersive\nturning=snap\n",
        "version=2\nturning=snap\n", "version=2\ncinematics=immersive\n",
        "version=1\ncinematics=immersive\nturning=smooth\n", "version=1\n",
        "version=2\nversion=2\ncinematics=immersive\nturning=snap\n",
        "version=2\ncinematics=immersive\ncinematics=theater\nturning=snap\n",
        "version=2\ncinematics=immersive\nturning=snap\nturning=snap\n",
        "version=2\ncinematics=immersive\nturning=Snap\n", "version=2\ncinematics=other\nturning=snap\n",
        "version=2\ncinematics=immersive\nturning=snap\nunknown=1\n",
        "version=2\ncinematics=immersive\nturning=snap=1\n", "version=2\ncinematics=immersive\nturning=\n",
        "version=2\ncinematics=immersive\nturning=snap\nbroken\n"}) {
        Write(file, invalid);
        loaded = LoadVrSettings(file);
        Require(loaded.status == SettingsLoadStatus::invalid && loaded.mode == CinematicsMode::theater &&
            loaded.turning == TurnMode::smooth, "invalid schema admitted partial/unsafe preferences");
    }
    Write(file, std::string(4097, 'x'));
    Require(LoadVrSettings(file).status == SettingsLoadStatus::invalid, "oversized settings admitted");
    Write(file, std::string("version=2\ncinematics=immersive\nturning=snap\n") + '\0');
    Require(LoadVrSettings(file).status == SettingsLoadStatus::invalid, "embedded NUL admitted");
    Require(SaveVrSettings(file, VrPreferences{}).ok, "valid v2 baseline failed");
    const auto before = Bytes(file);
    for (auto invalid : {VrPreferences{static_cast<CinematicsMode>(99), TurnMode::smooth},
        VrPreferences{CinematicsMode::immersive, static_cast<TurnMode>(99)}}) {
        const auto save = SaveVrSettings(file, invalid);
        Require(!save.ok && save.error_code != 0 && Bytes(file) == before,
            "v2 saver admitted an invalid enum or damaged existing values");
    }
    Require(!SaveVrSettings({}, VrPreferences{}).ok, "v2 saver accepted empty path");
}

// An older runtime must never erase a newer schema, even after an explicit session edit/retry.
void PreserveUnrecognizedSettings(const std::filesystem::path& root) {
    const auto file = root / "future.ini";
    for (std::string_view bytes : {"version=9\ncinematics=immersive\nturning=snap\n",
        "version=9\ncinematics=immersive\nturning=snap\nfuture=keep\n",
        "version=2\ncinematics=immersive\n"}) {
        Write(file, bytes);
        VrPreferenceStore store;
        const auto initial = store.Initialize(file);
        Require(initial.load_status == SettingsLoadStatus::invalid, "unrecognized schema did not fail closed");
        const auto result = store.Apply({CinematicsMode::immersive, TurnMode::snap}, initial.revision);
        Require(result.accepted && !result.saved && !result.conflict && result.error_code != 0 &&
            result.snapshot.save_failed && result.snapshot.save_error == result.error_code && Bytes(file) == bytes,
            "owner erased unrecognized configuration instead of keeping a session-only preference");
        Published(result.snapshot.values);
        const auto retry = store.Apply(result.snapshot.values, result.snapshot.revision);
        Require(retry.accepted && !retry.saved && retry.snapshot.save_failed && Bytes(file) == bytes,
            "unchanged retry erased protected unrecognized configuration");
        const auto stale = store.Apply({}, initial.revision);
        Require(stale.conflict && !stale.accepted && !stale.saved && Bytes(file) == bytes,
            "protected persistence bypassed revision conflict handling");
    }
    Write(file, "version=9\ncinematics=immersive\nturning=snap\n");
    VrPreferenceStore unreadable;
    VrPreferenceSnapshot initial;
    {
        ReadLock locked(file, 0);
        initial = unreadable.Initialize(file);
        Require(initial.load_status == SettingsLoadStatus::io_error &&
            initial.load_error == ERROR_SHARING_VIOLATION && Equal(initial.values, {}),
            "startup failed to preserve the unreadable config diagnostic and safe defaults");
    }
    const auto result = unreadable.Apply({CinematicsMode::immersive, TurnMode::snap}, initial.revision);
    Require(result.accepted && !result.saved && result.snapshot.load_error == ERROR_SHARING_VIOLATION &&
        Bytes(file) == "version=9\ncinematics=immersive\nturning=snap\n",
        "read failure allowed a future schema to be erased after its lock was released");
}

// Catches reloads overwriting a session and initialization rewriting/misapplying bad files.
void Initialization(const std::filesystem::path& root) {
    const auto file = root / "initialize.ini";
    VrPreferenceStore uninitialized;
    const auto refused = uninitialized.Apply({CinematicsMode::immersive, TurnMode::snap},
        uninitialized.Read().revision);
    Require(!refused.accepted && !refused.saved && !refused.conflict && refused.error_code != 0,
        "uninitialized store accepted an unowned persistence destination");
    Require(SaveVrSettings(file, CinematicsMode::immersive).ok, "v1 fixture failed");
    VrPreferenceStore store;
    const auto initial = store.Initialize(file);
    Require(Equal(initial.values, {CinematicsMode::immersive, TurnMode::smooth}) &&
        initial.load_status == SettingsLoadStatus::loaded && initial.load_error == 0 &&
        !initial.save_failed && initial.save_error == 0,
        "owner failed to initialize migrated values");
    Published(initial.values);
    Require(Bytes(file) == "version=1\ncinematics=immersive\n", "initialize wrote a migration eagerly");
    const auto migrated = store.Apply(initial.values, initial.revision);
    Require(migrated.accepted && migrated.saved && !migrated.conflict && migrated.error_code == 0,
        "unchanged v1 preference could not be persisted as v2");
    Require(Bytes(file) == "version=2\ncinematics=immersive\nturning=smooth\n", "owner failed v1->v2 migration");
    const auto ignored = root / "ignored.ini";
    Write(ignored, "version=2\ncinematics=theater\nturning=snap\n");
    const auto again = store.Initialize(ignored);
    Require(again.revision == migrated.snapshot.revision && Equal(again.values, migrated.snapshot.values),
        "Initialize did not retain its once-only ownership");
    auto result = store.Apply({CinematicsMode::theater, TurnMode::smooth}, again.revision);
    Require(result.saved, "apply to original initialization path failed");
    Persisted(file, result.snapshot.values);
    Persisted(ignored, {CinematicsMode::theater, TurnMode::snap});

    const auto missing = root / "missing" / "settings.ini";
    VrPreferenceStore missing_store;
    auto snapshot = missing_store.Initialize(missing);
    Require(snapshot.load_status == SettingsLoadStatus::missing && snapshot.load_error == ERROR_PATH_NOT_FOUND &&
        Equal(snapshot.values, {}) &&
        !std::filesystem::exists(missing.parent_path()), "missing config did not use non-writing safe defaults");
    Published({});
    Write(file, "version=2\ncinematics=immersive\n");
    VrPreferenceStore invalid_store;
    snapshot = invalid_store.Initialize(file);
    Require(snapshot.load_status == SettingsLoadStatus::invalid && snapshot.load_error == 0 && Equal(snapshot.values, {}),
        "invalid config did not publish both safe defaults");
    Published({});
    Require(Bytes(file) == "version=2\ncinematics=immersive\n", "initialization overwrote malformed input");
    VrPreferenceStore io_store;
    snapshot = io_store.Initialize({});
    Require(snapshot.load_status == SettingsLoadStatus::io_error && snapshot.load_error == ERROR_INVALID_NAME &&
        Equal(snapshot.values, {}),
        "I/O failure did not initialize safe defaults");
    Published({});
    result = io_store.Apply({CinematicsMode::immersive, TurnMode::snap}, snapshot.revision);
    Require(result.accepted && !result.saved && result.snapshot.save_failed && result.error_code != 0,
        "unavailable persistence path prevented session choice or hid save failure");
    Published(result.snapshot.values);
}

// Catches stale editors overwriting another preference, and failure losing the session choice or old file.
void EditorsAndRetry(const std::filesystem::path& root) {
    const auto file = root / "editors" / "settings.ini";
    VrPreferenceStore store;
    const auto native = store.Initialize(file);
    const auto overlay = store.Read();
    const auto selected = store.Apply({CinematicsMode::theater, TurnMode::snap}, native.revision);
    Require(selected.accepted && selected.saved && !selected.conflict &&
        selected.snapshot.revision > native.revision, "native editor failed to save snap choice");
    const auto before = Bytes(file);
    const auto stale = store.ApplyCinematics(CinematicsMode::immersive, overlay.revision);
    Require(!stale.accepted && !stale.saved && stale.conflict && stale.error_code != 0 &&
        stale.snapshot.revision == selected.snapshot.revision && Equal(stale.snapshot.values, selected.snapshot.values) &&
        Bytes(file) == before, "stale overlay overwrote another editor's turn choice");
    Published(selected.snapshot.values);
    const auto fresh = store.ApplyCinematics(CinematicsMode::immersive, stale.snapshot.revision);
    Require(fresh.saved && fresh.accepted && Equal(fresh.snapshot.values, {CinematicsMode::immersive, TurnMode::snap}),
        "cinema-only overlay update reset turning");
    Persisted(file, fresh.snapshot.values);
    Published(fresh.snapshot.values);
    PublishCinematicMode(CinematicsMode::theater);
    Published({CinematicsMode::theater, TurnMode::snap});
    PublishCinematicMode(static_cast<CinematicsMode>(99));
    Published({CinematicsMode::theater, TurnMode::snap});

    const auto old_bytes = Bytes(file);
    VrPreferenceApplyResult failed;
    {
        ReadLock locked(file);
        failed = store.Apply({CinematicsMode::theater, TurnMode::smooth}, fresh.snapshot.revision);
        Require(failed.accepted && !failed.saved && !failed.conflict && failed.error_code != 0 &&
            failed.snapshot.save_failed && failed.snapshot.save_error == failed.error_code &&
            failed.snapshot.revision > fresh.snapshot.revision, "save failure was not distinct from session application");
        Published(failed.snapshot.values);
        Require(Bytes(file) == old_bytes, "locked failed save changed the previous file");
        const auto conflict = store.Apply(fresh.snapshot.values, fresh.snapshot.revision);
        Require(!conflict.accepted && conflict.conflict && !conflict.saved &&
            conflict.snapshot.save_failed && conflict.snapshot.save_error == failed.error_code &&
            conflict.snapshot.revision == failed.snapshot.revision, "conflict mutated save-failure state");
    }
    for (const auto& entry : std::filesystem::directory_iterator(file.parent_path()))
        Require(entry.path() == file, "failed replacement leaked an owned temporary");
    for (const auto invalid : {VrPreferences{static_cast<CinematicsMode>(99), TurnMode::smooth},
        VrPreferences{CinematicsMode::immersive, static_cast<TurnMode>(99)}}) {
        const auto rejected = store.Apply(invalid, failed.snapshot.revision);
        Require(!rejected.accepted && !rejected.saved && !rejected.conflict && rejected.error_code != 0 &&
            rejected.snapshot.revision == failed.snapshot.revision && rejected.snapshot.save_failed &&
            rejected.snapshot.save_error == failed.error_code && Bytes(file) == old_bytes,
            "invalid enums changed publication, revision, disk or last save status");
        Published(failed.snapshot.values);
    }
    const auto invalid_overlay = store.ApplyCinematics(static_cast<CinematicsMode>(99), failed.snapshot.revision);
    Require(!invalid_overlay.accepted && !invalid_overlay.saved && !invalid_overlay.conflict &&
        invalid_overlay.error_code != 0 && invalid_overlay.snapshot.revision == failed.snapshot.revision,
        "cinema-only path admitted invalid enum");
    const auto retry = store.Apply(failed.snapshot.values, failed.snapshot.revision);
    Require(retry.accepted && retry.saved && !retry.conflict && !retry.snapshot.save_failed &&
        retry.snapshot.save_error == 0 && retry.error_code == 0, "fresh unchanged choice did not retry persistence");
    Persisted(file, retry.snapshot.values);
    Require(!store.Apply(fresh.snapshot.values, failed.snapshot.revision).accepted,
        "successful retry did not retire the previous revision");
    // Deleting persistence externally cannot make Read perform I/O or lose session values.
    std::filesystem::remove(file);
    Require(Equal(store.Read().values, retry.snapshot.values) && !std::filesystem::exists(file),
        "Read reloaded or recreated the removed file");
    Published(retry.snapshot.values);
}

// Both editors start from one revision: exactly one wins, and disk must match that winner.
void ConcurrentEditors(const std::filesystem::path& root) {
    const auto file = root / "concurrent.ini";
    VrPreferenceStore store;
    (void)store.Initialize(file);
    std::atomic<bool> stop{false}, incoherent{false};
    std::thread reader([&] {
        while (!stop.load()) {
            const auto snapshot = store.Read();
            if (!Equal(snapshot.values, {CinematicsMode::theater, TurnMode::smooth}) &&
                !Equal(snapshot.values, {CinematicsMode::immersive, TurnMode::snap})) incoherent.store(true);
        }
    });
    bool correct = true;
    for (int round = 0; round < 32; ++round) {
        const auto revision = store.Read().revision;
        std::barrier start(3);
        std::array<VrPreferenceApplyResult, 2> results;
        std::thread native([&] { start.arrive_and_wait();
            results[0] = store.Apply({CinematicsMode::immersive, TurnMode::snap}, revision); });
        std::thread overlay([&] { start.arrive_and_wait(); results[1] = store.Apply({}, revision); });
        start.arrive_and_wait();
        native.join(); overlay.join();
        const auto& winner = results[results[0].accepted ? 0 : 1];
        const auto& loser = results[results[0].accepted ? 1 : 0];
        const auto snapshot = store.Read();
        const auto loaded = LoadVrSettings(file);
        correct = correct && results[0].accepted != results[1].accepted && winner.saved && !winner.conflict &&
            loser.conflict && !loser.saved && winner.snapshot.revision == revision + 1 &&
            loser.snapshot.revision == winner.snapshot.revision && Equal(snapshot.values, winner.snapshot.values) &&
            Equal(loser.snapshot.values, winner.snapshot.values) && loaded.status == SettingsLoadStatus::loaded &&
            loaded.mode == snapshot.values.cinematics && loaded.turning == snapshot.values.turning;
    }
    stop.store(true); reader.join();
    Require(correct, "concurrent requests both won, overwrote the winner, or desynchronized disk/publication");
    Require(!incoherent.load(), "reader observed a torn preference pair");
    Published(store.Read().values);
}

// Concurrent startup chooses one destination exactly once; later initialization cannot republish defaults.
void ConcurrentInitialization(const std::filesystem::path& root) {
    const auto first = root / "first.ini", second = root / "second.ini";
    Write(first, "version=2\ncinematics=immersive\nturning=snap\n");
    Write(second, "version=2\ncinematics=theater\nturning=smooth\n");
    VrPreferenceStore store;
    std::barrier start(3);
    VrPreferenceSnapshot a, b;
    std::thread native([&] { start.arrive_and_wait(); a = store.Initialize(first); });
    std::thread overlay([&] { start.arrive_and_wait(); b = store.Initialize(second); });
    start.arrive_and_wait(); native.join(); overlay.join();
    Require(Equal(a.values, b.values) && a.revision == b.revision, "initializers selected different owners");
    const bool first_won = a.values.cinematics == CinematicsMode::immersive;
    const auto result = store.Apply({CinematicsMode::theater, TurnMode::snap}, a.revision);
    Require(result.saved, "initialized concurrent destination could not save");
    Persisted(first_won ? first : second, {CinematicsMode::theater, TurnMode::snap});
    Persisted(first_won ? second : first, first_won ? VrPreferences{} :
        VrPreferences{CinematicsMode::immersive, TurnMode::snap});
}
}
int main() {
    try {
        TemporaryDirectory root;
        StrictSchemas(root.path);
        PreserveUnrecognizedSettings(root.path);
        Initialization(root.path);
        EditorsAndRetry(root.path);
        ConcurrentEditors(root.path);
        ConcurrentInitialization(root.path);
        auto& shared = SharedVrPreferences();
        Require(&shared == &SharedVrPreferences(), "shared preference owner is not a singleton");
        const auto initial = shared.Initialize(root.path / "shared.ini");
        const auto applied = shared.Apply({CinematicsMode::immersive, TurnMode::snap}, initial.revision);
        Require(applied.saved && Equal(SharedVrPreferences().Read().values, applied.snapshot.values),
            "shared owner did not coordinate application");
        Published(applied.snapshot.values);
        std::cout << "VR preferences: schemas, migration, editors, locked saves/retries and concurrency passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
