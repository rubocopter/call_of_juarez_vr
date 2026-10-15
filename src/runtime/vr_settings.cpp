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
std::atomic<CinematicsMode> current_mode{CinematicsMode::theater};
std::atomic<bool> input_capture{false};
std::atomic<unsigned> save_sequence{0};
CinematicsMode SafeMode(CinematicsMode mode) noexcept {
    return mode==CinematicsMode::immersive ? mode : CinematicsMode::theater;
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
std::uint16_t PressedGlobals(const SettingsMenuInput& input) noexcept {
    const bool values[]{input.options,input.accept,input.back,input.select_left,input.select_right,
        input.auxiliary,input.shoulder_left,input.shoulder_right};
    std::uint16_t mask=0;
    for(unsigned i=0;i<std::size(values);++i)if(values[i])mask|=static_cast<std::uint16_t>(1U<<i);
    return mask;
}
}
CinematicsMode CurrentCinematicMode() noexcept { return current_mode.load(std::memory_order_acquire); }
bool SettingsInputCaptured() noexcept { return input_capture.load(std::memory_order_acquire); }
void PublishCinematicMode(CinematicsMode mode) noexcept { current_mode.store(SafeMode(mode),std::memory_order_release); }
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
        bool version=false,cinematics=false;CinematicsMode mode=CinematicsMode::theater;
        while(!text.empty()){
            const auto end=text.find('\n');const auto line=Trim(text.substr(0,end));
            if(end==std::string_view::npos)text={};else text.remove_prefix(end+1);
            if(line.empty()||line.front()=='#')continue;
            const auto equals=line.find('=');if(equals==std::string_view::npos)return result;
            const auto key=Trim(line.substr(0,equals)),value=Trim(line.substr(equals+1));
            if(key=="version"){if(version||value!="1")return result;version=true;}
            else if(key=="cinematics"){
                if(cinematics||(value!="theater"&&value!="immersive"))return result;
                cinematics=true;mode=value=="immersive"?CinematicsMode::immersive:CinematicsMode::theater;
            }else return result;
        }
        if(version&&cinematics){result.status=SettingsLoadStatus::loaded;result.mode=mode;}
    }catch(...){result.status=SettingsLoadStatus::io_error;result.error_code=ERROR_NOT_ENOUGH_MEMORY;}
    return result;
}
SettingsSaveResult SaveVrSettings(const std::filesystem::path& path,CinematicsMode mode) noexcept {
    SettingsSaveResult result;
    std::filesystem::path temporary;
    bool temporary_owned=false;
    try {
        if(path.empty()||(mode!=CinematicsMode::theater&&mode!=CinematicsMode::immersive)){
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
        const std::string_view content=mode==CinematicsMode::immersive?
            "version=1\ncinematics=immersive\n":"version=1\ncinematics=theater\n";
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
void VrSettingsMenu::Invalidate() noexcept {
    active_=false;capture_=barrier_=true;
    options_armed_=options_down_=accept_down_=back_down_=stick_armed_=false;
}
SettingsMenuResult VrSettingsMenu::Update(const SettingsMenuInput& input) noexcept {
    SettingsMenuResult result{};
    const auto pressed=Pressed(input.gameplay);
    const auto global_held=PressedGlobals(input);
    if(input.available){
        global_release_mask_|=global_held&input.global_available;
        global_release_mask_&=~(input.global_available&~global_held);
    }
    if(input.available&&input.gameplay.active){
        digital_release_mask_|=pressed;
        // Failed polls/focus loss and inactive sources cannot clear history.
        digital_release_mask_&=~(input.gameplay.digital_available&~pressed);
        if(input.gameplay.radial_available)radial_release_required_=input.gameplay.weapon_radial;
        if(input.gameplay.move_available)move_release_required_=!Centered(input.gameplay.move);
        if(input.gameplay.turn_available)turn_release_required_=!Centered(input.gameplay.turn);
    }
    constexpr auto essentials=SettingsMenuInput::options_bit|SettingsMenuInput::accept_bit|SettingsMenuInput::back_bit;
    const bool can_open=(input.global_available&essentials)==essentials;
    const bool source_lost=(previous_global_available_&~input.global_available)!=0;
    const bool options_appeared_held=input.options&&
        (input.global_available&SettingsMenuInput::options_bit)!=0&&
        (previous_global_available_&SettingsMenuInput::options_bit)==0;
    const bool lost=!input.available||!input.context_token||
        options_appeared_held||((active_||options_down_)&&source_lost)||(active_&&!can_open)||
        (context_&&context_!=input.context_token)||(have_time_&&input.monotonic_ms<last_ms_);
    previous_global_available_=input.available?input.global_available:0;
    context_=input.context_token;last_ms_=input.monotonic_ms;have_time_=true;
    if(lost)Invalidate();
    const auto finish=[&]() noexcept {
        result.active=active_;result.input_captured=capture_;result.requested_mode=mode_;return result;
    };
    if(!input.available||!input.context_token)return finish();
    const bool neutral=!(global_held&input.global_available)&&!global_release_mask_&&!digital_release_mask_&&
        !radial_release_required_&&!move_release_required_&&!turn_release_required_;
    if(barrier_){
        if(neutral){
            barrier_=false;stick_armed_=true;
            options_armed_=(input.global_available&SettingsMenuInput::options_bit)!=0;
        }
        return finish(); // The release baseline is itself consumed.
    }
    if(active_){
        capture_=true;
        if(input.back&&!back_down_){Invalidate();return finish();}
        const bool finite=input.gameplay.turn_available&&std::isfinite(input.gameplay.turn.x)&&std::isfinite(input.gameplay.turn.y);
        if(finite&&Centered(input.gameplay.turn))stick_armed_=true;
        else if(!finite)stick_armed_=false;
        const bool left=finite&&stick_armed_&&input.gameplay.turn.x<=-.65F;
        const bool right=finite&&stick_armed_&&input.gameplay.turn.x>=.65F;
        if(left||right||(input.accept&&!accept_down_)){
            const auto next=left?CinematicsMode::theater:right?CinematicsMode::immersive:
                mode_==CinematicsMode::theater?CinematicsMode::immersive:CinematicsMode::theater;
            result.mode_changed=next!=mode_;mode_=next;if(left||right)stick_armed_=false;
        }
        accept_down_=input.accept;back_down_=input.back;
        return finish();
    }
    capture_=false;
    if((input.global_available&SettingsMenuInput::options_bit)==0){options_armed_=options_down_=false;return finish();}
    if(input.options){
        if(options_armed_){
            if(!options_down_){options_down_=true;started_ms_=input.monotonic_ms;}
            capture_=true;
            if(can_open&&input.monotonic_ms-started_ms_>=1000){active_=true;barrier_=true;options_down_=options_armed_=false;}
        }
    }else{
        if(options_down_){
            if(input.monotonic_ms-started_ms_>=1000){
                if(can_open){active_=true;capture_=barrier_=true;}
            }
            else {result.native_pause=true;capture_=false;}
        }
        options_down_=false;options_armed_=true;
    }
    return finish();
}
HudText BuildVrSettingsText(CinematicsMode mode,SettingsLanguage language,bool save_failed,bool awaiting_release,std::uint32_t save_error) noexcept {
    HudText text;
    const bool es=language==SettingsLanguage::spanish;
    const auto append=[&](std::u16string_view value){
        const auto count=std::min(value.size(),text.characters.size()-1-text.length);
        std::copy_n(value.data(),count,text.characters.data()+text.length);text.length+=static_cast<std::uint32_t>(count);
    };
    append(es?u"AJUSTES VR\nCinemáticas: ":u"VR SETTINGS\nCinematics: ");
    append(mode==CinematicsMode::immersive?(es?u"Inmersivo (experimental)":u"Immersive (experimental)"):
        (es?u"Cine (predeterminado)":u"Theater (default)"));
    if(mode==CinematicsMode::immersive)
        append(es?u"\nSubtítulos de cinemáticas pendientes":u"\nMovie subtitles not yet shown");
    append(es?u"\nStick derecho: izq./der. · Cross: cambiar\nCircle: cerrar":u"\nRight stick: left/right · Cross: change\nCircle: close");
    if(awaiting_release)append(es?u"\nSuelta botones y centra los sticks":u"\nPlease release buttons and center sticks");
    else append(es?u"\nMantén Options 1 s para abrir":u"\nHold Options 1 s to open");
    if(save_failed){
        append(es?u"\nNo se pudo guardar":u"\nSave failed");
        if(save_error){
            append(u" (Windows ");
            char16_t digits[10]{};std::size_t count=0;
            do{digits[count++]=u'0'+save_error%10;save_error/=10;}while(save_error);
            for(std::size_t i=count;i>0;--i)append(std::u16string_view(digits+i-1,1));
            append(u")");
        }
        append(es?u"; solo esta sesión":u"; this session only");
    }
    return text;
}
}
