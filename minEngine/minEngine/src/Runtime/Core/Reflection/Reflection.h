#pragma once

#include <algorithm>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <typeindex>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "EngineAPI.h"
#include "ReflectionUtils.h"
#include "MEClass.h"
#include "MEEnum.h"
#include "MEFunction.h"
#include "Math/Math.h"
#include "Core/TypeTraits.h"

namespace minEngine::Reflection
{
    

    // For iterating properties in a class hierarchy, the visitor returns a bool indicating whether to continue iterating (true) or stop (false)
    using PropertyVisitorFn = std::function<bool(const MEProperty&)>;

    // Pending reference structs for handling cases where a class references another class that has not been registered yet. 
    // These will be resolved during the finalization step of the reflection system.
    struct PendingSuperClassRef
    {
        MEClass* derivedClass = nullptr;
        std::type_index superTypeIndex = typeid(void);
    };

    struct PendingPropertyClassRef
    {
        MEClass* ownerClass = nullptr;
        MEProperty* property = nullptr;
        std::type_index referencedTypeIndex = typeid(void);
    };

    struct PendingEnumPropertyRef
    {
        MEPrimitiveProperty* property = nullptr;
        std::type_index enumTypeIndex = typeid(void);
    };

    // The main reflection system class that manages registration and lookup of reflected types
    class MINENGINE_API ReflectionSystem
    {
        enum class ReflectionSystemState
        {
            Collecting,
            Finalizing,
            Ready,
            Failed
        };

    public:
        static ReflectionSystem& Get();
        void Reset();
        ~ReflectionSystem() { Reset(); }
    
        // Non-copyable and non-movable
        ReflectionSystem(const ReflectionSystem&) = delete;
        ReflectionSystem& operator=(const ReflectionSystem&) = delete;
        ReflectionSystem(ReflectionSystem&&) = delete;
        ReflectionSystem& operator=(ReflectionSystem&&) = delete;

        // Reflection type creation methods
        MEClass* CreateClass(const std::string& className)
        {
            MEClass* classInfo = new MEClass(className);
            m_OwnedClasses.push_back(classInfo);
            return classInfo;
        }

        template<typename TProperty, typename... TArgs>
        TProperty* CreateProperty(TArgs&&... args)
        {
            TProperty* property = new TProperty(std::forward<TArgs>(args)...);
            m_OwnedProperties.push_back(property);
            return property;
        }

        MEEnum* CreateEnum(const std::string& enumName)
        {
            MEEnum* enumInfo = new MEEnum(enumName);
            m_OwnedEnums.push_back(enumInfo);
            return enumInfo;
        }

        MEFunction* CreateFunction(const std::string& functionName)
        {
            MEFunction* function = new MEFunction(functionName);
            m_OwnedFunctions.push_back(function);
            return function;
        }

        template<typename TParam>
        MEProperty* CreateFunctionParamProperty(const std::string& paramName)
        {
            using RawParamType = RemoveCvRefT<TParam>;
            return CreatePropertyByType<RawParamType>(nullptr, paramName);
        }

        bool RegisterFunction(MEClass* ownerClass, MEFunction* function);

        // Reflection type registration methods
        template<typename T>
        bool RegisterClass(MEClass* classInfo)
        {
            if (classInfo == nullptr)
            {
                AppendError("[Reflection] Typed RegisterClass received null class info.");
                return false;
            }

            if (!classInfo->HasFactory())
            {
                classInfo->SetFactory(&MEClass::CreateDefaultInstance_Impl<T>);
            }
            if (!classInfo->HasCaster())
            {
                classInfo->SetCaster(&MEClass::CastObject_Impl<T>);
            }
            if(!classInfo->HasSharedPtrSetter())
            {
                classInfo->SetSharedPtrSetter(&MEClass::SetSharedPtr_Impl<T>);
            }

            if (!RegisterClass_Internal(classInfo))
            {
                return false;
            }

            m_DeclaredNameByTypeIndex[std::type_index(typeid(T*))] = classInfo->GetName();
            return true;
        }

        template<typename TEnum>
        bool RegisterEnum(MEEnum* enumInfo)
        {
            static_assert(std::is_enum_v<TEnum>, "RegisterEnum<TEnum> requires enum type");

            if (!RegisterEnum_Internal(enumInfo))
            {
                return false;
            }

            enumInfo->SetSize(sizeof(TEnum));

            m_DeclaredEnumNameByTypeIndex[std::type_index(typeid(TEnum))] = enumInfo->GetName();
            m_DeclaredEnumNameByTypeIdName[typeid(TEnum).name()] = enumInfo->GetName();
            return true;
        }

