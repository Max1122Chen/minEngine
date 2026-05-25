#include "Scene/SceneEditorInspectorSource.h"

#include "Scene/SceneEditor.h"
#include "Shell/IEditorContext.h"
#include "UI/Property/ObjectPtrWidget.h"
#include "UI/Property/PropertyEditPolicy.h"
#include "UI/Property/PropertyValueWidget.h"

#include "imgui.h"

#include "Runtime/Core/Object/MEObject.h"
#include "Runtime/Core/Object/ObjectManager.h"
#include "Runtime/Core/Serialization/Serializer.h"
#include "Runtime/Function/Framework/Components/SceneComponent.h"
#include "Runtime/Function/Framework/Components/StaticMeshComponent.h"
#include "Runtime/Function/Render/Material.h"
#include "Runtime/Function/Render/StaticMesh.h"
#include "Runtime/Resource/AssetManager.h"

namespace minEngine
{
    SceneEditorInspectorSource::SceneEditorInspectorSource(SceneEditor& sceneEditor)
        : m_SceneEditor(sceneEditor)
        , m_PropertyEditSession(PropertyEditSession::ForSceneEditor(sceneEditor))
    {
    }

    bool SceneEditorInspectorSource::HasInspectableSelection() const
    {
        return m_SceneEditor.HasSelectedGameObject();
    }

    void SceneEditorInspectorSource::DrawInspector()
    {
        DrawGameObjectDetails(m_SceneEditor.GetSelectedGameObject());
    }

    void SceneEditorInspectorSource::DrawGameObjectDetails(GameObject* gameObject)
    {
        ImGui::Begin(kWindowTitle);

        if (!gameObject)
        {
            ImGui::TextUnformatted("No selected GameObject.");
            ImGui::End();
            return;
        }

        const bool requestRenameByHotkey = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)
            && ImGui::IsKeyPressed(ImGuiKey_F2, false);
        if (requestRenameByHotkey)
        {
            BeginRenameSelectedGameObject(*gameObject);
        }

