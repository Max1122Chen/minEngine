#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <cstddef>
#include <type_traits>
#include <typeinfo>

namespace minEngine::Reflection
{
    struct FieldInfo
    {
        std::string name;
        std::string typeName;
        size_t offset = 0;
    };

    struct TypeInfo
    {
        std::string name;
        size_t size = 0;
        std::vector<FieldInfo> fields;
    };

    template<typename T>
    inline std::string GetTypeName()
    {
        using RawType = std::remove_cv_t<std::remove_reference_t<T>>;
        return typeid(RawType).name();
    }

    template<>
    inline std::string GetTypeName<bool>()
    {
        return "bool";
    }

    template<>
    inline std::string GetTypeName<int>()
    {
        return "int";
    }

    template<>
    inline std::string GetTypeName<float>()
    {
        return "float";
    }

    template<>
    inline std::string GetTypeName<double>()
    {
        return "double";
    }

    template<>
    inline std::string GetTypeName<std::string>()
    {
        return "std::string";
    }

    class ReflectionSystem
    {
    public:
        static ReflectionSystem& Get()
        {
            static ReflectionSystem system;
            return system;
        }

        template<typename T>
        void RegisterType(TypeInfo info)
        {
            const std::string declaredName = info.name;
            const std::string typeIdName = typeid(T).name();

            m_TypeInfoByDeclaredName[declaredName] = std::move(info);
            m_DeclaredNameByTypeId[typeIdName] = declaredName;
        }

        const TypeInfo* GetTypeInfo(const std::string& declaredName) const
        {
            const auto iter = m_TypeInfoByDeclaredName.find(declaredName);
            if (iter == m_TypeInfoByDeclaredName.end())
            {
                return nullptr;
            }

            return &iter->second;
        }

        template<typename T>
        const TypeInfo* GetTypeInfo() const
        {
            const auto typeIdIter = m_DeclaredNameByTypeId.find(typeid(T).name());
            if (typeIdIter == m_DeclaredNameByTypeId.end())
            {
                return nullptr;
            }

            return GetTypeInfo(typeIdIter->second);
        }

        const std::unordered_map<std::string, TypeInfo>& GetAllTypeInfo() const
        {
            return m_TypeInfoByDeclaredName;
        }

    private:
        std::unordered_map<std::string, TypeInfo> m_TypeInfoByDeclaredName;
        std::unordered_map<std::string, std::string> m_DeclaredNameByTypeId;
    };
}
