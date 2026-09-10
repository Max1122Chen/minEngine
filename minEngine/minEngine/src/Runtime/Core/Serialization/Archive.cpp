#include "Archive.h"

#include "Runtime/Core/Reflection/MEClass.h"

namespace minEngine::Serialization
{
    bool WriterArchive::BeginObject(const Reflection::MEClass* classInfo, bool writeTypeName)
    {
        if (writeTypeName && classInfo != nullptr)
        {
            return BeginObject(classInfo->GetName());
        }
        return BeginObject(std::string());
    }

    bool WriterArchive::BeginObjectPtr(const Reflection::MEClass* classInfo)
    {
        return BeginObjectPtr(classInfo != nullptr ? classInfo->GetName() : std::string());
    }
}
