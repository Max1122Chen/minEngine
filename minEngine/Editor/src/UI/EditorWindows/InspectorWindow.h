#pragma once

#include "Core.h"

#include "imgui.h"

#include "Editor.h"
#include "EditorWindow.h"

#include "Runtime/Core/Reflection/Reflection.h"
#include "Runtime/Function/Framework/GameObject/GameObject.h"
#include "Runtime/Function/Framework/Components/Component.h"

#include <algorithm>
#include <cfloat>
#include <cstring>
#include <limits>

namespace minEngine
{
    

    class InspectorWindow final : public EditorWindow
    {
    public:
        explicit InspectorWindow(Editor& editor)
            : EditorWindow(editor)
        {
        }

        const std::string& GetId() const override
        {
            return m_Id;
        }

        const std::string& GetTitle() const override
        {
            return m_Title;
        }

        void OnDraw() override;

    private:
        std::string GetShortTypeName(const std::string& fullTypeName);
        
        static constexpr uint64_t kInvalidGameObjectId = std::numeric_limits<uint64_t>::max();

        void DrawGameObjectDetails(GameObject* gameObject);

        bool DrawProperty(const Reflection::MEProperty& property, void* propertyPtr);

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

        bool DrawObjectProperty(const Reflection::MEObjectProperty& objectProperty, void* propertyPtr);
        bool DrawObjectPtrProperty(const Reflection::MEObjectPtrProperty& objectPtrProperty, void* propertyPtr);
        bool DrawArrayProperty(const Reflection::MEArrayProperty& arrayProperty, void* propertyPtr);

        void BeginRenameSelectedGameObject(const GameObject& gameObject)
        {
            m_IsRenamingSelectedGameObject = true;
            m_RenameTargetGameObjectId = gameObject.GetID();
            std::memset(m_RenameBuffer, 0, sizeof(m_RenameBuffer));
            std::strncpy(m_RenameBuffer, gameObject.GetName().c_str(), sizeof(m_RenameBuffer) - 1);
            m_RequestRenameFocus = true;
        }

        const std::string m_Id = "inspector";
        const std::string m_Title = "Inspector";
        std::string m_SelectedAddComponentTypeName;
        bool m_IsRenamingSelectedGameObject = false;
        bool m_RequestRenameFocus = false;
        uint64_t m_RenameTargetGameObjectId = kInvalidGameObjectId;
        char m_RenameBuffer[256] = {};
    };
}
