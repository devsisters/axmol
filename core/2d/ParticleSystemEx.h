// ParticleSystem Extended

#ifndef __AX_PARTICLE_SYSTEM_EX_H__
#define __AX_PARTICLE_SYSTEM_EX_H__

#include "ParticleSystemQuad.h"

NS_AX_BEGIN

class AX_DLL ParticleSystemEx : public ParticleSystemQuad
{
public:
    static ParticleSystemEx* create(int numberOfParticles);
    static ParticleSystemEx* createWithSpriteFrame(int numberOfParticles, const std::string& spriteFrameName);

    virtual bool initWithTotalParticles(int numberOfParticles) override;

    ParticleSystemEx();
    virtual ~ParticleSystemEx();

    virtual void setVisible(bool visible) override;
};

NS_AX_END

#endif //__AX_PARTICLE_SYSTEM_EX_H__
