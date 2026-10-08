#include "games/call_of_juarez/interaction_selection.hpp"

#include <array>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <limits>
#include <string>
#include <type_traits>
#include <vector>

#if defined(_WIN32)
#define TEST_JNICALL __stdcall
#else
#define TEST_JNICALL
#endif

using namespace cojvr::games::call_of_juarez;
namespace {
void Check(bool ok, const char* message) {
    if (!ok) { std::cerr << message << '\n'; std::exit(1); }
}
enum Object { being, active, executing, replacement, being_class, target_class, exception };
struct Ref { Object object{}; bool live = false; };
enum Token { active_getter, executing_getter, id_getter, element_field, range_field, disabled_field };
struct Operation { std::string name; bool optional; };

struct Fake {
    // First member has the same pointer-to-function-table shape as JNIEnv*.
    void** functions = nullptr;
    std::array<void*, 174> table{};
    std::array<Ref, 128> locals{};
    Ref owner{being, true};
    std::array<Token, 6> tokens{active_getter, executing_getter, id_getter,
        element_field, range_field, disabled_field};
    std::vector<Operation> operations;
    bool pending = false, null_fault = false, returned_ref_fault = false;
    int fail = -1, live_refs = 0, clears = 0;
    int active_reads = 0, executing_reads = 0, element_reads = 0;
    int active_change = -1, executing_change = -1;
    bool active_present = true, executing_present = true, shared = false;
    bool change_element = false;
    std::int32_t active_id = 101, executing_id = 202, element = 7;
    float range = 250.0F;
    std::uint8_t disabled = 0;

