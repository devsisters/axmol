#pragma once

#include "platform/PlatformMacros.h"
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace ax
{
class FileUtils;
class ResizableBuffer;

// 지정된 pack과 내부 경로만 조회. 파일명만으로 다른 pack 검색 금지
class AX_DLL GBPackCache
{
public:
    GBPackCache();
    ~GBPackCache();
    GBPackCache(const GBPackCache&) = delete;
    GBPackCache& operator=(const GBPackCache&) = delete;

    void load(const FileUtils& files, const std::vector<std::string>& paths);
    std::string resolve(std::string_view name) const;
    bool contains(std::string_view path) const;
    bool read(std::string_view path, ResizableBuffer* buffer) const;

private:
    struct Impl;
    std::unique_ptr<Impl> _impl;
};
}
