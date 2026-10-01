// Host-only JNI 1.4 ABI fixture. Compile this TU alone: it includes the real
// adapter and intercepts JVM discovery without loading a JVM or the game.
#include <windows.h>
#include <cstdlib>
#include <cstring>
#include <cwchar>
#include <iostream>
#include <limits>

namespace fixture {
HMODULE WINAPI Module(const wchar_t* name);
FARPROC WINAPI Symbol(HMODULE module, const char* name);
}
#define GetModuleHandleW fixture::Module
#define GetProcAddress fixture::Symbol
#include "../src/games/call_of_juarez/java_player_bridge.cpp"
#undef GetModuleHandleW
#undef GetProcAddress

namespace fixture {
using namespace cojvr::games::call_of_juarez;

struct Object { float x = 0, y = 0, z = 0; int locals = 0, globals = 0; };
Object game, lawman, single, menu_class, module_class, cursor_class, vector_class, ui_class;
Object global_menu, active_menu, module, global_cursor, active_cursor, vector, exception, ui;
Object modal_ui;
Object* const objects[] = {&game, &lawman, &single, &menu_class, &module_class,
    &cursor_class, &vector_class, &global_menu, &active_menu, &module,
    &global_cursor, &active_cursor, &vector, &exception, &ui_class, &ui, &modal_ui};
enum class Id { menu = 1, active, cmenu, cursor, current_ui, ctor, getpos,
    setpos, move, process, x, y, z, ui_index, mousepos, object_id, find_ui,
    yes_no_visible, yes_no_dialog };
enum class Fault { none, owner_lookup, lookup, allocation, getpos_false, getpos_throw, float_throw,
    cursor_throw, cursor_null };
Fault fault = Fault::none;
bool global_available = true, active_available = true, jvm_available = true;
bool current_ui_available = false;
bool modal_visible = false;
std::int32_t ui_index = 4;
bool pending = false;
int mutations = 0, unexpected = 0;
float input_x = 77.0F, input_y = 88.0F;
float modal_input_x = 177.0F, modal_input_y = 188.0F;
int native_deliveries = 0;
bool native_enabled = false;
bool dispatch_available = true;
std::uint32_t last_native_object_id = 0;
int creating_ui_lookups = 0, cursor_factory_lookups = 0;
void* env_table[180]{};
void** env_holder = env_table;
void* vm_table[8]{};
void** vm_holder = vm_table;

void Require(bool ok) { if (!ok) { ++unexpected; std::abort(); } }
void* Token(Id id) { return reinterpret_cast<void*>(static_cast<std::uintptr_t>(id)); }
void* Local(Object& object) { ++object.locals; return &object; }
void CheckClean() {
    Require(!pending && unexpected == 0);
    for (auto* object : objects) Require(object->locals == 0 && object->globals == 0);
}
bool Is(const char* value, const char* expected) { return std::strcmp(value, expected) == 0; }
void* COJVR_JNICALL Find(void*, const char* name) {
    Require(!pending);
    if (Is(name, "GameWithMenu")) {
        if (fault == Fault::owner_lookup) pending = true;
        return Local(game);
    }
    if (Is(name, "LawmanGame")) return Local(lawman);
    if (Is(name, "LawmanModuleSingle")) return Local(single);
    if (Is(name, "Vector")) return Local(vector_class);
    Require(false); return nullptr;
}
void* COJVR_JNICALL Exception(void*) { return pending ? Local(exception) : nullptr; }
void COJVR_JNICALL Clear(void*) { pending = false; }
void* COJVR_JNICALL Global(void*, void* obj) {
    Require(!pending); ++static_cast<Object*>(obj)->globals; return obj;
}
void COJVR_JNICALL DeleteGlobal(void*, void* obj) {
    Require(obj && static_cast<Object*>(obj)->globals > 0);
    --static_cast<Object*>(obj)->globals;
}
void COJVR_JNICALL DeleteLocal(void*, void* obj) {
    Require(obj && static_cast<Object*>(obj)->locals > 0);
    --static_cast<Object*>(obj)->locals;
}
void* COJVR_JNICALL Class(void*, void* obj) {
    Require(!pending);
    if (obj == &global_menu || obj == &active_menu) return Local(menu_class);
    if (obj == &module) return Local(module_class);
    if (obj == &global_cursor || obj == &active_cursor) return Local(cursor_class);
    if (obj == &ui || obj == &modal_ui) return Local(ui_class);
    Require(false); return nullptr;
}
void* COJVR_JNICALL Method(void*, void* cls, const char* name, const char* sig) {
    Require(!pending);
    if (cls == &menu_class && Is(name, "GetGlobalCursor") && Is(sig, "()LUICursorGame;"))
        return Token(Id::cursor);
    if (cls == &menu_class && Is(name, "GetCurrentUI") && Is(sig, "()LGameUserInterface;"))
        return Token(Id::current_ui);
    if (cls == &menu_class && Is(name, "FindUI") && Is(sig, "(I)LGameUserInterface;"))
        return Token(Id::find_ui);
    if (cls == &vector_class && Is(name, "<init>") && Is(sig, "()V")) return Token(Id::ctor);
    if (cls == &cursor_class && Is(name, "GetPos") && Is(sig, "(LVector;)Z")) {
        if (fault == Fault::lookup) { pending = true; return nullptr; }
        return Token(Id::getpos);
    }
    if (cls == &cursor_class && Is(name, "SetPos") && Is(sig, "(LVector;)V"))
        return Token(Id::setpos);
    if (cls == &cursor_class && Is(name, "OnMouseMove") && Is(sig, "(FFI)V"))
        return Token(Id::move);
    if (cls == &ui_class && Is(name, "SetProcessMouse") && Is(sig, "()V"))
        return Token(Id::process);
    if (cls == &ui_class && Is(name, "GetMousePos") && Is(sig, "(LVector;)Z"))
        return Token(Id::mousepos);
    if (cls == &ui_class && Is(name, "GetThisID") && Is(sig, "()I"))
        return Token(Id::object_id);
    Require(false); return nullptr;
}
void* COJVR_JNICALL StaticField(void*, void* cls, const char* name, const char* sig) {
    Require(!pending);
    if (cls == &menu_class && Is(name, "m_nCurUI") && Is(sig, "I")) return Token(Id::ui_index);
    if (cls == &game && Is(name, "sm_cMenuModule") && Is(sig, "LMainMenuModule;"))
        return Token(Id::menu);
    if (cls == &lawman && Is(name, "sm_cActiveGameModule") && Is(sig, "LLawmanModule;"))
        return Token(Id::active);
    Require(false); return nullptr;
}
std::int32_t COJVR_JNICALL StaticInt(void*, void* cls, void* field) {
    Require(!pending && cls == &menu_class && field == Token(Id::ui_index));
    return ui_index;
}
void* COJVR_JNICALL StaticObject(void*, void*, void* field) {
    Require(!pending);
    if (field == Token(Id::menu)) return global_available ? Local(global_menu) : nullptr;
    Require(field == Token(Id::active));
    return active_available ? Local(module) : nullptr;
}
void* COJVR_JNICALL Field(void*, void* cls, const char* name, const char* sig) {
    Require(!pending);
    if (cls == &module_class && Is(name, "cMenu") && Is(sig, "LMainMenuModule;"))
        return Token(Id::cmenu);
    if (cls == &menu_class && Is(name, "m_cCursor") && Is(sig, "LUICursorGame;"))
        return Token(Id::cursor);
    if (cls == &menu_class && Is(name, "m_bYesNoDlgVisible") && Is(sig, "Z"))
        return Token(Id::yes_no_visible);
    if (cls == &menu_class && Is(name, "m_cYesNoDlg") && Is(sig, "LMenuYesNoDialog;"))
        return Token(Id::yes_no_dialog);
    Require(cls == &vector_class && Is(sig, "F"));
    if (Is(name, "fX")) return Token(Id::x);
    if (Is(name, "fY")) return Token(Id::y);
    Require(Is(name, "fZ")); return Token(Id::z);
}
void* COJVR_JNICALL ObjectField(void*, void* obj, void* field) {
    if (obj == &global_menu || obj == &active_menu) {
        Require(!pending);
        if (field == Token(Id::yes_no_dialog))
            return modal_visible ? Local(modal_ui) : nullptr;
        Require(field == Token(Id::cursor));
        if (fault == Fault::cursor_throw) { pending = true; return nullptr; }
        if (fault == Fault::cursor_null) return nullptr;
        return Local(obj == &global_menu ? global_cursor : active_cursor);
    }
    Require(!pending && obj == &module && field == Token(Id::cmenu));
    return Local(active_menu);
}
std::uint8_t COJVR_JNICALL BooleanField(void*, void* obj, void* field) {
    Require(!pending && (obj == &global_menu || obj == &active_menu));
    Require(field == Token(Id::yes_no_visible));
    return modal_visible ? 1 : 0;
}
void* COJVR_JNICALL CallObject(void*, void* obj, void* method, const JValue* args) {
    Require(!pending && (obj == &global_menu || obj == &active_menu));
    if (method == Token(Id::current_ui)) {
        ++creating_ui_lookups;
        return current_ui_available ? Local(ui) : nullptr;
    }
    if (method == Token(Id::find_ui)) {
        Require(args && args[0].i == ui_index);
        return current_ui_available ? Local(ui) : nullptr;
    }
    Require(method == Token(Id::cursor));
    ++cursor_factory_lookups;
    if (fault == Fault::cursor_throw) { pending = true; return nullptr; }
    if (fault == Fault::cursor_null) return nullptr;
    return Local(obj == &global_menu ? global_cursor : active_cursor);
}
void* COJVR_JNICALL New(void*, void* cls, void* ctor, const JValue* args) {
    Require(!pending && cls == &vector_class && ctor == Token(Id::ctor) && !args);
    if (fault == Fault::allocation) { pending = true; return nullptr; }
    vector.x = vector.y = vector.z = 0;
    return Local(vector);
}
std::uint8_t COJVR_JNICALL Boolean(void*, void* obj, void* method, const JValue* args) {
    if (obj == &ui || obj == &modal_ui) {
        Require(!pending && method == Token(Id::mousepos) && args && args[0].l == &vector);
        if (obj == &modal_ui) {
            vector.x = modal_input_x; vector.y = 0; vector.z = modal_input_y;
        } else {
            vector.x = input_x; vector.y = 0; vector.z = input_y;
        }
        return 1;
    }
    Require(!pending && (obj == &global_cursor || obj == &active_cursor));
    Require(method == Token(Id::getpos) && args && args[0].l == &vector);
    if (fault == Fault::getpos_false) return 0;
    if (fault == Fault::getpos_throw) { pending = true; return 1; }
    auto* cursor = static_cast<Object*>(obj);
    vector.x = cursor->x; vector.y = cursor->y; vector.z = cursor->z;
    return 1;
}
float COJVR_JNICALL Float(void*, void* obj, void* field) {
    Require(obj == &vector);
    if (fault == Fault::float_throw) pending = true;
    if (field == Token(Id::x)) return vector.x;
    if (field == Token(Id::y)) return vector.y;
    Require(field == Token(Id::z)); return vector.z;
}
void COJVR_JNICALL SetFloat(void*, void* obj, void* field, float value) {
    Require(!pending && obj == &vector); ++mutations;
    if (field == Token(Id::x)) vector.x = value;
    else if (field == Token(Id::y)) vector.y = value;
    else { Require(field == Token(Id::z)); vector.z = value; }
}
void COJVR_JNICALL Void(void*, void* obj, void* method, const JValue* args) {
    if (obj == &ui || obj == &modal_ui) {
        Require(!pending && method == Token(Id::process) && !args);
        native_enabled = true; ++mutations; return;
    }
    Require(!pending && (obj == &global_cursor || obj == &active_cursor) && args);
    auto* cursor = static_cast<Object*>(obj); ++mutations;
    if (method == Token(Id::setpos)) {
        Require(args[0].l == &vector);
        cursor->x = vector.x; cursor->y = vector.y; cursor->z = vector.z;
    } else {
        Require(method == Token(Id::move) && args[0].f == cursor->x &&
            args[1].f == cursor->y && args[2].i == 0);
    }
}
std::int32_t COJVR_JNICALL Int(void*, void* obj, void* method, const JValue* args) {
    Require(!pending && (obj == &ui || obj == &modal_ui) &&
        method == Token(Id::object_id) && !args);
    return obj == &modal_ui ? 456 : 123;
}
bool DispatchMouse(std::uint32_t id, float x, float y, std::string* error) noexcept {
    Require((id == 123 || id == 456) && native_enabled && !pending);
    if (!dispatch_available) { *error = "native consumer unavailable"; return false; }
    ++native_deliveries;
    last_native_object_id = id;
    if (id == 456) {
        modal_input_x = x; modal_input_y = y;
    } else {
        input_x = x; input_y = y;
    }
    return true;
}
std::int32_t COJVR_JNICALL GetEnv(void*, void** env, std::int32_t version) {
    Require(version == 0x00010004); *env = &env_holder; return 0;
}
std::int32_t COJVR_JNICALL Vms(void** vm, std::int32_t capacity, std::int32_t* count) {
    Require(capacity == 1); *vm = &vm_holder; *count = 1; return 0;
}
HMODULE WINAPI Module(const wchar_t* name) {
    Require(std::wcscmp(name, L"jvm.dll") == 0);
    return jvm_available ? reinterpret_cast<HMODULE>(&vm_holder) : nullptr;
}
FARPROC WINAPI Symbol(HMODULE mod, const char* name) {
    Require(mod == reinterpret_cast<HMODULE>(&vm_holder) && Is(name, "JNI_GetCreatedJavaVMs"));
    return reinterpret_cast<FARPROC>(&Vms);
}
template<class Bridge>
bool Read(Bridge& bridge, JavaPlayerPosition& value, std::string* error) {
    if constexpr (requires { bridge.TryReadUiPointer(value, error); })
        return bridge.TryReadUiPointer(value, error);
    else { *error = "TryReadUiPointer API is missing"; return false; }
}
bool Check(bool ok, const char* message) {
    if (!ok) std::cerr << "FAIL: " << message << '\n';
    return ok;
}
int Run() {
    env_table[6] = reinterpret_cast<void*>(&Find);
    env_table[15] = reinterpret_cast<void*>(&Exception);
    env_table[17] = reinterpret_cast<void*>(&Clear);
    env_table[21] = reinterpret_cast<void*>(&Global);
    env_table[22] = reinterpret_cast<void*>(&DeleteGlobal);
    env_table[23] = reinterpret_cast<void*>(&DeleteLocal);
    env_table[30] = reinterpret_cast<void*>(&New);
    env_table[31] = reinterpret_cast<void*>(&Class);
    env_table[33] = reinterpret_cast<void*>(&Method);
    env_table[36] = reinterpret_cast<void*>(&CallObject);
    env_table[39] = reinterpret_cast<void*>(&Boolean);
    env_table[51] = reinterpret_cast<void*>(&Int);
    env_table[63] = reinterpret_cast<void*>(&Void);
    env_table[94] = reinterpret_cast<void*>(&Field);
    env_table[95] = reinterpret_cast<void*>(&ObjectField);
    env_table[96] = reinterpret_cast<void*>(&BooleanField);
    env_table[102] = reinterpret_cast<void*>(&Float);
    env_table[111] = reinterpret_cast<void*>(&SetFloat);
    env_table[144] = reinterpret_cast<void*>(&StaticField);
    env_table[145] = reinterpret_cast<void*>(&StaticObject);
    env_table[150] = reinterpret_cast<void*>(&StaticInt);
    vm_table[6] = reinterpret_cast<void*>(&GetEnv);
    global_cursor.x = 123.25F; global_cursor.y = 456.5F; global_cursor.z = 7.0F;
    active_cursor.x = 321.0F; active_cursor.y = 654.0F; active_cursor.z = 9.0F;
    bool ok = true;
    for (const bool use_global : {true, false}) {
        global_available = use_global;
        JavaPlayerBridge bridge;
        JavaPlayerPosition value{99, 99, 99}; std::string error = "stale";
        const bool read = Read(bridge, value, &error);
        ok &= Check(read && error.empty() && value.x == (use_global ? 123.25F : 321.0F) &&
            value.y == (use_global ? 456.5F : 654.0F) && value.z == (use_global ? 7.0F : 9.0F),
            "read global/active cursor with null current UI");
        ok &= Check(mutations == 0, "observation must not mutate cursor");
        ok &= Check(creating_ui_lookups == 0 && cursor_factory_lookups == 0,
            "observation must not invoke factories which load UI or create a cursor");
        std::int32_t menu_index = -999;
        ok &= Check(bridge.TryReadUiPointer(value, &error, &menu_index) && menu_index == ui_index,
            "read cursor and shipped static menu index with null current UI");
        ++ui_index;
        ok &= Check(bridge.TryReadUiPointer(value, &error, &menu_index) && menu_index == ui_index,
            "observe a submenu identity transition at unchanged coordinates");
        env_table[150] = nullptr;
        value = {99, 99, 99}; menu_index = 99;
        ok &= Check(!bridge.TryReadUiPointer(value, &error, &menu_index) &&
            value.x == 0 && menu_index == 0, "missing static-int API fails closed without partial output");
        env_table[150] = reinterpret_cast<void*>(&StaticInt);
        ok &= Check(bridge.TryProcessUiPointer(800, 600, &error), "existing writer with null UI");
        value = {};
        ok &= Check(Read(bridge, value, &error) && value.x == 800 && value.y == 600 && value.z == 0,
            "read back accepted write");
        bridge.Reset(); CheckClean(); mutations = 0;
        creating_ui_lookups = cursor_factory_lookups = 0;
    }
    for (const Fault failure : {Fault::owner_lookup, Fault::lookup, Fault::allocation, Fault::getpos_false,
        Fault::getpos_throw, Fault::float_throw, Fault::cursor_throw, Fault::cursor_null}) {
        fault = failure; JavaPlayerBridge bridge; JavaPlayerPosition value{99, 99, 99};
        std::string error;
        ok &= Check(!Read(bridge, value, &error) && value.x == 0 && value.y == 0 && value.z == 0,
            "JNI failures do not publish partial coordinates");
        ok &= Check(!pending && mutations == 0, "exception cleared and cursor untouched");
        bridge.Reset(); CheckClean();
    }
    fault = Fault::none;
    {
        // Ownership lookup errors must not mutate a different menu.
        fault = Fault::owner_lookup;
        JavaPlayerBridge bridge; std::string error;
        ok &= Check(!bridge.TryProcessUiPointer(850, 650, &error),
            "writer fails closed on an ambiguous UI owner lookup");
        bridge.Reset(); CheckClean(); mutations = 0; fault = Fault::none;
    }
    // The shared resolver must retain the writer's optional concrete-UI path.
    {
        current_ui_available = true;
        JavaPlayerBridge bridge; std::string error;
        JavaPlayerPosition value{};
        ok &= Check(Read(bridge, value, &error) && value.x == input_x && value.y == input_y && mutations == 0,
            "sprite echo must not masquerade as the active UI's X/Z mouse input");
        bool consumed = true;
        ok &= Check(!bridge.TryProcessUiPointer(900, 700, &error, nullptr, &consumed) &&
            !consumed && mutations == 0, "a flag/sprite-only route cannot deliver active UI input");
        ok &= Check(bridge.TryProcessUiPointer(900, 700, &error, &DispatchMouse, &consumed) &&
            consumed && native_deliveries == 1 && mutations == 6,
            "writer delivers native hover before moving the cursor sprite");
        ok &= Check(Read(bridge, value, &error) && value.x == 900 && value.y == 700 &&
            mutations == 6, "read actual UI input after native delivery");
        dispatch_available = false;
        ok &= Check(!bridge.TryProcessUiPointer(901, 701, &error, &DispatchMouse, &consumed) &&
            !consumed && native_deliveries == 1 && active_cursor.x == 900,
            "failed native delivery cannot move the cursor or accept a click");
        dispatch_available = true;
        bridge.Reset(); CheckClean(); mutations = 0; current_ui_available = false;
    }
    {
        // The in-game pause menu is driven through the global MainMenuModule
        // when its current UI exists. Keep this distinct from the active-game
        // fallback exercised above.
        global_available = true;
        current_ui_available = true;
        native_enabled = false;
        const int deliveries_before = native_deliveries;
        JavaPlayerBridge bridge; std::string error;
        bool consumed = false;
        ok &= Check(bridge.TryProcessUiPointer(620, 350, &error, &DispatchMouse, &consumed) &&
            consumed && native_deliveries == deliveries_before + 1 && last_native_object_id == 123,
            "global current UI must receive pause-menu native laser hover");
        bridge.Reset(); CheckClean(); mutations = 0; current_ui_available = false;
    }
    {
        // A visible Yes/No dialog disables its parent UI's mouse processing and
        // becomes the actual input root. Hover/readback must follow the dialog,
        // even while the parent remains the current MainMenuModule UI.
        global_available = true;
        active_available = true;
        current_ui_available = true;
        modal_visible = true;
        native_enabled = false;
        const int deliveries_before = native_deliveries;
        JavaPlayerBridge bridge; std::string error;
        bool consumed = false;
        ok &= Check(bridge.TryProcessUiPointer(640, 360, &error, &DispatchMouse, &consumed) &&
            consumed && native_deliveries == deliveries_before + 1 && last_native_object_id == 456,
            "visible Yes/No dialog must receive native laser hover instead of its disabled parent UI");
        JavaPlayerPosition value{};
        ok &= Check(Read(bridge, value, &error) && value.x == 640 && value.y == 360,
            "UI pointer readback must observe the visible Yes/No dialog input root");
        bridge.Reset(); CheckClean();
        mutations = 0; modal_visible = false; current_ui_available = false;
        global_available = false;
    }
    for (const std::size_t missing : {15U, 17U, 30U, 39U, 102U}) {
        void* saved = env_table[missing]; env_table[missing] = nullptr;
        JavaPlayerBridge bridge; JavaPlayerPosition value{99, 99, 99}; std::string error;
        ok &= Check(!Read(bridge, value, &error) && value.x == 0 && value.y == 0 && value.z == 0,
            "missing required JNI callback fails closed");
        env_table[missing] = saved; bridge.Reset(); CheckClean();
    }
    {
        pending = true;
        JavaPlayerBridge bridge; JavaPlayerPosition value{99, 99, 99}; std::string error;
        ok &= Check(!Read(bridge, value, &error) && value.x == 0 && value.y == 0 && value.z == 0,
            "preexisting JNI exception fails closed");
        bridge.Reset(); CheckClean();
    }
    for (const bool no_jvm : {false, true}) {
        active_available = false; jvm_available = !no_jvm;
        JavaPlayerBridge bridge; JavaPlayerPosition value{99, 99, 99}; std::string error;
        ok &= Check(!Read(bridge, value, &error) && value.x == 0 && value.y == 0 && value.z == 0,
            "unavailable menu/JVM fails closed");
        bridge.Reset(); CheckClean();
    }
    active_available = jvm_available = true;
    active_cursor.x = std::numeric_limits<float>::quiet_NaN();
    JavaPlayerBridge bridge; JavaPlayerPosition value{99, 99, 99}; std::string error;
    ok &= Check(!Read(bridge, value, &error) && value.x == 0 && value.y == 0 && value.z == 0,
        "nonfinite cursor fails closed");
    bridge.Reset(); CheckClean();
    if (ok) std::cout << "ui_cursor_jni: all host ABI cases passed\n";
    return ok ? 0 : 1;
}
} // namespace fixture
int main() { return fixture::Run(); }