    void* Env() { return &functions; }
    static Fake& From(void* env) {
        return *static_cast<Fake*>(env);
    }
    bool Fault(const char* name, bool optional = false) {
        Check(!pending, "JNI observation performed with pending exception");
        operations.push_back({name, optional});
        if (static_cast<int>(operations.size()) != fail) return false;
        pending = !null_fault;
        return true;
    }
    void* Local(Object object) {
        for (auto& ref : locals) if (!ref.live) {
            ref = {object, true}; ++live_refs; return &ref;
        }
        Check(false, "fixture reference capacity exceeded"); return nullptr;
    }
    Object Identity(void* ref) {
        Check(ref != nullptr && static_cast<Ref*>(ref)->live, "invalid or deleted JNI local");
        return static_cast<Ref*>(ref)->object;
    }
    Token Value(void* token) {
        Check(token != nullptr, "null JNI member ID used");
        return *static_cast<Token*>(token);
    }
    static void TEST_JNICALL Forbidden() {
        Check(false, "reader invoked write/global/allocation/trace/update JNI entry");
    }
    static void* TEST_JNICALL Class(void* env, void* object) {
        auto& f = From(env); const auto id = f.Identity(object);
        Check(id == being || id == active || id == executing, "unexpected class owner");
        const bool failed = f.Fault("class");
        if (failed && !f.returned_ref_fault) return nullptr;
        return f.Local(id == being ? being_class : target_class);
    }
    static void* TEST_JNICALL Method(void* env, void* cls, const char* name, const char* signature) {
        auto& f = From(env); const auto id = f.Identity(cls);
        Token token{};
        if (id == being_class && std::strcmp(name, "GetActiveTrigger") == 0) token = active_getter;
        else if (id == being_class && std::strcmp(name, "GetExecutingTrigger") == 0) token = executing_getter;
        else {
            Check(id == target_class && std::strcmp(name, "GetThisID") == 0,
                "unapproved method/owner (including tracing overloaded getter)");
            token = id_getter;
        }
        Check(std::strcmp(signature, token == id_getter ? "()I" : "()LTriggerObject;") == 0,
            "only audited zero-argument method signatures allowed");
        if (f.Fault(name)) return nullptr;
        return &f.tokens[token];
    }
    static void* TEST_JNICALL ObjectCall(void* env, void* object, void* method, const void* args) {
        auto& f = From(env); Check(f.Identity(object) == being && args == nullptr, "getter owner/arguments");
        const auto token = f.Value(method);
        Check(token == active_getter || token == executing_getter, "unapproved object method");
        const bool is_active = token == active_getter;
        auto& reads = is_active ? f.active_reads : f.executing_reads;
        ++reads; Check(reads <= 2, "reader retried cached getter");
        const bool failed = f.Fault(is_active ? "active" : "executing");
        if (failed && !f.returned_ref_fault) return nullptr;
        const int change = is_active ? f.active_change : f.executing_change;
        if (reads == 2 && change >= 0) return change == 0 ? nullptr : f.Local(replacement);
        if (!(is_active ? f.active_present : f.executing_present)) return nullptr;
        return f.Local(is_active || f.shared ? active : executing);
    }
    static std::int32_t TEST_JNICALL IntCall(void* env, void* object, void* method, const void* args) {
        auto& f = From(env); const auto id = f.Identity(object);
        Check((id == active || id == executing) && f.Value(method) == id_getter && args == nullptr,
            "unapproved integer method/owner/arguments");
        if (f.Fault("id")) return 0;
        return id == active ? f.active_id : f.executing_id;
    }
    static void* TEST_JNICALL Field(void* env, void* cls, const char* name, const char* signature) {
        auto& f = From(env); const auto id = f.Identity(cls);
        Token token{};
        if (id == target_class && std::strcmp(name, "nActiveElement") == 0) {
            token = element_field; Check(std::strcmp(signature, "I") == 0, "element descriptor");
        } else if (id == being_class && std::strcmp(name, "m_fTriggerVisibilityRange") == 0) {
            token = range_field; Check(std::strcmp(signature, "F") == 0, "range descriptor");
        } else {
            Check(id == being_class && std::strcmp(name, "m_bDisableTriggerDetection") == 0 &&
                std::strcmp(signature, "Z") == 0, "unapproved field/owner/descriptor");
            token = disabled_field;
        }
        if (f.Fault(name, token != element_field)) return nullptr;
        return &f.tokens[token];
    }
    static std::int32_t TEST_JNICALL IntField(void* env, void* object, void* field) {
        auto& f = From(env);
        Check(f.Identity(object) == active && f.Value(field) == element_field, "element field owner");
        ++f.element_reads; Check(f.element_reads <= 2, "reader retried element read");
        if (f.Fault("element")) return 0;
        return f.element + (f.change_element && f.element_reads == 2 ? 1 : 0);
    }
    static float TEST_JNICALL FloatField(void* env, void* object, void* field) {
        auto& f = From(env);
        Check(f.Identity(object) == being && f.Value(field) == range_field, "range field owner");
        if (f.Fault("range", true)) return 0.0F;
        return f.range;
    }
    static std::uint8_t TEST_JNICALL BooleanField(void* env, void* object, void* field) {
        auto& f = From(env);
        Check(f.Identity(object) == being && f.Value(field) == disabled_field, "disabled field owner");
        if (f.Fault("disabled", true)) return 0;
        return f.disabled;
    }
    static std::uint8_t TEST_JNICALL Same(void* env, void* left, void* right) {
        auto& f = From(env);
        if (f.Fault("same")) return 0;
        return (!left || !right) ? left == right : f.Identity(left) == f.Identity(right);
    }
    static void* TEST_JNICALL Exception(void* env) {
        auto& f = From(env); return f.pending ? f.Local(exception) : nullptr;
    }
    static void TEST_JNICALL Clear(void* env) {
        auto& f = From(env); Check(f.pending, "spurious exception clear"); f.pending = false; ++f.clears;
    }
    static void TEST_JNICALL Delete(void* env, void* object) {
        auto& f = From(env); Check(object != &f.owner, "reader deleted caller-owned player");
        (void)f.Identity(object); static_cast<Ref*>(object)->live = false; --f.live_refs;
    }
    template<class Fn> void Set(int index, Fn fn) { table[index] = reinterpret_cast<void*>(fn); }
    Fake() {
        functions = table.data();
        table.fill(reinterpret_cast<void*>(Forbidden));
        Set(15, Exception); Set(17, Clear); Set(23, Delete); Set(24, Same);
        Set(31, Class); Set(33, Method); Set(36, ObjectCall); Set(51, IntCall);
        Set(94, Field); Set(96, BooleanField); Set(100, IntField); Set(102, FloatField);
    }
    void Balanced() {
        Check(!pending && live_refs == 0 && owner.live, "JNI references/exception/borrowed owner unbalanced");
        Check(active_reads <= 2 && executing_reads <= 2, "retry observed");
    }
};

CoJInteractionSelection Poisoned() {
    CoJInteractionSelection out{};
    out.valid = out.range_valid = out.disabled_valid = out.detection_disabled = true;
    out.active_id = 999; out.executing_id = 888; out.active_element = 777; out.native_range_cm = 666;
    return out;
}
void Empty(const CoJInteractionSelection& out) {
    Check(!out.valid && out.active_id == 0 && out.executing_id == 0 && out.active_element == -1 &&
        !out.range_valid && out.native_range_cm == 0 && !out.disabled_valid && !out.detection_disabled,
        "failed read published partial or stale values");
}
bool Read(Fake& f, CoJInteractionSelection& out) {
    const bool ok = ReadCoJInteractionSelection(f.Env(), &f.owner, out); f.Balanced(); return ok;
}
}

