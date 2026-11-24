#pragma once

#include "renderer/Texture2D.h"
#include "platform/Image.h"
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace ax
{

struct Ktx2Decoded
{
    std::unique_ptr<void, void (*)(void*)> storage{nullptr, [](void*) {}};
    std::vector<MipmapInfo> mipmaps;
    backend::PixelFormat format = backend::PixelFormat::RGBA8;
    int width                   = 0;
    int height                  = 0;
    bool premultiplied          = true;

    bool isValid() const { return storage != nullptr && !mipmaps.empty(); }
};

class AX_DLL Ktx2Loader
{
public:
    static Ktx2Decoded decode(std::string_view filename);
    static void warmUp();
    static bool isKtx2Path(std::string_view path) { return path.ends_with(".ktx2"); }
};

}
