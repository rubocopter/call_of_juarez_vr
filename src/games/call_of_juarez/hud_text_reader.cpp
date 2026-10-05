#include "games/call_of_juarez/hud_text_reader.hpp"
#include <algorithm>
#include <cstring>

namespace cojvr::games::call_of_juarez {
namespace {
// Presentation only: exact default Sense action routes, never the native string
// or subtitle dialogue. Replacements do not grow the bounded UTF-16 buffer.
void ControllerHint(runtime::HudText& text) noexcept {
    const auto source=text.view();
    runtime::HudText translated{};
    const auto word=[](char16_t c) {
        return (c>=u'A' && c<=u'Z') || (c>=u'a' && c<=u'z') ||
            (c>=u'0' && c<=u'9') || c==u'_' || c>=0x80;
    };
    for (std::size_t at=0;at<source.size();) {
        std::u16string_view before{},after{};
        for (const auto token:{std::u16string_view(u"SPACE BAR"),std::u16string_view(u"SPACEBAR"),std::u16string_view(u"LMB"),std::u16string_view(u"RMB")}) {
            if (source.substr(at,token.size())==token &&
                (at==0 || !word(source[at-1])) &&
                (at+token.size()==source.size() || !word(source[at+token.size()]))) {
                before=token;after=token==u"LMB" ? u"L2" : token==u"RMB" ? u"R2" : u"Cross";break;
            }
        }
        if (before.empty()) translated.characters[translated.length++]=source[at++];
        else {
            for (const auto c:after) translated.characters[translated.length++]=c;
            at+=before.size();
        }
    }
    text=translated;
}
// Stable JNI 1.4 subset. All class/field/index contracts below are specific to
// the hash-pinned shipped code.pak; callers enter through the exact-build bridge.
union Value { std::int64_t alignment; void* object; };
static_assert(sizeof(Value) == 8);
template<typename T> T Function(void* env, const std::size_t index) noexcept {
    return env ? reinterpret_cast<T>((*static_cast<void***>(env))[index]) : nullptr;
}
using Find = void*(__stdcall*)(void*, const char*);
using Object = void*(__stdcall*)(void*, void*);
using Delete = void(__stdcall*)(void*, void*);
using Lookup = void*(__stdcall*)(void*, void*, const char*, const char*);
using Field = void*(__stdcall*)(void*, void*, void*);
using Boolean = std::uint8_t(__stdcall*)(void*, void*, void*);
using Integer = std::int32_t(__stdcall*)(void*, void*, void*);
using Same = std::uint8_t(__stdcall*)(void*, void*, void*);
using CallBool = std::uint8_t(__stdcall*)(void*, void*, void*, const Value*);
using CallObject = void*(__stdcall*)(void*, void*, void*, const Value*);
using Length = std::int32_t(__stdcall*)(void*, void*);
using Element = void*(__stdcall*)(void*, void*, std::int32_t);
using Characters = const char16_t*(__stdcall*)(void*, void*, std::uint8_t*);
using Release = void(__stdcall*)(void*, void*, const char16_t*);

struct Reader {
    void* env;
    bool ok = true;
    bool Check() noexcept {
        void* exception = Function<void*(__stdcall*)(void*)>(env, 15)(env);
        if (exception) {
            Function<void(__stdcall*)(void*)>(env, 17)(env);
            Function<Delete>(env, 23)(env, exception);
            ok = false;
        }
        return ok;
    }
    struct Local {
        Reader& reader;
        void* value;
        ~Local() { if (value) Function<Delete>(reader.env, 23)(reader.env, value); }
        operator void*() const noexcept { return value; }
        Local(const Local&) = delete;
        Local(Reader& r, void* v) : reader(r), value(v) {}
    };
    Local Class(const char* name) noexcept {
        void* result = ok ? Function<Find>(env, 6)(env, name) : nullptr;
        Check(); if (!result) ok = false;
        return {*this, result};
    }
    Local Get(void* owner, const char* name, const char* signature, bool is_static = false) noexcept {
        if (!owner || !ok) return {*this, nullptr};
        Local cls(*this, is_static ? nullptr : Function<Object>(env, 31)(env, owner));
        Check();
        void* type = is_static ? owner : cls.value;
        if (!type) ok = false;
        void* id = ok ? Function<Lookup>(env, is_static ? 144 : 94)(env, type, name, signature) : nullptr;
        Check(); if (!id) ok = false;
        void* result = ok ? Function<Field>(env, is_static ? 145 : 95)(env, owner, id) : nullptr;
        Check();
        return {*this, result};
    }
    bool Bool(void* owner, const char* name) noexcept {
        if (!owner || !ok) return false;
        Local cls(*this, Function<Object>(env,31)(env,owner)); Check();
        void* id = ok && cls.value ? Function<Lookup>(env,94)(env,cls,name,"Z") : nullptr;
        Check(); if (!id) ok = false;
        const bool result = ok && Function<Boolean>(env,96)(env,owner,id) != 0;
        Check(); return ok && result;
    }
    int Int(void* owner, const char* name) noexcept {
        if (!owner || !ok) return -1;
        Local cls(*this, Function<Object>(env,31)(env,owner)); Check();
        void* id = ok && cls.value ? Function<Lookup>(env,94)(env,cls,name,"I") : nullptr;
        Check(); if (!id) ok = false;
        const int result = ok ? Function<Integer>(env,100)(env,owner,id) : -1;
        Check(); return ok ? result : -1;
    }
    bool Visible(void* owner, const char* method = "IsActuallyVisible") noexcept {
        if (!owner || !ok) return false;
        Local cls(*this, Function<Object>(env,31)(env,owner)); Check();
        void* id = ok && cls.value ? Function<Lookup>(env,33)(env,cls,method,"()Z") : nullptr;
        Check(); if (!id) ok = false;
        const bool result = ok && Function<CallBool>(env,39)(env,owner,id,nullptr) != 0;
        Check(); return ok && result;
    }
    int Size(void* array) noexcept {
        const int size = ok && array ? Function<Length>(env,171)(env,array) : 0;
        Check(); return ok ? size : 0;
    }
    Local At(void* array, int index) noexcept {
        void* result = ok && array ? Function<Element>(env,173)(env,array,index) : nullptr;
        Check(); return {*this,result};
    }
    bool Instance(void* object, const char* name) noexcept {
        auto cls = Class(name);
        const bool result = ok && object && Function<Same>(env,32)(env,object,cls) != 0;
        Check(); if (!result) ok = false;
        return ok;
    }
    void Copy(void* string, runtime::HudText& out) noexcept {
        out = {};
        if (!ok || !string) return;
        const int length = Function<Length>(env,164)(env,string); Check();
        if (!ok || length <= 0) return;
        const char16_t* data = Function<Characters>(env,165)(env,string,nullptr); Check();
        if (!data) ok = false;
        if (ok) {
            auto count = std::min(static_cast<std::size_t>(length), runtime::HudText::capacity - 1);
            const bool truncated = count < static_cast<std::size_t>(length);
            if (truncated) {
                --count; // Reserve the ellipsis before checking the pair boundary.
                if (data[count-1] >= 0xD800 && data[count-1] <= 0xDBFF) --count;
            }
            std::copy_n(data,count,out.characters.begin());
            if (truncated) out.characters[count++] = u'\u2026';
            out.length = static_cast<std::uint32_t>(count);
        }
        if (data) Function<Release>(env,166)(env,string,data);
        Check(); if (!ok) out = {};
    }
    void LocalizedSubtitle(void* string, runtime::HudText& out) noexcept {
        if (!string || !ok) return;
        auto cls = Class("Text");
        void* method = ok ? Function<Lookup>(env,113)(env,cls,"Get","(Ljava/lang/String;)Ljava/lang/String;") : nullptr;
        Check(); if (!method) ok = false;
        const Value arg{.object = string};
        Local localized(*this,ok ? Function<CallObject>(env,116)(env,cls,method,&arg) : nullptr);
        Check(); Copy(localized,out);
    }
};

bool ReadComponents(void* env, void* player, runtime::HudTextSnapshot& out) noexcept {
    Reader r{env};
    auto cls = r.Class("HUDManager");
    auto hud = r.Get(cls,"sm_cMainHUDManager","LHUDManager;",true);
    auto being = r.Get(hud,"m_Being","LBeing;");
    if (!r.ok || !hud.value || !being.value) return r.ok;
    const bool matches = Function<Same>(env,24)(env,being,player) != 0;
    r.Check();
    if (!matches || !r.Visible(hud)) return r.ok;
    auto components = r.Get(hud,"m_aHudComponents","[LHUDComponent;");
    // HUDManager.<clinit>/InitInGame establish 23 slots and exact names.
    if (r.Size(components) != 23) return false;
    {
        auto component = r.At(components,9);
        if (component.value && r.Instance(component,"HUDActiveTrigger") && r.Visible(component)) {
            auto text_owner = r.Get(component,"m_cIcon","LUIStatic;");
            if (r.Visible(text_owner)) {
                auto string = r.Get(text_owner,"m_sLocalizedText","Ljava/lang/String;");
                r.Copy(string,out.interaction);
            }
        }
    }
    {
        auto component = r.At(components,11);
        if (component.value && r.Instance(component,"HUDHint") && r.Visible(component)) {
            auto window = r.Get(component,"m_cMainWindow","LUIWindowInfo;");
            if (r.Visible(window)) {
                auto text_owner = r.Get(window,"m_cInfo","LUIWindowInfoElement;");
                if (r.Visible(text_owner)) {
                    auto string = r.Get(text_owner,"m_sLocalizedText","Ljava/lang/String;");
                    r.Copy(string,out.hint);
                }
            }
        }
    }
    if (!r.ok) { out.interaction = {}; out.hint = {}; }
    return r.ok;
}

bool ReadSubtitle(void* env, runtime::HudText& out) noexcept {
    Reader r{env};
    auto game = r.Class("LawmanGame");
    auto settings = r.Get(game,"sm_cSettings","LSettings;",true);
    if (!r.Bool(settings,"bSubtitles")) return r.ok;
    auto cls = r.Class("Dialog");
    auto dialog = r.Get(cls,"cPlayingDialog","LDialog;",true);
    if (!r.Bool(dialog,"m_bCurrentLineVisible")) return r.ok;
    const int index = r.Int(dialog,"nCurrentLine");
    auto lines = r.Get(dialog,"aLines","[LDialogLine;");
    const int length = r.Size(lines);
    if (index < 0 || index >= length || length > 10000) return r.ok;
    auto line = r.At(lines,index);
    auto actor = r.Get(line,"cCharacter","LAnimActor;");
    if (!r.Visible(actor,"AreSubtitlesVisible")) return r.ok;
    auto subtitle = r.Get(actor,"m_cSubtitle","LDialogSubtitle;");
    if (!r.Visible(subtitle)) return r.ok;
    auto string = r.Get(actor,"sSubtitleText","Ljava/lang/String;");
    r.LocalizedSubtitle(string,out);
    return r.ok;
}
}

bool ReadCoJHudText(void* env, void* player, runtime::HudTextSnapshot& out) noexcept {
    out = {};
    if (!env || !player || !*static_cast<void***>(env)) return false;
    constexpr std::size_t required[]{6,15,17,23,24,31,32,33,39,94,95,96,100,113,116,144,145,164,165,166,171,173};
    for (auto index : required) if (!(*static_cast<void***>(env))[index]) return false;
    Reader entry{env}; if (!entry.Check()) return false;
    const bool hud = ReadComponents(env,player,out);
    const bool subtitle = ReadSubtitle(env,out.subtitle);
    ControllerHint(out.hint);
    ControllerHint(out.interaction);
    return hud && subtitle;
}
}
