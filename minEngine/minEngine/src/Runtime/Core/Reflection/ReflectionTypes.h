#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <initializer_list>
#include <utility>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <type_traits>
#include <typeinfo>
#include <functional>
#include <algorithm>
#include <unordered_set>

#include "Math/Math.h"
#include "Runtime/Core/Serialization/Json.h"

namespace minEngine::Reflection
{
    struct FieldInfo;

    template<typename T>
    struct FieldAccessor;

    using FieldVisitorFn = std::function<bool(const FieldInfo&)>;

    using WriteToJsonFn = Json (*)(const void*);
    using WritePointerToJsonFn = Json (*)(const void*);

    using CreateInstanceFn = std::shared_ptr<void> (*)();

    using FieldConstAccessorFn = const void* (*)(const void*);
    using FieldMutableAccessorFn = void* (*)(void*);
    
    using UpcastConstFn = const void* (*)(const void*);
    using UpcastMutableFn = void* (*)(void*);

    using ArrayGetSizeFn = size_t (*)(const void*);
    using ArrayGetConstElementFn = const void* (*)(const void*, size_t);
    using ArrayResizeFn = void (*)(void*, size_t);
    using ArrayGetMutableElementFn = void* (*)(void*, size_t);

    using MetadataMap = std::unordered_map<std::string, std::string>;

    inline std::pair<std::string, std::string> MetaKV(const char* key, const char* value)
    {
        return { key, value };
    }

    enum class TypeCategory
    {
        Unknown,
        Primitive,
        Enum,
        Object,
        Pointer,
        Array
    };

    struct FieldInfo
    {
        std::string fieldName;
        std::string fieldTypeName;
        TypeCategory category;
        FieldConstAccessorFn constAccessor = nullptr;
        FieldMutableAccessorFn mutableAccessor = nullptr;
        MetadataMap metadata;

        const void BuildMetadata(std::initializer_list<std::pair<std::string, std::string>> entries)
        {
            for (const auto& entry : entries)
            {
                metadata[entry.first] = entry.second;
            }
        }

        const std::string* FindMetadata(const std::string& key) const
        {
            const auto iter = metadata.find(key);
            if (iter == metadata.end())
            {
                return nullptr;
            }
            return &iter->second;
        }
    };

    struct BaseTypeInfo
    {
        std::string typeName;
        UpcastConstFn constUpcast = nullptr;
        UpcastMutableFn mutableUpcast = nullptr;
    };

    struct TypeInfo
    {
        std::string typeName;
        TypeCategory category;
        size_t size = 0;
        std::vector<BaseTypeInfo> directBases;
        std::vector<FieldInfo> fields;

        WriteToJsonFn writeToJson;
        WritePointerToJsonFn writePointerToJson;
        CreateInstanceFn createInstance = nullptr;

        MetadataMap metadata;

        const void BuildMetadata(std::initializer_list<std::pair<std::string, std::string>> entries)
        {
            for (const auto& entry : entries)
            {
                metadata[entry.first] = entry.second;
            }
        }

        const std::string* FindMetadata(const std::string& key) const
        {
            const auto iter = metadata.find(key);
            if (iter == metadata.end())
            {
                return nullptr;
            }
            return &iter->second;
        }
    };

    struct ArrayTypeInfo
    {
        using ElementTypeCategory = Reflection::TypeCategory;
        std::string typeName;
        std::string elementTypeName;
        ElementTypeCategory elementCategory;

        ArrayGetSizeFn getSize = nullptr;
        ArrayResizeFn resize = nullptr;
        ArrayGetConstElementFn getConstElement = nullptr;
        ArrayGetMutableElementFn getMutableElement = nullptr;
    };

    struct EnumValueInfo
    {
        std::string name;
        int64_t value = 0;
    };

    struct EnumInfo
    {
        std::string enumName;
        std::vector<EnumValueInfo> entries;

        const EnumValueInfo* FindByName(const std::string& enumName) const
        {
            for (const auto& entry : entries)
            {
                if (entry.name == enumName)
                {
                    return &entry;
                }
            }
            return nullptr;
        }

        const EnumValueInfo* FindByValue(int64_t enumValue) const
        {
            for (const auto& entry : entries)
            {
                if (entry.value == enumValue)
                {
                    return &entry;
                }
            }
            return nullptr;
        }
    };
}