        template<typename TSuper>
        void AddPendingSuperClass(MEClass* derivedClass)
        {
            if (derivedClass == nullptr)
            {
                return;
            }

            using RawSuperType = RemoveCvRefT<TSuper>;
            m_PendingSuperClassRefs.push_back(PendingSuperClassRef{derivedClass, std::type_index(typeid(RawSuperType*))});
        }

        template<typename TReferenced>
        void AddPendingPropertyClass(MEClass* ownerClass, MEProperty* property)
        {
            if (ownerClass == nullptr || property == nullptr)
            {
                return;
            }

            using RawReferencedType = RemoveCvRefT<TReferenced>;
            m_PendingPropertyClassRefs.push_back(PendingPropertyClassRef{ownerClass, property, std::type_index(typeid(RawReferencedType*))});
        }

        template<typename TEnum>
        void AddPendingEnumProperty(MEPrimitiveProperty* property)
        {
            if (property == nullptr)
            {
                return;
            }

            using RawEnumType = RemoveCvRefT<TEnum>;
            m_PendingEnumPropertyRefs.push_back(PendingEnumPropertyRef{property, std::type_index(typeid(RawEnumType))});
        }

        template<typename TOwner, typename TField>
        MEProperty* AddFieldByType(MEClass* ownerClass,
                                   const std::string& fieldName,
                                   FieldConstAccessorFn constAccessor,
                                   FieldMutableAccessorFn mutableAccessor,
                                   PropertySpecifierMask specifierMask,
                                   PropertyMetadata metadata)
        {
            if (ownerClass == nullptr)
            {
                return nullptr;
            }

            MEProperty* property = CreatePropertyByType<TField>(ownerClass, fieldName);
            if (property == nullptr)
            {
                AppendError("[Reflection] Failed to create property for field '" + ownerClass->GetName() + "::" + fieldName + "'.");
                return nullptr;
            }

            property->SetAccessors(constAccessor, mutableAccessor);
            property->SetAnnotations(specifierMask, std::move(metadata));
            ownerClass->AddProperty(property);
            return property;
        }

        

        ReflectionSystemState GetState() const
        {
            return m_State;
        }

        bool IsReady() const
        {
            return m_State == ReflectionSystemState::Ready;
        }

        bool IsClassSameOrDerived(const MEClass* classInfo, const MEClass* baseClass) const
        {
            if (classInfo == nullptr)
            {
                return false;
            }

            return classInfo->IsA(baseClass);
        }

        bool IsClassNameSameOrDerived(const std::string& className, const MEClass* baseClass) const
        {
            const MEClass* classInfo = FindClass(className);
            return IsClassSameOrDerived(classInfo, baseClass);
        }

        const MEClass* FindClass(const std::string& className) const
        {
            auto iter = m_ClassesByName.find(className);
            if (iter == m_ClassesByName.end())
            {
                return nullptr;
            }
            return iter->second;
        }

        template<typename T>
        const MEClass* FindClass() const
        {
            return FindClass(GetClassName<T>());
        }

        const MEClass* FindClassByTypeIndex(const std::type_index& typeIndex) const 
        {
            auto nameIter = m_DeclaredNameByTypeIndex.find(typeIndex);
            if (nameIter == m_DeclaredNameByTypeIndex.end())
            {
                return nullptr;
            }

            return FindClass(nameIter->second);
        }

        const MEEnum* FindEnum(const std::string& enumName) const
        {
            auto iter = m_EnumsByName.find(enumName);
            if (iter == m_EnumsByName.end())
            {
                return nullptr;
            }
            return iter->second;
        }

        template<typename TEnum>
        const MEEnum* FindEnum() const
        {
            return FindEnumByTypeIndex(std::type_index(typeid(TEnum)));
        }

        const MEEnum* FindEnumByTypeIndex(const std::type_index& typeIndex) const
        {
            auto iter = m_DeclaredEnumNameByTypeIndex.find(typeIndex);
            if (iter == m_DeclaredEnumNameByTypeIndex.end())
            {
                return nullptr;
            }
            return FindEnum(iter->second);
         }

        const std::vector<const MEClass*> GetAllClasses() const
        {
            std::vector<const MEClass*> classes;
            for (const auto& pair : m_ClassesByName)
            {
                classes.push_back(pair.second);
            }
            return classes;
        }

