#pragma once

#include "Core.h"

namespace minEngine
{
    enum MaterialProperty
    {
        MP_Albedo = 0,
        MP_Normal,
        MP_AO,
        MP_Metallic,
        MP_Roughness,
        MP_Emissive,
        MP_Opacity,

        MaterialShadingPropertyCount,

        MP_WorldPositionOffset,

        MaterialPropCount,
    };
}