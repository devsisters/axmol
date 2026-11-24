/****************************************************************************
Copyright (c) 2010-2012 cocos2d-x.org
Copyright (c) 2013-2016 Chukong Technologies Inc.
Copyright (c) 2017-2018 Xiamen Yaji Software Co., Ltd.

https://axmol.dev/

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
****************************************************************************/
#import <Cocoa/Cocoa.h>
#import <Metal/Metal.h>
#include <algorithm>
#include <cmath>
#include <thread>

#include "platform/Application.h"
#include "platform/FileUtils.h"
#include "math/Math.h"
#include "base/Director.h"
#include "base/Utils.h"
#include "renderer/backend/metal/DriverMTL.h"
#import <CoreVideo/CoreVideo.h>
#import <QuartzCore/CAMetalLayer.h>
#include <atomic>

namespace ax
{

Application* Application::sm_pSharedApplication = nullptr;

Application::Application() : _animationInterval(16666667)
{
    AXASSERT(!sm_pSharedApplication, "sm_pSharedApplication already exist");
    sm_pSharedApplication = this;
}

Application::~Application()
{
    AXASSERT(this == sm_pSharedApplication, "sm_pSharedApplication != this");
    sm_pSharedApplication = 0;
}

// 디스플레이 링크 페이싱 (맥 전용).
// VSync 가 켜져 있으면 창이 있는 디스플레이의 vblank 신호(CVDisplayLink)를 받아 N = round(animationInterval / 주사 주기) 번째
// vblank 마다 프레임을 시작한다 (120Hz → 2 = 60fps, 60·75Hz → 1). Metal present 는 CPU 를 블로킹하지 않아 sleep 이나
// 드로어블 대기로는 vblank 위상에 고정되지 않으므로 루프 시작 시점을 vblank 에 묶는다. VSync 꺼짐·링크 실패 시 기존 sleep 페이싱
struct MacDisplayLinkPacer
{
    CVDisplayLinkRef displayLink    = nullptr;
    CGDirectDisplayID displayId     = kCGNullDirectDisplay;
    dispatch_semaphore_t vblankSignal = nullptr;
    std::atomic<int64_t> vblankCount{0};
    int64_t frameStartVblank        = 0;
    int vblanksPerFrame             = 1;
};

static MacDisplayLinkPacer macDisplayLinkPacer;

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
static CVReturn onMacDisplayLinkVblank(CVDisplayLinkRef, const CVTimeStamp*, const CVTimeStamp*, CVOptionFlags, CVOptionFlags*, void*)
{
    macDisplayLinkPacer.vblankCount.fetch_add(1);
    dispatch_semaphore_signal(macDisplayLinkPacer.vblankSignal);
    return kCVReturnSuccess;
}

static bool prepareMacDisplayLinkPacing(RenderView* renderView, std::chrono::nanoseconds animationInterval)
{
    auto& pacer              = macDisplayLinkPacer;
    CAMetalLayer* metalLayer = backend::DriverMTL::getCAMetalLayer();
    NSWindow* window         = (NSWindow*)renderView->getCocoaWindow();
    if (!metalLayer || !metalLayer.displaySyncEnabled || !window || !window.screen)
        return false;

    const auto displayId = static_cast<CGDirectDisplayID>([window.screen.deviceDescription[@"NSScreenNumber"] unsignedIntValue]);
    if (!pacer.displayLink)
    {
        if (CVDisplayLinkCreateWithCGDisplay(displayId, &pacer.displayLink) != kCVReturnSuccess)
        {
            pacer.displayLink = nullptr;
            return false;
        }
        if (!pacer.vblankSignal)
            pacer.vblankSignal = dispatch_semaphore_create(0);
        CVDisplayLinkSetOutputCallback(pacer.displayLink, onMacDisplayLinkVblank, nullptr);
        CVDisplayLinkStart(pacer.displayLink);
        pacer.displayId = displayId;
    }
    else if (pacer.displayId != displayId)
    {
        // 창이 다른 모니터로 이동
        CVDisplayLinkSetCurrentCGDisplay(pacer.displayLink, displayId);
        pacer.displayId = displayId;
    }

    double refreshPeriod = CVDisplayLinkGetActualOutputVideoRefreshPeriod(pacer.displayLink);
    if (refreshPeriod <= 0)
    {
        const CVTime nominal = CVDisplayLinkGetNominalOutputVideoRefreshPeriod(pacer.displayLink);
        if (!(nominal.flags & kCVTimeIsIndefinite) && nominal.timeScale > 0)
            refreshPeriod = static_cast<double>(nominal.timeValue) / nominal.timeScale;
    }
    if (refreshPeriod <= 0)
        return false;

    pacer.vblanksPerFrame = std::max(1, static_cast<int>(std::lround(animationInterval.count() / 1e9 / refreshPeriod)));
    return true;
}

static void stopMacDisplayLinkPacing()
{
    auto& pacer = macDisplayLinkPacer;
    if (pacer.displayLink)
    {
        CVDisplayLinkStop(pacer.displayLink);
        CVDisplayLinkRelease(pacer.displayLink);
        pacer.displayLink = nullptr;
    }
}
#pragma clang diagnostic pop

// 직전 프레임 시작 vblank 로부터 vblanksPerFrame 번째 vblank 까지 대기. 이미 지났으면 바로 진행.
// vblank 신호가 끊기면(디스플레이 절전 등) animationInterval 2배 뒤 진행해 루프가 멈추지 않게 한다
static void waitMacDisplayLinkVblank(std::chrono::nanoseconds animationInterval)
{
    auto& pacer          = macDisplayLinkPacer;
    const int64_t target = pacer.frameStartVblank + pacer.vblanksPerFrame;
    const auto deadline  = dispatch_time(DISPATCH_TIME_NOW, animationInterval.count() * 2);
    while (pacer.vblankCount.load() < target)
    {
        if (dispatch_semaphore_wait(pacer.vblankSignal, deadline) != 0)
            break;
    }
    pacer.frameStartVblank = pacer.vblankCount.load();
}

int Application::run()
{
    initGfxContextAttrs();
    if (!applicationDidFinishLaunching())
    {
        return 1;
    }

    std::chrono::steady_clock::time_point lastTime{};

    constexpr std::chrono::nanoseconds _1ms{1000000};

    auto director = Director::getInstance();
    auto renderView   = director->getRenderView();

    // Retain renderView to avoid renderView being released in the while loop
    renderView->retain();

    while (!renderView->windowShouldClose())
    {
        lastTime = std::chrono::steady_clock::now();
        bool isDisplayPaced = false;
        @autoreleasepool
        {
            isDisplayPaced = prepareMacDisplayLinkPacing(renderView, _animationInterval);
        }
        director->mainLoop();
        renderView->pollEvents();

        auto interval = std::chrono::steady_clock::now() - lastTime;
        auto waitDuration = _animationInterval - interval - _1ms;
        if (isDisplayPaced)
        {
            waitMacDisplayLinkVblank(_animationInterval);
        }
        else if (waitDuration.count() > 0)
            std::this_thread::sleep_for(waitDuration);
        else
            std::this_thread::yield();
    }

    /* Only work on Desktop
     *  Director::mainLoop is really one frame logic
     *  when we want to close the window, we should call Director::end();
     *  then call Director::mainLoop to do release of internal resources
     */
    stopMacDisplayLinkPacing();

    if (renderView->isGfxContextReady())
    {
        director->end();
        director->mainLoop();
    }

    renderView->release();

    return 0;
}

void Application::setAnimationInterval(float interval)
{
    _animationInterval =
        std::chrono::nanoseconds{static_cast<std::chrono::nanoseconds::rep>(std::nano::den * interval)};
}

Application::Platform Application::getTargetPlatform()
{
    return Platform::OSX;
}

std::string Application::getVersion()
{
    NSString* version = [[[NSBundle mainBundle] infoDictionary] objectForKey:@"CFBundleShortVersionString"];
    if (version)
    {
        return [version UTF8String];
    }
    return "";
}

/////////////////////////////////////////////////////////////////////////////////////////////////
// static member function
//////////////////////////////////////////////////////////////////////////////////////////////////

Application* Application::getInstance()
{
    AXASSERT(sm_pSharedApplication, "sm_pSharedApplication not set");
    return sm_pSharedApplication;
}

const char* Application::getCurrentLanguageCode()
{
    static char code[3]       = {0};
    NSUserDefaults* defaults  = [NSUserDefaults standardUserDefaults];
    NSArray* languages        = [defaults objectForKey:@"AppleLanguages"];
    NSString* currentLanguage = [languages objectAtIndex:0];

    // get the current language code.(such as English is "en", Chinese is "zh" and so on)
    NSDictionary* temp     = [NSLocale componentsFromLocaleIdentifier:currentLanguage];
    NSString* languageCode = [temp objectForKey:NSLocaleLanguageCode];
    [languageCode getCString:code maxLength:3 encoding:NSASCIIStringEncoding];
    code[2] = '\0';
    return code;
}

LanguageType Application::getCurrentLanguage()
{
    // get the current language and country config
    NSUserDefaults* defaults  = [NSUserDefaults standardUserDefaults];
    NSArray* languages        = [defaults objectForKey:@"AppleLanguages"];
    NSString* currentLanguage = [languages objectAtIndex:0];

    // get the current language code.(such as English is "en", Chinese is "zh" and so on)
    NSDictionary* temp     = [NSLocale componentsFromLocaleIdentifier:currentLanguage];
    NSString* languageCode = [temp objectForKey:NSLocaleLanguageCode];

    return utils::getLanguageTypeByISO2([languageCode UTF8String]);
}

bool Application::openURL(std::string_view url)
{
    NSString* msg = [NSString stringWithCString:url.data() encoding:NSUTF8StringEncoding];
    NSURL* nsUrl  = [NSURL URLWithString:msg];
    return [[NSWorkspace sharedWorkspace] openURL:nsUrl];
}

void Application::setStartupScriptFilename(std::string_view startupScriptFile)
{
    _startupScriptFilename = startupScriptFile;
    std::replace(_startupScriptFilename.begin(), _startupScriptFilename.end(), '\\', '/');
}

std::string_view Application::getStartupScriptFilename()
{
    return _startupScriptFilename;
}

}
