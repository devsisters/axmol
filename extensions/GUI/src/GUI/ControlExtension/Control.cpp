/*
 * Copyright (c) 2012 cocos2d-x.org
 * https://axmol.dev/
 *
 * Copyright 2011 Yannick Loriot.
 * http://yannickloriot.com
 *
 * Copyright (c) 2017-2018 Xiamen Yaji Software Co., Ltd.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 *
 * converted to c++ / cocos2d-x by Angus C
 */

#include "Control.h"
#include "base/Director.h"
#include "2d/Menu.h"
#include "base/Touch.h"
#include "Invocation.h"
#include "base/EventDispatcher.h"
#include "base/EventListenerTouch.h"

NS_AX_EXT_BEGIN

Control::Control()
    : _enabled(false)
    , _selected(false)
    , _highlighted(false)
    , _hasVisibleParents(false)
    , _isOpacityModifyRGB(false)
    , _state(State::NORMAL)
{}

Control* Control::create()
{
    Control* pRet = new Control();
    if (pRet->init())
    {
        pRet->autorelease();
        return pRet;
    }
    else
    {
        AX_SAFE_DELETE(pRet);
        return nullptr;
    }
}

bool Control::init()
{
    if (Layer::init())
    {
        // Initialise instance variables
        _state = Control::State::NORMAL;
        setEnabled(true);
        setSelected(false);
        setHighlighted(false);

        auto dispatcher    = Director::getInstance()->getEventDispatcher();
        auto touchListener = EventListenerTouchOneByOne::create();
        touchListener->setSwallowTouches(true);
        touchListener->onTouchBegan     = AX_CALLBACK_2(Control::onTouchBegan, this);
        touchListener->onTouchMoved     = AX_CALLBACK_2(Control::onTouchMoved, this);
        touchListener->onTouchEnded     = AX_CALLBACK_2(Control::onTouchEnded, this);
        touchListener->onTouchCancelled = AX_CALLBACK_2(Control::onTouchCancelled, this);

        dispatcher->addEventListenerWithSceneGraphPriority(touchListener, this);

        return true;
    }
    else
    {
        return false;
    }
}

Control::~Control()
{
    for (auto iter = _dispatchTable.begin(); iter != _dispatchTable.end(); ++iter)
    {
        delete iter->second;
    }

    _dispatchTable.clear();
}

void Control::sendActionsForControlEvents(EventType controlEvents)
{
    retain();
    // For each control events
    for (int i = 0; i < kControlEventTotalNumber; i++)
    {
        // If the given controlEvents bitmask contains the current event
        if (((int)controlEvents & (1 << i)))
        {
            // Call invocations
            const auto& invocationList = this->dispatchListforControlEvent((Control::EventType)(1 << i));

            for (const auto& invocation : invocationList)
            {
                invocation->invoke(this);
            }
#if AX_ENABLE_SCRIPT_BINDING
            ax::BasicScriptData data(this, (void*)&controlEvents);
            ax::ScriptEvent event(ax::kControlEvent, (void*)&data);
            auto scriptEngine = ax::ScriptEngineManager::getInstance()->getScriptEngine();
            if (scriptEngine)
                scriptEngine->sendEvent(event);
#endif
        }
    }
    release();
}

void Control::addHandlerForControlEvents(Handler handler, EventType controlEvents, const std::string& key /*= ""*/)
{
    // For each control events
    for (int i = 0; i < kControlEventTotalNumber; i++)
    {
        // If the given controlEvents bitmask contains the current event
        if (((int)controlEvents & (1 << i)))
        {
            this->addHandlerForControlEvent(handler, (EventType)(1<<i), key);
        }
    }
}

void Control::addHandlerForControlEvents(MenuHandler handler, EventType controlEvents, const std::string& key /*= ""*/)
{
    addHandlerForControlEvents([handler](Object* pRef, EventType){ handler(pRef); }, controlEvents, key);
}

void Control::addHandlerForControlEvent(Handler handler, EventType controlEvent, const std::string& key /*= ""*/)
{
    // Create the invocation object
    Invocation *invocation = Invocation::create(handler, controlEvent, key);

    // Add the invocation into the dispatch list for the given control event
    auto& eventInvocationList = this->dispatchListforControlEvent(controlEvent);
    eventInvocationList.pushBack(invocation);
}

