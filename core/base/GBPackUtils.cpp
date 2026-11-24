#include "base/GBPackUtils.h"
#include "base/TarBundleUtils.h"
#include "platform/FileUtils.h"
#include "mio/mio.hpp"
#include <algorithm>
#include <climits>
#include <cstring>
#include <mutex>
#include <unordered_map>
#include <unordered_set>

namespace ax::gbPackDetail
{
constexpr uint64_t headerSize = 32;
constexpr uint64_t entrySize = 40;
constexpr uint64_t maxIndexSize = 64 * 1024 * 1024;

uint64_t readLE(const uint8_t* p, unsigned count)
{
    uint64_t value = 0;
    for (unsigned i = 0; i < count; ++i) value |= uint64_t(p[i]) << (i * 8);
    return value;
}

bool validName(std::string_view name)
{
    if (name.empty() || name.front() == '/' || name.find_first_of("\\:") != name.npos ||
        name.find('\0') != name.npos) return false;
    size_t begin = 0;
    while (begin < name.size())
    {
        const auto end = name.find('/', begin);
        const auto part = name.substr(begin, end == name.npos ? name.size() - begin : end - begin);
        if (part.empty() || part == "." || part == "..") return false;
        if (end == name.npos) return true;
        begin = end + 1;
    }
    return false;
}

struct Entry
{
    std::string name;
    uint64_t offset, size;
    uint32_t width, height, codec;
};

struct Pack
{
    std::unique_ptr<IFileStream> stream;
    uint64_t base = 0, size = 0;
    mutable std::mutex mutex;
    std::vector<Entry> entries;

    bool readRange(uint64_t offset, size_t length, void* output) const
    {
        if (offset > size || length > size - offset || base > uint64_t(INT64_MAX) - offset) return false;
        const uint64_t absolute = base + offset;
        std::lock_guard<std::mutex> lock(mutex);
        const auto total = stream->size();
        if (total < 0 || absolute > uint64_t(total) || length > uint64_t(total) - absolute) return false;
        // 같은 열린 파일 핸들을 사용. 다운로드 파일의 atomic 교체 중에도 인덱스와 payload 유지
        if (absolute <= SIZE_MAX && length <= SIZE_MAX - size_t(absolute))
        {
            mio::mmap_source mapping;
            std::error_code error;
            mapping.map(stream->nativeHandle(), size_t(absolute), length, error);
            if (!error)
            {
                std::memcpy(output, mapping.data(), length);
                return true;
            }
        }
        // 압축 APK/OBB asset은 mmap 불가. 전체 pack 대신 요청 범위만 읽기
        if (stream->seek(int64_t(absolute), SEEK_SET) != int64_t(absolute)) return false;
        size_t read = 0;
        while (read < length)
        {
            const int n = stream->read(static_cast<uint8_t*>(output) + read,
                static_cast<unsigned>(std::min<size_t>(length - read, INT_MAX)));
            if (n <= 0) return false;
            read += size_t(n);
        }
        return true;
    }

