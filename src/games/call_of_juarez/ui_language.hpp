#pragma once
#include <filesystem>
#include <string_view>

namespace cojvr::games::call_of_juarez {
// Authored VR labels follow the exact game's startup Game.ini Locale setting.
// Copied native text stays UTF-16 and is never translated by this table.
enum class CoJUiLanguage { english,spanish };
enum class CoJUiPhrase {
    health,left,right,long_weapon,dynamite,equipment,bow,rifle_reserve,shotgun_reserve,pistol_reserve,
    standing,standing_shadow,crouched,crouched_shadow,countdown,horse_health,fatigue,
    concentration_active,concentration_cooldown,concentration_ready,concentration_unavailable,
    left_pistol,right_pistol,wheel_long,wheel_dynamite,bible_whip,wheel_bow,hands,discard,whip,bible,cancel,confirm,
    hold_r1,create_tap,radial_left,radial_right,radial_up,radial_down_right,radial_up_left,radial_up_right,radial_down,radial_down_left
};
inline std::u16string_view CoJUiText(CoJUiLanguage language,CoJUiPhrase phrase) noexcept {
    struct Pair {std::u16string_view english,spanish;};
    constexpr Pair text[]{
        {u"Health  ",u"Salud  "},{u"Left  ",u"Izquierda  "},{u"Right  ",u"Derecha  "},
        {u"Long weapon  ",u"Arma larga  "},{u"Dynamite  ",u"Dinamita  "},{u"Equipment  ",u"Equipo  "},{u"Bow  ",u"Arco  "},
        {u"Rifle reserve  ",u"Reserva rifle  "},{u"Shotgun reserve  ",u"Reserva escopeta  "},{u"Pistol reserve  ",u"Reserva pistola  "},
        {u"Standing",u"De pie"},{u"Standing · In shadow",u"De pie · En sombra"},
        {u"Crouched",u"Agachado"},{u"Crouched · In shadow",u"Agachado · En sombra"},
        {u"Countdown  ",u"Cuenta atrás  "},{u"Horse · Health ",u"Caballo · Salud "},{u" · Fatigue ",u" · Fatiga "},
        {u"Concentration · Active",u"Concentración · En uso"},{u"Concentration · Recharging",u"Concentración · Recargando"},
        {u"Concentration · Ready",u"Concentración · Lista"},{u"Concentration · Unavailable",u"Concentración · No preparada"},
        {u"Left pistol",u"Pistola izquierda"},{u"Right pistol",u"Pistola derecha"},{u"Long weapon",u"Arma larga"},
        {u"Dynamite",u"Dinamita"},{u"Bible / whip",u"Biblia / látigo"},{u"Bow",u"Arco"},{u"Hands",u"Manos"},
        {u"Discard",u"Soltar"},{u"Whip",u"Látigo"},{u"Bible",u"Biblia"},{u"Cancel",u"Cancelar"},{u"Confirm",u"Confirmar"},
        {u"R1 (hold)",u"R1 (mantener)"},{u"Create tap",u"Create (pulsar)"},
        {u"Triangle hold / right stick Left / release",u"Mantener Triangle / stick derecho Izquierda / soltar"},
        {u"Triangle hold / right stick Right / release",u"Mantener Triangle / stick derecho Derecha / soltar"},
        {u"Triangle hold / right stick Up / release",u"Mantener Triangle / stick derecho Arriba / soltar"},
        {u"Triangle hold / right stick Down-right / release",u"Mantener Triangle / stick derecho Abajo-derecha / soltar"},
        {u"Triangle hold / right stick Up-left / release",u"Mantener Triangle / stick derecho Arriba-izquierda / soltar"},
        {u"Triangle hold / right stick Up-right / release",u"Mantener Triangle / stick derecho Arriba-derecha / soltar"},
        {u"Triangle hold / right stick Down / release",u"Mantener Triangle / stick derecho Abajo / soltar"},
        {u"Triangle hold / right stick Down-left / release",u"Mantener Triangle / stick derecho Abajo-izquierda / soltar"}
    };
    static_assert(std::size(text)==static_cast<std::size_t>(CoJUiPhrase::radial_down_left)+1);
    const auto index=static_cast<std::size_t>(phrase);
    if(index>=std::size(text))return {};
    return language==CoJUiLanguage::spanish?text[index].spanish:text[index].english;
}
[[nodiscard]] CoJUiLanguage ParseCoJUiLanguage(std::string_view configuration) noexcept;
[[nodiscard]] CoJUiLanguage ReadCoJUiLanguage(const std::filesystem::path& configuration) noexcept;
}