        const std::vector<const MEClass*> GetDerivedClasses(const MEClass* baseClass) const
        {
            std::vector<const MEClass*> derivedClasses;
            for (const auto& pair : m_ClassesByName)
            {
                const MEClass* classInfo = pair.second;
                if (classInfo->IsA(baseClass))
                {
                    derivedClasses.push_back(classInfo);
                }
            }
            return derivedClasses;
        }

        template<typename TBase>
        const std::vector<const MEClass*> GetDerivedClasses() const
        {
            ME_ASSERT(TBase::StaticClass() != nullptr, "GetDerivedClasses<TBase> requires TBase to be a reflected class with StaticClass() method.");
            return GetDerivedClasses(TBase::StaticClass());
        }

        const std::vector<std::string>& GetLastErrors() const
        {
            return m_LastErrors;
        }

        void ClearErrors()
        {
            m_LastErrors.clear();
        }

        bool ForEachPropertyInHierarchy(const std::string& rootClassName, const PropertyVisitorFn& visitor) const
        {
            const MEClass* rootClass = FindClass(rootClassName);
            if (rootClass == nullptr)
            {
                return false;
            }

            return ForEachPropertyInHierarchy(rootClass, visitor);
        }

        bool ForEachPropertyInHierarchy(const MEClass* rootClass, const PropertyVisitorFn& visitor) const
        {
            if (!visitor || m_State != ReflectionSystemState::Ready)
            {
                return false;
            }

            if (rootClass == nullptr)
            {
                return false;
            }

            std::unordered_set<const MEClass*> visited;
            return ForEachPropertyInHierarchy_Recursive(*rootClass, visitor, visited);
        }

        bool FinalizeReflection();
    private:
        ReflectionSystem() = default;

