#pragma once
#include "Core.h"

namespace minEngine
{
    class MIRPrimitiveType;
    class MIRObjectType;

    enum MIRValueTypeKind
    {
        TK_Void,
        TK_Poison,
        TK_Primitive,
        TK_Object,
    };

    struct MIRValueType
    {
        MIRValueTypeKind Kind;

        static const MIRValueType* GetVoid();
        static const MIRValueType* GetPoison();

        bool IsPoison() const;

        const MIRPrimitiveType* AsPrimitive() const;
        const MIRPrimitiveType* AsScalar() const;
        const MIRPrimitiveType* AsVector() const;
        const MIRPrimitiveType* AsMatrix() const;
        const MIRObjectType* AsObject() const;
    };

    enum MIRScalarKind
    {
        SK_Bool,
        SK_Int,
        SK_Float,
    };

    struct MIRPrimitiveType : public MIRValueType
    {
        MIRScalarKind ScalarKind;
        int NumRows;
        int NumCols;

        static const MIRPrimitiveType* GetBool();
        static const MIRPrimitiveType* GetInt();
        static const MIRPrimitiveType* GetFloat();
        static const MIRPrimitiveType* GetFloat2();
        static const MIRPrimitiveType* GetFloat3();
        static const MIRPrimitiveType* GetFloat4();

        static const MIRPrimitiveType* GetScalar(MIRScalarKind inKind);
        static const MIRPrimitiveType* GetVector(MIRScalarKind inKind, int numRows);
        static const MIRPrimitiveType* GetMatrix(MIRScalarKind inKind, int numRows, int numCols);
        
        static const MIRPrimitiveType* Get(MIRScalarKind inKind, int numRows, int numCols);

        int GetNumComponents() const { return NumRows * NumCols; }
        bool IsScalar() const { return GetNumComponents() == 1; }
        bool IsVector() const { return NumCols == 1 && NumRows > 1; }
        bool IsMatrix() const { return NumCols > 1 && NumRows > 1; }
    };

    enum MIRObjectKind
    {
        OK_Texture2D
    };

    struct MIRObjectType : public MIRValueType
    {
        MIRObjectKind ObjectKind;

        static const MIRObjectType* GetTexture2D();
    };
}