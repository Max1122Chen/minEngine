#pragma once

#include "Core.h"

#include "Runtime/Resource/AssetManager.h"

namespace minEngine
{
    enum class MeshImportProductChoice
    {
        None,
        StaticMesh,
        SkeletalMesh,
        Cancel
    };

    class EditorMeshImportProductDialog
    {
    public:
        void Open(int sourceFileCount);
        MeshImportProductChoice Draw();
        bool IsOpen() const { return m_Open; }
        void Close();

    private:
        bool m_Open = false;
        int m_SourceFileCount = 0;
    };
}