        template<typename TField>
        MEProperty* CreatePropertyByType(MEClass* ownerClass, const std::string& propertyName)
        {
            using RawFieldType = minEngine::RemoveCvRefT<TField>;

            // First check if it's an array type (currently we only support std::vector as array, but we can extend this in the future if needed)
            if constexpr (minEngine::is_vector<RawFieldType>::value)
            {
                using ElementType = RemoveCvRefT<typename minEngine::is_vector<RawFieldType>::ElementType>;
                if constexpr(minEngine::is_vector<ElementType>::value)
                {
                    static_assert(false, "Nested vectors are not supported for reflection properties.");
                }
                // TODO: prevent array of struct/class instance in the same struct/class.

                MEProperty* innerProperty = CreatePropertyByType<ElementType>(ownerClass, propertyName + "_Inner");
                MEArrayProperty* arrayProperty = CreateProperty<MEArrayProperty>(propertyName, innerProperty);
                arrayProperty->SetStorageSize(sizeof(RawFieldType));
                arrayProperty->SetStorageAlignment(alignof(RawFieldType));
                arrayProperty->SetArrayAccessors(
                    [](const void* arrayObject) -> size_t
                    {
                        if (arrayObject == nullptr)
                        {
                            return 0;
                        }

                        const RawFieldType* typedArray = static_cast<const RawFieldType*>(arrayObject);
                        return typedArray->size();
                    },
                    [](const void* arrayObject, size_t index) -> const void*
                    {
                        if (arrayObject == nullptr)
                        {
                            return nullptr;
                        }

                        const RawFieldType* typedArray = static_cast<const RawFieldType*>(arrayObject);
                        if (index >= typedArray->size())
                        {
                            return nullptr;
                        }

                        return static_cast<const void*>(&((*typedArray)[index]));
                    },
                    [](void* arrayObject, size_t newSize)
                    {
                        if (arrayObject == nullptr)
                        {
                            return;
                        }

                        RawFieldType* typedArray = static_cast<RawFieldType*>(arrayObject);
                        typedArray->resize(newSize);
                    },
                    [](void* arrayObject, size_t index) -> void*
                    {
                        if (arrayObject == nullptr)
                        {
                            return nullptr;
                        }

                        RawFieldType* typedArray = static_cast<RawFieldType*>(arrayObject);
                        if (index >= typedArray->size())
                        {
                            return nullptr;
                        }

                        return static_cast<void*>(&((*typedArray)[index]));
                    });
                return arrayProperty;
            }
            // Then check if it's a pointer-like type (raw pointer, smart pointer, etc.)
            else if constexpr (kIsPointerLike<RawFieldType>)
            {
                using PointeeType = RemoveCvRefT<PointeeT<RawFieldType>>;   // This traits will give us the type that the pointer-like type is pointing to (e.g. for Foo* & std::shared_ptr<Foo>, it will give us Foo)
                if constexpr (std::is_class_v<PointeeType>)
                {
                    MEObjectPtrProperty* property = CreateProperty<MEObjectPtrProperty>(propertyName);
                    if constexpr (std::is_pointer_v<RawFieldType>)
                    {
                        property->SetPtrCategory(MEObjectPtrCategory::Raw);
                        property->SetPointingDataAccessors(
                        [](const void* ptrToPtr) -> const void*
                        {
                            PointeeType* const* typedPtrToPtr = static_cast<PointeeType* const*>(ptrToPtr);
                            return static_cast<const void*>(*typedPtrToPtr);
                        },
                        [](void* ptrToPtr) -> void*
                        {
                            PointeeType** typedPtrToPtr = static_cast<PointeeType**>(ptrToPtr);
                            return static_cast<void*>(*typedPtrToPtr);
                        });
                    }
                    else if constexpr (minEngine::is_smart_ptr<RawFieldType>::value)
                    {
                        if constexpr (minEngine::is_shared_ptr<RawFieldType>::value)
                        {
                            property->SetPtrCategory(MEObjectPtrCategory::Shared);
                            property->SetPointingDataAccessors(
                            [](const void* ptrToSmartPtr) -> const void*
                            {
                                const std::shared_ptr<PointeeType>* typedPtrToSmartPtr = static_cast<const std::shared_ptr<PointeeType>*>(ptrToSmartPtr);
                                return static_cast<const void*>(typedPtrToSmartPtr->get());
                            },
                            [](void* ptrToSmartPtr) -> void*
                            {
                                std::shared_ptr<PointeeType>* typedPtrToSmartPtr = static_cast<std::shared_ptr<PointeeType>*>(ptrToSmartPtr);
                                return static_cast<void*>(typedPtrToSmartPtr->get());
                            });
                        }
                        else
                        {
                            static_assert(false, "Unsupported smart pointer type for reflection property. Currently only std::shared_ptr is supported.");
                        }
                    }
                    AddPendingPropertyClass<PointeeType>(ownerClass, property);
                    property->SetStorageSize(sizeof(RawFieldType));
                    property->SetStorageAlignment(alignof(RawFieldType));
                    return property;
                }
                else
                {
                    // We do not support pointer-like property for non-class types!!!
                    AppendError("[Reflection] Unsupported pointer-like property type: '" + std::string(typeid(RawFieldType).name()) + "'. Only pointer-like types pointing to class types are supported.");
                }
            }
            // Then check if it's a primitive-like type (arithmetic types, std::string, Vector2/3/4, etc.)
            else if constexpr (kIsPrimitiveLike<RawFieldType>)
            {
                std::string primitiveTypeName;

                // Handle boolean type
                if constexpr(std::is_same_v<RawFieldType, bool>) { primitiveTypeName = GetPrimitiveName<bool>(); }
                // Handle integral types (except bool)
                else if constexpr(std::is_same_v<RawFieldType, int>) { primitiveTypeName = GetPrimitiveName<int32_t>(); }
                else if constexpr(std::is_same_v<RawFieldType, int16_t>) { primitiveTypeName = GetPrimitiveName<int16_t>(); }
                else if constexpr(std::is_same_v<RawFieldType, int32_t>) { primitiveTypeName = GetPrimitiveName<int32_t>(); }
                else if constexpr(std::is_same_v<RawFieldType, long>) { primitiveTypeName = GetPrimitiveName<long>(); }
                else if constexpr(std::is_same_v<RawFieldType, int64_t>) { primitiveTypeName = GetPrimitiveName<int64_t>(); }
                else if constexpr(std::is_same_v<RawFieldType, uint8_t>) { primitiveTypeName = GetPrimitiveName<uint8_t>(); }
                else if constexpr(std::is_same_v<RawFieldType, uint32_t>) { primitiveTypeName = GetPrimitiveName<uint32_t>(); }
                else if constexpr(std::is_same_v<RawFieldType, unsigned long>) { primitiveTypeName = GetPrimitiveName<unsigned long>(); }
                else if constexpr(std::is_same_v<RawFieldType, uint64_t>) { primitiveTypeName = GetPrimitiveName<uint64_t>(); }
                // Handle floating point types
                else if constexpr(std::is_same_v<RawFieldType, float>) { primitiveTypeName = GetPrimitiveName<float>(); }
                else if constexpr(std::is_same_v<RawFieldType, double>) { primitiveTypeName = GetPrimitiveName<double>(); }
                else if constexpr(std::is_same_v<RawFieldType, long double>) { primitiveTypeName = GetPrimitiveName<long double>(); }
                // Handle std::string
                else if constexpr(std::is_same_v<RawFieldType, std::string>) { primitiveTypeName = GetPrimitiveName<std::string>(); }
                // Handle Vector2/3/4
                else if constexpr(std::is_same_v<RawFieldType, Vector2>) { primitiveTypeName = GetPrimitiveName<Vector2>(); }
                else if constexpr(std::is_same_v<RawFieldType, Vector3>) { primitiveTypeName = GetPrimitiveName<Vector3>(); }
                else if constexpr(std::is_same_v<RawFieldType, Vector4>) { primitiveTypeName = GetPrimitiveName<Vector4>(); }
                else if constexpr (std::is_enum_v<RawFieldType>)
                {
                    primitiveTypeName = "UnresolvedEnum";
                }
                else { static_assert(minEngine::AlwaysFalse<RawFieldType>::value, "GetPrimitiveName<T> is not specialized for this type T. Please provide a specialization that returns the primitive type name for this type."); }

                MEPrimitiveProperty* property = CreateProperty<MEPrimitiveProperty>(propertyName, primitiveTypeName);
                property->SetStorageSize(sizeof(RawFieldType));
                property->SetStorageAlignment(alignof(RawFieldType));
                if constexpr (std::is_enum_v<RawFieldType>)
                {
                    AddPendingEnumProperty<RawFieldType>(property);
                }
                return property;
            }
            // Finally, if it's a class type, we treat it as an object property
            else if constexpr (std::is_class_v<RawFieldType>)
            {
                MEObjectProperty* property = CreateProperty<MEObjectProperty>(propertyName);
                AddPendingPropertyClass<RawFieldType>(ownerClass, property);
                property->SetStorageSize(sizeof(RawFieldType));
                property->SetStorageAlignment(alignof(RawFieldType));
                return property;
            }
            else
            {
                AppendError("[Reflection] Unsupported property type: '" + std::string(typeid(RawFieldType).name()) + "'.");
                return nullptr;
            }
        }

