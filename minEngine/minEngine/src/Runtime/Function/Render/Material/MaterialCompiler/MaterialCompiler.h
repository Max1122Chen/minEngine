#pragma once
#include "Core.h"
#include "Render/Material/MaterialTypes.h"

namespace minEngine
{
    class MaterialEdGraph;
    class MaterialGraphNodeDef;

    class MaterialCompiler
    {
    public:
        virtual ~MaterialCompiler() = default;

        // Temporary graph compile entry for the current MaterialGraph -> IR -> shader pipeline.
        // The implementation will be provided by a concrete backend / translator.
        virtual MaterialCompileResult Compile(const MaterialEdGraph& graph, int32_t nodeIndex = -1, int32_t outputIndex = 0) = 0;

        virtual int32_t Constant(float x) = 0;
        virtual int32_t Constant2(float x, float y) = 0;
        virtual int32_t Constant3(float x, float y, float z) = 0;
        virtual int32_t Constant4(float x, float y, float z, float w) = 0;

        virtual int32_t Add(int32_t left, int32_t right) = 0;
        virtual int32_t Sub(int32_t left, int32_t right) = 0;
        virtual int32_t Multiply(int32_t left, int32_t right) = 0;

        virtual int32_t GenericConstant(const MaterialLiteralValue& value) = 0;
        virtual MaterialValueType GetType(int32_t code) const = 0;

    };
}