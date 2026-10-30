#include "backends/openvr/neural_candidate.hpp"
#include <windows.h>
#include <cwchar>
namespace cojvr::backends::openvr::neural {
Options LoadOptions(const std::filesystem::path& file) noexcept{try{
    if(!std::filesystem::is_regular_file(file))return {};
    const auto read=[&](const wchar_t* key,const wchar_t* fallback){
        wchar_t text[32768]{};const auto n=GetPrivateProfileStringW(L"Neural",key,fallback,text,32768,file.c_str());
        return n<32767?std::wstring(text):std::wstring{};
    };
    if(read(L"Enabled",L"0")!=L"1")return {};
    Options o;o.helper=read(L"HostPath",L"");o.runtime=read(L"RuntimeDirectory",L"");
    const auto number=[&](const wchar_t* key,const wchar_t* fallback,unsigned lo,unsigned hi,unsigned& value){
        const auto s=read(key,fallback);if(s.empty()||s.size()>4)return false;
        unsigned v=0;for(auto c:s){if(c<L'0'||c>L'9')return false;v=v*10+unsigned(c-L'0');}
        if(v<lo||v>hi)return false;value=v;return true;
    };
    if(!o.helper.is_absolute()||!o.runtime.is_absolute()||
       !number(L"StageHeight",L"384",256,1024,o.stage_height)||
       !number(L"TimeoutMs",L"8",1,12,o.timeout_ms))return {};
    o.enabled=true;return o;
}catch(...){return {};}}
bool BuildViews(const runtime::Pose& head,const std::array<runtime::EyeView,2>& eyes,std::array<View,2>& output) noexcept{
    output={};if(!head.orientation_valid||!head.position_valid||eyes[0].eye!=runtime::Eye::left||eyes[1].eye!=runtime::Eye::right)return false;
    const auto valid=[](runtime::Quaternion q){return std::isfinite(q.x)&&std::isfinite(q.y)&&std::isfinite(q.z)&&std::isfinite(q.w)&&
        std::abs(q.x*q.x+q.y*q.y+q.z*q.z+q.w*q.w-1)<.01F;};
    if(!valid(head.orientation))return false;
    Header h;h.width=h.height=256;h.request=1;h.input={1,1,1,1,1,1};
    for(unsigned i=0;i<2;++i){
        if(!eyes[i].eye_to_head.orientation_valid||!valid(eyes[i].eye_to_head.orientation))return false;
        const auto a=head.orientation,b=eyes[i].eye_to_head.orientation;
        h.views[i].orientation={a.w*b.x+a.x*b.w+a.y*b.z-a.z*b.y,
            a.w*b.y-a.x*b.z+a.y*b.w+a.z*b.x,a.w*b.z+a.x*b.y-a.y*b.x+a.z*b.w,
            a.w*b.w-a.x*b.x-a.y*b.y-a.z*b.z};
        const auto& f=eyes[i].fov;h.views[i].fov={f.angle_left,f.angle_right,f.angle_up,f.angle_down};
    }
    if(!Valid(h))return false;output=h.views;return true;
}
}