    bool open(const FileUtils& files, const std::string& path)
    {
        std::string container = path;
        const auto crp = path.rfind(".crp/");
        if (crp != std::string::npos)
        {
            auto* entity = files.getBundlePack() ? files.getBundlePack()->getEntity(path) : nullptr;
            if (!entity || entity->offset_data < 0 || entity->size < int64_t(headerSize)) return false;
            base = uint64_t(entity->offset_data); size = uint64_t(entity->size);
            container = files.fullPathForFilename(path.substr(0, crp + 4));
        }
        stream = files.openFileStream(container, IFileStream::Mode::READ);
        if (!stream) return false;
        const auto total = stream->size();
        if (total < int64_t(headerSize) || base > uint64_t(total)) return false;
        if (crp == std::string::npos) size = uint64_t(total);
        if (size > uint64_t(total) - base) return false;

        uint8_t header[headerSize];
        if (!readRange(0, sizeof(header), header) || std::memcmp(header, "GBPK", 4) ||
            readLE(header + 4, 2) != 1 || readLE(header + 6, 2) != headerSize ||
            readLE(header + 8, 8) != headerSize || readLE(header + 24, 8) != size) return false;
        const auto indexSize = readLE(header + 16, 8);
        if (indexSize < 8 || indexSize > maxIndexSize || indexSize > size - headerSize) return false;
        std::vector<uint8_t> index(static_cast<size_t>(indexSize));
        if (!readRange(headerSize, index.size(), index.data())) return false;
        const auto count = readLE(index.data(), 4);
        if (readLE(index.data() + 4, 4) != 0 || count > (indexSize - 8) / entrySize) return false;
        uint64_t nameEnd = 8 + count * entrySize;
        uint64_t payloadEnd = (headerSize + indexSize + 7) & ~uint64_t(7);
        std::unordered_set<std::string> names;
        for (uint64_t i = 0; i < count; ++i)
        {
            const auto* p = index.data() + 8 + i * entrySize;
            const auto nameOffset = readLE(p, 4), nameSize = readLE(p + 4, 4);
            Entry entry{{}, readLE(p + 24, 8), readLE(p + 32, 8), uint32_t(readLE(p + 8, 4)),
                uint32_t(readLE(p + 12, 4)), uint32_t(readLE(p + 16, 4))};
            if (nameOffset != nameEnd || !nameSize || nameSize > 65535 || nameSize > indexSize - nameEnd ||
                !entry.width || !entry.height || uint64_t(entry.width) * entry.height > INT_MAX / 4 ||
                entry.codec < 1 || entry.codec > 4 || readLE(p + 20, 4) != 0 ||
                entry.offset != payloadEnd || entry.offset > size || entry.size < 12 ||
                entry.size > INT_MAX || entry.size > size - entry.offset) return false;
            entry.name.assign(reinterpret_cast<const char*>(index.data() + nameOffset), size_t(nameSize));
            if (!validName(entry.name) || !names.insert(entry.name).second) return false;
            nameEnd += nameSize;
            payloadEnd = (entry.offset + entry.size + 7) & ~uint64_t(7);
            entries.push_back(std::move(entry));
        }
        return nameEnd == indexSize && payloadEnd == size;
    }
};

bool validPayload(const Entry& entry, const uint8_t* bytes)
{
    if (entry.codec == 1)
    {
        if (entry.size < 33 || std::memcmp(bytes, "\x89PNG\r\n\x1a\n", 8) || std::memcmp(bytes + 12, "IHDR", 4)) return false;
        const auto be32 = [](const uint8_t* p) { return uint32_t(p[0]) << 24 | uint32_t(p[1]) << 16 | uint32_t(p[2]) << 8 | p[3]; };
        return be32(bytes + 16) == entry.width && be32(bytes + 20) == entry.height;
    }
    if (entry.codec == 2)
        return entry.size >= 80 && !std::memcmp(bytes, "\xabKTX 20\xbb\r\n\x1a\n", 12) &&
            readLE(bytes + 20, 4) == entry.width && readLE(bytes + 24, 4) == entry.height;
    if (entry.codec == 3) return bytes[0] == 0xff && bytes[1] == 0xd8 && bytes[2] == 0xff;
    return !std::memcmp(bytes, "RIFF", 4) && !std::memcmp(bytes + 8, "WEBP", 4);
}
}

namespace ax
{
struct GBPackCache::Impl
{
    struct Image { std::shared_ptr<gbPackDetail::Pack> pack; size_t index; };
    hlookup::string_map<Image> images;
};

GBPackCache::GBPackCache() : _impl(new Impl) {}
GBPackCache::~GBPackCache() = default;

void GBPackCache::load(const FileUtils& files, const std::vector<std::string>& paths)
{
    auto next = std::make_unique<Impl>();
    std::unordered_set<std::string> loaded;
    for (const auto& candidate : paths)
    {
        const auto path = files.fullPathForFilename(candidate);
        if (path.empty())
        {
            AXLOGW("GBPack not found: {}", candidate);
            continue;
        }
        if (!loaded.insert(path).second) continue;
        auto pack = std::make_shared<gbPackDetail::Pack>();
        if (!pack->open(files, path))
        {
            AXLOGW("GBPack rejected: {}", path);
            continue;
        }
        bool ktx2 = false;
        for (size_t i = 0; i < pack->entries.size(); ++i)
        {
            const auto& entry = pack->entries[i];
            const std::string virtualPath = path + "/" + entry.name;
            next->images.emplace(virtualPath, Impl::Image{pack, i});
            ktx2 |= entry.codec == 2;
        }
        if (ktx2) AXLOGW("GBPack contains KTX2: image rendering requires a KTX2 decoder ({})", path);
    }
    _impl = std::move(next);
}

std::string GBPackCache::resolve(std::string_view name) const
{
    return contains(name) ? std::string{name} : std::string{};
}

bool GBPackCache::contains(std::string_view path) const { return _impl->images.find(path) != _impl->images.end(); }

bool GBPackCache::read(std::string_view path, ResizableBuffer* buffer) const
{
    const auto it = _impl->images.find(path);
    if (it == _impl->images.end()) return false;
    const auto& image = it->second;
    const auto& entry = image.pack->entries[image.index];
    // 기존 Image가 소유하는 버퍼로 해당 이미지 한 장만 복사. mmap 포인터를 디코더에 넘기지 않음
    buffer->resize(size_t(entry.size));
    if (!image.pack->readRange(entry.offset, size_t(entry.size), buffer->buffer()) ||
        !gbPackDetail::validPayload(entry, static_cast<const uint8_t*>(buffer->buffer())))
    {
        buffer->resize(0);
        return false;
    }
    return true;
}
}
