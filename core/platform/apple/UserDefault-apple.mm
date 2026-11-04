/****************************************************************************
 Cocos2d-x 시절의 CCUserDefault-apple.mm 과 동일하게 NSUserDefaults 를 저장소로 사용한다.
 (Axmol 기본 구현은 UserDefault.bin 을 쓰기 때문에 Cocos 시절에 저장한 값을 읽지 못한다)
 ****************************************************************************/
#import <Foundation/Foundation.h>

#include "base/Data.h"
#include "base/UserDefault.h"

namespace ax
{

class UserDefaultApple : public UserDefault
{
public:
    UserDefaultApple() = default;

    bool getBoolForKey(const char* key, bool defaultValue) override
    {
        NSNumber* value = getObject(key);
        return value ? [value boolValue] : defaultValue;
    }

    int getIntegerForKey(const char* key, int defaultValue) override
    {
        NSNumber* value = getObject(key);
        return value ? [value intValue] : defaultValue;
    }

    int64_t getLargeIntForKey(const char* key, int64_t defaultValue) override
    {
        NSNumber* value = getObject(key);
        return value ? [value longLongValue] : defaultValue;
    }

    float getFloatForKey(const char* key, float defaultValue) override
    {
        NSNumber* value = getObject(key);
        return value ? [value floatValue] : defaultValue;
    }

    double getDoubleForKey(const char* key, double defaultValue) override
    {
        NSNumber* value = getObject(key);
        return value ? [value doubleValue] : defaultValue;
    }

    std::string_view getStringForKey(const char* key, std::string_view defaultValue) override
    {
        if (!key)
        {
            return defaultValue;
        }

        NSString* str = [[NSUserDefaults standardUserDefaults] stringForKey:[NSString stringWithUTF8String:key]];
        if (!str)
        {
            return defaultValue;
        }

        // string_view 반환을 위해 _values 를 캐시로 사용한다 (다음 set/get 호출 전까지 유효)
        updateValueForKey(key, [str UTF8String]);
        return _values.find(key)->second;
    }

    Data getDataForKey(const char* key, const Data& defaultValue) override
    {
        if (!key)
        {
            return defaultValue;
        }

        NSData* data = [[NSUserDefaults standardUserDefaults] dataForKey:[NSString stringWithUTF8String:key]];
        if (!data)
        {
            return defaultValue;
        }

        Data ret;
        ret.copy(static_cast<const unsigned char*>(data.bytes), data.length);
        return ret;
    }

    void setBoolForKey(const char* key, bool value) override { setObject(key, [NSNumber numberWithBool:value]); }

    void setIntegerForKey(const char* key, int value) override { setObject(key, [NSNumber numberWithInt:value]); }

    void setLargeIntForKey(const char* key, int64_t value) override
    {
        setObject(key, [NSNumber numberWithLongLong:value]);
    }

    void setFloatForKey(const char* key, float value) override { setObject(key, [NSNumber numberWithFloat:value]); }

    void setDoubleForKey(const char* key, double value) override { setObject(key, [NSNumber numberWithDouble:value]); }

    void setStringForKey(const char* key, std::string_view value) override
    {
        std::string str(value);
        setObject(key, [NSString stringWithUTF8String:str.c_str()]);
    }

    void setDataForKey(const char* key, const Data& value) override
    {
        if (value.isNull())
        {
            return;
        }
        setObject(key, [NSData dataWithBytes:value.getBytes() length:value.getSize()]);
    }

    void flush() override { [[NSUserDefaults standardUserDefaults] synchronize]; }

    void deleteValueForKey(const char* key) override
    {
        if (!key)
        {
            return;
        }
        [[NSUserDefaults standardUserDefaults] removeObjectForKey:[NSString stringWithUTF8String:key]];
    }

private:
    static NSNumber* getObject(const char* key)
    {
        if (!key)
        {
            return nil;
        }
        return [[NSUserDefaults standardUserDefaults] objectForKey:[NSString stringWithUTF8String:key]];
    }

    static void setObject(const char* key, id object)
    {
        if (!key)
        {
            return;
        }
        [[NSUserDefaults standardUserDefaults] setObject:object forKey:[NSString stringWithUTF8String:key]];
    }
};

UserDefault* createPlatformUserDefault()
{
    return new UserDefaultApple();
}

}  // namespace ax
