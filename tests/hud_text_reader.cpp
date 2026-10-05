#include "games/call_of_juarez/hud_text_reader.hpp"
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <unordered_map>

namespace fixture {
struct Object {
    std::unordered_map<std::string, Object*> fields;
    std::u16string string;
    int index = 0, locals = 0;
    bool visible = true, boolean = true;
};
Object hud_class, game_class, dialog_class, text_class, exception;
Object player, other, hud, components, trigger, icon, hint, window, info;
Object settings, dialog, lines, line, actor, subtitle;
Object interaction_text, hint_text, subtitle_text, localized_subtitle;
Object* all[] = {&hud_class,&game_class,&dialog_class,&text_class,&exception,&player,&other,
    &hud,&components,&trigger,&icon,&hint,&window,&info,&settings,&dialog,&lines,&line,&actor,
    &subtitle,&interaction_text,&hint_text,&subtitle_text,&localized_subtitle};
void* table[180]{}; void** holder = table;
bool pending = false, fail_field = false, fail_chars = false, wrong_component = false;
int pinned = 0, mutations = 0, calls = 0;
void Require(bool value, const char* message) { if (!value) { std::cerr<<message<<'\n'; std::exit(1); } }
void* Local(Object* object) { if (object) ++object->locals; return object; }
void Clean() { Require(!pending && pinned == 0 && mutations == 0, "JNI cleanup or read-only ownership failed");
    for (auto* object: all) Require(object->locals == 0, "leaked JNI local reference"); }
void* __stdcall Find(void*, const char* name) {
    Require(!pending, "JNI called with pending exception");
    if (!std::strcmp(name,"HUDManager")) return Local(&hud_class);
    if (!std::strcmp(name,"LawmanGame")) return Local(&game_class);
    if (!std::strcmp(name,"Dialog")) return Local(&dialog_class);
    if (!std::strcmp(name,"Text")) return Local(&text_class);
    if (!std::strcmp(name,"HUDActiveTrigger")) return Local(&trigger);
    if (!std::strcmp(name,"HUDHint")) return Local(&hint);
    Require(false,"unexpected native class lookup"); return nullptr;
}
void* __stdcall Exception(void*) { return pending ? Local(&exception) : nullptr; }
void __stdcall Clear(void*) { pending = false; }
void __stdcall Delete(void*, void* value) { auto* o=static_cast<Object*>(value); Require(o && o->locals>0,"invalid local release"); --o->locals; }
unsigned char __stdcall Same(void*, void* a, void* b) { return a == b; }
void* __stdcall Class(void*, void* object) { Require(!pending,"pending class"); return Local(static_cast<Object*>(object)); }
unsigned char __stdcall Instance(void*, void* object, void* cls) { return !wrong_component && object == cls; }
std::unordered_map<std::string, std::string> tokens;
void* __stdcall Field(void*, void*, const char* name, const char*) {
    Require(!pending,"pending lookup");
    if (fail_field && std::strcmp(name,"m_cIcon")==0) { pending=true; return nullptr; }
    return &tokens.emplace(name,name).first->second;
}
void* __stdcall ObjectField(void*, void* object, void* field) {
    Require(!pending,"pending field read");
    return Local(static_cast<Object*>(object)->fields[*static_cast<std::string*>(field)]);
}
unsigned char __stdcall Boolean(void*, void* object, void*) { Require(!pending,"pending boolean"); return static_cast<Object*>(object)->boolean; }
int __stdcall Integer(void*, void* object, void*) { return static_cast<Object*>(object)->index; }
void* __stdcall Method(void* env, void* cls, const char* name, const char* sig) { return Field(env,cls,name,sig); }
union Value { long long alignment; void* object; };
unsigned char __stdcall CallBoolean(void*, void* object, void* method, const Value*) {
    Require(!pending,"pending visibility"); ++calls;
    const auto& name=*static_cast<std::string*>(method);
    Require(name=="IsActuallyVisible" || name=="AreSubtitlesVisible", "mutating method requested");
    return static_cast<Object*>(object)->visible;
}
void* __stdcall CallStaticObject(void*, void* cls, void* method, const Value* args) {
    Require(cls==&text_class && *static_cast<std::string*>(method)=="Get" && args[0].object==&subtitle_text,
        "subtitle localization must use shipped Text.Get");
    return Local(&localized_subtitle);
}
int __stdcall ArrayLength(void*, void* array) { return array==&components ? 23 : 1; }
void* __stdcall Element(void*, void* array, int index) {
    if(array==&components) { Require(index==9 || index==11,"wrong shipped HUD component index"); return Local(index==9? &trigger : &hint); }
    Require(array==&lines && index==0,"subtitle index must be checked before read"); return Local(&line);
}
int __stdcall Length(void*, void* object) { return static_cast<int>(static_cast<Object*>(object)->string.size()); }
const char16_t* __stdcall Chars(void*, void* object, unsigned char*) {
    if (fail_chars) { pending=true; return nullptr; }
    ++pinned; return static_cast<Object*>(object)->string.data();
}
void __stdcall Release(void*, void*, const char16_t*) { --pinned; }
template<typename T> void Set(int i,T fn) { table[i]=reinterpret_cast<void*>(fn); }
void Initialize() {
    Set(6,Find); Set(15,Exception); Set(17,Clear); Set(23,Delete); Set(24,Same); Set(31,Class); Set(32,Instance);
    Set(33,Method); Set(39,CallBoolean); Set(94,Field); Set(95,ObjectField); Set(96,Boolean); Set(100,Integer);
    Set(113,Method); Set(116,CallStaticObject); Set(144,Field); Set(145,ObjectField);
    Set(164,Length); Set(165,Chars); Set(166,Release); Set(171,ArrayLength); Set(173,Element);
    hud_class.fields["sm_cMainHUDManager"]=&hud; hud.fields["m_Being"]=&player;
    hud.fields["m_aHudComponents"]=&components; trigger.fields["m_cIcon"]=&icon;
    icon.fields["m_sLocalizedText"]=&interaction_text;
    hint.fields["m_cMainWindow"]=&window; window.fields["m_cInfo"]=&info;
    info.fields["m_sLocalizedText"]=&hint_text;
    game_class.fields["sm_cSettings"]=&settings; dialog_class.fields["cPlayingDialog"]=&dialog;
    dialog.fields["aLines"]=&lines; line.fields["cCharacter"]=&actor;
    actor.fields["sSubtitleText"]=&subtitle_text; actor.fields["m_cSubtitle"]=&subtitle;
    interaction_text.string=u"Pulsa F para recoger el revólver";
    hint_text.string=u"¡Agáchate y salta! Niño, caballo, acción.";
    subtitle_text.string=u"&DIALOG_KEY&"; localized_subtitle.string=u"¿Quién anda ahí?";
}
}
int main() {
    using namespace fixture;
    using namespace cojvr::games::call_of_juarez;
    Initialize(); cojvr::runtime::HudTextSnapshot text{};
    Require(ReadCoJHudText(&holder,&player,text),"visible native owners must be readable");
    Require(text.interaction.view()==interaction_text.string && text.hint.view()==hint_text.string &&
        text.subtitle.view()==localized_subtitle.string,"must preserve native localized Unicode strings"); Clean();
    hint_text.string=u"Press 'RMB' to catch; LMB + RMB release. SPACE BAR jumps; SPACEBAR too. ALMB remains.";
    interaction_text.string=u"LMB para atacar; RMB para enganchar";
    localized_subtitle.string=u"El diálogo dice LMB y RMB.";
    Require(ReadCoJHudText(&holder,&player,text),"controller hint owners must remain readable");
    Require(text.hint.view()==u"Press 'R2' to catch; L2 + R2 release. Cross jumps; Cross too. ALMB remains." &&
        text.interaction.view()==u"L2 para atacar; R2 para enganchar" &&
        text.subtitle.view()==localized_subtitle.string && hint_text.string.find(u"RMB")!=std::u16string::npos,
        "VR hints must translate mouse/space tokens without changing dialogue or native text"); Clean();
    icon.visible=false; info.visible=false; settings.boolean=false;
    Require(ReadCoJHudText(&holder,&player,text) && text.interaction.view().empty() &&
        text.hint.view().empty() && text.subtitle.view().empty(),"hidden owners/settings must clear all text"); Clean();
    icon.visible=info.visible=settings.boolean=true;
    dialog.index=-1;
    Require(ReadCoJHudText(&holder,&player,text) && text.subtitle.view().empty(),"negative line index must not reach array"); Clean();
    dialog.index=1;
    Require(ReadCoJHudText(&holder,&player,text) && text.subtitle.view().empty(),"out-of-range line index must clear subtitle"); Clean(); dialog.index=0;
    hud.fields["m_Being"]=&other;
    Require(ReadCoJHudText(&holder,&player,text) && text.hint.view().empty() && text.interaction.view().empty(),"foreign HUD owner must fail closed"); Clean();
    hud.fields["m_Being"]=&player;
    fail_field=true;
    Require(!ReadCoJHudText(&holder,&player,text) && text.interaction.view().empty(),"lookup exceptions must clear affected text"); Clean(); fail_field=false;
    fail_chars=true;
    Require(!ReadCoJHudText(&holder,&player,text) && text.hint.view().empty() && text.subtitle.view().empty(),"failed pinned string reads must clear text"); Clean(); fail_chars=false;
    interaction_text.string=std::u16string(2000,u'a'); interaction_text.string[1022]=0xD83D;
    Require(ReadCoJHudText(&holder,&player,text) && text.interaction.length < 1024 &&
        text.interaction.view().back()!=0xD83D,"bounded UTF16 must not cut a surrogate pair"); Clean();
    interaction_text.string=std::u16string(2000,u'a');
    interaction_text.string[1021]=0xD83D; interaction_text.string[1022]=0xDE00;
    Require(ReadCoJHudText(&holder,&player,text),"long native Unicode text must remain readable");
    for (std::size_t i=0; i<text.interaction.length; ++i) {
        const auto c=text.interaction.characters[i];
        Require(c<0xD800 || c>0xDBFF || (i+1<text.interaction.length &&
            text.interaction.characters[i+1]>=0xDC00 && text.interaction.characters[i+1]<=0xDFFF),
            "truncation ellipsis must not replace the low half of a Unicode pair");
    }
    Clean();
    wrong_component=true;
    Require(!ReadCoJHudText(&holder,&player,text) && text.hint.view().empty(),"component type mismatch must fail closed"); Clean();
    Require(!ReadCoJHudText(nullptr,&player,text) && text.subtitle.view().empty(),"missing JNI must clear prior text");
    std::cout<<"Native HUD ownership, visibility, Unicode and exception cleanup passed\n";
}
