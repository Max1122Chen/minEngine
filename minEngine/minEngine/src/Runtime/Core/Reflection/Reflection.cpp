#include "Reflection.h"
#include "Runtime/Core/Serialization/PrimitiveCodecRegistry.h"

namespace minEngine::Reflection
{
    ReflectionSystem &ReflectionSystem::Get()
    {
        static ReflectionSystem system;
        return system;
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

        PrepareForResolve();

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
