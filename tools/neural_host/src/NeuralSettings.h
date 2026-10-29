#pragma once
#include <windows.h>
#include <algorithm>
#include <cmath>
#include <cwchar>
#include <cwctype>
struct NeuralOptions {
    bool enabled=false,residual=true,motionVectors=true,autoMask=true,skinAuto=true,outerEnabled=false,outerPack=true;
    int scalePercent=25,intensityPercent=60,passes=1,style=0,preset=0,foveaPercent=100,foveaShape=1,outerScalePercent=25,viewportPercent=100,backend=2,logFrames=3;
    float localStructure=1,localTone=1,skinStrength=1,featherPixels=400,motionSign=1,motionScale=1,depth=.5f;
    unsigned long long appId=0x24480451ull;
    static NeuralOptions Read(const wchar_t* file){
        auto number=[&](const wchar_t* key,double fallback,double lo,double hi){
            wchar_t value[80]{};GetPrivateProfileStringW(L"Neural",key,L"",value,80,file);
            wchar_t* end{};double n=wcstod(value,&end);while(end&&iswspace(*end))++end;
            return end!=value&&end&&!*end&&std::isfinite(n)?std::clamp(n,lo,hi):fallback;
        };
        auto integer=[&](const wchar_t* key,int fallback,int lo,int hi){double n=number(key,fallback,lo,hi);return n==std::floor(n)?int(n):fallback;};
        auto flag=[&](const wchar_t* key,bool fallback){return integer(key,fallback?1:0,0,1)!=0;};
        NeuralOptions o;
        o.enabled=flag(L"Enabled",false);o.scalePercent=integer(L"ScalePercent",25,25,100);o.intensityPercent=integer(L"IntensityPercent",60,0,100);
        o.residual=flag(L"Residual",true);o.passes=integer(L"Passes",1,1,4);
        o.localStructure=float(number(L"LocalStructure",1,0,1));o.localTone=float(number(L"LocalTone",1,0,1));
        o.skinAuto=flag(L"SkinAuto",true);o.skinStrength=float(number(L"SkinStrength",1,0,1));
        o.style=integer(L"Style",0,0,2);o.preset=integer(L"Preset",0,0,1);o.autoMask=flag(L"AutoMask",true);
        o.foveaPercent=integer(L"FoveaPercent",100,20,100);o.foveaShape=integer(L"FoveaShape",1,0,1);
        o.featherPixels=float(number(L"FeatherPixels",400,0,4096));o.outerEnabled=flag(L"OuterEnabled",false);
        o.outerScalePercent=integer(L"OuterScalePercent",25,10,100);o.outerPack=flag(L"OuterPack",true);
        o.viewportPercent=integer(L"ViewportReferencePercent",100,25,100);
        o.motionVectors=flag(L"MotionVectors",true);o.motionSign=integer(L"MotionSign",1,-1,1)==-1?-1.f:1.f;
        o.motionScale=float(number(L"MotionScale",1,0,4));o.depth=float(number(L"ConstantDepth",.5,0,1));
        o.backend=integer(L"Backend",2,0,2);o.logFrames=integer(L"LogFrames",3,0,60);
        o.appId=static_cast<unsigned long long>(number(L"AppId",0x24480451ull,0,4294967295.));
        return o;
    }
};
