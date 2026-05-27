#include "Reflection.h"

#include "MEFunction.h"
#include "Runtime/Core/Serialization/PrimitiveCodecRegistry.h"

#include <limits>

namespace minEngine::Reflection
{
    namespace
    {
        uint32_t AlignUpU32(uint32_t value, uint32_t alignment)
        {
            if (alignment <= 1)
            {
                return value;
            }
            const uint32_t remainder = value % alignment;
            return remainder == 0 ? value : value + (alignment - remainder);
        }
    } // namespace

    ReflectionSystem &ReflectionSystem::Get()
    {
        static ReflectionSystem system;
        return system;
    }

    void ReflectionSystem::Reset()
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

        for (MEFunction* function : m_OwnedFunctions)
        {
            delete function;
        }
        m_OwnedFunctions.clear();

        m_State = ReflectionSystemState::Collecting;
    }

    bool ReflectionSystem::RegisterFunction(MEClass* ownerClass, MEFunction* function)
    {
        if (!EnsureCanRegister("RegisterFunction"))
        {
            return false;
        }

        if (ownerClass == nullptr)
        {
            AppendError("[Reflection] RegisterFunction received null owner class.");
            return false;
        }

        if (function == nullptr)
        {
            AppendError("[Reflection] RegisterFunction received null function.");
            return false;
        }

        std::string layoutError;
        if (!function->FinalizeLayout(layoutError))
        {
            AppendError("[Reflection] RegisterFunction failed to finalize layout for '" + function->GetName()
                        + "' on class '" + ownerClass->GetName() + "': " + layoutError);
            return false;
        }

        function->SetOwnerClass(ownerClass);
        if (!ownerClass->AddFunction(function))
        {
            AppendError("[Reflection] Duplicate function registration: '" + ownerClass->GetName() + "::"
                        + function->GetName() + "'.");
            return false;
        }

        return true;
    }

    bool ReflectionSystem::ValidateFunctions()
    {
        bool succeeded = true;

        for (const auto& classPair : m_ClassesByName)
        {
            const MEClass* classInfo = classPair.second;
            if (classInfo == nullptr)
            {
                continue;
            }

            std::unordered_set<std::string> functionNames;
            for (const MEFunction* function : classInfo->GetFunctions())
            {
                if (function == nullptr)
                {
                    AppendError("[Reflection] Class '" + classInfo->GetName() + "' contains null function entry.");
                    succeeded = false;
                    continue;
                }

                if (!functionNames.insert(function->GetName()).second)
                {
                    AppendError("[Reflection] Duplicate function name on class '" + classInfo->GetName() + "': '"
                                + function->GetName() + "'.");
                    succeeded = false;
                }

                uint32_t returnCount = 0;
                uint32_t expectedOffset = 0;
                uint32_t maxAlignment = 1;
                for (const MEParamDescriptor& param : function->GetParams())
                {
                    if (param.Property == nullptr)
                    {
                        AppendError("[Reflection] Function '" + classInfo->GetName() + "::" + function->GetName()
                                    + "' has a parameter with null property.");
                        succeeded = false;
                        continue;
                    }

                    const MEProperty* property = param.Property;
                    if (property->GetCategory() != MEPropertyCategory::Primitive)
                    {
                        AppendError("[Reflection] Function '" + classInfo->GetName() + "::" + function->GetName()
                                    + "' parameter '" + property->GetName() + "' must be primitive in Phase A.");
                        succeeded = false;
                    }
                    else
                    {
                        const size_t rawStorageSize = property->GetStorageSize();
                        const size_t rawStorageAlignment = property->GetStorageAlignment();
                        const uint32_t storageSize = (rawStorageSize > 0
                                                      && rawStorageSize <= static_cast<size_t>(std::numeric_limits<uint32_t>::max()))
                                                         ? static_cast<uint32_t>(rawStorageSize)
                                                         : 0;
                        const uint32_t storageAlignment = (rawStorageAlignment > 0
                                                           && rawStorageAlignment <= static_cast<size_t>(std::numeric_limits<uint32_t>::max()))
                                                              ? static_cast<uint32_t>(rawStorageAlignment)
                                                              : 0;
                        if (storageSize == 0)
                        {
                            AppendError("[Reflection] Function '" + classInfo->GetName() + "::" + function->GetName()
                                        + "' parameter '" + property->GetName() + "' has unsupported storage size.");
                            succeeded = false;
                        }
                        else if (storageAlignment == 0)
                        {
                            AppendError("[Reflection] Function '" + classInfo->GetName() + "::" + function->GetName()
                                        + "' parameter '" + property->GetName() + "' has unsupported storage alignment.");
                            succeeded = false;
                        }
                        else
                        {
                            const uint32_t alignedExpectedOffset = AlignUpU32(expectedOffset, storageAlignment);
                            if (param.Offset != alignedExpectedOffset)
                            {
                                AppendError("[Reflection] Function '" + classInfo->GetName() + "::" + function->GetName()
                                            + "' parameter '" + param.Property->GetName()
                                            + "' has invalid offset (expected " + std::to_string(alignedExpectedOffset)
                                            + ", got " + std::to_string(param.Offset) + ").");
                                succeeded = false;
                            }
                            expectedOffset = alignedExpectedOffset;
                            expectedOffset += storageSize;
                            if (storageAlignment > maxAlignment)
                            {
                                maxAlignment = storageAlignment;
                            }
                        }
                    }

                    if (param.IsReturn())
                    {
                        ++returnCount;
                    }

                    if (param.Role != MEParamRole::In && param.Role != MEParamRole::Return)
                    {
                        AppendError("[Reflection] Function '" + classInfo->GetName() + "::" + function->GetName()
                                    + "' parameter '" + param.Property->GetName()
                                    + "' uses unsupported param role for Phase A.");
                        succeeded = false;
                    }

                    if (param.PassKind != MEParamPassKind::Value)
                    {
                        AppendError("[Reflection] Function '" + classInfo->GetName() + "::" + function->GetName()
                                    + "' parameter '" + param.Property->GetName()
                                    + "' uses unsupported pass kind for Phase A.");
                        succeeded = false;
                    }
                }

                if (returnCount > 1)
                {
                    AppendError("[Reflection] Function '" + classInfo->GetName() + "::" + function->GetName()
                                + "' has more than one return parameter.");
                    succeeded = false;
                }

                expectedOffset = AlignUpU32(expectedOffset, maxAlignment);
                if (function->GetParmsSize() != expectedOffset)
                {
                    AppendError("[Reflection] Function '" + classInfo->GetName() + "::" + function->GetName()
                                + "' ParmsSize mismatch (expected " + std::to_string(expectedOffset) + ", got "
                                + std::to_string(function->GetParmsSize()) + ").");
                    succeeded = false;
                }

                if (returnCount == 1 && function->GetReturnValueOffset() < 0)
                {
                    AppendError("[Reflection] Function '" + classInfo->GetName() + "::" + function->GetName()
                                + "' has return parameter but ReturnValueOffset is invalid.");
                    succeeded = false;
                }

                if (returnCount == 0 && function->GetReturnValueOffset() >= 0)
                {
                    AppendError("[Reflection] Function '" + classInfo->GetName() + "::" + function->GetName()
                                + "' has no return parameter but ReturnValueOffset is set.");
                    succeeded = false;
                }
            }
        }

        return succeeded;
    }

    bool ReflectionSystem::RegisterClass_Internal(MEClass* classInfo)
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

    bool ReflectionSystem::RegisterEnum_Internal(MEEnum* enumInfo)
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

    void ReflectionSystem::AppendError(std::string message)
    {
        m_LastErrors.push_back(std::move(message));
    }

    void ReflectionSystem::PrepareForResolution()
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

    void ReflectionSystem::ResetPropertyResolvedRefs(MEProperty* property)
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

    bool ReflectionSystem::EnsureCanRegister(const char* operationName)
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

    bool ReflectionSystem::ResolvePendingSuperClasses()
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

            const MEClass* resolvedSuperClass = FindClassByTypeIndex(ref.superTypeIndex);
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

            ref.derivedClass->SetResolvedSuperClass(const_cast<MEClass*>(resolvedSuperClass));
        }

        return succeeded;
    }

    bool ReflectionSystem::ResolvePendingPropertyClasses()
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

            const MEClass* resolvedClass = FindClassByTypeIndex(ref.referencedTypeIndex);
            if (resolvedClass == nullptr)
            {
                AppendError("[Reflection] Unresolved property type for '" + ref.ownerClass->GetName() + "::" + ref.property->GetName() + "'.");
                succeeded = false;
                continue;
            }

            if (ref.property->GetCategory() == MEPropertyCategory::Object
                || ref.property->GetCategory() == MEPropertyCategory::ObjectPtr)
            {
                static_cast<MEObjectProperty*>(ref.property)->SetValueClass(const_cast<MEClass*>(resolvedClass));
            }
            else
            {
                AppendError("[Reflection] Pending class reference is bound to a non-object property '" + ref.ownerClass->GetName() + "::" + ref.property->GetName() + "'.");
                succeeded = false;
            }
        }

        return succeeded;
    }

    bool ReflectionSystem::ResolvePendingEnumPropertyRefs()
    {
        bool succeeded = true;

        for (const PendingEnumPropertyRef& ref : m_PendingEnumPropertyRefs)
        {
            if (ref.property == nullptr)
            {
                AppendError("[Reflection] Null property found in pending enum property references.");
                succeeded = false;
                continue;
            }

            const MEEnum* resolvedEnum = FindEnumByTypeIndex(ref.enumTypeIndex);
            if (resolvedEnum == nullptr)
            {
                AppendError("[Reflection] Unresolved enum type for property '" + ref.property->GetName() + "'.");
                succeeded = false;
                continue;
            }

            if (resolvedEnum->GetSize() == 0)
            {
                AppendError(
                    "[Reflection] Enum '" + resolvedEnum->GetName() + "' has zero Size for property '"
                    + ref.property->GetName() + "'.");
                succeeded = false;
                continue;
            }

            ref.property->primitiveTypeName = resolvedEnum->GetName();
            ref.property->boundEnum = resolvedEnum;
        }

        return succeeded;
    }

    bool ReflectionSystem::ValidateInheritanceGraph()
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

    bool ReflectionSystem::VisitClassForCycle(const MEClass& classInfo,
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

    void ReflectionSystem::BuildDerivedClassLinks()
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

    bool ReflectionSystem::FinalizeReflection()
    {
        if (m_State == ReflectionSystemState::Finalizing)
        {
            AppendError("[Reflection] FinalizeReflection re-entered.");
            return false;
        }

        m_LastErrors.clear();
        m_State = ReflectionSystemState::Finalizing;

        PrepareForResolution();

        bool succeeded = true;
        if (!ResolvePendingSuperClasses())
        {
            AppendError("[Reflection] Failed to resolve superclass references when Finalizing.");
            succeeded = false;
        }

        if (!ValidateInheritanceGraph())
        {
            AppendError("[Reflection] Inheritance graph validation failed when Finalizing (possible cycle detected).");
            succeeded = false;
        }

        if (!ResolvePendingPropertyClasses())
        {
            AppendError("[Reflection] Failed to resolve property class references when Finalizing.");
            succeeded = false;
        }

        if (!ResolvePendingEnumPropertyRefs())
        {
            AppendError("[Reflection] Failed to resolve enum property references when Finalizing.");
            succeeded = false;
        }

        if (!ValidateFunctions())
        {
            AppendError("[Reflection] Function metadata validation failed when Finalizing.");
            succeeded = false;
        }

        if (succeeded)
        {
            BuildDerivedClassLinks();
            SetCodecForEnums();
            m_State = ReflectionSystemState::Ready;
        }
        else
        {
            m_State = ReflectionSystemState::Failed;
        }

        return succeeded;
    }

    bool ReflectionSystem::ForEachPropertyInHierarchy_Recursive(const MEClass& classInfo,
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
            if (!ForEachPropertyInHierarchy_Recursive(*superClass, visitor, visited))
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

    void ReflectionSystem::SetCodecForEnums()
    {
        using minEngine::Serialization::PrimitiveCodecRegistry;
        using minEngine::Serialization::PrimitiveCodec;
        using minEngine::Serialization::ReaderArchive;
        using minEngine::Serialization::WriterArchive;
        PrimitiveCodecRegistry& codecRegistry = PrimitiveCodecRegistry::Get();
        
        for(auto& [enumTypeIdName, enumName] : m_DeclaredEnumNameByTypeIdName)
        {
            codecRegistry.RegisterCodecWithAliases(
                PrimitiveCodec{
                    [](WriterArchive& archive, const void* valuePtr) -> bool
                    {
                        if (valuePtr == nullptr)
                        {
                            return false;
                        }

                        return archive.WriteInt64(*static_cast<const int64_t*>(valuePtr));
                    },
                    [](ReaderArchive& archive, void* outValuePtr) -> bool
                    {
                        if (outValuePtr == nullptr)
                        {
                            return false;
                        }

                        int64_t intValue = 0;
                        if (!archive.ReadInt64(intValue))
                        {
                            return false;
                        }

                        *static_cast<int64_t*>(outValuePtr) = intValue;
                        return true;
                    }},
                {enumName, enumTypeIdName});
        }
        
    }
}
