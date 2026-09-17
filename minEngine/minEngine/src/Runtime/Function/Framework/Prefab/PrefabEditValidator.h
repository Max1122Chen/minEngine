#pragma once

#include "Core.h"
#include "Runtime/Function/Framework/Prefab/PrefabTypes.h"

#include <string>
#include <string_view>

namespace minEngine
{
    class PrefabEditValidator
    {
    public:
        static PrefabEditValidationResult ValidateEdit(
            Scene& scene,
            const PrefabInstanceRecord& record,
            const PrefabEditOp& op);
    };
}
