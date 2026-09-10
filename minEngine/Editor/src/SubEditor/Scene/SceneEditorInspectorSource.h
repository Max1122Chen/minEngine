#pragma once

#include "Core.h"
#include "Runtime/Core/GUID/GUID.h"
#include "Shell/IEditorInspectorSource.h"
#include "UI/Property/PropertyEditSession.h"

#include "Runtime/Core/Reflection/Reflection.h"
#include "Runtime/Function/Framework/Components/Component.h"
#include "Runtime/Function/Framework/GameObject/GameObject.h"

#include <algorithm>
#include <cfloat>
#include <cstdint>
#include <cstring>
#include <limits>
#include <string>
#include <unordered_map>
#include <vector>

namespace minEngine
{
    class SceneEditor;
    class EditorAppearance;

    struct PropertyUndoCaptureContext
    {
        GUID ownerGuid{};
        std::string ownerClassName;
        std::string capturePropertyPath;

        bool IsValid() const
        {
            return !ownerGuid.IsZero() && !ownerClassName.empty() && !capturePropertyPath.empty();
        }
    };

    class SceneEditorInspectorSource : public IEditorInspectorSource
    {
    public:
        explicit SceneEditorInspectorSource(SceneEditor& sceneEditor);

        bool HasInspectableSelection() const override;
        void DrawInspector() override;

        void StartInlineRename(const GameObject& gameObject);
        void BeginComponentRename(Component& component, uint64_t ownerGameObjectId);

    private:
        std::string GetShortTypeName(const std::string& fullTypeName);

        static constexpr uint64_t kInvalidGameObjectId = std::numeric_limits<uint64_t>::max();

        void DrawGameObjectDetails(GameObject* gameObject);

        PropertyUndoCaptureContext MakePropertyUndoCaptureContext(const MEObject* owner,
                                                                  const Reflection::MEClass* ownerClass,
                                                                  const std::string& capturePropertyPath) const;

        bool DrawProperty(const MEObject* owner,
                          const Reflection::MEClass* ownerClass,
                          const Reflection::MEProperty& property,
                          void* propertyPtr,
                          const PropertyUndoCaptureContext* parentUndoContext = nullptr);

        // Edit via temp buffer then AssignProperty (Setter or member + optional virtual PostEdit).
        bool DrawLeafPropertyViaAssign(MEObject* owner, const Reflection::MEProperty& property);

        bool CanUndoInspectorProperty(const Reflection::MEProperty& property) const;

        bool SerializePropertyUndoBlob(const PropertyUndoCaptureContext& context, std::vector<uint8_t>& outBlob) const;

        void TryPropertyUndoActivated(const PropertyUndoCaptureContext& context, uint32_t editId);

        void TryPropertyUndoCommitAfterEdit(const PropertyUndoCaptureContext& context, uint32_t editId);

        void TryPropertyUndoCommitImmediate(const PropertyUndoCaptureContext& context,
                                            const std::vector<uint8_t>& beforeBlob,
                                            const std::vector<uint8_t>& afterBlob);

        void ApplyPropertyUndoCaptureHooks(const PropertyUndoCaptureContext& context, bool allowRowCapture);

        bool DrawPrimitiveProperty(const Reflection::MEPrimitiveProperty& primitiveProperty, void* propertyPtr);

        bool DrawObjectProperty(const MEObject* owner,
                                const Reflection::MEClass* ownerClass,
                                const Reflection::MEObjectProperty& objectProperty,
                                void* propertyPtr,
                                const PropertyUndoCaptureContext* objectUndoContext);
        bool DrawObjectPtrProperty(const MEObject* owner,
                                   const Reflection::MEClass* ownerClass,
                                   const Reflection::MEObjectPtrProperty& objectPtrProperty,
                                   void* propertyPtr,
                                   const PropertyUndoCaptureContext* undoContext);
        bool DrawArrayProperty(const Reflection::MEArrayProperty& arrayProperty, void* propertyPtr);

        void DrawGameObjectHeaderContextMenu(GameObject& gameObject);
        void DrawComponentContextMenu(Component& component);
        bool DrawComponentHeaderRow(GameObject& gameObject,
                                    Component& component,
                                    const Reflection::MEClass* classInfo,
                                    EditorAppearance* appearance,
                                    bool& outComponentOpen,
                                    bool& inOutOpenHeaderContextMenu);

        static std::string MakeAssetPropertyUndoKey(const GUID& ownerGuid, const std::string& propertyName);

        SceneEditor& m_SceneEditor;
        PropertyEditSession m_PropertyEditSession;
        static constexpr const char* kWindowTitle = "Inspector";
        std::string m_SelectedAddComponentTypeName;
        char m_AddComponentFilterBuffer[128] = {};
        bool m_IsRenamingSelectedGameObject = false;
        bool m_RequestRenameFocus = false;
        uint64_t m_RenameTargetGameObjectId = kInvalidGameObjectId;
        char m_RenameBuffer[256] = {};

        bool m_IsRenamingComponent = false;
        bool m_RequestComponentRenameFocus = false;
        GUID m_RenamingComponentGuid{};
        uint64_t m_RenamingComponentOwnerId = kInvalidGameObjectId;
        char m_ComponentRenameBuffer[256] = {};

        std::unordered_map<uint32_t, std::vector<uint8_t>> m_PropertyUndoBeforeByEditId;
        std::unordered_map<std::string, std::vector<uint8_t>> m_AssetPropertyUndoBeforeByKey;
    };
}
