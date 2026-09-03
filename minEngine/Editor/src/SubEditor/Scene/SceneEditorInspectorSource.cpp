#include "SubEditor/Scene/SceneEditorInspectorSource.h"
#include "Runtime/Function/Physics/PhysicsEditorSideEffects.h"

#include "ContextMenu/Contexts/SceneInspectorMenuContext.h"
#include "ContextMenu/EditorContextMenuSystem.h"
#include "ContextMenu/EditorMenuContext.h"
#include "SubEditor/Scene/SceneEditor.h"
#include "Shell/IEditorContext.h"
#include "UI/Appearance/EditorAppearance.h"
#include "UI/Appearance/EditorThemeScope.h"
#include "UI/Appearance/EditorTypographyScope.h"
#include "UI/Appearance/EditorWindowTheme.h"
#include "UI/Appearance/EditorWindowTypography.h"
#include "UI/Property/ObjectPtrWidget.h"
#include "UI/Property/PropertyEditPolicy.h"
#include "UI/Property/PropertyValueWidget.h"
#include "UI/Property/TransformWidget.h"

#include "imgui.h"

#include <memory>

#include "Runtime/Function/Framework/Project/EditorTypographyRole.h"

#include "Runtime/Core/Object/MEObject.h"
#include "Runtime/Core/Object/ObjectManager.h"
#include "Runtime/Core/Serialization/Serializer.h"
#include "Runtime/Function/Framework/Components/SceneComponent.h"
#include "Runtime/Function/Physics/PhysicsEditorSideEffects.h"
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
        IEditorContext* editorContext = m_SceneEditor.GetEditorContext();
        if (editorContext == nullptr)
        {
            ImGui::Begin(kWindowTitle);
            ImGui::TextUnformatted("No selected GameObject.");
            ImGui::End();
            return;
        }

        const char* panelTitle = kWindowTitle;
        if (!EditorWindowTypography::BeginPanel(*editorContext, panelTitle))
        {
            return;
        }

        EditorTypographyScope bodyTypography(editorContext->GetEditorAppearance(), EditorTypographyRole::Body);

        if (editorContext->IsPlaying())
        {
            ImGui::TextDisabled("Inspecting: PIE");
        }

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
            StartInlineRename(*gameObject);
        }

        if (m_RenameTargetGameObjectId != gameObject->GetID())
        {
            m_IsRenamingSelectedGameObject = false;
        }

        EditorAppearance& appearance = editorContext->GetEditorAppearance();
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(12.0f, 7.0f));

        const std::string selectedName = m_SceneEditor.GetSelectedGameObjectName();
        if (m_IsRenamingSelectedGameObject && m_RenameTargetGameObjectId == gameObject->GetID())
        {
            EditorThemeScope renameFieldTheme = EditorWindowTheme::Field(appearance);

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

        }
        else
        {
            EditorThemeScope selectedHeaderTheme = EditorWindowTheme::SectionHeader(appearance);
            const std::string headerLabel = "  " + selectedName + "##SelectedGameObjectHeader";
            ImGui::Selectable(headerLabel.c_str(), true, ImGuiSelectableFlags_SpanAllColumns);
            if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
            {
                StartInlineRename(*gameObject);
            }

            if (ImGui::BeginPopupContextItem())
            {
                DrawGameObjectHeaderContextMenu(*gameObject);
                ImGui::EndPopup();
            }
        }

        ImGui::PopStyleVar();

        ImGui::Text("Selected ID: %llu", static_cast<unsigned long long>(gameObject->GetID()));

        ImGui::Spacing();

        // Add Component Section
        const std::vector<std::string> componentTypeNames = m_SceneEditor.GetAllComponentTypeNames();
        ImGui::PushID("InspectorAddComponent");
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
                    ImGui::PushID(typeName.c_str());
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
                    ImGui::PopID();
                }

                ImGui::EndCombo();
            }
            ImGui::PopItemWidth();

            ImGui::SameLine();
            if (ImGui::Button("Add Component##Button"))
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
        ImGui::PopID();

        Reflection::ReflectionSystem& reflectionSystem = Reflection::ReflectionSystem::Get();

        // GameObject's RootComponent Section
        if (SceneComponent* rootComponent = gameObject->GetRootComponent())
        {
            ImGui::Spacing();
            if (IEditorContext* editorContext = m_SceneEditor.GetEditorContext())
            {
                EditorTypographyScope sectionTypography(
                    editorContext->GetEditorAppearance(),
                    EditorTypographyRole::Subheading);
                ImGui::SeparatorText("Root Transform");
            }
            else
            {
                ImGui::SeparatorText("Root Transform");
            }

            const Reflection::MEClass* classInfo = rootComponent->GetClass();
            MEObject* rootComponentObject = static_cast<MEObject*>(rootComponent);
            if (classInfo && rootComponentObject)
            {
                Transform* transform = const_cast<Transform*>(&rootComponent->GetTransform());
                const PropertyUndoCaptureContext baseContext =
                    MakePropertyUndoCaptureContext(rootComponentObject, classInfo, "m_Transform");

                const ImGuiTreeNodeFlags treeFlags =
                    ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_FramePadding;

                const auto applyUndoForField =
                    [this, baseContext](std::string_view fieldName)
                    {
                        if (!baseContext.IsValid())
                        {
                            return;
                        }

                        PropertyUndoCaptureContext fieldContext = baseContext;
                        fieldContext.capturePropertyPath += ".";
                        fieldContext.capturePropertyPath += std::string(fieldName);
                        ApplyPropertyUndoCaptureHooks(fieldContext, true);
                    };

                EditorAppearance* appearance =
                    m_SceneEditor.GetEditorContext() != nullptr
                        ? &m_SceneEditor.GetEditorContext()->GetEditorAppearance()
                        : nullptr;
                const bool valueChanged =
                    TransformWidget::Draw(transform, treeFlags, applyUndoForField, appearance);
                if (valueChanged)
                {
                    rootComponent->MarkRenderStateDirty();
                }
            }
            else
            {
                ImGui::TextUnformatted("Root component type info missing.");
            }
        }

        // Components Section
        ImGui::Spacing();
        if (IEditorContext* editorContext = m_SceneEditor.GetEditorContext())
        {
            EditorTypographyScope sectionTypography(
                editorContext->GetEditorAppearance(),
                EditorTypographyRole::Subheading);
            ImGui::SeparatorText("Components");
        }
        else
        {
            ImGui::SeparatorText("Components");
        }

        // Right-click empty area in GO Inspector (Components section) to add components
        // via the shared context menu ActionProvider system.
        if (ImGui::BeginPopupContextWindow(
                "SceneInspectorComponentsBlankContextMenu",
                ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems))
        {
            DrawGameObjectHeaderContextMenu(*gameObject);
            ImGui::EndPopup();
        }

        SceneComponent* rootComponent = gameObject->GetRootComponent();
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

            IEditorContext* componentEditorContext = m_SceneEditor.GetEditorContext();
            EditorAppearance* appearance =
                componentEditorContext != nullptr ? &componentEditorContext->GetEditorAppearance() : nullptr;

            const std::string headerLabel = GetShortTypeName(classInfo->GetName()) + "##component_" +
                                            std::to_string(reinterpret_cast<uintptr_t>(component.get()));
            bool componentOpen = false;

            bool componentActive = component->IsActive();
            const bool inactiveStylePushed = !componentActive;
            const std::string activeCheckboxId =
                std::string("##component_active_") + std::to_string(reinterpret_cast<uintptr_t>(component.get()));
            const PropertyUndoCaptureContext activeUndoContext =
                MakePropertyUndoCaptureContext(component.get(), classInfo, "m_bActive");
            std::vector<uint8_t> activeBeforeBlob;
            const bool capturedActiveBefore = SerializePropertyUndoBlob(activeUndoContext, activeBeforeBlob);

            if (inactiveStylePushed)
            {
                ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.55f);
            }

            if (ImGui::Checkbox(activeCheckboxId.c_str(), &componentActive))
            {
                // Capture before blob prior to SetActive; checkbox toggles are same-frame
                // so IsItemActivated/IsItemDeactivatedAfterEdit hooks are unreliable here.
                component->SetActive(componentActive);
                m_SceneEditor.MarkSceneDirty();

                std::vector<uint8_t> activeAfterBlob;
                if (capturedActiveBefore && SerializePropertyUndoBlob(activeUndoContext, activeAfterBlob))
                {
                    TryPropertyUndoCommitImmediate(activeUndoContext, activeBeforeBlob, activeAfterBlob);
                }
            }

            ImGui::SameLine();
            {
                std::unique_ptr<EditorThemeScope> componentSectionTheme;
                std::unique_ptr<EditorTypographyScope> componentHeaderTypography;
                if (appearance != nullptr)
                {
                    componentSectionTheme = std::make_unique<EditorThemeScope>(
                        EditorWindowTheme::SectionHeader(*appearance));
                    componentHeaderTypography = std::make_unique<EditorTypographyScope>(
                        *appearance,
                        EditorTypographyRole::Subheading);
                }

                ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8.0f, 3.0f));
                componentOpen = ImGui::CollapsingHeader(headerLabel.c_str(), ImGuiTreeNodeFlags_DefaultOpen);
                ImGui::PopStyleVar();
            }

            if (inactiveStylePushed)
            {
                ImGui::PopStyleVar();
            }

            (void)TryDrawComponentContextMenu(*component);

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
            const bool isRootComponent = (rootComponent != nullptr && component.get() == rootComponent);
            reflectionSystem.ForEachPropertyInHierarchy(compClassName,
            [&](const Reflection::MEProperty& property) -> bool
            {
                hasAnyReflectedField = true;
                if (isRootComponent && property.GetName() == "m_Transform")
                {
                    return true;
                }
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
        if (IEditorContext* editorContext = m_SceneEditor.GetEditorContext();
            editorContext != nullptr && editorContext->IsPlaying())
        {
            return false;
        }

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
        const std::string& capturePropertyPath) const
    {
        PropertyUndoCaptureContext context;
        if (owner == nullptr || ownerClass == nullptr || capturePropertyPath.empty())
        {
            return context;
        }

        context.ownerGuid = owner->GetGuid();
        context.ownerClassName = ownerClass->GetName();
        context.capturePropertyPath = capturePropertyPath;
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

        const Serialization::SerializeResult result = Serialization::Serializer::SerializePropertyByPathToBuffer(
            ownerObject.get(),
            ownerClass,
            context.capturePropertyPath,
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
            context.capturePropertyPath,
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
        PropertyUndoCaptureContext nestedUndoContext;
        const PropertyUndoCaptureContext* activeUndoContext = parentUndoContext;

        if (activeUndoContext != nullptr && activeUndoContext->IsValid() && CanUndoInspectorProperty(property))
        {
            nestedUndoContext = *activeUndoContext;
            nestedUndoContext.capturePropertyPath += ".";
            nestedUndoContext.capturePropertyPath += property.GetName();
            activeUndoContext = &nestedUndoContext;
        }

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
                    valueChanged = DrawObjectProperty(owner, ownerClass, objectProperty, propertyPtr, activeUndoContext);
                }

                break;
            }
            case Reflection::MEPropertyCategory::ObjectPtr:
                valueChanged = DrawObjectPtrProperty(
                    owner,
                    ownerClass,
                    static_cast<const Reflection::MEObjectPtrProperty&>(property),
                    propertyPtr,
                    activeUndoContext);
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

        if (valueChanged && owner != nullptr)
        {
            ApplyPhysicsEditorSideEffects(const_cast<MEObject*>(owner), property.GetName());
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
                                                        void* propertyPtr,
                                                        const PropertyUndoCaptureContext* objectUndoContext)
    {
        Reflection::MEClass* valueClass = objectProperty.GetValueClass();
        if (!valueClass)
        {
            return false;
        }

        if (objectUndoContext == nullptr || !objectUndoContext->IsValid())
        {
            // Without a valid path context, we can't provide nested per-field undo.
            // Still draw the nested object fields.
        }

        const Reflection::MEClass* transformClass = Reflection::ReflectionSystem::Get().FindClass<Transform>();

        bool valueChanged = false;

        const ImGuiTreeNodeFlags treeFlags =
            ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_FramePadding;

        if (transformClass != nullptr && valueClass->IsA(transformClass))
        {
            Transform* transform = static_cast<Transform*>(propertyPtr);
            const std::function<void(std::string_view)> applyUndoForField = [this, objectUndoContext](std::string_view fieldName)
            {
                if (objectUndoContext == nullptr || !objectUndoContext->IsValid())
                {
                    return;
                }

                PropertyUndoCaptureContext fieldContext = *objectUndoContext;
                fieldContext.capturePropertyPath += ".";
                fieldContext.capturePropertyPath += std::string(fieldName);
                ApplyPropertyUndoCaptureHooks(fieldContext, true);
            };

            EditorAppearance* appearance =
                m_SceneEditor.GetEditorContext() != nullptr
                    ? &m_SceneEditor.GetEditorContext()->GetEditorAppearance()
                    : nullptr;
            valueChanged |= TransformWidget::Draw(transform, treeFlags, applyUndoForField, appearance);
            return valueChanged;
        }

        const bool open = ImGui::TreeNodeEx("##StructTree", treeFlags, "%s", valueClass->GetName().c_str());
        if (open)
        {
            const std::string tableId = std::string("##NestedTable_") + valueClass->GetName();
            if (ImGui::BeginTable(tableId.c_str(), 2, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_RowBg))
            {
                ImGui::TableSetupColumn("Property", ImGuiTableColumnFlags_WidthStretch, 0.35f);
                ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch, 0.65f);

                Reflection::ReflectionSystem& reflectionSystem = Reflection::ReflectionSystem::Get();
                reflectionSystem.ForEachPropertyInHierarchy(
                    valueClass->GetName(),
                    [&](const Reflection::MEProperty& nestedProperty) -> bool
                    {
                        void* valuePtr = nestedProperty.GetMutable(propertyPtr);
                        valueChanged |= DrawProperty(owner, ownerClass, nestedProperty, valuePtr, objectUndoContext);
                        return true;
                    });

                ImGui::EndTable();
            }

            ImGui::TreePop();
        }

        return valueChanged;
    }

    bool SceneEditorInspectorSource::DrawObjectPtrProperty(const MEObject* owner,
                                                           const Reflection::MEClass* ownerClass,
                                                           const Reflection::MEObjectPtrProperty& objectPtrProperty,
                                                           void* propertyPtr,
                                                           const PropertyUndoCaptureContext* undoContext)
    {
        const std::string assetUndoKey = MakeAssetPropertyUndoKey(
            (undoContext && undoContext->IsValid()) ? undoContext->ownerGuid : GUID::Zero(),
            objectPtrProperty.GetName());

        ObjectPtrWidgetHooks hooks;
        hooks.OnComboActivated = [this, undoContext, assetUndoKey]()
        {
            if (undoContext == nullptr || !undoContext->IsValid())
            {
                return;
            }

            std::vector<uint8_t> beforeBlob;
            if (SerializePropertyUndoBlob(*undoContext, beforeBlob))
            {
                m_AssetPropertyUndoBeforeByKey[assetUndoKey] = std::move(beforeBlob);
            }
        };
        hooks.OnComboDeactivatedAfterEdit = [this, undoContext, assetUndoKey]()
        {
            if (undoContext != nullptr && undoContext->IsValid())
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
            if (!selectionChanged || undoContext == nullptr || !undoContext->IsValid())
            {
                return;
            }

            std::vector<uint8_t> beforeBlob;
            const auto beforeIter = m_AssetPropertyUndoBeforeByKey.find(assetUndoKey);
            if (beforeIter != m_AssetPropertyUndoBeforeByKey.end())
            {
                beforeBlob = beforeIter->second;
            }
                else if (!SerializePropertyUndoBlob(*undoContext, beforeBlob))
            {
                beforeBlob.clear();
            }

            std::vector<uint8_t> afterBlob;
                if (!beforeBlob.empty() && SerializePropertyUndoBlob(*undoContext, afterBlob))
            {
                    TryPropertyUndoCommitImmediate(*undoContext, beforeBlob, afterBlob);
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

    void SceneEditorInspectorSource::StartInlineRename(const GameObject& gameObject)
    {
        m_IsRenamingSelectedGameObject = true;
        m_RenameTargetGameObjectId = gameObject.GetID();
        std::memset(m_RenameBuffer, 0, sizeof(m_RenameBuffer));
        std::strncpy(m_RenameBuffer, gameObject.GetName().c_str(), sizeof(m_RenameBuffer) - 1);
        m_RequestRenameFocus = true;
    }

    void SceneEditorInspectorSource::DrawGameObjectHeaderContextMenu(GameObject& gameObject)
    {
        IEditorContext* editorContext = m_SceneEditor.GetEditorContext();
        if (!editorContext)
        {
            return;
        }

        auto inspectorContext = std::make_shared<SceneInspectorMenuContext>();
        inspectorContext->SelectionKind = SceneInspectorSelectionKind::GameObjectHeader;
        inspectorContext->GameObjectId = gameObject.GetID();

        EditorMenuContext menuContext;
        menuContext.Add(inspectorContext);
        editorContext->GetContextMenu().BuildAndDraw(*editorContext, menuContext);
    }

    void SceneEditorInspectorSource::DrawComponentContextMenu(Component& component)
    {
        IEditorContext* editorContext = m_SceneEditor.GetEditorContext();
        GameObject* owner = component.GetOwner();
        if (!editorContext || !owner)
        {
            return;
        }

        auto inspectorContext = std::make_shared<SceneInspectorMenuContext>();
        inspectorContext->SelectionKind = SceneInspectorSelectionKind::Component;
        inspectorContext->GameObjectId = owner->GetID();
        inspectorContext->HoveredComponent = &component;

        EditorMenuContext menuContext;
        menuContext.Add(inspectorContext);
        editorContext->GetContextMenu().BuildAndDraw(*editorContext, menuContext);
    }

    bool SceneEditorInspectorSource::TryDrawComponentContextMenu(Component& component)
    {
        if (ImGui::BeginPopupContextItem(("ComponentContextMenu##" + std::to_string(reinterpret_cast<uintptr_t>(&component))).c_str()))
        {
            DrawComponentContextMenu(component);
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