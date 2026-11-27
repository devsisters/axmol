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
#include <mach/mach_time.h>
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
// 메인 스레드는 CVDisplayLink 스레드(QoS 없음)의 신호를 기다리지 않는다 (우선순위 역전). 콜백이 남긴 vblank 시각에서 목표 vblank 시각을 구해 그때까지 잔다
struct MacDisplayLinkPacer
{
    CVDisplayLinkRef displayLink    = nullptr;
    CGDirectDisplayID displayId     = kCGNullDirectDisplay;
    // 마지막 vblank 의 호스트 시각(mach_absolute_time 단위)과 지금까지의 vblank 수. 콜백 스레드가 쓴다
    std::atomic<uint64_t> lastVblankHostTime{0};
    std::atomic<int64_t> vblankCount{0};
    int64_t frameStartVblank        = 0;
    int vblanksPerFrame             = 1;
    double refreshPeriod            = 0.0;
};

static MacDisplayLinkPacer macDisplayLinkPacer;

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
static CVReturn onMacDisplayLinkVblank(CVDisplayLinkRef, const CVTimeStamp* now, const CVTimeStamp*, CVOptionFlags, CVOptionFlags*, void*)
{
    // 시각을 먼저 쓰고 수를 올린다 (읽는 쪽은 수가 바뀌지 않은 동안의 시각만 쓴다)
    macDisplayLinkPacer.lastVblankHostTime.store(now->hostTime);
    macDisplayLinkPacer.vblankCount.fetch_add(1);
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

    pacer.refreshPeriod   = refreshPeriod;
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

static uint64_t macNanosecondsToHostTime(double nanoseconds)
{
    static mach_timebase_info_data_t timebase = [] {
        mach_timebase_info_data_t info{};
        mach_timebase_info(&info);
        return info;
    }();
    return static_cast<uint64_t>(nanoseconds * timebase.denom / timebase.numer);
}

// 직전 프레임 시작 vblank 로부터 vblanksPerFrame 번째 vblank 까지 대기. 이미 지났으면 바로 진행.
// 마지막 vblank 시각 + 남은 vblank 수 × 주사 주기까지 잔다. vblank 신호가 끊기면(디스플레이 절전 등) 최대 animationInterval 2배만 자서 루프가 멈추지 않게 한다
static void waitMacDisplayLinkVblank(std::chrono::nanoseconds animationInterval)
{
    auto& pacer          = macDisplayLinkPacer;
    const int64_t target = pacer.frameStartVblank + pacer.vblanksPerFrame;
    // 마지막 vblank 의 수와 시각을 함께 읽는다 (읽는 사이 새 vblank 가 오면 다시)
    int64_t count     = 0;
    uint64_t hostTime = 0;
    do
    {
        count    = pacer.vblankCount.load();
        hostTime = pacer.lastVblankHostTime.load();
    } while (count != pacer.vblankCount.load());
    if (count >= target)
    {
        pacer.frameStartVblank = count;
        return;
    }
    const uint64_t nowTime   = mach_absolute_time();
    const uint64_t limitSpan = macNanosecondsToHostTime(static_cast<double>(animationInterval.count()) * 2.0);
    // vblank 신호가 끊겼거나 아직 오지 않았다: 한 프레임만큼 자고 기준 vblank 를 지금 수로 되돌린다 (신호가 돌아오면 다시 vblank 에 맞춘다)
    if (nowTime > hostTime + limitSpan)
    {
        mach_wait_until(nowTime + macNanosecondsToHostTime(static_cast<double>(animationInterval.count())));
        pacer.frameStartVblank = pacer.vblankCount.load();
        return;
    }
    const uint64_t wakeTime  = hostTime + macNanosecondsToHostTime(pacer.refreshPeriod * 1e9 * static_cast<double>(target - count));
    const uint64_t limitTime = nowTime + limitSpan;
    if (wakeTime > nowTime)
        mach_wait_until(std::min(wakeTime, limitTime));
    // 깨어난 순간 그 vblank 의 콜백이 아직 오지 않았을 수 있어 목표 vblank 를 기준으로 삼는다
    pacer.frameStartVblank = std::max(target, pacer.vblankCount.load());
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
