#pragma once

#include <algorithm>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <type_traits>
#include <typeindex>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "MEClass.h"
#include "Math/Math.h"
#include "TypeTraits.h"

namespace minEngine::Reflection
{
    using PropertyVisitorFn = std::function<bool(const MEProperty&)>;

    enum class ReflectionSystemState
    {
        Collecting,
        Finalizing,
        Ready,
        Failed
    };



    template<typename T>
    inline constexpr bool kIsPointerLike = PointerLike<RemoveCvRefT<T>>::value;

    template<typename T>
    using PointeeT = typename PointerLike<RemoveCvRefT<T>>::Type;

    template<typename T>
    inline constexpr bool kIsPrimitiveLike = std::is_arithmetic_v<T>
                                           || std::is_same_v<T, std::string>
                                           || std::is_same_v<T, Vector2>
                                           || std::is_same_v<T, Vector3>
                                           || std::is_same_v<T, Vector4>
                                           || std::is_enum_v<T>;

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

    class ReflectionSystem
    {
    public:
        static ReflectionSystem& Get()
        {
            static ReflectionSystem system;
            return system;
        }

        ~ReflectionSystem()
        {
            Reset();
        }

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
                classInfo->SetFactory(&MEClass::CreateDefaultInstance<T>);
            }

            if (!RegisterClass_Internal(classInfo))
            {
                return false;
            }

