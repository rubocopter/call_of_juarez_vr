#include "games/call_of_juarez/ui_language.hpp"
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
using namespace cojvr::games::call_of_juarez;
void Require(bool ok,const char* why){if(!ok){std::cerr<<why<<'\n';std::exit(1);}}
int main(){
    using L=CoJUiLanguage;
    Require(ParseCoJUiLanguage("!Locale(s)\r\nLocale(\"En\")")==L::english,"native English locale");
    Require(ParseCoJUiLanguage("\xEF\xBB\xBF// Locale(\"En\")\n!Locale(s)\n Locale ( \"Es\" ) // selected")==L::spanish,"Spanish BOM/comment/whitespace locale");
    for(auto text:{"", "Locale(\"Fr\")", "Locale(\"Es\")\nLocale(\"En\")", "Locale(\"Es\") garbage", "// Locale(\"Es\")", "OtherLocale(\"Es\")", "!Locale(\"Es\")", "Locale(\"Es\"", "Locale(\"es\")"})
        Require(ParseCoJUiLanguage(text)==L::english,"unknown or ambiguous locale must fall back to English");
    Require(CoJUiText(L::spanish,CoJUiPhrase::concentration_ready)==u"Concentraci\u00f3n \u00b7 Lista","source encoding must preserve actual UTF16 accents");
    Require(CoJUiText(L::spanish,CoJUiPhrase::countdown)==u"Cuenta atr\u00e1s  ","accented countdown");
    Require(CoJUiText(L::english,CoJUiPhrase::horse_health)==u"Horse \u00b7 Health ","English authored label");
    Require(CoJUiText(static_cast<L>(99),CoJUiPhrase::cancel)==u"Cancel","invalid language fallback");
    const auto directory=std::filesystem::temp_directory_path()/L"cojvr-locale-host";
    std::filesystem::create_directories(directory);
    const auto path=directory/L"Game.ini";
    {std::ofstream f(path,std::ios::binary);f<<"Locale(\"Es\")\n";}
    Require(ReadCoJUiLanguage(path)==L::spanish,"read actual configuration");
    {std::ofstream f(path,std::ios::binary);f<<std::string(65537,' ');}
    Require(ReadCoJUiLanguage(path)==L::english,"bounded file read");
    std::filesystem::remove(path);std::filesystem::remove(directory);
    Require(ReadCoJUiLanguage(path)==L::english,"missing configuration fallback");
    std::cout<<"Native locale parsing, bounded read and Unicode labels passed\n";
}
