#include "runtime/vr_settings.hpp"
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <algorithm>
#include <atomic>
#include <cmath>
#include <string>
namespace cojvr::runtime {
namespace {
// One atomic publication prevents a mixed preference snapshot. Separate getter
// calls may straddle an update; editors use the versioned Read snapshot.
std::atomic<std::uint16_t> current_preferences{0};
static_assert(decltype(current_preferences)::is_always_lock_free);
std::atomic<bool> input_capture{false};
std::atomic<unsigned> save_sequence{0};
CinematicsMode SafeMode(CinematicsMode mode) noexcept {
    return mode==CinematicsMode::immersive ? mode : CinematicsMode::theater;
}
bool Valid(VrPreferences values) noexcept {
    return (values.cinematics==CinematicsMode::theater||values.cinematics==CinematicsMode::immersive)&&
        (values.turning==TurnMode::smooth||values.turning==TurnMode::snap);
}
void PublishPreferences(VrPreferences values) noexcept {
    const auto packed=static_cast<std::uint16_t>(static_cast<unsigned>(values.cinematics)|
        (static_cast<unsigned>(values.turning)<<8)|(values.holster_guides?0x200U:0U));
    current_preferences.store(packed,std::memory_order_release);
}
struct File {
    HANDLE handle = INVALID_HANDLE_VALUE;
    ~File() { if (handle!=INVALID_HANDLE_VALUE) CloseHandle(handle); }
};
std::string_view Trim(std::string_view text) noexcept {
    while(!text.empty()&&(text.front()==' '||text.front()=='\t'||text.front()=='\r'))text.remove_prefix(1);
    while(!text.empty()&&(text.back()==' '||text.back()=='\t'||text.back()=='\r'))text.remove_suffix(1);
    return text;
}
bool Centered(Vec2 stick) noexcept {
    return std::isfinite(stick.x)&&std::isfinite(stick.y)&&std::hypot(stick.x,stick.y)<=.25F;
}
std::uint32_t Pressed(const GameplayInputState& input) noexcept {
    std::uint32_t mask=0;
    for(std::size_t i=0;i<kGameplayDigitalMembers.size();++i)
        if(input.*kGameplayDigitalMembers[i])mask|=1U<<i;
    for(std::size_t i=0;i<input.equipment_select.size();++i)
        if(input.equipment_select[i])mask|=1U<<(i+kGameplayDigitalMembers.size());
    return mask;
}
std::uint16_t PressedGlobals(const UiReleaseInput& input) noexcept {
    const bool values[]{input.options,input.accept,input.back,input.select_left,input.select_right,
        input.auxiliary,input.shoulder_left,input.shoulder_right};
    std::uint16_t mask=0;
    for(unsigned i=0;i<std::size(values);++i)if(values[i])mask|=static_cast<std::uint16_t>(1U<<i);
    return mask;
}
}
CinematicsMode CurrentCinematicMode() noexcept {
    return static_cast<CinematicsMode>(current_preferences.load(std::memory_order_acquire)&0xFFU);
}
TurnMode CurrentTurnMode() noexcept {
    return static_cast<TurnMode>((current_preferences.load(std::memory_order_acquire)>>8)&1U);
}
bool CurrentHolsterGuidesEnabled() noexcept {
    return (current_preferences.load(std::memory_order_acquire)&0x200U)!=0;
}
bool SettingsInputCaptured() noexcept { return input_capture.load(std::memory_order_acquire); }
void PublishCinematicMode(CinematicsMode mode) noexcept {
    // Compatibility publisher: preserve turning and guides, including concurrent updates.
    auto previous=current_preferences.load(std::memory_order_acquire);
    std::uint16_t next;
    do {
        next=static_cast<std::uint16_t>((previous&0xFF00U)|static_cast<unsigned>(SafeMode(mode)));
    }while(!current_preferences.compare_exchange_weak(previous,next,std::memory_order_acq_rel,std::memory_order_acquire));
}
void PublishSettingsInputCapture(bool captured) noexcept { input_capture.store(captured,std::memory_order_release); }
std::filesystem::path DefaultVrSettingsPath() noexcept {
    try {
        std::wstring value(32768,L'\0');
        const DWORD length=GetEnvironmentVariableW(L"LOCALAPPDATA",value.data(),static_cast<DWORD>(value.size()));
        if(!length||length>=value.size())return {};
        value.resize(length);
        const std::filesystem::path local(value);
        if(!local.is_absolute())return {};
        return local/L"CoJVR"/L"CallOfJuarez"/L"settings.ini";
    }catch(...){return {};}
}
SettingsLoadResult LoadVrSettings(const std::filesystem::path& path) noexcept {
    SettingsLoadResult result;
    try {
        if(path.empty()){result.status=SettingsLoadStatus::io_error;result.error_code=ERROR_INVALID_NAME;return result;}
        File file{CreateFileW(path.c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr)};
        if(file.handle==INVALID_HANDLE_VALUE){
            result.error_code=GetLastError();
            result.status=(result.error_code==ERROR_FILE_NOT_FOUND||result.error_code==ERROR_PATH_NOT_FOUND)?
                SettingsLoadStatus::missing:SettingsLoadStatus::io_error;
            return result;
        }
        LARGE_INTEGER size{};
        if(!GetFileSizeEx(file.handle,&size)){result.status=SettingsLoadStatus::io_error;result.error_code=GetLastError();return result;}
        result.status=SettingsLoadStatus::invalid;
        if(size.QuadPart<=0||size.QuadPart>4096)return result;
        std::string buffer(static_cast<std::size_t>(size.QuadPart),'\0');DWORD read=0;
        if(!ReadFile(file.handle,buffer.data(),static_cast<DWORD>(buffer.size()),&read,nullptr)||read!=buffer.size()){
            result.status=SettingsLoadStatus::io_error;result.error_code=GetLastError();
            if(!result.error_code)result.error_code=ERROR_READ_FAULT;
            return result;
        }
        std::string_view text=buffer;
        unsigned version=0;
        bool cinematics=false,turning=false,holster_guides=false;
        VrPreferences values;
        while(!text.empty()){
            const auto end=text.find('\n');const auto line=Trim(text.substr(0,end));
            if(end==std::string_view::npos)text={};else text.remove_prefix(end+1);
            if(line.empty()||line.front()=='#')continue;
            const auto equals=line.find('=');if(equals==std::string_view::npos)return result;
            const auto key=Trim(line.substr(0,equals)),value=Trim(line.substr(equals+1));
            if(key=="version"){
                if(version||(value!="1"&&value!="2"&&value!="3"))return result;
                version=value=="1"?1U:value=="2"?2U:3U;
            }
            else if(key=="cinematics"){
                if(cinematics||(value!="theater"&&value!="immersive"))return result;
                cinematics=true;values.cinematics=value=="immersive"?CinematicsMode::immersive:CinematicsMode::theater;
            }else if(key=="turning"){
                if(turning||(value!="smooth"&&value!="snap"))return result;
                turning=true;values.turning=value=="snap"?TurnMode::snap:TurnMode::smooth;
            }else if(key=="holster_guides"){
                if(holster_guides||(value!="false"&&value!="true"))return result;
                holster_guides=true;values.holster_guides=value=="true";
            }else return result;
        }
        if(cinematics&&((version==1&&!turning&&!holster_guides)||(version==2&&turning&&!holster_guides)||
            (version==3&&turning&&holster_guides))){
            result.status=SettingsLoadStatus::loaded;result.mode=values.cinematics;
            result.turning=values.turning;result.holster_guides=values.holster_guides;
        }
    }catch(...){result.status=SettingsLoadStatus::io_error;result.error_code=ERROR_NOT_ENOUGH_MEMORY;}
    return result;
}
namespace {
SettingsSaveResult WriteVrSettings(const std::filesystem::path& path,std::string_view content) noexcept {
    SettingsSaveResult result;
    std::filesystem::path temporary;
    bool temporary_owned=false;
    try {
        if(path.empty()){
            result.error_code=ERROR_INVALID_PARAMETER;return result;
        }
        std::error_code error;
        if(!path.parent_path().empty())std::filesystem::create_directories(path.parent_path(),error);
        if(error){result.error_code=static_cast<std::uint32_t>(error.value());return result;}
        // Unique local temporary file; never part of the game's staging journal.
        temporary=path;
        temporary+=L".tmp-"+std::to_wstring(GetCurrentProcessId())+L"-"+
            std::to_wstring(save_sequence.fetch_add(1));
        File file{CreateFileW(temporary.c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr)};
        if(file.handle==INVALID_HANDLE_VALUE){result.error_code=GetLastError();return result;}
        temporary_owned=true;
        DWORD written=0;
        if(!WriteFile(file.handle,content.data(),static_cast<DWORD>(content.size()),&written,nullptr)||written!=content.size()){
            result.error_code=GetLastError();if(!result.error_code)result.error_code=ERROR_WRITE_FAULT;
        }else if(!FlushFileBuffers(file.handle))result.error_code=GetLastError();
        const bool closed=CloseHandle(file.handle)!=FALSE;file.handle=INVALID_HANDLE_VALUE;
        if(!closed&&!result.error_code)result.error_code=GetLastError();
        if(!result.error_code&&!MoveFileExW(temporary.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))
            result.error_code=GetLastError();
        result.ok=result.error_code==0;
    }catch(...){result.error_code=ERROR_NOT_ENOUGH_MEMORY;}
    if(!result.ok&&temporary_owned)DeleteFileW(temporary.c_str());
    return result;
}
}
SettingsSaveResult SaveVrSettings(const std::filesystem::path& path,CinematicsMode mode) noexcept {
    if(mode!=CinematicsMode::theater&&mode!=CinematicsMode::immersive)return {false,ERROR_INVALID_PARAMETER};
    // Read persisted preferences before a cinema-only replacement so this
    // compatibility overload cannot downgrade comfort values.
    const auto loaded=LoadVrSettings(path);
    if(loaded.status==SettingsLoadStatus::invalid)return {false,ERROR_INVALID_DATA};
    if(loaded.status==SettingsLoadStatus::io_error)return {false,loaded.error_code?loaded.error_code:ERROR_READ_FAULT};
    return SaveVrSettings(path,VrPreferences{mode,loaded.turning,loaded.holster_guides});
}
SettingsSaveResult SaveVrSettings(const std::filesystem::path& path,VrPreferences values) noexcept {
    if(!Valid(values))return {false,ERROR_INVALID_PARAMETER};
    try {
        std::string content="version=3\ncinematics=";
        content+=values.cinematics==CinematicsMode::immersive?"immersive":"theater";
        content+="\nturning=";
        content+=values.turning==TurnMode::snap?"snap":"smooth";
        content+="\nholster_guides=";
        content+=values.holster_guides?"true\n":"false\n";
        return WriteVrSettings(path,content);
    }catch(...){return {false,ERROR_NOT_ENOUGH_MEMORY};}
}
VrPreferenceSnapshot VrPreferenceStore::Initialize(const std::filesystem::path& file) noexcept {
    std::lock_guard lock(mutex_);
    if(initialized_)return snapshot_;
    try {
        file_=file;
        const auto loaded=LoadVrSettings(file_);
        snapshot_.values={loaded.mode,loaded.turning,loaded.holster_guides};
        snapshot_.load_status=loaded.status;
        snapshot_.load_error=loaded.error_code;
    }catch(...){
        file_.clear();
        snapshot_.values={};
        snapshot_.load_status=SettingsLoadStatus::io_error;
        snapshot_.load_error=ERROR_NOT_ENOUGH_MEMORY;
    }
    initialized_=true;
    snapshot_.revision=1;
    PublishPreferences(snapshot_.values);
    return snapshot_;
}
VrPreferenceSnapshot VrPreferenceStore::Read() const noexcept {
    std::lock_guard lock(mutex_);
    return snapshot_;
}
VrPreferenceApplyResult VrPreferenceStore::ApplyLocked(VrPreferences values,std::uint64_t expected_revision) noexcept {
    VrPreferenceApplyResult result;
    if(!Valid(values))result.error_code=ERROR_INVALID_PARAMETER;
    else if(!initialized_)result.error_code=ERROR_NOT_READY;
    else if(expected_revision!=snapshot_.revision){
        result.conflict=true;result.error_code=ERROR_REVISION_MISMATCH;
    }else{
        result.accepted=true;
        snapshot_.values=values;
        ++snapshot_.revision;
        PublishPreferences(values);
        // The editor lock remains held through replacement: an older save can
        // never complete after a newer edit and overwrite its persisted choice.
        // Never erase an unrecognized future schema, malformed data, or a file
        // we could not read. Only loaded/missing startup states authorize I/O.
        const auto saved=snapshot_.load_status==SettingsLoadStatus::invalid?
            SettingsSaveResult{false,ERROR_INVALID_DATA}:
            snapshot_.load_status==SettingsLoadStatus::io_error?
                SettingsSaveResult{false,snapshot_.load_error?snapshot_.load_error:ERROR_READ_FAULT}:
                SaveVrSettings(file_,values);
        result.saved=saved.ok;result.error_code=saved.error_code;
        snapshot_.save_failed=!saved.ok;snapshot_.save_error=saved.error_code;
    }
    result.snapshot=snapshot_;
    return result;
}
VrPreferenceApplyResult VrPreferenceStore::Apply(VrPreferences values,std::uint64_t expected_revision) noexcept {
    std::lock_guard lock(mutex_);
    return ApplyLocked(values,expected_revision);
}
VrPreferenceApplyResult VrPreferenceStore::ApplyCinematics(CinematicsMode mode,std::uint64_t expected_revision) noexcept {
    std::lock_guard lock(mutex_);
    auto values=snapshot_.values;
    values.cinematics=mode;
    return ApplyLocked(values,expected_revision);
}
VrPreferenceStore& SharedVrPreferences() noexcept {
    static VrPreferenceStore store;
    return store;
}
void VrInputReleaseGate::Invalidate() noexcept {
    capture_=barrier_=true;options_armed_=options_down_=false;
}
UiReleaseResult VrInputReleaseGate::Update(const UiReleaseInput& input) noexcept {
    UiReleaseResult result{};
    const auto pressed=Pressed(input.gameplay);
    const auto global_held=PressedGlobals(input);
    if(input.available){
        global_release_mask_|=global_held&input.global_available;
        global_release_mask_&=~(input.global_available&~global_held);
    }
    if(input.available&&input.gameplay.active){
        digital_release_mask_|=pressed;
        digital_release_mask_&=~(input.gameplay.digital_available&~pressed);
        if(input.gameplay.radial_available)radial_release_required_=input.gameplay.weapon_radial;
        if(input.gameplay.move_available)move_release_required_=!Centered(input.gameplay.move);
        if(input.gameplay.turn_available)turn_release_required_=!Centered(input.gameplay.turn);
    }
    const bool options_available=(input.global_available&UiReleaseInput::options_bit)!=0;
    const bool appeared_held=input.options&&options_available&&
        !(previous_global_available_&UiReleaseInput::options_bit);
    const bool lost=!input.available||!input.context_token||appeared_held||
        (options_down_&&!options_available)||(context_&&context_!=input.context_token)||
        (have_time_&&input.monotonic_ms<last_ms_);
    previous_global_available_=input.available?input.global_available:0;
    context_=input.context_token;last_ms_=input.monotonic_ms;have_time_=true;
    if(lost)Invalidate();
    if(!input.available||!input.context_token)return result;
    const bool neutral=!(global_held&input.global_available)&&!global_release_mask_&&!digital_release_mask_&&
        !radial_release_required_&&!move_release_required_&&!turn_release_required_;
    if(barrier_){
        if(neutral){barrier_=false;options_armed_=options_available;}
        return result; // Consume the baseline itself.
    }
    capture_=false;result.input_captured=false;
    if(!options_available){options_armed_=options_down_=false;return result;}
    if(input.options){
        result.native_pause=options_armed_&&!options_down_;
        options_down_=true;options_armed_=false;
    }else {options_down_=false;options_armed_=true;}
    return result;
}
}
