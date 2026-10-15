#include "games/call_of_juarez/ui_language.hpp"
#include <fstream>
#include <string>
namespace cojvr::games::call_of_juarez {
namespace {
std::string_view Trim(std::string_view text) noexcept {
    constexpr std::string_view space=" \t\r";
    const auto first=text.find_first_not_of(space);
    if(first==text.npos)return {};
    return text.substr(first,text.find_last_not_of(space)-first+1);
}
}
CoJUiLanguage ParseCoJUiLanguage(std::string_view text) noexcept {
    if(text.size()>65536)return CoJUiLanguage::english;
    if(text.starts_with("\xEF\xBB\xBF"))text.remove_prefix(3);
    bool seen=false;CoJUiLanguage result=CoJUiLanguage::english;
    while(!text.empty()){
        const auto end=text.find('\n');auto line=Trim(text.substr(0,end));
        text=end==text.npos?std::string_view{}:text.substr(end+1);
        line=Trim(line.substr(0,line.find("//")));
        // Native script declarations (!Locale(s)) are not assignments.
        if(!line.starts_with("Locale"))continue;
        line.remove_prefix(6);line=Trim(line);
        if(line.empty()||line.front()!='(')continue; // Different identifier.
        if(seen)return CoJUiLanguage::english;
        seen=true;line.remove_prefix(1);line=Trim(line);
        if(!line.starts_with('"'))return CoJUiLanguage::english;
        line.remove_prefix(1);const auto quote=line.find('"');
        if(quote==line.npos)return CoJUiLanguage::english;
        const auto locale=line.substr(0,quote);line=Trim(line.substr(quote+1));
        if(line!=")")return CoJUiLanguage::english;
        if(locale=="Es")result=CoJUiLanguage::spanish;
        else if(locale!="En")return CoJUiLanguage::english;
    }
    return result;
}
CoJUiLanguage ReadCoJUiLanguage(const std::filesystem::path& path) noexcept {
    try{
        std::ifstream input(path,std::ios::binary|std::ios::ate);
        if(!input)return CoJUiLanguage::english;
        const auto size=input.tellg();if(size<0||size>65536)return CoJUiLanguage::english;
        std::string text(static_cast<std::size_t>(size),'\0');input.seekg(0);
        if(!input.read(text.data(),static_cast<std::streamsize>(text.size())))return CoJUiLanguage::english;
        return ParseCoJUiLanguage(text);
    }catch(...){return CoJUiLanguage::english;}
}
}
