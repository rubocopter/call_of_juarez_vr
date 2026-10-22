#include "runtime/action_prompts.hpp"

#include <mutex>

namespace cojvr::runtime {
namespace {
std::mutex prompts_mutex;
ActionPromptSnapshot current_prompts{};

struct SemanticLabel { std::u16string_view en, es; };
constexpr std::array<SemanticLabel, static_cast<std::size_t>(PromptAction::count)> fallback{{
    {u"Left fire", u"Disparar izquierda"},
    {u"Right fire", u"Disparar derecha"},
    {u"Reload", u"Recargar"},
    {u"Interact", u"Interactuar"},
    {u"Kick", u"Patada"},
    {u"Jump", u"Saltar"},
    {u"Alternate fire", u"Disparo alternativo"},
    {u"Weapon wheel", u"Rueda de armas"},
    {u"Objectives", u"Objetivos"},
    {u"Focus zoom", u"Zoom"},
    {u"Crouch", u"Agacharse"},
    {u"Pause", u"Pausa"},
    {u"Recenter", u"Recentrar"},
    {u"Selection direction", u"Direcci\u00f3n de selecci\u00f3n"},
}};

bool ReadScalar(std::string_view input, std::size_t& cursor, std::uint32_t& scalar) noexcept {
    const auto lead = static_cast<unsigned char>(input[cursor++]);
    if (lead < 0x80) {
        scalar = lead;
        return true;
    }
    unsigned remaining = 0;
    std::uint32_t minimum = 0;
    if (lead >= 0xc2 && lead <= 0xdf) {
        remaining = 1; minimum = 0x80; scalar = lead & 0x1f;
    } else if (lead >= 0xe0 && lead <= 0xef) {
        remaining = 2; minimum = 0x800; scalar = lead & 0x0f;
    } else if (lead >= 0xf0 && lead <= 0xf4) {
        remaining = 3; minimum = 0x10000; scalar = lead & 0x07;
    } else {
        return false;
    }
    if (input.size() - cursor < remaining) return false;
    for (unsigned i = 0; i < remaining; ++i) {
        const auto byte = static_cast<unsigned char>(input[cursor++]);
        if ((byte & 0xc0) != 0x80) return false;
        scalar = (scalar << 6) | (byte & 0x3f);
    }
    return scalar >= minimum && scalar <= 0x10ffff &&
        !(scalar >= 0xd800 && scalar <= 0xdfff);
}

bool IsControlOrLineBreak(std::uint32_t scalar) noexcept {
    return scalar <= 0x1f || (scalar >= 0x7f && scalar <= 0x9f) ||
        scalar == 0x2028 || scalar == 0x2029;
}
} // namespace

bool SetActionPromptLabel(ActionPromptSnapshot& snapshot, PromptAction action,
    std::string_view utf8) noexcept {
    const auto index = static_cast<std::size_t>(action);
    if (index >= snapshot.labels.size()) return false;
    auto& destination = snapshot.labels[index];
    destination = {};
    // Bound decoding work as well as output; no UTF-8 scalar exceeds four bytes.
    if (utf8.empty() || utf8.size() > 4 * (PromptLabel::capacity - 1)) return false;
    PromptLabel candidate{};
    std::size_t cursor = 0;
    while (cursor < utf8.size()) {
        std::uint32_t scalar = 0;
        if (!ReadScalar(utf8, cursor, scalar) || IsControlOrLineBreak(scalar)) return false;
        const std::size_t units = scalar > 0xffff ? 2 : 1;
        if (candidate.length_ + units >= PromptLabel::capacity) return false;
        if (units == 1) {
            candidate.characters_[candidate.length_++] = static_cast<char16_t>(scalar);
        } else {
            scalar -= 0x10000;
            candidate.characters_[candidate.length_++] = static_cast<char16_t>(0xd800 + (scalar >> 10));
            candidate.characters_[candidate.length_++] = static_cast<char16_t>(0xdc00 + (scalar & 0x3ff));
        }
    }
    destination = candidate;
    return true;
}

std::u16string_view ResolveActionPrompt(const ActionPromptSnapshot& snapshot,
    PromptAction action, bool spanish) noexcept {
    const auto index = static_cast<std::size_t>(action);
    if (index >= snapshot.labels.size()) return {};
    const auto label = snapshot.labels[index].view();
    if (snapshot.active && !label.empty()) return label;
    return spanish ? fallback[index].es : fallback[index].en;
}

void PublishActionPrompts(const ActionPromptSnapshot& snapshot) noexcept {
    try {std::lock_guard lock(prompts_mutex);current_prompts = snapshot;}catch(...){}
}

ActionPromptSnapshot CurrentActionPrompts() noexcept {
    try {std::lock_guard lock(prompts_mutex);return current_prompts;}catch(...){return {};}
}

void ClearActionPrompts() noexcept {
    PublishActionPrompts({});
}
} // namespace cojvr::runtime
