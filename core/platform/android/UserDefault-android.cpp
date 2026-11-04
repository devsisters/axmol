/****************************************************************************
 Cocos2d-x 시절의 CCUserDefault-android.cpp 와 동일하게 SharedPreferences 를 저장소로 사용한다.
 (Axmol 기본 구현은 UserDefault.bin 을 쓰기 때문에 Cocos 시절에 저장한 값을 읽지 못한다)
 Data 는 Cocos 와 동일하게 base64 문자열로 저장한다.
 ****************************************************************************/
#include "platform/PlatformConfig.h"

#if AX_TARGET_PLATFORM == AX_PLATFORM_ANDROID

#    include <string>

#    include "base/Data.h"
#    include "base/UserDefault.h"
#    include "base/base64.h"
#    include "platform/android/jni/JniHelper.h"

namespace ax
{

namespace
{
constexpr const char* helperClassName = "dev.axmol.lib.AxmolEngine";
}

class UserDefaultAndroid : public UserDefault
{
public:
    UserDefaultAndroid() = default;

    bool getBoolForKey(const char* key, bool defaultValue) override
    {
        return JniHelper::callStaticBooleanMethod(helperClassName, "getBoolForKey", key, defaultValue);
    }

    int getIntegerForKey(const char* key, int defaultValue) override
    {
        return JniHelper::callStaticIntMethod(helperClassName, "getIntegerForKey", key, defaultValue);
    }

    // SharedPreferences 의 long 은 Cocos 에 없던 타입이라 문자열로 저장한다
    int64_t getLargeIntForKey(const char* key, int64_t defaultValue) override
    {
        auto str = JniHelper::callStaticStringMethod(helperClassName, "getStringForKey", key, "");
        if (str.empty())
        {
            return defaultValue;
        }
        return std::strtoll(str.c_str(), nullptr, 10);
    }

    float getFloatForKey(const char* key, float defaultValue) override
    {
        return JniHelper::callStaticFloatMethod(helperClassName, "getFloatForKey", key, defaultValue);
    }

    double getDoubleForKey(const char* key, double defaultValue) override
    {
        return JniHelper::callStaticDoubleMethod(helperClassName, "getDoubleForKey", key, defaultValue);
    }

    std::string_view getStringForKey(const char* key, std::string_view defaultValue) override
    {
        std::string defaultStr(defaultValue);
        auto str = JniHelper::callStaticStringMethod(helperClassName, "getStringForKey", key, defaultStr);

        // string_view 반환을 위해 _values 를 캐시로 사용한다 (다음 set/get 호출 전까지 유효)
        updateValueForKey(key, str);
        return _values.find(key)->second;
    }

    Data getDataForKey(const char* key, const Data& defaultValue) override
    {
        auto encoded = JniHelper::callStaticStringMethod(helperClassName, "getStringForKey", key, "");
        if (encoded.empty())
        {
            return defaultValue;
        }

        std::string decoded(base64::decoded_size(encoded.size()), '\0');
        auto decodedSize = base64::decode(decoded.data(), encoded.data(), encoded.size());
        if (decodedSize == 0)
        {
            return defaultValue;
        }

        Data ret;
        ret.copy(reinterpret_cast<const unsigned char*>(decoded.data()), decodedSize);
        return ret;
    }

    void setBoolForKey(const char* key, bool value) override
    {
        JniHelper::callStaticVoidMethod(helperClassName, "setBoolForKey", key, value);
    }

    void setIntegerForKey(const char* key, int value) override
    {
        JniHelper::callStaticVoidMethod(helperClassName, "setIntegerForKey", key, value);
    }

    void setLargeIntForKey(const char* key, int64_t value) override
    {
        JniHelper::callStaticVoidMethod(helperClassName, "setStringForKey", key, std::to_string(value));
    }

    void setFloatForKey(const char* key, float value) override
    {
        JniHelper::callStaticVoidMethod(helperClassName, "setFloatForKey", key, value);
    }

    void setDoubleForKey(const char* key, double value) override
    {
        JniHelper::callStaticVoidMethod(helperClassName, "setDoubleForKey", key, value);
    }

    void setStringForKey(const char* key, std::string_view value) override
    {
        JniHelper::callStaticVoidMethod(helperClassName, "setStringForKey", key, std::string(value));
    }

    void setDataForKey(const char* key, const Data& value) override
    {
        if (value.isNull())
        {
            return;
        }

        std::string encoded(base64::encoded_size(value.getSize()), '\0');
        auto encodedSize = base64::encode(encoded.data(), value.getBytes(), value.getSize());
        encoded.resize(encodedSize);

        JniHelper::callStaticVoidMethod(helperClassName, "setStringForKey", key, encoded);
    }

    // SharedPreferences.apply() 로 저장하므로 별도 flush 는 필요 없다
    void flush() override {}

    void deleteValueForKey(const char* key) override
    {
        if (!key)
        {
            return;
        }
        JniHelper::callStaticVoidMethod(helperClassName, "deleteValueForKey", key);
    }
};

UserDefault* createPlatformUserDefault()
{
    return new UserDefaultAndroid();
}

}  // namespace ax

#endif
