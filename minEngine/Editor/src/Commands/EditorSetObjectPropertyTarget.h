#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace minEngine
{
    struct GUID;

    class EditorSetObjectPropertyTarget
    {
    public:
        virtual ~EditorSetObjectPropertyTarget() = default;

        virtual bool ApplySetObjectProperty(const GUID& ownerGuid,
                                            const std::string& ownerClassName,
                                            const std::string& propertyPath,
                                            const std::vector<uint8_t>& valueBlob) = 0;
    };
}
