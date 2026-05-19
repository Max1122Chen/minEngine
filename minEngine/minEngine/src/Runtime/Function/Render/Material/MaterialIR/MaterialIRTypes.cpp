#include "MaterialIRTypes.h"

namespace minEngine
{
    const MIRValueType *MIRValueType::GetVoid()
    {
        static MIRValueType voidType = { TK_Void };
        return &voidType;
    }

    const MIRValueType *MIRValueType::GetPoison()
    {
        static MIRValueType poisonType = { TK_Poison };
        return &poisonType;
    }

    bool MIRValueType::IsPoison() const
    {
        return Kind == TK_Poison;
    }

    const MIRPrimitiveType *MIRValueType::AsPrimitive() const
    {
        return Kind == TK_Primitive ? static_cast<const MIRPrimitiveType*>(this) : nullptr;
    }

    const MIRPrimitiveType *MIRValueType::AsScalar() const
    {
        const MIRPrimitiveType* primitive = AsPrimitive();
        return (primitive != nullptr && primitive->IsScalar()) ? primitive : nullptr;
    }

    const MIRPrimitiveType *MIRValueType::AsVector() const
    {
        const MIRPrimitiveType* primitive = AsPrimitive();
        return (primitive != nullptr && primitive->IsVector()) ? primitive : nullptr;
    }

    const MIRPrimitiveType *MIRValueType::AsMatrix() const
    {
        const MIRPrimitiveType* primitive = AsPrimitive();
        return (primitive != nullptr && primitive->IsMatrix()) ? primitive : nullptr;
    }

    const MIRObjectType *MIRValueType::AsObject() const
    {
        return Kind == TK_Object ? static_cast<const MIRObjectType*>(this) : nullptr;
    }

    const MIRPrimitiveType *MIRPrimitiveType::GetBool()
    {
        return GetScalar(SK_Bool);
    }

    const MIRPrimitiveType *MIRPrimitiveType::GetInt()
    {
        return GetScalar(SK_Int);
    }

    const MIRPrimitiveType *MIRPrimitiveType::GetFloat()
    {
        return GetScalar(SK_Float);
    }

    const MIRPrimitiveType *MIRPrimitiveType::GetFloat2()
    {
        return GetVector(SK_Float, 2);
    }

    const MIRPrimitiveType *MIRPrimitiveType::GetFloat3()
    {
        return GetVector(SK_Float, 3);
    }

    const MIRPrimitiveType *MIRPrimitiveType::GetFloat4()
    {
        return GetVector(SK_Float, 4);
    }

    const MIRPrimitiveType *MIRPrimitiveType::GetScalar(MIRScalarKind inKind)
    {
        return Get(inKind, 1, 1);
    }

    const MIRPrimitiveType *MIRPrimitiveType::GetVector(MIRScalarKind inKind, int numRows)
    {
        return Get(inKind, numRows, 1);
    }

    const MIRPrimitiveType *MIRPrimitiveType::GetMatrix(MIRScalarKind inKind, int numRows, int numCols)
    {
        return Get(inKind, numRows, numCols);
    }

    const MIRPrimitiveType* MIRPrimitiveType::Get(MIRScalarKind inKind, int numRows, int numCols)
    {
        static const MIRPrimitiveType types[] = {
            { TK_Primitive, SK_Bool, 1, 1 },    // Bool
            { TK_Primitive, SK_Bool, 1, 2 },    // Invalid
            { TK_Primitive, SK_Bool, 1, 3 },    // Invalid
            { TK_Primitive, SK_Bool, 1, 4 },    // Invalid
            { TK_Primitive, SK_Bool, 2, 1 },    // Bool2
            { TK_Primitive, SK_Bool, 2, 2 },    // Bool2x2
            { TK_Primitive, SK_Bool, 2, 3 },    // Bool2x3
            { TK_Primitive, SK_Bool, 2, 4 },    // Bool2x4
            { TK_Primitive, SK_Bool, 3, 1 },    // Bool3
            { TK_Primitive, SK_Bool, 3, 2 },    // Bool3x2
            { TK_Primitive, SK_Bool, 3, 3 },    // Bool3x3
            { TK_Primitive, SK_Bool, 3, 4 },    // Bool3x4
            { TK_Primitive, SK_Bool, 4, 1 },    // Bool4
            { TK_Primitive, SK_Bool, 4, 2 },    // Bool4x2
            { TK_Primitive, SK_Bool, 4, 3 },    // Bool4x3
            { TK_Primitive, SK_Bool, 4, 4 },    // Bool4x4
            { TK_Primitive, SK_Int, 1, 1 },     // Int
            { TK_Primitive, SK_Int, 1, 2 },     // Invalid
            { TK_Primitive, SK_Int, 1, 3 },     // Invalid
            { TK_Primitive, SK_Int, 1, 4 },     // Invalid
            { TK_Primitive, SK_Int, 2, 1 },     // Int2
            { TK_Primitive, SK_Int, 2, 2 },     // Int2x2
            { TK_Primitive, SK_Int, 2, 3 },     // Int2x3
            { TK_Primitive, SK_Int, 2, 4 },     // Int2x4
            { TK_Primitive, SK_Int, 3, 1 },     // Int3
            { TK_Primitive, SK_Int, 3, 2 },     // Int3x2
            { TK_Primitive, SK_Int, 3, 3 },     // Int3x3
            { TK_Primitive, SK_Int, 3, 4 },     // Int3x4
            { TK_Primitive, SK_Int, 4, 1 },     // Int4
            { TK_Primitive, SK_Int, 4, 2 },     // Int4x2
            { TK_Primitive, SK_Int, 4, 3 },     // Int4x3
            { TK_Primitive, SK_Int, 4, 4 },     // Int4x4
            { TK_Primitive, SK_Float, 1, 1 },   // Float
            { TK_Primitive, SK_Float, 1, 2 },   // Invalid
            { TK_Primitive, SK_Float, 1, 3 },   // Invalid
            { TK_Primitive, SK_Float, 1, 4 },   // Invalid
            { TK_Primitive, SK_Float, 2, 1 },   // Float2
            { TK_Primitive, SK_Float, 2, 2 },   // Float2x2
            { TK_Primitive, SK_Float, 2, 3 },   // Float2x3
            { TK_Primitive, SK_Float, 2, 4 },   // Float2x4
            { TK_Primitive, SK_Float, 3, 1 },   // Float3
            { TK_Primitive, SK_Float, 3, 2 },   // Float3x2
            { TK_Primitive, SK_Float, 3, 3 },   // Float3x3
            { TK_Primitive, SK_Float, 3, 4 },   // Float3x4
            { TK_Primitive, SK_Float, 4, 1 },   // Float4
            { TK_Primitive, SK_Float, 4, 2 },   // Float4x2
            { TK_Primitive, SK_Float, 4, 3 },   // Float4x3
            { TK_Primitive, SK_Float, 4, 4 },   // Float4x4
        };

        int index = inKind * 4 * 4 + (numRows - 1) * 4 + (numCols - 1);
        return &types[index];
    }

    const MIRObjectType* MIRObjectType::GetTexture2D()
    {
        static const MIRObjectType texture2DType = { TK_Object, OK_Texture2D };
        return &texture2DType;
    }
}