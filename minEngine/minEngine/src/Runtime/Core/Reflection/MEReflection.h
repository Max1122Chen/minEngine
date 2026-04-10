#pragma once

#include <algorithm>
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

namespace minEngine::MEReflection
{
    using MEPropertyVisitorFn = std::function<bool(const MEProperty&)>;

    enum class MEReflectionState
    {
        Collecting,
        Finalizing,
        Ready,
        Failed
    };

    template<typename T>
    using MERemoveCvRefT = std::remove_cv_t<std::remove_reference_t<T>>;

    template<typename T>
    struct MEPointerLike
    {
        static constexpr bool value = false;
    };

    template<typename T>
    struct MEPointerLike<T*>
    {
        static constexpr bool value = true;
        using Type = T;
    };

    template<typename T>
    struct MEPointerLike<std::shared_ptr<T>>
    {
        static constexpr bool value = true;
        using Type = T;
    };

    template<typename T, typename TDeleter>
    struct MEPointerLike<std::unique_ptr<T, TDeleter>>
    {
        static constexpr bool value = true;
        using Type = T;
    };

    template<typename T>
    struct MEPointerLike<std::weak_ptr<T>>
    {
        static constexpr bool value = true;
        using Type = T;
    };

    template<typename T>
    inline constexpr bool kIsPointerLike = MEPointerLike<MERemoveCvRefT<T>>::value;

    template<typename T>
    using MEPointeeT = typename MEPointerLike<MERemoveCvRefT<T>>::Type;

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

    class MEReflectionSystem
    {
    public:
        static MEReflectionSystem& Get()
        {
            static MEReflectionSystem system;
            return system;
        }

        ~MEReflectionSystem()
        {
            Reset();
        }

        MEReflectionSystem(const MEReflectionSystem&) = delete;
        MEReflectionSystem& operator=(const MEReflectionSystem&) = delete;
        MEReflectionSystem(MEReflectionSystem&&) = delete;
        MEReflectionSystem& operator=(MEReflectionSystem&&) = delete;

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

        bool RegisterClass(MEClass* classInfo)
        {
            if (classInfo == nullptr)
            {
                AppendError("[MEReflection] RegisterClass received null class info.");
                return false;
            }

            if (m_State == MEReflectionState::Finalizing)
            {
                AppendError("[MEReflection] RegisterClass is not allowed while finalizing.");
                return false;
            }

            if (m_State == MEReflectionState::Ready)
            {
                AppendError("[MEReflection] RegisterClass is not allowed after reflection is ready.");
                return false;
            }

            if (m_State == MEReflectionState::Failed)
            {
                m_State = MEReflectionState::Collecting;
            }

            const std::string& className = classInfo->GetName();
            if (className.empty())
            {
                AppendError("[MEReflection] RegisterClass rejected empty class name.");
                return false;
            }

            if (m_ClassesByName.find(className) != m_ClassesByName.end())
            {
                AppendError("[MEReflection] Duplicate class registration: '" + className + "'.");
                return false;
            }

            m_ClassesByName[className] = classInfo;
            return true;
        }

        template<typename T>
        bool RegisterClass(MEClass* classInfo)
        {
            if (classInfo == nullptr)
            {
                AppendError("[MEReflection] Typed RegisterClass received null class info.");
                return false;
            }

            if (!RegisterClass(classInfo))
            {
                return false;
            }

            m_DeclaredNameByTypeIndex[std::type_index(typeid(T))] = classInfo->GetName();
            return true;
        }

        template<typename TSuper>
        void AddPendingSuperClass(MEClass* derivedClass)
        {
            if (derivedClass == nullptr)
            {
                return;
            }

            using RawSuperType = MERemoveCvRefT<TSuper>;
            m_PendingSuperClassRefs.push_back(PendingSuperClassRef{derivedClass, std::type_index(typeid(RawSuperType))});
        }

        template<typename TReferenced>
        void AddPendingPropertyClass(MEClass* ownerClass, MEProperty* property)
        {
            if (ownerClass == nullptr || property == nullptr)
            {
                return;
            }

            using RawReferencedType = MERemoveCvRefT<TReferenced>;
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
                AppendError("[MEReflection] Failed to create property for field '" + ownerClass->GetName() + "::" + fieldName + "'.");
                return nullptr;
            }

            property->constAccessor = constAccessor;
            property->mutableAccessor = mutableAccessor;
            ownerClass->AddProperty(property);
            return property;
        }

        bool FinalizeReflection()
        {
            if (m_State == MEReflectionState::Finalizing)
            {
                AppendError("[MEReflection] FinalizeReflection re-entered.");
                return false;
            }

            m_LastErrors.clear();
            m_State = MEReflectionState::Finalizing;

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
                m_State = MEReflectionState::Ready;
            }
            else
            {
                m_State = MEReflectionState::Failed;
            }

