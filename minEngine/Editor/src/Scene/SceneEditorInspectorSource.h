#pragma once

#include "Core.h"
#include "Shell/IEditorInspectorSource.h"

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

    class SceneEditorInspectorSource : public IEditorInspectorSource
    {
    public:
        explicit SceneEditorInspectorSource(SceneEditor& sceneEditor);

        bool HasInspectableSelection() const override;
        void DrawInspector() override;

    private:
        std::string GetShortTypeName(const std::string& fullTypeName);

        static constexpr uint64_t kInvalidGameObjectId = std::numeric_limits<uint64_t>::max();

        void DrawGameObjectDetails(GameObject* gameObject);

        bool DrawProperty(const MEObject* owner,
                          const Reflection::MEClass* ownerClass,
                          const Reflection::MEProperty& property,
                          void* propertyPtr);

        bool CanUndoInspectorProperty(const Reflection::MEProperty& property) const;

        void TryCapturePropertyUndoBefore(uint32_t editId,
                                          const MEObject* owner,
                                          const Reflection::MEClass* ownerClass,
                                          const std::string& propertyName);

        void TryCommitPropertyUndoAfter(uint32_t editId,
                                        const MEObject* owner,
                                        const Reflection::MEClass* ownerClass,
                                        const std::string& propertyName);

        bool DrawPrimitiveProperty(const Reflection::MEPrimitiveProperty& primitiveProperty, void* propertyPtr);
        bool DrawIntProperty(const Reflection::MEPrimitiveProperty& primitiveProperty, void* propertyPtr);
        bool DrawFloatProperty(const Reflection::MEPrimitiveProperty& primitiveProperty, void* propertyPtr);
        bool DrawDoubleProperty(const Reflection::MEPrimitiveProperty& primitiveProperty, void* propertyPtr);
        bool DrawBoolProperty(const Reflection::MEPrimitiveProperty& primitiveProperty, void* propertyPtr);
        bool DrawStringProperty(const Reflection::MEPrimitiveProperty& primitiveProperty, void* propertyPtr);
        bool DrawVector2Property(const Reflection::MEPrimitiveProperty& primitiveProperty, void* propertyPtr);
        bool DrawVector3Property(const Reflection::MEPrimitiveProperty& primitiveProperty, void* propertyPtr);
        bool DrawVector4Property(const Reflection::MEPrimitiveProperty& primitiveProperty, void* propertyPtr);
        bool DrawEnumProperty(const Reflection::MEPrimitiveProperty& primitiveProperty, void* propertyPtr);

        bool DrawObjectProperty(const MEObject* owner,
                                const Reflection::MEClass* ownerClass,
                                const Reflection::MEObjectProperty& objectProperty,
                                void* propertyPtr);
        bool DrawObjectPtrProperty(const MEObject* owner,
                                   const Reflection::MEClass* ownerClass,
                                   const Reflection::MEObjectPtrProperty& objectPtrProperty,
                                   void* propertyPtr);
        bool DrawAssetRef(const MEObject* owner,
                          const Reflection::MEObjectPtrProperty& objectPtrProperty,
                          void* propertyPtr);
        bool DrawArrayProperty(const Reflection::MEArrayProperty& arrayProperty, void* propertyPtr);

        void BeginRenameSelectedGameObject(const GameObject& gameObject)
        {
            m_IsRenamingSelectedGameObject = true;
            m_RenameTargetGameObjectId = gameObject.GetID();
            std::memset(m_RenameBuffer, 0, sizeof(m_RenameBuffer));
            std::strncpy(m_RenameBuffer, gameObject.GetName().c_str(), sizeof(m_RenameBuffer) - 1);
            m_RequestRenameFocus = true;
        }

        bool TryDrawComponentContextMenu(Component& component);

        SceneEditor& m_SceneEditor;
        static constexpr const char* kWindowTitle = "Inspector";
        std::string m_SelectedAddComponentTypeName;
        bool m_IsRenamingSelectedGameObject = false;
        bool m_RequestRenameFocus = false;
        uint64_t m_RenameTargetGameObjectId = kInvalidGameObjectId;
        char m_RenameBuffer[256] = {};
        std::unordered_map<uint32_t, std::vector<uint8_t>> m_PropertyUndoBeforeByEditId;
    };
}
