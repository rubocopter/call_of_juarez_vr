#include "runtime/graphics_settings.hpp"
#include <array>
#include <filesystem>
#include <iostream>
int wmain(int argc,wchar_t** argv) {
    if(argc!=1&&(argc!=3||std::wstring_view(argv[1])!=L"--file"))return 2;
    const auto path=argc==3&&std::wstring_view(argv[1])==L"--file"?
        std::filesystem::path(argv[2]):cojvr::runtime::DefaultGraphicsSettingsPath();
    const auto settings=cojvr::runtime::LoadGraphicsSettings(path);
    if(settings.status!=cojvr::runtime::GraphicsSettingsStatus::loaded&&
       settings.status!=cojvr::runtime::GraphicsSettingsStatus::missing)return 3;
    constexpr std::array<const char*,6> profiles{"Auto","75%","100%","125%","150%","200%"};
    std::cout<<profiles[settings.values.render_scale]<<'\n';
}