            return succeeded;
        }

        void Reset()
        {
            m_ClassesByName.clear();
            m_DeclaredNameByTypeIndex.clear();
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

            m_State = MEReflectionState::Collecting;
        }

        MEReflectionState GetState() const
        {
            return m_State;
        }

        bool IsReady() const
        {
            return m_State == MEReflectionState::Ready;
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

        const std::vector<std::string>& GetLastErrors() const
        {
            return m_LastErrors;
        }

        bool ForEachPropertyInHierarchy(const std::string& rootClassName, const MEPropertyVisitorFn& visitor) const
        {
            if (!visitor || m_State != MEReflectionState::Ready)
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
        MEReflectionSystem() = default;

        enum class VisitColor
        {
            White,
            Gray,
            Black
        };

        template<typename TField>
        MEProperty* CreatePropertyByType(MEClass* ownerClass, const std::string& propertyName)
        {
            using RawFieldType = MERemoveCvRefT<TField>;

            if constexpr (minEngine::is_vector<RawFieldType>::value)
            {
                using ElementType = MERemoveCvRefT<typename minEngine::is_vector<RawFieldType>::ElementType>;
                MEProperty* innerProperty = CreatePropertyByType<ElementType>(ownerClass, propertyName + "_Inner");
                return CreateProperty<MEArrayProperty>(propertyName, innerProperty);
            }
            else if constexpr (kIsPointerLike<RawFieldType>)
            {
                using PointeeType = MERemoveCvRefT<MEPointeeT<RawFieldType>>;
                if constexpr (std::is_class_v<PointeeType>)
                {
                    MEObjectPtrProperty* property = CreateProperty<MEObjectPtrProperty>(propertyName);
                    AddPendingPropertyClass<PointeeType>(ownerClass, property);
                    return property;
                }
                else
                {
                    return CreateProperty<MEPrimitiveProperty>(propertyName, typeid(RawFieldType).name());
                }
            }
            else if constexpr (kIsPrimitiveLike<RawFieldType>)
            {
                // TODO: here we are not distinguishing between different primitive types, we might want to have more specific property types for some of them (e.g. int, float, enum, etc.)
                return CreateProperty<MEPrimitiveProperty>(propertyName, typeid(RawFieldType).name());
            }
            else if constexpr (std::is_class_v<RawFieldType>)
            {
                MEObjectProperty* property = CreateProperty<MEObjectProperty>(propertyName);
                AddPendingPropertyClass<RawFieldType>(ownerClass, property);
                return property;
            }
            else
            {
                return CreateProperty<MEPrimitiveProperty>(propertyName, typeid(RawFieldType).name());
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
                    AppendError("[MEReflection] Null derived class found in pending super class references.");
                    succeeded = false;
                    continue;
                }

                MEClass* resolvedSuperClass = FindClassByTypeIndex(ref.superTypeIndex);
                if (resolvedSuperClass == nullptr)
                {
                    AppendError("[MEReflection] Unresolved super class type for '" + ref.derivedClass->GetName() + "'.");
                    succeeded = false;
                    continue;
                }

                if (resolvedSuperClass == ref.derivedClass)
                {
                    AppendError("[MEReflection] Class '" + ref.derivedClass->GetName() + "' cannot inherit from itself.");
                    succeeded = false;
                    continue;
                }

                if (ref.derivedClass->GetSuperClass() != nullptr && ref.derivedClass->GetSuperClass() != resolvedSuperClass)
                {
                    AppendError("[MEReflection] Class '" + ref.derivedClass->GetName() + "' has multiple direct super classes.");
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
                    AppendError("[MEReflection] Null owner/property found in pending property references.");
                    succeeded = false;
                    continue;
                }

                MEClass* resolvedClass = FindClassByTypeIndex(ref.referencedTypeIndex);
                if (resolvedClass == nullptr)
                {
                    AppendError("[MEReflection] Unresolved property type for '" + ref.ownerClass->GetName() + "::" + ref.property->name + "'.");
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
                    AppendError("[MEReflection] Pending class reference is bound to a non-object property '" + ref.ownerClass->GetName() + "::" + ref.property->name + "'.");
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
                                                 const MEPropertyVisitorFn& visitor,
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

        std::unordered_map<std::string, MEClass*> m_ClassesByName;
        std::unordered_map<std::type_index, std::string> m_DeclaredNameByTypeIndex;

        std::vector<PendingSuperClassRef> m_PendingSuperClassRefs;
        std::vector<PendingPropertyClassRef> m_PendingPropertyClassRefs;

        std::vector<std::string> m_LastErrors;
        MEReflectionState m_State = MEReflectionState::Collecting;
    };
}