void Control::removeHandlerForControlEvents(const std::string& key, EventType controlEvents)
{
     // For each control events
    for (int i = 0; i < kControlEventTotalNumber; i++)
    {
        // If the given controlEvents bitmask contains the current event
        if (((int)controlEvents & (1 << i)))
        {
            this->removeHandlerForControlEvent(key, (EventType)(1 << i));
        }
    }
}

void Control::removeHandlerForControlEvent(const std::string& key, EventType controlEvent)
{
    // Retrieve all invocations for the given control event
    //<Invocation*>
    auto& eventInvocationList = this->dispatchListforControlEvent(controlEvent);
    
    //remove all invocations if the key is empty
    //TODO: should the invocations be deleted, or just removed from the array? Won't that cause issues if you add a single invocation for multiple events?

    if(key == "")
    {
        //remove objects
        eventInvocationList.clear();
    }
    else
    {
        std::vector<Invocation*> tobeRemovedInvocations;
        
        //normally we would use a predicate, but this won't work here. Have to do it manually
        for(const auto &invocation : eventInvocationList) {
            if(key == invocation->getKey())
            {
                tobeRemovedInvocations.push_back(invocation);
            }
        }

        for(const auto &invocation : tobeRemovedInvocations) {
            eventInvocationList.eraseObject(invocation);
        }
    }
}


// CRGBA protocol
void Control::setOpacityModifyRGB(bool bOpacityModifyRGB)
{
    _isOpacityModifyRGB = bOpacityModifyRGB;

    for (auto&& child : _children)
    {
        child->setOpacityModifyRGB(bOpacityModifyRGB);
    }
}

bool Control::isOpacityModifyRGB() const
{
    return _isOpacityModifyRGB;
}

Vec2 Control::getTouchLocation(Touch* touch)
{
    Vec2 touchLocation = touch->getLocation();                     // Get the touch position
    touchLocation      = this->convertToNodeSpace(touchLocation);  // Convert to the node space of this class

    return touchLocation;
}

bool Control::onTouchBegan(Touch* /*touch*/, Event* /*event*/)
{
    return false;
}

void Control::onTouchMoved(Touch* /*touch*/, Event* /*event*/) {}

void Control::onTouchEnded(Touch* /*touch*/, Event* /*event*/) {}

void Control::onTouchCancelled(Touch* /*touch*/, Event* /*event*/) {}

bool Control::isTouchInside(Touch* touch)
{
    Vec2 touchLocation = touch->getLocation();  // Get the touch position
    touchLocation      = this->getParent()->convertToNodeSpace(touchLocation);
    Rect bBox          = getBoundingBox();
    return bBox.containsPoint(touchLocation);
}

Vector<Invocation*>& Control::dispatchListforControlEvent(EventType controlEvent)
{
    Vector<Invocation*>* invocationList = nullptr;
    auto iter                           = _dispatchTable.find((int)controlEvent);

    // If the invocation list does not exist for the  dispatch table, we create it
    if (iter == _dispatchTable.end())
    {
        invocationList                    = new Vector<Invocation*>();
        _dispatchTable[(int)controlEvent] = invocationList;
    }
    else
    {
        invocationList = iter->second;
    }
    return *invocationList;
}

void Control::needsLayout() {}

void Control::setEnabled(bool bEnabled)
{
    _enabled = bEnabled;
    if (_enabled)
    {
        _state = Control::State::NORMAL;
    }
    else
    {
        _state = Control::State::DISABLED;
    }

    this->needsLayout();
}

bool Control::isEnabled() const
{
    return _enabled;
}

void Control::setSelected(bool bSelected)
{
    _selected = bSelected;
    this->needsLayout();
}

bool Control::isSelected() const
{
    return _selected;
}

void Control::setHighlighted(bool bHighlighted)
{
    _highlighted = bHighlighted;
    this->needsLayout();
}

bool Control::isHighlighted() const
{
    return _highlighted;
}

bool Control::hasVisibleParents() const
{
    auto parent = this->getParent();
    for (auto c = parent; c != nullptr; c = c->getParent())
    {
        if (!c->isVisible())
        {
            return false;
        }
    }
    return true;
}

Control::EventType operator|(Control::EventType a, Control::EventType b)
{
    return static_cast<Control::EventType>(static_cast<int>(a) | static_cast<int>(b));
}

NS_AX_EXT_END
