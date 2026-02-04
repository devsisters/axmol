// ParticleSystem Extended

#include <algorithm>

#include "ParticleSystemEx.h"
#include "2d/SpriteFrame.h"
#include "2d/SpriteFrameCache.h"

NS_AX_BEGIN

ParticleSystemEx* ParticleSystemEx::create(int numberOfParticles)
{
    ParticleSystemEx* ret = new (std::nothrow)ParticleSystemEx();
    if (ret && ret->initWithTotalParticles(numberOfParticles))
    {
        ret->autorelease();
    }
    else
    {
        AX_SAFE_DELETE(ret);
    }
    return ret;
}

ParticleSystemEx* ParticleSystemEx::createWithSpriteFrame(int numberOfParticles, const std::string& spriteFrameName)
{
    ParticleSystemEx* ret = new (std::nothrow)ParticleSystemEx();
    if (ret && ret->initWithTotalParticles(numberOfParticles))
    {
        ret->autorelease();

        auto spriteFrame = SpriteFrameCache::getInstance()->getSpriteFrameByName(spriteFrameName);
        if (spriteFrame)
        {
            auto rect = spriteFrame->getRect();
            if (spriteFrame->isRotated())
            {
                rect.size.width = spriteFrame->getRect().size.height;
                rect.size.height = spriteFrame->getRect().size.width;
            }
            ret->setTextureWithRect(spriteFrame->getTexture(), rect);
        }
    }
    else
    {
        AX_SAFE_DELETE(ret);
    }
    return ret;
}

bool ParticleSystemEx::initWithTotalParticles(int numberOfParticles)
{
    if (ParticleSystemQuad::initWithTotalParticles(numberOfParticles) == false)
        return false;
    
    return true;
}


ParticleSystemEx::ParticleSystemEx()
: ParticleSystemQuad()
{
    _duration = DURATION_INFINITY;
    _sourcePosition = Vec2::ZERO;
    _posVar = Vec2::ZERO;
    _life = 1.0f;
    _lifeVar = 0;
    _angle = 0;
    _angleVar = 0;
    _emitterMode = Mode::GRAVITY;
    _startSize = 10;
    _startSizeVar = 0;
    _endSize = START_SIZE_EQUAL_TO_END_SIZE;
    _endSizeVar = 0;
    _startSpin = 0;
    _startSpinVar = 0;
    _endSpin = START_SPIN_EQUAL_TO_END_SPIN;
    _endSpinVar = 0;
    _emissionRate = 1000;
    _positionType = PositionType::RELATIVE;
    _startColor = Color4F::WHITE;
    _startColorVar = Color4F(0, 0, 0, 0);
    _endColor = Color4F::WHITE;
    _endColorVar = Color4F(0, 0, 0, 0);
    modeA.gravity.setZero();
    modeA.speed = 0;
    modeA.speedVar = 0;
    modeA.tangentialAccel = 0;
    modeA.tangentialAccelVar = 0;
    modeA.radialAccel = 0;
    modeA.radialAccelVar = 0;
    modeA.rotationIsDir = false;
    modeB.startRadius = 0;
    modeB.startRadiusVar = 0;
    modeB.endRadius = START_RADIUS_EQUAL_TO_END_RADIUS;
    modeB.endRadiusVar = 0;
    modeB.rotatePerSecond = 0;
    modeB.rotatePerSecondVar = 0;
}

ParticleSystemEx::~ParticleSystemEx()
{
}

void ParticleSystemEx::setVisible(bool visible)
{
    ParticleSystemQuad::setVisible(visible);
}

NS_AX_END