    private:
        // Reflection type registration helpers
        bool RegisterClass_Internal(MEClass* classInfo);
        bool RegisterEnum_Internal(MEEnum* enumInfo);

        // Reflection finalization helpers
        enum class VisitColor
        {
            White,
            Gray,
            Black
        };
        void AppendError(std::string message);
        void PrepareForResolution();
        void ResetPropertyResolvedRefs(MEProperty* property);
        bool EnsureCanRegister(const char* operationName);
        bool ResolvePendingSuperClasses();
        bool ResolvePendingPropertyClasses();
        bool ResolvePendingEnumPropertyRefs();
        bool ValidateInheritanceGraph();
        bool VisitClassForCycle(const MEClass& classInfo,
                                std::unordered_map<const MEClass*, VisitColor>& visitMap,
                                std::vector<const MEClass*>& stack);
        void BuildDerivedClassLinks();
        bool ValidateFunctions();

        // Property hierarchy iteration helper
        bool ForEachPropertyInHierarchy_Recursive(const MEClass& classInfo,
                                                 const PropertyVisitorFn& visitor,
                                                 std::unordered_set<const MEClass*>& visited) const;
        void SetCodecForEnums();

    private:
        std::vector<MEClass*> m_OwnedClasses;
        std::vector<MEProperty*> m_OwnedProperties;
        std::vector<MEEnum*> m_OwnedEnums;
        std::vector<MEFunction*> m_OwnedFunctions;

        std::unordered_map<std::string, MEClass*> m_ClassesByName;
        std::unordered_map<std::type_index, std::string> m_DeclaredNameByTypeIndex;    // Use type_index(typeid(T*)) as key to avoid including the header of T when registering class info for T

        std::unordered_map<std::string, MEEnum*> m_EnumsByName;
        std::unordered_map<std::type_index, std::string> m_DeclaredEnumNameByTypeIndex;
        std::unordered_map<std::string, std::string> m_DeclaredEnumNameByTypeIdName;    // Used for register enum codec for serialization

        std::vector<PendingSuperClassRef> m_PendingSuperClassRefs;
        std::vector<PendingPropertyClassRef> m_PendingPropertyClassRefs;
        std::vector<PendingEnumPropertyRef> m_PendingEnumPropertyRefs;

        std::vector<std::string> m_LastErrors;
        ReflectionSystemState m_State = ReflectionSystemState::Collecting;
    };
}
