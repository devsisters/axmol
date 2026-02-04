//
//  TarUtil.h
//  cocos2d_libs
//
//  Created by JaegeonLee on 2021/09/13.
//

#ifndef TarUtil_h
#define TarUtil_h

#include "platform/PlatformMacros.h"
#include "platform/FileUtils.h"
#include <string>
#include <unordered_map>

NS_AX_BEGIN

class AX_DLL TarEntity
{
public:
    TarEntity(){}
    std::string path;
    int64_t offset_data;
    int64_t size;
};
class AX_DLL TarBundleFile
{
public:
    TarBundleFile();
    static bool isCRPPath(const std::string& fileName);
    void load();
    bool getFileData(const std::string& relativePath, ResizableBuffer* buffer, const std::string& bundlePackPath);
    bool isFileExist(const std::string& fileName);
    TarEntity* getEntity(const std::string& fileName);
private:
    std::unordered_map<std::string, std::shared_ptr<TarEntity>> _entityMap;
    std::vector<std::string> _loadedCrpInfoFiles;
    void loadCrpInfo(const std::string& bundlePackPath);
};

NS_AX_END


#endif /* TarUtil_h */