        if (m_RenameTargetGameObjectId != gameObject->GetID())
        {
            m_IsRenamingSelectedGameObject = false;
        }

        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(12.0f, 7.0f));
        ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.21f, 0.31f, 0.45f, 0.95f));
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.25f, 0.37f, 0.53f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0.23f, 0.34f, 0.49f, 1.0f));

        const std::string selectedName = m_SceneEditor.GetSelectedGameObjectName();
        if (m_IsRenamingSelectedGameObject && m_RenameTargetGameObjectId == gameObject->GetID())
        {
            ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.18f, 0.27f, 0.40f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.23f, 0.34f, 0.49f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(0.21f, 0.31f, 0.45f, 1.0f));

            ImGui::SetNextItemWidth(-FLT_MIN);
            if (m_RequestRenameFocus)
            {
                ImGui::SetKeyboardFocusHere();
                m_RequestRenameFocus = false;
            }

            const bool committed = ImGui::InputText("##SelectedGameObjectRename",
                m_RenameBuffer,
                sizeof(m_RenameBuffer),
                ImGuiInputTextFlags_AutoSelectAll | ImGuiInputTextFlags_EnterReturnsTrue);

            if (committed || ImGui::IsItemDeactivatedAfterEdit())
            {
                if (IEditorContext* context = m_SceneEditor.GetEditorContext())
                {
                    m_SceneEditor.SubmitRenameGameObject(
                        *context, gameObject->GetID(), m_RenameBuffer);
                }
                m_IsRenamingSelectedGameObject = false;
            }
            else if (ImGui::IsKeyPressed(ImGuiKey_Escape, false))
            {
                m_IsRenamingSelectedGameObject = false;
            }

            ImGui::PopStyleColor(3);
        }
        else
        {
            const std::string headerLabel = "  " + selectedName + "##SelectedGameObjectHeader";
            ImGui::Selectable(headerLabel.c_str(), true, ImGuiSelectableFlags_SpanAllColumns);
            if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
            {
                BeginRenameSelectedGameObject(*gameObject);
            }
        }

        ImGui::PopStyleColor(3);
        ImGui::PopStyleVar();

        ImGui::Text("Selected ID: %llu", static_cast<unsigned long long>(gameObject->GetID()));

        ImGui::Spacing();

        // Add Component Section
        const std::vector<std::string> componentTypeNames = m_SceneEditor.GetAllComponentTypeNames();
        ImGui::SeparatorText("Add Component");
        if (!componentTypeNames.empty())
        {
            if (std::find(componentTypeNames.begin(), componentTypeNames.end(), m_SelectedAddComponentTypeName) == componentTypeNames.end())
            {
                m_SelectedAddComponentTypeName = componentTypeNames.front();
            }

            ImGui::PushItemWidth(260.0f);
            const std::string selectedDisplayName = GetShortTypeName(m_SelectedAddComponentTypeName);
            if (ImGui::BeginCombo("##AddComponentCombo", selectedDisplayName.c_str()))
            {
                for (const std::string& typeName : componentTypeNames)
                {
                    const bool isSelected = (typeName == m_SelectedAddComponentTypeName);
                    const std::string displayName = GetShortTypeName(typeName);
                    if (ImGui::Selectable(displayName.c_str(), isSelected))
                    {
                        m_SelectedAddComponentTypeName = typeName;
                    }

                    if (isSelected)
                    {
                        ImGui::SetItemDefaultFocus();
                    }
                }

                ImGui::EndCombo();
            }
            ImGui::PopItemWidth();

            ImGui::SameLine();
            if (ImGui::Button("Add Component"))
            {
                if (IEditorContext* context = m_SceneEditor.GetEditorContext())
                {
                    m_SceneEditor.SubmitAddComponentToSelectedGameObject(*context, m_SelectedAddComponentTypeName);
                }
            }
        }
        else
        {
            ImGui::TextUnformatted("No reflected Component derived types found.");
        }

        Reflection::ReflectionSystem& reflectionSystem = Reflection::ReflectionSystem::Get();

        // GameObject's RootComponent Section
        
        if (gameObject->GetRootComponent())
        {
            ImGui::Spacing();
            ImGui::SeparatorText("Root Transform");
            std::string tableId = "RootComponentTable##" + std::to_string(gameObject->GetID());
            if (ImGui::BeginTable(tableId.c_str(), 2, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_RowBg))
            {
                ImGui::TableSetupColumn("Property", ImGuiTableColumnFlags_WidthStretch, 0.35f);
                ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch, 0.65f);
                
                SceneComponent* rootComponent = gameObject->GetRootComponent();
                const Reflection::MEClass* classInfo = rootComponent->GetClass();
                MEObject* rootComponentObject = static_cast<MEObject*>(rootComponent);
                bool valueChanged = false;
                if (classInfo && rootComponentObject)
                {
                    reflectionSystem.ForEachPropertyInHierarchy(classInfo->GetName(),
                    [&](const Reflection::MEProperty& property) -> bool
                    {
                        if(property.GetName() == "m_Transform")
                        {
                            void* valuePtr = property.GetMutable(rootComponentObject);
                            valueChanged |= DrawProperty(rootComponentObject, classInfo, property, valuePtr);
                        }
                        return true;
                    });
                    if(valueChanged)
                    {
                        rootComponent->MarkRenderStateDirty(); // Mark render state dirty to ensure changes are reflected in the editor viewport
                    }
                }
                else
                {
                    ImGui::TextUnformatted("Root component type info missing.");
                }
                ImGui::EndTable();
            }
            
        }

        // Components Section
        ImGui::Spacing();
        ImGui::SeparatorText("Components");

        for (const std::shared_ptr<Component>& component : gameObject->GetAllComponents())
        {
            if (!component)
            {
                continue;
            }

            const Reflection::MEClass* classInfo = component->GetClass();
            if (classInfo == nullptr)
            {
                ImGui::TextUnformatted("Component type info missing.");
                continue;
            }

            ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.18f, 0.26f, 0.36f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.23f, 0.33f, 0.46f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0.20f, 0.30f, 0.42f, 1.0f));
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8.0f, 3.0f));
            const std::string headerLabel = GetShortTypeName(classInfo->GetName()) + "##component_" + std::to_string(reinterpret_cast<uintptr_t>(component.get()));
            const bool componentOpen = ImGui::CollapsingHeader(headerLabel.c_str(), ImGuiTreeNodeFlags_DefaultOpen);
            ImGui::PopStyleVar();
            ImGui::PopStyleColor(3);
            if (!componentOpen)
            {
                continue;
            }

            const std::string tableId = "ComponentTable##" + std::to_string(reinterpret_cast<uintptr_t>(component.get()));
            if (!ImGui::BeginTable(tableId.c_str(), 2, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_RowBg))
            {
                continue;
            }

            ImGui::TableSetupColumn("Property", ImGuiTableColumnFlags_WidthStretch, 0.35f);
            ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch, 0.65f);
            bool hasAnyReflectedField = false;
            MEObject* componentObject = static_cast<MEObject*>(component.get());
            if (componentObject == nullptr)
            {
                ImGui::EndTable();
                continue;
            }
            const Reflection::MEClass* compClass = componentObject->GetClass();
            if (compClass == nullptr)
            {
                ImGui::TextUnformatted("Component class info missing.");
                ImGui::EndTable();
                continue;
            }
            const std::string& compClassName = compClass->GetName();
            bool valueChanged = false;
            reflectionSystem.ForEachPropertyInHierarchy(compClassName,
            [&](const Reflection::MEProperty& property) -> bool
            {
                hasAnyReflectedField = true;
                void* valuePtr = property.GetMutable(componentObject);
                valueChanged |= DrawProperty(componentObject, compClass, property, valuePtr);
                return true;
            });
            if(valueChanged)
            {
                if(compClass->IsA(SceneComponent::StaticClass()))
                {
                    SceneComponent* sceneComponent = static_cast<SceneComponent*>(componentObject);
                    sceneComponent->MarkRenderStateDirty(); // Mark render state dirty to ensure changes are reflected in the editor viewport
                }
            }

            TryDrawComponentContextMenu(*component);
            
            if (!hasAnyReflectedField)
            {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::TextUnformatted("Info");
                ImGui::TableSetColumnIndex(1);
                ImGui::TextUnformatted("No reflected fields.");
            }

            ImGui::EndTable();
        }

        ImGui::End();
    }


    bool SceneEditorInspectorSource::CanUndoInspectorProperty(const Reflection::MEProperty& property) const
    {
        if (!PropertyEditPolicy::CanEdit(property, m_PropertyEditSession.ContextKind))
        {
            return false;
        }

        switch (property.GetCategory())
        {
        case Reflection::MEPropertyCategory::Primitive:
        case Reflection::MEPropertyCategory::Object:
        case Reflection::MEPropertyCategory::ObjectPtr:
            return true;
        case Reflection::MEPropertyCategory::Array:
        default:
            return false;
        }
    }

    PropertyUndoCaptureContext SceneEditorInspectorSource::MakePropertyUndoCaptureContext(
        const MEObject* owner,
        const Reflection::MEClass* ownerClass,
        const std::string& capturePropertyName) const
    {
        PropertyUndoCaptureContext context;
        if (owner == nullptr || ownerClass == nullptr || capturePropertyName.empty())
        {
            return context;
        }

        context.ownerGuid = owner->GetGuid();
        context.ownerClassName = ownerClass->GetName();
        context.capturePropertyName = capturePropertyName;
        return context;
    }

    bool SceneEditorInspectorSource::SerializePropertyUndoBlob(const PropertyUndoCaptureContext& context,
                                                                 std::vector<uint8_t>& outBlob) const
    {
        outBlob.clear();
        if (!context.IsValid())
        {
            return false;
        }

        std::shared_ptr<MEObject> ownerObject = ObjectManager::Get().FindObject(context.ownerGuid);
        if (!ownerObject)
        {
            return false;
        }

        const Reflection::MEClass* ownerClass = Reflection::ReflectionSystem::Get().FindClass(context.ownerClassName);
        if (ownerClass == nullptr)
        {
            return false;
        }

        const Serialization::SerializeResult result = Serialization::Serializer::SerializePropertyToBuffer(
            ownerObject.get(),
            ownerClass,
            context.capturePropertyName,
            outBlob,
            m_SceneEditor.GetPropertyCommandSerializerOptions());
        return result.ok;
    }

    void SceneEditorInspectorSource::TryPropertyUndoCommitImmediate(const PropertyUndoCaptureContext& context,
                                                                    const std::vector<uint8_t>& beforeBlob,
                                                                    const std::vector<uint8_t>& afterBlob)
    {
        if (!context.IsValid() || beforeBlob.empty() || afterBlob == beforeBlob)
        {
            return;
        }

        IEditorContext* editorContext = m_SceneEditor.GetEditorContext();
        if (editorContext == nullptr)
        {
            return;
        }

        m_SceneEditor.SubmitSetObjectProperty(
            *editorContext,
            context.ownerGuid,
            context.ownerClassName,
            context.capturePropertyName,
            beforeBlob,
            afterBlob);
    }

    void SceneEditorInspectorSource::TryPropertyUndoActivated(const PropertyUndoCaptureContext& context, uint32_t editId)
    {
        if (!context.IsValid() || !ImGui::IsItemActivated())
        {
            return;
        }

        std::vector<uint8_t> beforeBlob;
        if (!SerializePropertyUndoBlob(context, beforeBlob))
        {
            return;
        }

        m_PropertyUndoBeforeByEditId[editId] = std::move(beforeBlob);
    }

    void SceneEditorInspectorSource::TryPropertyUndoCommitAfterEdit(const PropertyUndoCaptureContext& context,
                                                                    uint32_t editId)
    {
        if (!context.IsValid() || !ImGui::IsItemDeactivatedAfterEdit())
        {
            return;
        }

        const auto beforeIter = m_PropertyUndoBeforeByEditId.find(editId);
        if (beforeIter == m_PropertyUndoBeforeByEditId.end())
        {
            return;
        }

        std::vector<uint8_t> beforeBlob = std::move(beforeIter->second);
        m_PropertyUndoBeforeByEditId.erase(beforeIter);

        std::vector<uint8_t> afterBlob;
        if (!SerializePropertyUndoBlob(context, afterBlob))
        {
            return;
        }

        TryPropertyUndoCommitImmediate(context, beforeBlob, afterBlob);
    }

    void SceneEditorInspectorSource::ApplyPropertyUndoCaptureHooks(const PropertyUndoCaptureContext& context,
                                                                   bool allowRowCapture)
    {
        if (!allowRowCapture || !context.IsValid() || !ImGui::GetItemID())
        {
            return;
        }

        const uint32_t editId = static_cast<uint32_t>(ImGui::GetItemID());
        TryPropertyUndoActivated(context, editId);
        TryPropertyUndoCommitAfterEdit(context, editId);
    }

    std::string SceneEditorInspectorSource::MakeAssetPropertyUndoKey(const GUID& ownerGuid,
                                                                     const std::string& propertyName)
    {
        return ownerGuid.ToString() + "|" + propertyName;
    }

    bool SceneEditorInspectorSource::DrawProperty(const MEObject* owner,
                                                  const Reflection::MEClass* ownerClass,
                                                  const Reflection::MEProperty& property,
                                                  void* propertyPtr,
                                                  const PropertyUndoCaptureContext* parentUndoContext)
    {
        if (!PropertyEditPolicy::ShouldShow(property, m_PropertyEditSession.ContextKind))
        {
            return false;
        }

        ImGui::PushID(property.GetName().c_str());
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::TextUnformatted(PropertyEditPolicy::GetDisplayName(property));
        ImGui::TableSetColumnIndex(1);

        const bool canEdit = PropertyEditPolicy::CanEdit(property, m_PropertyEditSession.ContextKind);
        if (!canEdit)
        {
            ImGui::BeginDisabled();
        }

        PropertyUndoCaptureContext localUndoContext;
        const PropertyUndoCaptureContext* activeUndoContext = parentUndoContext;
        if (activeUndoContext == nullptr && owner != nullptr && ownerClass != nullptr
            && CanUndoInspectorProperty(property))
        {
            localUndoContext = MakePropertyUndoCaptureContext(owner, ownerClass, property.GetName());
            activeUndoContext = &localUndoContext;
        }

        bool valueChanged = false;
        const bool allowRowCapture = (property.GetCategory() != Reflection::MEPropertyCategory::Object
            && property.GetCategory() != Reflection::MEPropertyCategory::ObjectPtr);

        switch(property.GetCategory())
        {
            case Reflection::MEPropertyCategory::Primitive:
                valueChanged = DrawPrimitiveProperty(static_cast<const Reflection::MEPrimitiveProperty&>(property), propertyPtr);
                break;
            case Reflection::MEPropertyCategory::Object:
            {
                const Reflection::MEObjectProperty& objectProperty =
                    static_cast<const Reflection::MEObjectProperty&>(property);
                if (PropertyValueWidget::Draw(property, propertyPtr, -FLT_MIN))
                {
                    m_SceneEditor.MarkSceneDirty();
                    valueChanged = true;
                }
                else
                {
                    valueChanged = DrawObjectProperty(owner, ownerClass, objectProperty, propertyPtr);
                }

                break;
            }
            case Reflection::MEPropertyCategory::ObjectPtr:
                valueChanged = DrawObjectPtrProperty(
                    owner,
                    ownerClass,
                    static_cast<const Reflection::MEObjectPtrProperty&>(property),
                    propertyPtr);
                break;
            case Reflection::MEPropertyCategory::Array:
                valueChanged = DrawArrayProperty(static_cast<const Reflection::MEArrayProperty&>(property), propertyPtr);
                break;
            default:
                break;
        }

        if (activeUndoContext != nullptr)
        {
            ApplyPropertyUndoCaptureHooks(*activeUndoContext, allowRowCapture);
        }

        if (!canEdit)
        {
            ImGui::EndDisabled();
        }

        if (const char* tooltip = PropertyEditPolicy::GetTooltip(property))
        {
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
            {
                ImGui::SetTooltip("%s", tooltip);
            }
        }

        ImGui::PopID();
        return valueChanged;
    }

    bool SceneEditorInspectorSource::DrawPrimitiveProperty(const Reflection::MEPrimitiveProperty& primitiveProperty,
                                                           void* propertyPtr)
    {
        if (PropertyValueWidget::Draw(primitiveProperty, propertyPtr, -FLT_MIN))
        {
            m_SceneEditor.MarkSceneDirty();
            return true;
        }

        return false;
    }

    bool SceneEditorInspectorSource::DrawObjectProperty(const MEObject* owner,
                                                        const Reflection::MEClass* ownerClass,
                                                        const Reflection::MEObjectProperty& objectProperty,
                                                        void* propertyPtr)
    {
        Reflection::MEClass* valueClass = objectProperty.GetValueClass();
        if (!valueClass)
        {
            return false;
        }

        const PropertyUndoCaptureContext objectUndoContext =
            MakePropertyUndoCaptureContext(owner, ownerClass, objectProperty.GetName());

        bool valueChanged = false;
        Reflection::ReflectionSystem& reflectionSystem = Reflection::ReflectionSystem::Get();
        reflectionSystem.ForEachPropertyInHierarchy(
            valueClass->GetName(),
            [&](const Reflection::MEProperty& property) -> bool
            {
                void* valuePtr = property.GetMutable(propertyPtr);
                const PropertyUndoCaptureContext* nestedContext =
                    objectUndoContext.IsValid() ? &objectUndoContext : nullptr;
                valueChanged |= DrawProperty(owner, ownerClass, property, valuePtr, nestedContext);
                return true;
            });
        return valueChanged;
    }

    bool SceneEditorInspectorSource::DrawObjectPtrProperty(const MEObject* owner,
                                                           const Reflection::MEClass* ownerClass,
                                                           const Reflection::MEObjectPtrProperty& objectPtrProperty,
                                                           void* propertyPtr)
    {
        const PropertyUndoCaptureContext undoContext =
            MakePropertyUndoCaptureContext(owner, ownerClass, objectPtrProperty.GetName());
        const std::string assetUndoKey = MakeAssetPropertyUndoKey(
            undoContext.IsValid() ? undoContext.ownerGuid : GUID::Zero(),
            objectPtrProperty.GetName());

        ObjectPtrWidgetHooks hooks;
        hooks.OnComboActivated = [this, undoContext, assetUndoKey]()
        {
            if (!undoContext.IsValid())
            {
                return;
            }

            std::vector<uint8_t> beforeBlob;
            if (SerializePropertyUndoBlob(undoContext, beforeBlob))
            {
                m_AssetPropertyUndoBeforeByKey[assetUndoKey] = std::move(beforeBlob);
            }
        };
        hooks.OnComboDeactivatedAfterEdit = [this, undoContext, assetUndoKey]()
        {
            if (undoContext.IsValid())
            {
                m_AssetPropertyUndoBeforeByKey.erase(assetUndoKey);
            }
        };
        hooks.TryApplySelection = [owner, ownerClass, &objectPtrProperty, propertyPtr](
                                      const PropertyRefCandidate& selected) -> bool
        {
            if (selected.Kind != PropertyRefCandidateKind::AssetMeta || selected.Meta == nullptr)
            {
                return false;
            }

            std::string errorMessage;
            const std::shared_ptr<Asset> asset =
                AssetManager::Get().LoadAssetByPath(selected.Meta->AssetPath, errorMessage);
            if (!asset)
            {
                ME_CORE_ERROR(
                    "ObjectPtrWidget: failed to load asset '{}' for property '{}': {}",
                    selected.Meta->AssetPath,
                    objectPtrProperty.GetName(),
                    errorMessage);
                return false;
            }

            if (objectPtrProperty.GetPtrCategory() != Reflection::MEObjectPtrCategory::Shared)
            {
                return false;
            }

            const Reflection::MEClass* valueClass = objectPtrProperty.GetValueClass();
            if (owner != nullptr && valueClass != nullptr)
            {
                if (StaticMeshComponent* meshComponent =
                        dynamic_cast<StaticMeshComponent*>(const_cast<MEObject*>(owner)))
                {
                    if (objectPtrProperty.GetName() == "m_Mesh")
                    {
                        meshComponent->SetMesh(std::static_pointer_cast<StaticMesh>(asset));
                        return true;
                    }

                    if (objectPtrProperty.GetName() == "m_Material")
                    {
                        meshComponent->SetMaterial(std::static_pointer_cast<Material>(asset));
                        return true;
                    }
                }
            }

            return false;
        };
        hooks.OnSelectionCommitted = [this, undoContext, assetUndoKey](bool selectionChanged)
        {
            if (!selectionChanged || !undoContext.IsValid())
            {
                return;
            }

            std::vector<uint8_t> beforeBlob;
            const auto beforeIter = m_AssetPropertyUndoBeforeByKey.find(assetUndoKey);
            if (beforeIter != m_AssetPropertyUndoBeforeByKey.end())
            {
                beforeBlob = beforeIter->second;
            }
            else if (!SerializePropertyUndoBlob(undoContext, beforeBlob))
            {
                beforeBlob.clear();
            }

            std::vector<uint8_t> afterBlob;
            if (!beforeBlob.empty() && SerializePropertyUndoBlob(undoContext, afterBlob))
            {
                TryPropertyUndoCommitImmediate(undoContext, beforeBlob, afterBlob);
            }

            m_AssetPropertyUndoBeforeByKey.erase(assetUndoKey);
        };
        hooks.OnMarkDirty = [this]() { m_SceneEditor.MarkSceneDirty(); };
        hooks.OnRenderStateDirty = [owner]()
        {
            if (owner == nullptr)
            {
                return;
            }

            if (SceneComponent* sceneComponent = dynamic_cast<SceneComponent*>(const_cast<MEObject*>(owner)))
            {
                sceneComponent->MarkRenderStateDirty();
            }
        };

        return ObjectPtrWidget::Draw(
            objectPtrProperty,
            propertyPtr,
            m_PropertyEditSession,
            hooks,
            260.0f);
    }

    bool SceneEditorInspectorSource::DrawArrayProperty(const Reflection::MEArrayProperty& arrayProperty, void* propertyPtr)
    {
        ImGui::TextUnformatted("Array properties are not supported in this version.");
        return false;
    }

    bool SceneEditorInspectorSource::TryDrawComponentContextMenu(Component &component)
    {
        if (ImGui::BeginPopupContextItem(("ComponentContextMenu##" + std::to_string(reinterpret_cast<uintptr_t>(&component))).c_str()))
        {
            if (ImGui::MenuItem("Remove"))
            {
                if (IEditorContext* context = m_SceneEditor.GetEditorContext())
                {
                    m_SceneEditor.SubmitRemoveComponentFromGO(*context, *component.GetOwner(), component);
                }
            }
            ImGui::EndPopup();
            return true;
         }
         return false;
    }

    std::string SceneEditorInspectorSource::GetShortTypeName(const std::string& fullTypeName)
    {
        const size_t scopePos = fullTypeName.rfind("::");
        if (scopePos == std::string::npos)
        {
            return fullTypeName;
        }

        return fullTypeName.substr(scopePos + 2);
    }
}