#pragma once

#include "EngineAPI.h"
#include "MEProperties.h"

#include <string_view>

namespace minEngine
{
    class MEObject;
}

namespace minEngine::Reflection
{
    // PropertyChangedEvent lives in MEObject.h (shared with PostEditChangeProperty).

    struct PropertyAssignOptions
    {
        // Semantic PostEdit always applies to authoritative edits (Design §3.5).
        // Implementation: invoke MEObject::PostEditChangeProperty only when the property
        // has NO Setter — Setter itself carries change propagation.
        bool notifyPostEdit = false;
        MEObject* postEditObject = nullptr;
        std::string_view postEditPropertyName;
    };

    // Overloads avoid MinGW AVX by-value default-arg copies (vmovdqa to misaligned stack).
    MINENGINE_API bool AssignProperty(void* owner,
                                      const MEProperty& property,
                                      const void* valuePtr);

    MINENGINE_API bool AssignProperty(void* owner,
                                      const MEProperty& property,
                                      const void* valuePtr,
                                      const PropertyAssignOptions& options);

    MINENGINE_API bool GetPropertyValue(const void* owner,
                                        const MEProperty& property,
                                        void* outValuePtr);

    MINENGINE_API void CopyPropertyStorage(const MEProperty& property, void* dst, const void* src);
}
