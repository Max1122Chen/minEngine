#pragma once

#include <cstdint>
#include <string>

namespace minEngine
{
    enum class GraphEditorError : uint16_t
    {
        None = 0,
        NotFound,
        InvalidArgument,
        InvalidPropertyPath,
        Internal,
    };

    /** Status for Material / AnimationGraph editor services. Products use out-params. */
    struct GraphEditorResult
    {
        bool Ok = false;
        GraphEditorError Code = GraphEditorError::None;
        std::string Message;
    };
}
