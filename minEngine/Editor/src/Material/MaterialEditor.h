#pragma once



#include "Core.h"

#include "MaterialEditorPreview.h"

#include "MaterialEditorSession.h"



#include "Runtime/Function/Render/Material/MaterialCompiler/MaterialCompileTypes.h"

#include "Runtime/Resource/AssetMeta.h"



#include <vector>



namespace minEngine

{

    class Editor;



    /** Session + commands for material editing (no ImGui). */

    class MaterialEditor

    {

    public:

        static constexpr const char* kPreviewViewportPanelId = "material_editor_preview";



        explicit MaterialEditor(Editor& editor);



        void OnEnterMode();

        void OnExitMode();

        void Shutdown();



        /** Called when MaterialPreviewWindow attaches its viewport client. */

        void OnPreviewViewHostReady();



        void RefreshMaterialList();

        void OpenSession(const AssetMeta* meta);

        void CompileActiveMaterial();

        bool SaveActiveMaterial();

        void SetShadingModel(MaterialShadingModel model);



        /** Graph edit: Finalize + Compile + dirty + refresh preview. */

        void NotifyGraphChanged();



        void InvalidateGraphCanvas() { m_GraphCanvasInvalidated = true; }

        bool ConsumeGraphCanvasInvalidation()

        {

            const bool invalidated = m_GraphCanvasInvalidated;

            m_GraphCanvasInvalidated = false;

            return invalidated;

        }



        const MaterialEditorSession& GetSession() const { return m_Session; }

        MaterialEditorSession& GetSession() { return m_Session; }



        const std::vector<const AssetMeta*>& GetMaterialMetas() const { return m_MaterialMetas; }

        int GetSelectedMaterialIndex() const { return m_SelectedMaterialIndex; }



        MaterialEditorPreview& GetPreview() { return m_Preview; }

        const MaterialEditorPreview& GetPreview() const { return m_Preview; }



    private:

        void EnsureDefaultSession();

        void ApplySessionToPreview();



        Editor& m_Editor;

        MaterialEditorSession m_Session;

        MaterialEditorPreview m_Preview;

        std::vector<const AssetMeta*> m_MaterialMetas;

        int m_SelectedMaterialIndex = -1;

        bool m_GraphCanvasInvalidated = false;

    };

}

