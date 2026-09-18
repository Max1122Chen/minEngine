#pragma once

#include <cstdint>
#include <string>

namespace minEngine
{
    enum class SceneEditorError : uint16_t
    {
        None = 0,
        NotFound,
        InvalidArgument,
        InvalidPropertyPath,
        PrefabConstrained,
        Internal,
    };

    /** Lean status feedback for SceneEditorServices. Products use out-params. */
    struct SceneEditorResult
    {
        bool Ok = false;
        SceneEditorError Code = SceneEditorError::None;
        std::string Message;
    };
}
