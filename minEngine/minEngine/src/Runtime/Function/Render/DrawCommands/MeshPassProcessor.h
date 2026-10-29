#pragma once

#include "Runtime/Function/Render/DrawCommands/MeshBatch.h"
#include "Runtime/Function/Render/DrawCommands/MeshDrawCommand.h"

#include <vector>

namespace minEngine
{
    /**
     * Turns MeshBatches into MeshDrawCommands for one pass.
     * Acceptance policy lives on the subclass, not in a central pass-kind switch.
     */
    class MINENGINE_API MeshPassProcessor
    {
    public:
        virtual ~MeshPassProcessor() = default;

        virtual void AddMeshBatch(const MeshBatch& batch) = 0;

    protected:
        static bool IsDrawable(const MeshBatch& batch);
        static bool UsesTranslucentPath(const MeshBatch& batch);
        static MeshDrawCommand MakeDrawCommand(const MeshBatch& batch);
    };

    class MINENGINE_API OpaqueMeshPassProcessor : public MeshPassProcessor
    {
    public:
        explicit OpaqueMeshPassProcessor(std::vector<MeshDrawCommand>& outCommands);

        void AddMeshBatch(const MeshBatch& batch) override;

    private:
        std::vector<MeshDrawCommand>& m_OutCommands;
    };

    class MINENGINE_API TranslucentMeshPassProcessor : public MeshPassProcessor
    {
    public:
        explicit TranslucentMeshPassProcessor(std::vector<MeshDrawCommand>& outCommands);

        void AddMeshBatch(const MeshBatch& batch) override;

    private:
        std::vector<MeshDrawCommand>& m_OutCommands;
    };

    class MINENGINE_API ShadowDepthMeshPassProcessor : public MeshPassProcessor
    {
    public:
        explicit ShadowDepthMeshPassProcessor(std::vector<MeshDrawCommand>& outCommands);

        void AddMeshBatch(const MeshBatch& batch) override;

    private:
        std::vector<MeshDrawCommand>& m_OutCommands;
    };
}
