/****************************************************************************
 Copyright (c) 2018-2019 Xiamen Yaji Software Co., Ltd.

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

#include "CommandBuffer.h"
#include "DriverBase.h"
#include "Texture.h"

NS_AX_BACKEND_BEGIN

bool CommandBuffer::prepareCaptureTexture(TextureBackend*& texture, int width, int height, PixelFormat format)
{
    if (width <= 0 || height <= 0) return false;
    if (texture && texture->getWidth() == width && texture->getHeight() == height &&
        texture->getTextureFormat() == format) return true;

    TextureDescriptor descriptor;
    descriptor.width = width;
    descriptor.height = height;
    descriptor.textureFormat = format;
    descriptor.textureUsage = TextureUsage::RENDER_TARGET;
    descriptor.samplerDescriptor.magFilter = descriptor.samplerDescriptor.minFilter = SamplerFilter::NEAREST;
    descriptor.samplerDescriptor.sAddressMode = descriptor.samplerDescriptor.tAddressMode = SamplerAddressMode::CLAMP_TO_EDGE;
    auto* replacement = DriverBase::getInstance()->newTexture(descriptor);
    if (!replacement || !replacement->getHandler())
    {
        AX_SAFE_RELEASE(replacement);
        return false;
    }
    AX_SAFE_RELEASE(texture);
    texture = replacement;
    return true;
}

void CommandBuffer::setStencilReferenceValue(uint32_t value)
{
    _stencilReferenceValueBack = _stencilReferenceValueFront = value;
}

void CommandBuffer::setStencilReferenceValue(unsigned int frontRef, unsigned int backRef)
{
    _stencilReferenceValueFront = frontRef;
    _stencilReferenceValueBack  = backRef;
}

NS_AX_BACKEND_END
