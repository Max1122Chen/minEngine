#include "Services/AssetWorkflowModule.h"

#include "Material/MaterialEditor.h"
#include "Scene/SceneEditor.h"
#include "Shell/IEditorContext.h"

#include "Runtime/Resource/AssetMeta.h"

namespace minEngine
{
    void AssetWorkflowModule::Register(IEditorContext& context)
    {
        m_Context = &context;
    }

    void AssetWorkflowModule::Shutdown()
    {
        m_Context = nullptr;
    }

    bool AssetWorkflowModule::OpenAsset(const AssetMeta& meta)
    {
        if (!m_Context)
        {
            return false;
        }

        if (EditorSubModule* materialModule = m_Context->FindSubModule(MaterialEditor::kModuleId))
        {
            if (materialModule->CanOpenAsset(meta) && materialModule->OpenAsset(meta))
            {
                m_Context->ActivateSubModule(MaterialEditor::kModuleId);
                return true;
            }
        }

        if (EditorSubModule* sceneModule = m_Context->FindSubModule(SceneEditor::kModuleId))
        {
            if (sceneModule->CanOpenAsset(meta) && sceneModule->OpenAsset(meta))
            {
                m_Context->ActivateSubModule(SceneEditor::kModuleId);
                return true;
            }
        }

        return false;
    }
}
