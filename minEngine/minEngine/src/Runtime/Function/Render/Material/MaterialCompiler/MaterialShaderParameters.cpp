#include "MaterialShaderParameters.h"

namespace minEngine
{
    int GetRequiredMaterialTexCoordCount(bool usesTexCoord0)
    {
        return usesTexCoord0 ? 1 : 0;
    }
}