            m_DeclaredNameByTypeIndex[std::type_index(typeid(T))] = classInfo->GetName();
            return true;
        }

        std::shared_ptr<void> CreateInstance(const std::string& className) const
        {
            const MEClass* classInfo = FindClass(className);
            if (classInfo == nullptr)
            {
                return nullptr;
            }

            return classInfo->CreateInstance();
        }

        bool RegisterEnum(MEEnum* enumInfo)
        {
            if (enumInfo == nullptr)
            {
                AppendError("[Reflection] RegisterEnum received null enum info.");
                return false;
            }

            if (!EnsureCanRegister("RegisterEnum"))
            {
                return false;
            }

            const std::string& enumName = enumInfo->GetName();
            if (enumName.empty())
            {
                AppendError("[Reflection] RegisterEnum rejected empty enum name.");
                return false;
            }

            if (m_EnumsByName.find(enumName) != m_EnumsByName.end())
            {
                AppendError("[Reflection] Duplicate enum registration: '" + enumName + "'.");
                return false;
            }

            m_EnumsByName[enumName] = enumInfo;
            return true;
        }

        template<typename TEnum>
        bool RegisterEnum(MEEnum* enumInfo)
        {
            static_assert(std::is_enum_v<TEnum>, "RegisterEnum<TEnum> requires enum type");

            if (!RegisterEnum(enumInfo))
            {
                return false;
            }

            m_DeclaredEnumNameByTypeIndex[std::type_index(typeid(TEnum))] = enumInfo->GetName();
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
            m_PendingSuperClassRefs.push_back(PendingSuperClassRef{derivedClass, std::type_index(typeid(RawSuperType))});
        }

        template<typename TReferenced>
        void AddPendingPropertyClass(MEClass* ownerClass, MEProperty* property)
        {
            if (ownerClass == nullptr || property == nullptr)
            {
                return;
            }

            using RawReferencedType = RemoveCvRefT<TReferenced>;
            m_PendingPropertyClassRefs.push_back(PendingPropertyClassRef{ownerClass, property, std::type_index(typeid(RawReferencedType))});
        }

        template<typename TOwner, typename TField>
        MEProperty* AddFieldByType(MEClass* ownerClass,
                                   const std::string& fieldName,
                                   FieldConstAccessorFn constAccessor,
                                   FieldMutableAccessorFn mutableAccessor)
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

            property->constAccessor = constAccessor;
            property->mutableAccessor = mutableAccessor;
            ownerClass->AddProperty(property);
            return property;
        }

        bool FinalizeReflection()
        {
            if (m_State == ReflectionSystemState::Finalizing)
            {
                AppendError("[Reflection] FinalizeReflection re-entered.");
                return false;
            }

            m_LastErrors.clear();
            m_State = ReflectionSystemState::Finalizing;

            PrepareForResolve();

            bool succeeded = true;
            if (!ResolvePendingSuperClasses())
            {
                succeeded = false;
            }

            if (!ValidateInheritanceGraph())
            {
                succeeded = false;
            }

            if (!ResolvePendingPropertyClasses())
            {
                succeeded = false;
            }

            if (succeeded)
            {
                BuildDerivedClassLinks();
                m_State = ReflectionSystemState::Ready;
            }
            else
            {
                m_State = ReflectionSystemState::Failed;
            }

            return succeeded;
        }

        void Reset()
        {
            m_ClassesByName.clear();
            m_DeclaredNameByTypeIndex.clear();
            m_EnumsByName.clear();
            m_DeclaredEnumNameByTypeIndex.clear();
            m_PendingSuperClassRefs.clear();
            m_PendingPropertyClassRefs.clear();
            m_LastErrors.clear();

            for (MEProperty* property : m_OwnedProperties)
            {
                delete property;
            }
            m_OwnedProperties.clear();

            for (MEClass* classInfo : m_OwnedClasses)
            {
                delete classInfo;
            }
            m_OwnedClasses.clear();

            for (MEEnum* enumInfo : m_OwnedEnums)
            {
                delete enumInfo;
            }
            m_OwnedEnums.clear();

            m_State = ReflectionSystemState::Collecting;
        }

        ReflectionSystemState GetState() const
        {
            return m_State;
        }

        bool IsReady() const
        {
            return m_State == ReflectionSystemState::Ready;
        }

        MEClass* FindClass(const std::string& className)
        {
            auto iter = m_ClassesByName.find(className);
            if (iter == m_ClassesByName.end())
            {
                return nullptr;
            }
            return iter->second;
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
            auto iter = m_DeclaredNameByTypeIndex.find(std::type_index(typeid(T)));
            if (iter == m_DeclaredNameByTypeIndex.end())
            {
                return nullptr;
            }
            return FindClass(iter->second);
        }

        MEEnum* FindEnum(const std::string& enumName)
        {
            auto iter = m_EnumsByName.find(enumName);
            if (iter == m_EnumsByName.end())
            {
                return nullptr;
            }
            return iter->second;
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
            auto iter = m_DeclaredEnumNameByTypeIndex.find(std::type_index(typeid(TEnum)));
            if (iter == m_DeclaredEnumNameByTypeIndex.end())
            {
                return nullptr;
            }
            return FindEnum(iter->second);
        }

        const std::vector<std::string>& GetLastErrors() const
        {
            return m_LastErrors;
        }

        bool ForEachPropertyInHierarchy(const std::string& rootClassName, const PropertyVisitorFn& visitor) const
        {
            if (!visitor || m_State != ReflectionSystemState::Ready)
            {
                return false;
            }

            const MEClass* rootClass = FindClass(rootClassName);
            if (rootClass == nullptr)
            {
                return false;
            }

            std::unordered_set<const MEClass*> visited;
            return ForEachPropertyInHierarchyRecursive(*rootClass, visitor, visited);
        }

    private:
        ReflectionSystem() = default;

        enum class VisitColor
        {
            White,
            Gray,
            Black
        };

        // Reflection type registration helpers
        bool RegisterClass_Internal(MEClass* classInfo)
        {
            if (!EnsureCanRegister("RegisterClass"))
            {
                return false;
            }

            const std::string& className = classInfo->GetName();
            if (className.empty())
            {
                AppendError("[Reflection] RegisterClass rejected empty class name.");
                return false;
            }

            if (m_ClassesByName.find(className) != m_ClassesByName.end())
            {
                AppendError("[Reflection] Duplicate class registration: '" + className + "'.");
                return false;
            }

            m_ClassesByName[className] = classInfo;
            return true;
        }

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
                    // TODO: raw pointer and smart pointer should be treated differently, we might want to have different property types for them in the future if needed (e.g. MEObjectRawPtrProperty and MEObjectSmartPtrProperty), and we also need to consider how to handle the factory and instance creation for them since currently we only support default constructor without parameters.
                    if constexpr (std::is_pointer_v<RawFieldType>)
                    {
                        
                    }
                    else if constexpr (minEngine::is_smart_ptr<RawFieldType>::value)
                    {
                        
                    }
                    
                    MEObjectPtrProperty* property = CreateProperty<MEObjectPtrProperty>(propertyName);
                    AddPendingPropertyClass<PointeeType>(ownerClass, property);
                    return property;
                }
                else
                {
                    // We do not support pointer-like property for non-class types!!!
                    AppendError("[Reflection] Unsupported pointer-like property type: '" + std::string(typeid(RawFieldType).name()) + "'. Only pointer-like types pointing to class types are supported.");
                }
            }
            // Then check if it's a primitive-like type (arithmetic types, std::string, Vector2/3/4, enum, etc.)
            else if constexpr (kIsPrimitiveLike<RawFieldType>)
            {
                // TODO: here we are not distinguishing between different primitive types, we might want to have more specific property types for some of them (e.g. int, float, enum, etc.)
                return CreateProperty<MEPrimitiveProperty>(propertyName, typeid(RawFieldType).name());
            }
            // Finally, if it's a class type, we treat it as an object property
            else if constexpr (std::is_class_v<RawFieldType>)
            {
                MEObjectProperty* property = CreateProperty<MEObjectProperty>(propertyName);
                AddPendingPropertyClass<RawFieldType>(ownerClass, property);
                return property;
            }
            else
            {
                AppendError("[Reflection] Unsupported property type: '" + std::string(typeid(RawFieldType).name()) + "'.");
                return nullptr;
            }
        }

        void PrepareForResolve()
        {
            for (auto& [_, classInfo] : m_ClassesByName)
            {
                classInfo->SetResolvedSuperClass(nullptr);
                classInfo->ClearDirectDerivedClasses();

                for (MEProperty* property : classInfo->GetProperties())
                {
                    ResetPropertyResolvedRefs(property);
                }
            }
        }

        static void ResetPropertyResolvedRefs(MEProperty* property)
        {
            if (property == nullptr)
            {
                return;
            }

            if (property->GetCategory() == MEPropertyCategory::Object
                || property->GetCategory() == MEPropertyCategory::ObjectPtr)
            {
                static_cast<MEObjectProperty*>(property)->SetValueClass(nullptr);
                return;
            }

            if (property->GetCategory() == MEPropertyCategory::Array)
            {
                MEArrayProperty* arrayProperty = static_cast<MEArrayProperty*>(property);
                ResetPropertyResolvedRefs(arrayProperty->GetInnerProperty());
            }
        }

        void AppendError(std::string message)
        {
            m_LastErrors.push_back(std::move(message));
        }

        bool EnsureCanRegister(const char* operationName)
        {
            if (m_State == ReflectionSystemState::Finalizing)
            {
                AppendError(std::string("[Reflection] ") + operationName + " is not allowed while finalizing.");
                return false;
            }

            if (m_State == ReflectionSystemState::Ready)
            {
                AppendError(std::string("[Reflection] ") + operationName + " is not allowed after reflection is ready.");
                return false;
            }

            if (m_State == ReflectionSystemState::Failed)
            {
                m_State = ReflectionSystemState::Collecting;
            }

            return true;
        }

        MEClass* FindClassByTypeIndex(const std::type_index& typeIndex)
        {
            auto nameIter = m_DeclaredNameByTypeIndex.find(typeIndex);
            if (nameIter == m_DeclaredNameByTypeIndex.end())
            {
                return nullptr;
            }

            return FindClass(nameIter->second);
        }

        bool ResolvePendingSuperClasses()
        {
            bool succeeded = true;

            for (const PendingSuperClassRef& ref : m_PendingSuperClassRefs)
            {
                if (ref.derivedClass == nullptr)
                {
                    AppendError("[Reflection] Null derived class found in pending super class references.");
                    succeeded = false;
                    continue;
                }

                MEClass* resolvedSuperClass = FindClassByTypeIndex(ref.superTypeIndex);
                if (resolvedSuperClass == nullptr)
                {
                    AppendError("[Reflection] Unresolved super class type for '" + ref.derivedClass->GetName() + "'.");
                    succeeded = false;
                    continue;
                }

                if (resolvedSuperClass == ref.derivedClass)
                {
                    AppendError("[Reflection] Class '" + ref.derivedClass->GetName() + "' cannot inherit from itself.");
                    succeeded = false;
                    continue;
                }

                if (ref.derivedClass->GetSuperClass() != nullptr && ref.derivedClass->GetSuperClass() != resolvedSuperClass)
                {
                    AppendError("[Reflection] Class '" + ref.derivedClass->GetName() + "' has multiple direct super classes.");
                    succeeded = false;
                    continue;
                }

                ref.derivedClass->SetResolvedSuperClass(resolvedSuperClass);
            }

            return succeeded;
        }

        bool ResolvePendingPropertyClasses()
        {
            bool succeeded = true;

            for (const PendingPropertyClassRef& ref : m_PendingPropertyClassRefs)
            {
                if (ref.ownerClass == nullptr || ref.property == nullptr)
                {
                    AppendError("[Reflection] Null owner/property found in pending property references.");
                    succeeded = false;
                    continue;
                }

                MEClass* resolvedClass = FindClassByTypeIndex(ref.referencedTypeIndex);
                if (resolvedClass == nullptr)
                {
                    AppendError("[Reflection] Unresolved property type for '" + ref.ownerClass->GetName() + "::" + ref.property->name + "'.");
                    succeeded = false;
                    continue;
                }

                if (ref.property->GetCategory() == MEPropertyCategory::Object
                    || ref.property->GetCategory() == MEPropertyCategory::ObjectPtr)
                {
                    static_cast<MEObjectProperty*>(ref.property)->SetValueClass(resolvedClass);
                }
                else
                {
                    AppendError("[Reflection] Pending class reference is bound to a non-object property '" + ref.ownerClass->GetName() + "::" + ref.property->name + "'.");
                    succeeded = false;
                }
            }

            return succeeded;
        }

        bool ValidateInheritanceGraph()
        {
            bool succeeded = true;
            std::unordered_map<const MEClass*, VisitColor> visitMap;
            std::vector<const MEClass*> stack;

            for (const auto& [_, classInfo] : m_ClassesByName)
            {
                if (!VisitClassForCycle(*classInfo, visitMap, stack))
                {
                    succeeded = false;
                }
            }

            return succeeded;
        }

        bool VisitClassForCycle(const MEClass& classInfo,
                                std::unordered_map<const MEClass*, VisitColor>& visitMap,
                                std::vector<const MEClass*>& stack)
        {
            VisitColor& color = visitMap[&classInfo];
            if (color == VisitColor::Black)
            {
                return true;
            }

            if (color == VisitColor::Gray)
            {
                auto beginIter = std::find(stack.begin(), stack.end(), &classInfo);
                std::string cycleMessage = "[MEReflection] Inheritance cycle detected: ";
                if (beginIter == stack.end())
                {
                    cycleMessage += classInfo.GetName() + " -> " + classInfo.GetName();
                }
                else
                {
                    for (auto iter = beginIter; iter != stack.end(); ++iter)
                    {
                        if (iter != beginIter)
                        {
                            cycleMessage += " -> ";
                        }
                        cycleMessage += (*iter)->GetName();
                    }
                    cycleMessage += " -> " + classInfo.GetName();
                }

                AppendError(std::move(cycleMessage));
                return false;
            }

            color = VisitColor::Gray;
            stack.push_back(&classInfo);

            bool succeeded = true;
            const MEClass* superClass = classInfo.GetSuperClass();
            if (superClass != nullptr)
            {
                succeeded = VisitClassForCycle(*superClass, visitMap, stack);
            }

            stack.pop_back();
            color = VisitColor::Black;
            return succeeded;
        }

        void BuildDerivedClassLinks()
        {
            for (auto& [_, classInfo] : m_ClassesByName)
            {
                classInfo->ClearDirectDerivedClasses();
            }

            for (auto& [_, classInfo] : m_ClassesByName)
            {
                MEClass* superClass = classInfo->GetSuperClass();
                if (superClass != nullptr)
                {
                    superClass->AddDirectDerivedClass(classInfo);
                }
            }
        }

        bool ForEachPropertyInHierarchyRecursive(const MEClass& classInfo,
                                                 const PropertyVisitorFn& visitor,
                                                 std::unordered_set<const MEClass*>& visited) const
        {
            if (visited.find(&classInfo) != visited.end())
            {
                return true;
            }
            visited.insert(&classInfo);

            const MEClass* superClass = classInfo.GetSuperClass();
            if (superClass != nullptr)
            {
                if (!ForEachPropertyInHierarchyRecursive(*superClass, visitor, visited))
                {
                    return false;
                }
            }

            for (MEProperty* property : classInfo.GetProperties())
            {
                if (property != nullptr && !visitor(*property))
                {
                    return false;
                }
            }

            return true;
        }

    private:
        std::vector<MEClass*> m_OwnedClasses;
        std::vector<MEProperty*> m_OwnedProperties;
        std::vector<MEEnum*> m_OwnedEnums;

        std::unordered_map<std::string, MEClass*> m_ClassesByName;
        std::unordered_map<std::type_index, std::string> m_DeclaredNameByTypeIndex;
        std::unordered_map<std::string, MEEnum*> m_EnumsByName;
        std::unordered_map<std::type_index, std::string> m_DeclaredEnumNameByTypeIndex;

        std::vector<PendingSuperClassRef> m_PendingSuperClassRefs;
        std::vector<PendingPropertyClassRef> m_PendingPropertyClassRefs;

        std::vector<std::string> m_LastErrors;
        ReflectionSystemState m_State = ReflectionSystemState::Collecting;
    };
}
