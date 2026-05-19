#include "MaterialShadingModel.h"

#include "MaterialShaderParameters.h"
#include "../MaterialPropertyUtil.h"
#include "Render/Material/MaterialIR/MaterialIRTypes.h"

namespace minEngine
{
    namespace
    {
        std::string BuildMaterialParametersStructGlobal(int numTexCoords)
        {
            if (numTexCoords <= 0)
            {
                return {};
            }

            std::string declaration = "struct {\n";
            declaration += "    vec2 TexCoords[" + std::to_string(numTexCoords) + "];\n";
            declaration += "} ";
            declaration += GetMaterialParametersSymbol();
            declaration += ";\n\n";
            return declaration;
        }

        std::string BuildVertexTexCoordSetup(int numTexCoords)
        {
            std::string setup;
            for (int texCoordIndex = 0; texCoordIndex < numTexCoords; ++texCoordIndex)
            {
                setup += "    ";
                setup += GetMaterialParametersTexCoordAccess(texCoordIndex);
                setup += " = a_TexCoord;\n";
                setup += "    ";
                setup += GetMaterialTexCoordVaryingName(texCoordIndex);
                setup += " = ";
                setup += GetMaterialParametersTexCoordAccess(texCoordIndex);
                setup += ";\n";
            }
            return setup;
        }

        std::string BuildFragmentTexCoordSetup(int numTexCoords)
        {
            std::string setup;
            for (int texCoordIndex = 0; texCoordIndex < numTexCoords; ++texCoordIndex)
            {
                setup += "    ";
                setup += GetMaterialParametersTexCoordAccess(texCoordIndex);
                setup += " = ";
                setup += GetMaterialTexCoordVaryingName(texCoordIndex);
                setup += ";\n";
            }
            if (!setup.empty())
            {
                setup += "\n";
            }
            return setup;
        }

        std::string BuildFragmentMaterialInputsStructGlobal()
        {
            std::string declaration = "struct {\n";
            for (int propertyIndex = 0; propertyIndex < MaterialShadingPropertyCount; ++propertyIndex)
            {
                const MaterialProperty property = static_cast<MaterialProperty>(propertyIndex);
                if (!MaterialPropertyEvaluatesInStage(property, Stage_Fragment))
                {
                    continue;
                }

                const MIRPrimitiveType* propertyType = GetMaterialPropertyType(property);
                if (propertyType == nullptr)
                {
                    continue;
                }

                declaration += "    ";
                if (propertyType->IsVector())
                {
                    declaration += "vec" + std::to_string(propertyType->NumRows) + " ";
                }
                else
                {
                    declaration += "float ";
                }
                declaration += GetMaterialPropertyName(property);
                declaration += ";\n";
            }
            declaration += "} FragmentMaterialInputs;\n\n";
            return declaration;
        }

        class UnlitShadingModel final : public IMaterialShadingModel
        {
        public:
            bool AssembleVertexShader(MaterialCompiledShader& compiled) const override
            {
                const int numTexCoords = GetRequiredMaterialTexCoordCount(compiled.UsesTexCoord0);

                std::string vertexShader = "#version 330 core\n";
                vertexShader += "layout(location = 0) in vec3 a_Position;\n";
                if (numTexCoords > 0)
                {
                    vertexShader += "layout(location = 1) in vec2 a_TexCoord;\n";
                    for (int texCoordIndex = 0; texCoordIndex < numTexCoords; ++texCoordIndex)
                    {
                        vertexShader += "out vec2 ";
                        vertexShader += GetMaterialTexCoordVaryingName(texCoordIndex);
                        vertexShader += ";\n";
                    }
                }

                if (numTexCoords > 0)
                {
                    vertexShader += BuildMaterialParametersStructGlobal(numTexCoords);
                }

                vertexShader += "void main()\n{\n";
                if (numTexCoords > 0)
                {
                    vertexShader += BuildVertexTexCoordSetup(numTexCoords);
                }

                const MaterialStageSource& vertexStage = compiled.Stages[Stage_Vertex];
                if (!vertexStage.Body.empty())
                {
                    vertexShader += vertexStage.Body;
                    if (vertexStage.Body.back() != '\n')
                    {
                        vertexShader += '\n';
                    }
                }

                vertexShader += "    gl_Position = vec4(a_Position, 1.0);\n";
                vertexShader += "}\n";
                compiled.FullVertexShader = std::move(vertexShader);
                return true;
            }

            bool AssembleFragmentShader(MaterialCompiledShader& compiled) const override
            {
                const MaterialStageSource& fragmentStage = compiled.Stages[Stage_Fragment];
                const int numTexCoords = GetRequiredMaterialTexCoordCount(compiled.UsesTexCoord0);

                std::string fragmentShader = "#version 330 core\n";
                for (int texCoordIndex = 0; texCoordIndex < numTexCoords; ++texCoordIndex)
                {
                    fragmentShader += "in vec2 ";
                    fragmentShader += GetMaterialTexCoordVaryingName(texCoordIndex);
                    fragmentShader += ";\n";
                }
                fragmentShader += "out vec4 FragColor;\n\n";

                if (!fragmentStage.Preamble.empty())
                {
                    fragmentShader += fragmentStage.Preamble;
                }

                fragmentShader += BuildFragmentMaterialInputsStructGlobal();
                if (numTexCoords > 0)
                {
                    fragmentShader += BuildMaterialParametersStructGlobal(numTexCoords);
                }

                fragmentShader += "void main()\n{\n";
                if (numTexCoords > 0)
                {
                    fragmentShader += BuildFragmentTexCoordSetup(numTexCoords);
                }

                if (!fragmentStage.Body.empty())
                {
                    fragmentShader += fragmentStage.Body;
                    if (fragmentStage.Body.back() != '\n')
                    {
                        fragmentShader += '\n';
                    }
                }

                fragmentShader += "    FragColor = vec4(FragmentMaterialInputs.Albedo + FragmentMaterialInputs.Emissive, FragmentMaterialInputs.Opacity);\n";
                fragmentShader += "}\n";
                compiled.FullFragmentShader = std::move(fragmentShader);
                return true;
            }
        };

        const UnlitShadingModel G_UnlitShadingModel;
    }

    const IMaterialShadingModel& GetMaterialShadingModel(MaterialShadingMode shadingMode)
    {
        switch (shadingMode)
        {
        case MaterialShadingMode::Unlit:
            return G_UnlitShadingModel;
        default:
            return G_UnlitShadingModel;
        }
    }
}
