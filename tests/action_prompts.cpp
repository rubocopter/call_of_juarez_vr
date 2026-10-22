#include "runtime/action_prompts.hpp"

#include <atomic>
#include <cstdlib>
#include <iostream>
#include <string>
#include <thread>
#include <type_traits>
#include <utility>

namespace {
using namespace cojvr::runtime;

void Check(bool value, const char* message) {
    if (!value) {
        std::cerr << message << '\n';
        std::exit(1);
    }
}

constexpr std::size_t Index(PromptAction action) {
    return static_cast<std::size_t>(action);
}

void SemanticFallbacks() {
    struct Fixture { PromptAction action; std::u16string_view en, es; };
    constexpr Fixture fixtures[] = {
        {PromptAction::left_fire, u"Left fire", u"Disparar izquierda"},
        {PromptAction::right_fire, u"Right fire", u"Disparar derecha"},
        {PromptAction::reload, u"Reload", u"Recargar"},
        {PromptAction::interact, u"Interact", u"Interactuar"},
        {PromptAction::kick, u"Kick", u"Patada"},
        {PromptAction::jump, u"Jump", u"Saltar"},
        {PromptAction::alternate_fire, u"Alternate fire", u"Disparo alternativo"},
        {PromptAction::weapon_radial, u"Weapon wheel", u"Rueda de armas"},
        {PromptAction::objectives, u"Objectives", u"Objetivos"},
        {PromptAction::focus, u"Focus zoom", u"Zoom"},
        {PromptAction::crouch, u"Crouch", u"Agacharse"},
        {PromptAction::pause, u"Pause", u"Pausa"},
        {PromptAction::recenter, u"Recenter", u"Recentrar"},
        {PromptAction::turn, u"Selection direction", u"Direcci\u00f3n de selecci\u00f3n"},
    };
    static_assert(sizeof(fixtures) / sizeof(fixtures[0]) == Index(PromptAction::count));
    ActionPromptSnapshot snapshot{};
    for (const auto& fixture : fixtures) {
        Check(ResolveActionPrompt(snapshot, fixture.action) == fixture.en,
            "inactive source lost English semantic fallback");
        Check(ResolveActionPrompt(snapshot, fixture.action, true) == fixture.es,
            "inactive source lost Spanish semantic fallback");
        snapshot.active = true;
        Check(ResolveActionPrompt(snapshot, fixture.action) == fixture.en &&
            ResolveActionPrompt(snapshot, fixture.action, true) == fixture.es,
            "unbound action invented a device-specific binding");
        Check(SetActionPromptLabel(snapshot, fixture.action, "Reported binding"),
            "valid reported label rejected");
        Check(ResolveActionPrompt(snapshot, fixture.action, true) == u"Reported binding",
            "reported label was translated instead of preserved");
        snapshot.active = false;
        Check(ResolveActionPrompt(snapshot, fixture.action) == fixture.en,
            "inactive source leaked a retained reported binding");
    }
    snapshot.active = true;
    for (const auto action : {PromptAction::count, static_cast<PromptAction>(255)}) {
        Check(!SetActionPromptLabel(snapshot, action, "Invalid action"),
            "out-of-range action was writable");
        Check(ResolveActionPrompt(snapshot, action).empty() &&
            ResolveActionPrompt(snapshot, action, true).empty(),
            "out-of-range action resolved a label");
    }
    Check(ResolveActionPrompt(snapshot, PromptAction::reload) == u"Reported binding",
        "out-of-range action corrupted another label");
}

void UnicodeAndOwnedStorage() {
    ActionPromptSnapshot snapshot{};
    snapshot.binding_generation = 42;
    std::string text = "Bot\xc3\xb3n \xe6\x89\x8b \xf0\x9f\x8e\xaf";
    Check(SetActionPromptLabel(snapshot, PromptAction::interact, text),
        "valid multi-byte UTF-8 rejected");
    Check(!snapshot.active && snapshot.binding_generation == 42 &&
        ResolveActionPrompt(snapshot, PromptAction::interact) == u"Interact",
        "setting a label activated the source or changed its generation");
    snapshot.active = true;
    text.assign(text.size(), 'x');
    Check(ResolveActionPrompt(snapshot, PromptAction::interact) ==
        u"Bot\u00f3n \u624b \U0001f3af", "UTF-16 conversion or owned storage corrupted");
    Check(snapshot.binding_generation == 42 && snapshot.active,
        "setting a label changed source metadata");
    Check(SetActionPromptLabel(snapshot, PromptAction::jump,
        "\xc2\xa0\xdf\xbf\xe0\xa0\x80\xed\x9f\xbf\xee\x80\x80"
        "\xf0\x90\x80\x80\xf4\x8f\xbf\xbf"), "valid scalar boundaries rejected");
    Check(ResolveActionPrompt(snapshot, PromptAction::jump) ==
        u"\u00a0\u07ff\u0800\ud7ff\ue000\U00010000\U0010ffff",
        "scalar boundaries converted incorrectly");
    Check(!SetActionPromptLabel(snapshot, PromptAction::interact, {}),
        "empty label reported a usable binding");
    Check(snapshot.labels[Index(PromptAction::interact)].view().empty() &&
        ResolveActionPrompt(snapshot, PromptAction::interact) == u"Interact",
        "empty label failed to clear a previous binding");
}

void RejectWholeInvalidLabels() {
    const std::string_view malformed[] = {
        "\x80", "\xbf", "\xc0\xaf", "\xc1\xbf", "\xc2", "\xc2 ",
        "\xe0\x80\xaf", "\xe1\x80", "\xe1\x80 ", "\xed\xa0\x80",
        "\xed\xbf\xbf", "\xf0\x80\x80\xaf", "\xf0\x90\x80",
        "\xf1\x80\x80 ", "\xf4\x90\x80\x80", "\xf5\x80\x80\x80",
        "\xf8\x88\x80\x80\x80", "\xfe", "\xff",
    };
    ActionPromptSnapshot snapshot{};
    snapshot.active = true;
    Check(SetActionPromptLabel(snapshot, PromptAction::jump, "Other action"),
        "other-action fixture rejected");
    for (const auto invalid : malformed) {
        Check(SetActionPromptLabel(snapshot, PromptAction::reload, "Old binding"),
            "malformed-input fixture rejected");
        const std::string whole = std::string("Valid prefix ") + std::string(invalid);
        Check(!SetActionPromptLabel(snapshot, PromptAction::reload, whole),
            "malformed/overlong UTF-8 accepted");
        Check(snapshot.labels[Index(PromptAction::reload)].view().empty() &&
            ResolveActionPrompt(snapshot, PromptAction::reload) == u"Reload",
            "invalid suffix retained a prefix or stale binding");
    }
    // Reject every C0 and C1 control, including NUL, CR/LF and DEL.
    for (unsigned scalar = 0; scalar <= 0x9f; ++scalar) {
        if (scalar > 0x1f && scalar < 0x7f) continue;
        std::string control;
        if (scalar < 0x80) control.push_back(static_cast<char>(scalar));
        else {
            control.push_back(static_cast<char>(0xc2));
            control.push_back(static_cast<char>(scalar));
        }
        const std::string whole = "Before" + control + "After";
        Check(SetActionPromptLabel(snapshot, PromptAction::reload, "Old binding"),
            "control-input fixture rejected");
        Check(!SetActionPromptLabel(snapshot, PromptAction::reload, whole),
            "embedded control/NUL accepted");
        Check(snapshot.labels[Index(PromptAction::reload)].view().empty(),
            "control rejection retained a stale binding");
    }
    for (const auto separator : {"\xe2\x80\xa8", "\xe2\x80\xa9"}) {
        Check(!SetActionPromptLabel(snapshot, PromptAction::reload, separator),
            "Unicode line/paragraph separator accepted");
    }
    Check(ResolveActionPrompt(snapshot, PromptAction::jump) == u"Other action",
        "rejected label changed another action");
}

void CapacityAndSurrogateBoundaries() {
    static_assert(PromptLabel::capacity == 96);
    ActionPromptSnapshot snapshot{};
    snapshot.active = true;
    const std::string maximum(95, 'a');
    Check(SetActionPromptLabel(snapshot, PromptAction::reload, maximum) &&
        ResolveActionPrompt(snapshot, PromptAction::reload) == std::u16string(95, u'a'),
        "maximum terminated label rejected or truncated");
    Check(!SetActionPromptLabel(snapshot, PromptAction::reload, std::string(96, 'a')) &&
        snapshot.labels[Index(PromptAction::reload)].view().empty(),
        "oversized ASCII label accepted/truncated");
    const std::string pair_fits = std::string(93, 'a') + "\xf0\x9f\x8e\xaf";
    Check(SetActionPromptLabel(snapshot, PromptAction::reload, pair_fits) &&
        ResolveActionPrompt(snapshot, PromptAction::reload) ==
            std::u16string(93, u'a') + u"\U0001f3af",
        "surrogate pair at capacity was split or rejected");
    const std::string pair_overflows = std::string(94, 'a') + "\xf0\x9f\x8e\xaf";
    Check(!SetActionPromptLabel(snapshot, PromptAction::reload, pair_overflows) &&
        snapshot.labels[Index(PromptAction::reload)].view().empty(),
        "overflow left a partial surrogate or truncated prefix");
    std::string bmp;
    for (int i = 0; i < 95; ++i) bmp += "\xe6\x89\x8b";
    Check(SetActionPromptLabel(snapshot, PromptAction::reload, bmp) &&
        ResolveActionPrompt(snapshot, PromptAction::reload) == std::u16string(95, u'\u624b'),
        "capacity incorrectly measured UTF-8 bytes instead of UTF-16 units");
    bmp += "\xe6\x89\x8b";
    Check(!SetActionPromptLabel(snapshot, PromptAction::reload, bmp),
        "oversized multi-byte label accepted");
    Check(!SetActionPromptLabel(snapshot, PromptAction::reload, std::string(10000, 'x')),
        "unbounded label accepted");
    Check(SetActionPromptLabel(snapshot, PromptAction::reload, "New") &&
        ResolveActionPrompt(snapshot, PromptAction::reload) == u"New",
        "shorter replacement retained trailing text");
}

void SnapshotPublication() {
    static_assert(std::is_trivially_copyable_v<ActionPromptSnapshot>);
    static_assert(noexcept(SetActionPromptLabel(
        std::declval<ActionPromptSnapshot&>(), PromptAction::reload, {})));
    static_assert(noexcept(ResolveActionPrompt(
        std::declval<const ActionPromptSnapshot&>(), PromptAction::reload)));
    static_assert(noexcept(PublishActionPrompts(std::declval<const ActionPromptSnapshot&>())));
    static_assert(noexcept(CurrentActionPrompts()) && noexcept(ClearActionPrompts()));
    ClearActionPrompts();
    Check(!CurrentActionPrompts().active && CurrentActionPrompts().binding_generation == 0,
        "clear retained source metadata");
    ActionPromptSnapshot source{};
    source.active = true;
    source.binding_generation = 7;
    Check(SetActionPromptLabel(source, PromptAction::reload, "First"), "publish fixture rejected");
    PublishActionPrompts(source);
    source = {};
    auto retained = CurrentActionPrompts();
    Check(retained.active && retained.binding_generation == 7 &&
        ResolveActionPrompt(retained, PromptAction::reload) == u"First",
        "publication retained a caller reference or lost metadata");
    auto modified_copy = retained;
    Check(SetActionPromptLabel(modified_copy, PromptAction::reload, "Private"),
        "local-copy fixture rejected");
    Check(ResolveActionPrompt(retained, PromptAction::reload) == u"First" &&
        ResolveActionPrompt(CurrentActionPrompts(), PromptAction::reload) == u"First",
        "reader mutation changed another snapshot");
    source.active = true;
    source.binding_generation = 8;
    Check(SetActionPromptLabel(source, PromptAction::jump, "Second"), "new source rejected");
    PublishActionPrompts(source);
    const auto replacement = CurrentActionPrompts();
    Check(replacement.binding_generation == 8 &&
        ResolveActionPrompt(replacement, PromptAction::reload) == u"Reload" &&
        ResolveActionPrompt(replacement, PromptAction::jump) == u"Second",
        "complete generation replacement retained old labels");
    source.active = false;
    PublishActionPrompts(source);
    Check(ResolveActionPrompt(CurrentActionPrompts(), PromptAction::jump) == u"Jump",
        "inactive publication leaked a reported label");
    ClearActionPrompts();
    const auto cleared = CurrentActionPrompts();
    for (const auto& label : cleared.labels)
        Check(label.view().empty(), "clear retained label storage");
    Check(!cleared.active && cleared.binding_generation == 0 &&
        ResolveActionPrompt(retained, PromptAction::reload) == u"First",
        "clear invalidated a reader's owned snapshot");
}

void ConcurrentValueCopies() {
    ActionPromptSnapshot first{}, second{};
    first.active = second.active = true;
    first.binding_generation = 101;
    second.binding_generation = 202;
    for (std::size_t i = 0; i < Index(PromptAction::count); ++i) {
        Check(SetActionPromptLabel(first, static_cast<PromptAction>(i), "First"),
            "first concurrent fixture rejected");
        Check(SetActionPromptLabel(second, static_cast<PromptAction>(i), "Second"),
            "second concurrent fixture rejected");
    }
    ClearActionPrompts();
    std::atomic<bool> start{false}, coherent{true};
    const auto reader = [&] {
        while (!start.load()) std::this_thread::yield();
        for (int i = 0; i < 4000; ++i) {
            const auto snapshot = CurrentActionPrompts();
            if (snapshot.binding_generation == 0) {
                if (snapshot.active) coherent.store(false);
                for (const auto& label : snapshot.labels)
                    if (!label.view().empty()) coherent.store(false);
            } else {
                const auto expected = snapshot.binding_generation == 101 ? u"First" : u"Second";
                if (!snapshot.active || (snapshot.binding_generation != 101 &&
                    snapshot.binding_generation != 202)) coherent.store(false);
                for (const auto& label : snapshot.labels)
                    if (label.view() != expected) coherent.store(false);
            }
        }
    };
    std::thread left(reader), right(reader);
    start.store(true);
    for (int i = 0; i < 2000; ++i) {
        PublishActionPrompts(first);
        PublishActionPrompts(second);
        ClearActionPrompts();
    }
    left.join();
    right.join();
    Check(coherent.load(), "concurrent reader observed a torn snapshot");
    ClearActionPrompts();
}
} // namespace

int main() {
    SemanticFallbacks();
    UnicodeAndOwnedStorage();
    RejectWholeInvalidLabels();
    CapacityAndSurrogateBoundaries();
    SnapshotPublication();
    ConcurrentValueCopies();
    std::cout << "action_prompts: bounded Unicode, fallback and value publication passed\n";
}