int main() {
    static_assert(std::is_trivially_copyable_v<CoJInteractionSelection>);
    Fake baseline; auto out = Poisoned();
    Check(Read(baseline, out) && out.valid && out.active_id == 101 && out.executing_id == 202 &&
        out.active_element == 7 && out.range_valid && out.native_range_cm == 250 &&
        out.disabled_valid && !out.detection_disabled, "complete cached selection read failed");
    Check(baseline.active_reads == 2 && baseline.executing_reads == 2,
        "both identities must be rechecked even though distinct JNI handles denote the same objects");
    const auto operations = baseline.operations;

    for (const bool active_present : {false, true}) for (const bool executing_present : {false, true}) {
        Fake f; f.active_present = active_present; f.executing_present = executing_present; out = Poisoned();
        Check(Read(f, out) && out.valid && out.active_id == (active_present ? 101 : 0) &&
            out.executing_id == (executing_present ? 202 : 0) && out.active_element == (active_present ? 7 : -1),
            "null cache is a valid absence, not failed observation");
    }
    { Fake f; f.shared = true; Check(Read(f, out) && out.active_id == out.executing_id,
        "same active/executing target must be accepted with balanced distinct local refs"); }
    { Fake f; f.element = -1; Check(Read(f, out) && out.active_element == -1, "native element sentinel rejected"); }
    { Fake f; f.element = -2; Check(!Read(f, out), "invalid negative element accepted"); Empty(out); }
    for (const auto id : {0, -1, std::numeric_limits<std::int32_t>::min()}) for (bool active_id : {false, true}) {
        Fake f; (active_id ? f.active_id : f.executing_id) = id; out = Poisoned();
        Check(!Read(f, out), "non-null target with invalid native ID accepted"); Empty(out);
    }
    { Fake f; f.active_id = f.executing_id = std::numeric_limits<std::int32_t>::max();
      Check(Read(f, out) && out.active_id == f.active_id, "positive JNI int ID narrowed/reinterpreted"); }
    for (bool change_active : {false, true}) for (int change : {0, 1}) {
        Fake f; (change_active ? f.active_change : f.executing_change) = change; out = Poisoned();
        Check(!Read(f, out), "changed cache identity published"); Empty(out);
    }
    for (bool change_active : {false, true}) {
        Fake f; (change_active ? f.active_present : f.executing_present) = false;
        (change_active ? f.active_change : f.executing_change) = 1;
        Check(!Read(f, out), "null-to-target cache transition published"); Empty(out);
    }
    { Fake f; f.change_element = true; Check(!Read(f, out), "changed selected element published"); Empty(out); }
    for (float range : {std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::infinity(),
            -std::numeric_limits<float>::infinity(), -1.0F}) {
        Fake f; f.range = range; out = Poisoned();
        Check(Read(f, out) && !out.range_valid && out.native_range_cm == 0 && out.disabled_valid,
            "invalid optional range contaminated snapshot");
    }
    { Fake f; f.range = 0; f.disabled = 1;
      Check(Read(f, out) && out.range_valid && out.native_range_cm == 0 && out.detection_disabled,
          "native zero range/disabled caches must be observed without imposing eligibility"); }
    { Fake f; f.disabled = 2;
      Check(Read(f, out) && !out.disabled_valid && !out.detection_disabled && out.range_valid,
          "invalid JNI boolean metadata accepted"); }
    for (int index : {15, 17, 23, 24, 31, 33, 36, 51, 94, 100}) {
        Fake f; f.table[index] = nullptr; out = Poisoned();
        Check(!Read(f, out) && f.operations.empty(), "missing required JNI ABI entry not rejected before calls"); Empty(out);
    }
    for (int index : {96, 102}) {
        Fake f; f.table[index] = nullptr;
        Check(Read(f, out) && (index == 96 ? !out.disabled_valid && out.range_valid :
            !out.range_valid && out.disabled_valid), "missing optional JNI entry poisoned selection");
    }
    { Fake f; f.pending = true; out = Poisoned(); Check(!Read(f, out) && f.operations.empty() && f.clears == 1,
          "preexisting exception did not fail/clear before reads"); Empty(out); }
    { Fake f; out = Poisoned(); Check(!ReadCoJInteractionSelection(nullptr, &f.owner, out), "null env accepted"); Empty(out); }
    { Fake f; out = Poisoned(); Check(!ReadCoJInteractionSelection(f.Env(), nullptr, out), "null being accepted"); Empty(out); }
    { void** table = nullptr; out = Poisoned(); Check(!ReadCoJInteractionSelection(&table, &table, out), "null ABI table accepted"); Empty(out); }

    // Throw at every required and optional observation, including a non-null
    // returned local alongside an exception. Never continue with pending JNI.
    for (bool returned_ref : {false, true}) for (std::size_t i = 0; i < operations.size(); ++i) {
        Fake f; f.fail = static_cast<int>(i + 1); f.returned_ref_fault = returned_ref; out = Poisoned();
        const bool ok = Read(f, out);
        Check(ok == operations[i].optional, "exception did not respect required/optional publication boundary");
        Check(f.clears == 1, "injected exception not cleared exactly once");
        if (!ok) Empty(out);
        else Check(out.valid && out.active_id == 101 && out.executing_id == 202 &&
            out.active_element == 7, "optional exception changed required values");
    }
    // Null member/class lookups without an exception must also fail closed.
    for (std::size_t i = 0; i < operations.size(); ++i) {
        const auto& op = operations[i];
        if (op.name != "class" && op.name != "GetActiveTrigger" && op.name != "GetExecutingTrigger" &&
            op.name != "GetThisID" && op.name != "nActiveElement" &&
            op.name != "m_fTriggerVisibilityRange" && op.name != "m_bDisableTriggerDetection") continue;
        Fake f; f.fail = static_cast<int>(i + 1); f.null_fault = true; out = Poisoned();
        Check(Read(f, out) == op.optional, "null lookup did not respect required/optional boundary");
        if (!op.optional) Empty(out);
    }
    // Fixtures isolate owner and state: each invocation is fresh, no cached globals.
    { Fake f; f.active_id = 303; f.executing_id = 404; f.element = 0;
      Check(Read(f, out) && out.active_id == 303 && out.executing_id == 404 && out.active_element == 0,
          "reader reused state from a previous owner/read"); }
    std::cout << "interaction selection checks passed; " << operations.size()
        << " observation fault sites, identity/ref cleanup and read-only ABI whitelist\n";
}

#undef TEST_JNICALL
