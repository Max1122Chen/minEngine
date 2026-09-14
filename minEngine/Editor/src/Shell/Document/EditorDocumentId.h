#pragma once

#include <cstdint>
#include <functional>

namespace minEngine
{
    struct EditorDocumentId
    {
        uint64_t Value = 0;

        bool IsValid() const { return Value != 0; }

        bool operator==(const EditorDocumentId& other) const { return Value == other.Value; }
        bool operator!=(const EditorDocumentId& other) const { return Value != other.Value; }
    };

    struct EditorDocumentIdHash
    {
        size_t operator()(const EditorDocumentId& id) const
        {
            return std::hash<uint64_t>{}(id.Value);
        }
    };
}
