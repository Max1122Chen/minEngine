#include "Commands/EditorObjectPropertyApply.h"

#include "Runtime/Core/GUID/GUID.h"
#include "Runtime/Core/Log/LogSystem.h"
#include "Runtime/Core/Object/MEObject.h"
#include "Runtime/Core/Object/ObjectManager.h"
#include "Runtime/Core/Reflection/Reflection.h"

namespace minEngine
{
    Serialization::SerializerOptions EditorObjectPropertyApply::MakeDefaultOptions()
    {
        Serialization::SerializerOptions options;
        options.skipUnknownField = false;
        return options;
    }

    bool EditorObjectPropertyApply::ApplyBlob(const GUID& ownerGuid,
                                              const std::string& ownerClassName,
                                              const std::string& propertyPath,
                                              const std::vector<uint8_t>& valueBlob,
                                              const Serialization::SerializerOptions& options)
    {
        std::shared_ptr<MEObject> ownerObject = ObjectManager::Get().FindObject(ownerGuid);
        if (!ownerObject)
        {
            ME_LOG(LogEditor, Warn,
                "ApplySetObjectProperty: owner not found (guid='{}', property='{}').",
                ownerGuid.ToString(),
                propertyPath);
            return false;
        }

        const Reflection::MEClass* ownerClass = Reflection::ReflectionSystem::Get().FindClass(ownerClassName);
        if (ownerClass == nullptr)
        {
            ME_LOG(LogEditor, Warn,
                "ApplySetObjectProperty: class '{}' not found (property='{}').",
                ownerClassName,
                propertyPath);
            return false;
        }

        std::vector<Serialization::PendingObjectRef> unresolvedRefs;
        const Serialization::SerializeResult result = Serialization::Serializer::DeserializePropertyByPathFromBuffer(
            ownerObject.get(),
            ownerClass,
            propertyPath,
            valueBlob,
            unresolvedRefs,
            options);
        if (!result.ok)
        {
            ME_LOG(LogEditor, Warn,
                "ApplySetObjectProperty failed: {} (path='{}').",
                result.message,
                result.fieldPath);
            return false;
        }

        if (!unresolvedRefs.empty())
        {
            const Serialization::SerializeResult resolveResult =
                Serialization::Serializer::ResolvePendingObjectRefs(unresolvedRefs);
            if (!resolveResult.ok)
            {
                ME_LOG(LogEditor, Warn, "ApplySetObjectProperty: unresolved object references remain.");
            }
        }

        return true;
    }

    bool EditorObjectPropertyApply::SerializeBlob(const GUID& ownerGuid,
                                                  const std::string& ownerClassName,
                                                  const std::string& propertyPath,
                                                  std::vector<uint8_t>& outBlob,
                                                  const Serialization::SerializerOptions& options)
    {
        outBlob.clear();
        std::shared_ptr<MEObject> ownerObject = ObjectManager::Get().FindObject(ownerGuid);
        if (!ownerObject)
        {
            return false;
        }

        const Reflection::MEClass* ownerClass = Reflection::ReflectionSystem::Get().FindClass(ownerClassName);
        if (ownerClass == nullptr)
        {
            return false;
        }

        const Serialization::SerializeResult result = Serialization::Serializer::SerializePropertyByPathToBuffer(
            ownerObject.get(),
            ownerClass,
            propertyPath,
            outBlob,
            options);
        return result.ok;
    }
}
