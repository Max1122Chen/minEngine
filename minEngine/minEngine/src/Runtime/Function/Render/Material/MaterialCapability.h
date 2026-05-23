#pragma once

#include "MaterialCompiler/MaterialCompileTypes.h"
#include "MaterialTypes.h"

#include <vector>

namespace minEngine
{
    enum class MaterialPropertyPinVisibility : uint8_t
    {
        Hidden,
        Disabled,
        Active,
    };

    class Material;
    class MaterialEdGraph;

    class MaterialCapabilityUtil
    {
    public:
        static MaterialPropertyPinVisibility GetPropertyPinVisibility(
            MaterialProperty property,
            MaterialShadingModel shadingModel,
            MaterialBlendMode blendMode);

        static bool IsPropertyRequiredAtCompile(
            MaterialProperty property,
            MaterialShadingModel shadingModel,
            MaterialBlendMode blendMode);

        // Hidden pins are not emitted into MIR / FragmentMaterialInputs (Unlit skips Metallic, Normal, etc.).
        static bool IsPropertyEmittedAtCompile(
            MaterialProperty property,
            MaterialShadingModel shadingModel,
            MaterialBlendMode blendMode);

        static MaterialPropertyPinVisibility GetMaterialOutputInputVisibility(
            const char* inputName,
            MaterialShadingModel shadingModel,
            MaterialBlendMode blendMode);

        static void PruneInvalidMaterialOutputLinks(Material& material);

        /** Properties that appear in FragmentMaterialInputs for the given shading/blend (MIR + shell). */
        static std::vector<MaterialProperty> GetFragmentPropertiesEmittedAtCompile(
            MaterialShadingModel shadingModel,
            MaterialBlendMode blendMode);
    };
}
