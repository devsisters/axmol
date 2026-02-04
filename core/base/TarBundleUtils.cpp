//
//  TarUtil.c
//  cocos2d_libs
//
//  Created by JaegeonLee on 2021/09/13.
//

#include "base/TarBundleUtils.h"
#include "rapidjson/document-wrapper.h"
#include "platform/FileUtils.h"
#include "base/Data.h"
#include <fstream>
#include <sys/stat.h>

NS_AX_BEGIN

bool TarBundleFile::isCRPPath(const std::string& fileName)
{
    auto crpIt = fileName.rfind(".crp/");
    return (crpIt != std::string::npos);
}

TarBundleFile::TarBundleFile()
{
}

void TarBundleFile::load()
{
    /// 우선 나눠지 파일을 수동으로 입력
//    loadCrpInfo("release/bundle_ccb.crp");
//    loadCrpInfo("release/bundle_image.crp");
//    loadCrpInfo("release/bundle_map.crp");
    loadCrpInfo("release/bundle_crp_info.json");
}

void TarBundleFile::loadCrpInfo(const std::string &bundleInfoPath)
{
    std::string infoPath = bundleInfoPath;//bundlePackPath + ".json";
    auto nameIt = infoPath.rfind('/');
    auto name = infoPath.substr(nameIt);
    if (std::find(_loadedCrpInfoFiles.begin(), _loadedCrpInfoFiles.end(), name) != _loadedCrpInfoFiles.end()) return;
    auto bundleInfosString = FileUtils::getInstance()->getStringFromFile(infoPath);
//    auto fs = FileUtils::getInstance();
//    auto fullPath = fs->getDefaultResourceRootPath() + "release" +name;
//    if (fullPath.empty())
//    {
//        return;
//    }
//    std::string bundleInfosString;
//    ResizableBufferAdapter<std::string> buf(&bundleInfosString);
//    std::string suitableFullPath = fs->getSuitableFOpen(fullPath);
//    struct stat statBuf;
//    if (stat(suitableFullPath.c_str(), &statBuf) == -1) {
//        return; // fail
//    }
//    if (!(statBuf.st_mode & S_IFREG)) {
//        return; // fail
//    }
//    FILE *fp = fopen(suitableFullPath.c_str(), "rb");
//    if (!fp)return; // fail
//
//    size_t size = statBuf.st_size;
//    buf.resize(size);
//    size_t readsize = fread(buf.buffer(), 1, size, fp);
//    fclose(fp);
//    if (readsize < size) {
//        buf.resize(readsize);
//        return; // fail
//    }
    if (bundleInfosString.empty()) return;
    rapidjson::Document doc;
    doc.Parse(bundleInfosString.c_str());
    for (auto& value : doc["file_list"].GetArray())
    {
        auto entity = std::make_shared<TarEntity>();
        entity->path = std::string(value["path"].GetString());
        entity->offset_data = value["offset_data"].GetInt64();
        entity->size = value["size"].GetInt64();
        _entityMap[entity->path] = std::move(entity);
    }
    AXLOG("-cached : %s", infoPath.c_str());
    _loadedCrpInfoFiles.emplace_back(name);
}

bool TarBundleFile::getFileData(const std::string& relativePath, ResizableBuffer* buffer, const std::string& bundlePackPath)
{
//    loadCrpInfo(bundlePackPath);
    auto packFullPath = FileUtils::getInstance()->fullPathForFilename(bundlePackPath);
    if (_entityMap.find(relativePath) == _entityMap.end()) return false;
    auto entity = _entityMap[relativePath];
    FILE *fp = fopen(packFullPath.c_str(), "rb");
    buffer->resize(entity->size);
    fseek(fp, entity->offset_data, SEEK_SET);
    size_t readsize = fread(buffer->buffer(), 1, entity->size, fp);
    fclose(fp);
    if (readsize < entity->size)
    {
        return false;
    }
    return true;
}

TarEntity* TarBundleFile::getEntity(const std::string& fileName)
{
    auto crpIt = fileName.rfind(".crp/");
    if (crpIt == std::string::npos) return nullptr;
    auto name = fileName.substr(crpIt+5);
//    auto bundlePackPath = fileName.substr(0, crpIt+4);
//    loadCrpInfo(bundlePackPath);
    auto it = _entityMap.find(name);
    if (it != _entityMap.end())
    {
        return it->second.get();
    }
    return nullptr;
}

bool TarBundleFile::isFileExist(const std::string& fileName)
{
    auto crpIt = fileName.rfind(".crp/");
    if (crpIt == std::string::npos) return false;
    auto name = fileName.substr(crpIt+5);
//    auto bundlePackPath = fileName.substr(0, crpIt+4);
//    loadCrpInfo(bundlePackPath);
    return (_entityMap.find(name) != _entityMap.end());
}

//std::string TarBundleFile::getFullPathForFileNameWithInPack(const std::string& directory, const std::string& filename) const
//{
//
//}

NS_AX_END
