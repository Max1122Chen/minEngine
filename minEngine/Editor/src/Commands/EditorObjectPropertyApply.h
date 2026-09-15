#pragma once

#include "Runtime/Core/Serialization/Serializer.h"

#include <string>
#include <vector>

namespace minEngine
{
    struct GUID;

    class EditorObjectPropertyApply
    {
    public:
        static Serialization::SerializerOptions MakeDefaultOptions();

        static bool ApplyBlob(const GUID& ownerGuid,
                              const std::string& ownerClassName,
                              const std::string& propertyPath,
                              const std::vector<uint8_t>& valueBlob,
                              const Serialization::SerializerOptions& options);

        static bool SerializeBlob(const GUID& ownerGuid,
                                  const std::string& ownerClassName,
                                  const std::string& propertyPath,
                                  std::vector<uint8_t>& outBlob,
                                  const Serialization::SerializerOptions& options);
    };
}
