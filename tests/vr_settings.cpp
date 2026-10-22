#include "runtime/vr_settings.hpp"
#include <windows.h>
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <limits>
#include <thread>
using namespace cojvr::runtime;
namespace {
void Require(bool value, const char* why) {
    if (!value) { std::cerr << why << '\n'; std::exit(1); }
}
}
int main() {
    Require(CurrentCinematicMode()==CinematicsMode::theater,"startup must default to theater");
    const auto root=std::filesystem::temp_directory_path()/
        ("cojvr-settings-test-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    const auto file=root/"settings.ini";
    auto loaded=LoadVrSettings(file);
    Require(loaded.mode==CinematicsMode::theater&&loaded.status==SettingsLoadStatus::missing,
        "missing configuration did not default safely");
    Require(SaveVrSettings(file,CinematicsMode::immersive).ok,"valid settings failed to save");
    loaded=LoadVrSettings(file);
    Require(loaded.mode==CinematicsMode::immersive&&loaded.status==SettingsLoadStatus::loaded,"persisted immersive did not survive reload");
    {std::ofstream out(file);out<<"version=1\ncinematics=immersive\ncinematics=theater\n";}
    loaded=LoadVrSettings(file);
    Require(loaded.status==SettingsLoadStatus::invalid&&loaded.mode==CinematicsMode::theater,"duplicate setting admitted ambiguous immersive");
    {std::ofstream out(file);out<<"version=2\ncinematics=immersive\n";}
    Require(LoadVrSettings(file).status==SettingsLoadStatus::invalid,"unknown config version admitted");
    {std::ofstream out(file);out<<std::string(5000,'x');}
    Require(LoadVrSettings(file).status==SettingsLoadStatus::invalid,"oversized config admitted");
    {std::ofstream out(file);out<<"version=1\ncinematics=unknown\n";}
    Require(LoadVrSettings(file).mode==CinematicsMode::theater,"unknown mode enabled immersive");
    auto saved=SaveVrSettings(root,CinematicsMode::immersive);
    Require(!saved.ok&&saved.error_code!=0,"save to directory falsely reported success");
    Require(!SaveVrSettings({},CinematicsMode::immersive).ok,"empty persistence path accepted");
    Require(!SaveVrSettings(file,static_cast<CinematicsMode>(9)).ok,"invalid enum serialized");
    Require(SaveVrSettings(file,VrPreferences{}).ok,"replacement save failed");
    Require(LoadVrSettings(file).mode==CinematicsMode::theater,"replacement failed to persist theater");
    Require(SaveVrSettings(file,VrPreferences{CinematicsMode::immersive,TurnMode::snap,true}).ok,
        "guide preservation fixture failed");
    Require(SaveVrSettings(file,CinematicsMode::theater).ok,"legacy cinema edit failed");
    loaded=LoadVrSettings(file);
    Require(loaded.mode==CinematicsMode::theater&&loaded.turning==TurnMode::snap&&loaded.holster_guides,
        "legacy cinema edit erased comfort preferences");
    HANDLE locked=CreateFileW(file.c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,0,nullptr);
    Require(locked!=INVALID_HANDLE_VALUE,"settings write-denial fixture failed to lock destination");
    saved=SaveVrSettings(file,CinematicsMode::immersive);
    Require(!saved.ok&&saved.error_code!=0,"locked destination save falsely reported persisted selection");
    CloseHandle(locked);
    Require(LoadVrSettings(file).mode==CinematicsMode::theater,"failed replacement destroyed prior persisted setting");
    for(const auto& entry:std::filesystem::directory_iterator(root))
        Require(entry.path().filename()=="settings.ini","failed save left an owned temporary file behind");
    {std::ofstream out(file);out<<"  version = 1\r\n# local preference\r\ncinematics = immersive\r\n";}
    loaded=LoadVrSettings(file);
    Require(loaded.status==SettingsLoadStatus::loaded&&loaded.mode==CinematicsMode::immersive,"ordinary CRLF config failed to parse");
    Require(DefaultVrSettingsPath().filename()=="settings.ini","local settings path missing stable config filename");
    PublishCinematicMode(CinematicsMode::immersive);
    std::thread reader([]{Require(CurrentCinematicMode()==CinematicsMode::immersive,"game thread cannot read published mode");});reader.join();
    PublishCinematicMode(static_cast<CinematicsMode>(99));
    Require(CurrentCinematicMode()==CinematicsMode::theater,"invalid published mode did not fail safe");
    PublishSettingsInputCapture(true);
    Require(SettingsInputCaptured(),"published capture unavailable to consumer");
    PublishSettingsInputCapture(false);
    Require(!SettingsInputCaptured(),"capture publication failed to clear");
    std::filesystem::remove_all(root);
    std::cout<<"VR preference persistence passed\n";
}
