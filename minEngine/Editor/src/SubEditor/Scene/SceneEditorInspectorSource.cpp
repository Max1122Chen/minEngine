#include "SubEditor/Scene/SceneEditorInspectorSource.h"

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
#include "UI/Widgets/InlineRenameField.h"
#include "Services/AddComponentPicker.h"
#include "Services/ComponentTypeUiCatalog.h"

#include "imgui.h"

#include <memory>

#include "Runtime/Function/Framework/Project/EditorTypographyRole.h"

#include "Runtime/Core/Object/MEObject.h"
#include "Runtime/Core/Object/ObjectManager.h"
#include "Runtime/Core/Reflection/PropertyAssign.h"
#include "Runtime/Core/Serialization/Serializer.h"
#include "Runtime/Function/Framework/Components/SceneComponent.h"
#include "Runtime/Function/Framework/Components/StaticMeshComponent.h"
#include "Runtime/Function/Framework/Components/SkeletalMeshComponent.h"
#include "Runtime/Function/Render/MaterialInterface.h"
#include "Runtime/Function/Render/StaticMesh.h"
#include "Runtime/Function/Render/SkeletalMesh.h"
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
            const InlineRenameField::Result renameResult =
                InlineRenameField::Draw(m_RenameBuffer, sizeof(m_RenameBuffer), m_RequestRenameFocus);

            if (renameResult == InlineRenameField::Result::Commit)
            {
                if (IEditorContext* context = m_SceneEditor.GetEditorContext())
                {
                    m_SceneEditor.SubmitRenameGameObject(
                        *context, gameObject->GetID(), m_RenameBuffer);
                }
                m_IsRenamingSelectedGameObject = false;
            }
            else if (renameResult == InlineRenameField::Result::Cancel)
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

        DrawGameObjectStatusSwitches(*gameObject);

        ImGui::Text("Selected ID: %llu", static_cast<unsigned long long>(gameObject->GetID()));

        ImGui::Spacing();

        // Add Component Section
        ImGui::PushID("InspectorAddComponent");
        ImGui::SeparatorText("Add Component");
        if (IEditorContext* context = m_SceneEditor.GetEditorContext())
        {
            AddComponentPicker::DrawInspectorAddSection(
                *context,
                m_SceneEditor,
                m_SelectedAddComponentTypeName,
                m_AddComponentFilterBuffer,
                sizeof(m_AddComponentFilterBuffer));
        }
        else
        {
            ImGui::TextUnformatted("Editor context unavailable.");
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
                    rootComponent->PostEditChangeProperty(
                        Reflection::PropertyChangedEvent{std::string_view("m_Transform")});
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

            bool componentOpen = false;
            // The checkbox binds the AUTHORED flag, so an ancestor that is off never rewrites the
            // author's choice when the box is clicked.
            bool componentActive = component->IsActiveSelf();
            const bool inactiveStylePushed = !componentActive;
            const std::string activeCheckboxId =
                std::string("##component_active_") + std::to_string(reinterpret_cast<uintptr_t>(component.get()));
            const PropertyUndoCaptureContext activeUndoContext =
                MakePropertyUndoCaptureContext(component.get(), classInfo, "m_bActive");
            std::vector<uint8_t> activeBeforeBlob;
            const bool capturedActiveBefore = SerializePropertyUndoBlob(activeUndoContext, activeBeforeBlob);

            ImGui::PushID(component.get());

            if (inactiveStylePushed)
            {
                ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.55f);
            }

            // Full-width header tint from content left (not SameLine mid-cursor),
            // drawn before widgets so checkbox/header stay left-aligned.
            if (appearance != nullptr)
            {
                const ImVec2 winPos = ImGui::GetWindowPos();
                const float y0 = ImGui::GetCursorScreenPos().y;
                const float y1 = y0 + ImGui::GetFrameHeight();
                const float x0 = winPos.x + ImGui::GetCursorStartPos().x;
                const float x1 = winPos.x + ImGui::GetContentRegionMax().x;
                const ImU32 headerBg =
                    appearance->GetDisplayColorU32(appearance->GetActivePalette().Accent, 0.32f);
                ImGui::GetWindowDrawList()->AddRectFilled(
                    ImVec2(x0, y0), ImVec2(x1, y1), headerBg, 3.0f);
            }

            bool openHeaderContextMenu = false;
            if (ImGui::Checkbox(activeCheckboxId.c_str(), &componentActive))
            {
                component->SetActive(componentActive);
                m_SceneEditor.MarkSceneDirty();

                std::vector<uint8_t> activeAfterBlob;
                if (capturedActiveBefore && SerializePropertyUndoBlob(activeUndoContext, activeAfterBlob))
                {
                    TryPropertyUndoCommitImmediate(activeUndoContext, activeBeforeBlob, activeAfterBlob);
                }
            }
            if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Right))
            {
                openHeaderContextMenu = true;
            }

            // Only components with a presence in the scene carry the presentation axis.
            SceneComponent* sceneComponent =
                component->IsA(SceneComponent::StaticClass()) ? static_cast<SceneComponent*>(component.get()) : nullptr;
            if (sceneComponent != nullptr)
            {
                const std::string visibleCheckboxId =
                    std::string("##component_visible_") + std::to_string(reinterpret_cast<uintptr_t>(component.get()));
                const PropertyUndoCaptureContext visibleUndoContext =
                    MakePropertyUndoCaptureContext(component.get(), classInfo, "m_bVisible");
                std::vector<uint8_t> visibleBeforeBlob;
                const bool capturedVisibleBefore = SerializePropertyUndoBlob(visibleUndoContext, visibleBeforeBlob);

                // The box holds the AUTHORED flag so an ancestor that is off never rewrites the
                // author's choice, and the row stays writable to preset a subtree before it opens.
                bool componentVisible = sceneComponent->IsVisibleSelf();

                ImGui::SameLine();
                if (ImGui::Checkbox(visibleCheckboxId.c_str(), &componentVisible))
                {
                    sceneComponent->SetVisible(componentVisible);
                    m_SceneEditor.MarkSceneDirty();

                    std::vector<uint8_t> visibleAfterBlob;
                    if (capturedVisibleBefore && SerializePropertyUndoBlob(visibleUndoContext, visibleAfterBlob))
                    {
                        TryPropertyUndoCommitImmediate(visibleUndoContext, visibleBeforeBlob, visibleAfterBlob);
                    }
                }
                if (ImGui::IsItemHovered())
                {
                    ImGui::SetTooltip("Visible");
                }
                if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Right))
                {
                    openHeaderContextMenu = true;
                }
            }

            ImGui::SameLine();
            DrawComponentHeaderRow(
                *gameObject, *component, classInfo, appearance, componentOpen, openHeaderContextMenu);

            // Read-only: an off ancestor makes the component behave as off while its own box keeps
            // the author's value, so the reason is shown instead of silently clearing the switch.
            // The test is on the owner, not on the effective state, so a row the author turned off
            // *and* an ancestor turned off still reveals that something above is off too.
            GameObject* componentOwner = component->GetOwner();
            if (componentOwner != nullptr && !componentOwner->IsActive())
            {
                ImGui::SameLine();
                ImGui::TextDisabled("Inactive in Hierarchy");
            }
            if (sceneComponent != nullptr && componentOwner != nullptr && !componentOwner->IsVisible())
            {
                ImGui::SameLine();
                ImGui::TextDisabled("Hidden in Hierarchy");
            }

            if (openHeaderContextMenu)
            {
                ImGui::OpenPopup("##ComponentHeaderContext");
            }
            if (ImGui::BeginPopup("##ComponentHeaderContext"))
            {
                DrawComponentContextMenu(*component);
                ImGui::EndPopup();
            }

            if (inactiveStylePushed)
            {
                ImGui::PopStyleVar();
            }

            if (!componentOpen)
            {
                ImGui::PopID();
                continue;
            }

            const std::string tableId = "ComponentTable##" + std::to_string(reinterpret_cast<uintptr_t>(component.get()));
            if (!ImGui::BeginTable(tableId.c_str(), 2, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_RowBg))
            {
                ImGui::TreePop();
                ImGui::PopID();
                continue;
            }

            ImGui::TableSetupColumn("Property", ImGuiTableColumnFlags_WidthStretch, 0.35f);
            ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch, 0.65f);
            bool hasAnyReflectedField = false;
            MEObject* componentObject = static_cast<MEObject*>(component.get());
            if (componentObject == nullptr)
            {
                ImGui::EndTable();
                ImGui::TreePop();
                ImGui::PopID();
                continue;
            }
            const Reflection::MEClass* compClass = componentObject->GetClass();
            if (compClass == nullptr)
            {
                ImGui::TextUnformatted("Component class info missing.");
                ImGui::EndTable();
                ImGui::TreePop();
                ImGui::PopID();
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
            ImGui::TreePop();
            ImGui::PopID();
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
                                                                    const std::vector<uint8_t>& afterBlob,
                                                                    bool applyOnFirstExecute)
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
            afterBlob,
            applyOnFirstExecute);
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
        bool appliedViaAssign = false;
        const bool allowRowCapture = (property.GetCategory() != Reflection::MEPropertyCategory::Object
            && property.GetCategory() != Reflection::MEPropertyCategory::ObjectPtr);

        MEObject* mutableOwner = const_cast<MEObject*>(owner);
        const bool isOwnerDirectField =
            mutableOwner != nullptr
            && propertyPtr != nullptr
            && property.GetMutableAccessor() != nullptr
            && property.GetMutable(mutableOwner) == propertyPtr
            && property.GetStorageSize() > 0;

        switch (property.GetCategory())
        {
            case Reflection::MEPropertyCategory::Primitive:
                if (isOwnerDirectField)
                {
                    valueChanged = DrawLeafPropertyViaAssign(mutableOwner, property);
                    appliedViaAssign = true;
                }
                else
                {
                    valueChanged = DrawPrimitiveProperty(
                        static_cast<const Reflection::MEPrimitiveProperty&>(property), propertyPtr);
                }
                break;
            case Reflection::MEPropertyCategory::Object:
            {
                const Reflection::MEObjectProperty& objectProperty =
                    static_cast<const Reflection::MEObjectProperty&>(property);
                const bool isFlatObjectWidget =
                    PropertyValueWidget::IsLinearColorStruct(objectProperty.GetValueClass());
                if (isFlatObjectWidget && isOwnerDirectField)
                {
                    valueChanged = DrawLeafPropertyViaAssign(mutableOwner, property);
                    appliedViaAssign = true;
                }
                else if (!isFlatObjectWidget)
                {
                    valueChanged =
                        DrawObjectProperty(owner, ownerClass, objectProperty, propertyPtr, activeUndoContext);
                }
                else
                {
                    if (PropertyValueWidget::Draw(property, propertyPtr, -FLT_MIN))
                    {
                        m_SceneEditor.MarkSceneDirty();
                        valueChanged = true;
                    }
                }
                break;
            }
            case Reflection::MEPropertyCategory::ObjectPtr:
                if (isOwnerDirectField && property.HasPropertySetter())
                {
                    valueChanged = DrawObjectPtrLeafViaAssign(
                        mutableOwner,
                        ownerClass,
                        static_cast<const Reflection::MEObjectPtrProperty&>(property),
                        activeUndoContext);
                    appliedViaAssign = true;
                }
                else
                {
                    valueChanged = DrawObjectPtrProperty(
                        owner,
                        ownerClass,
                        static_cast<const Reflection::MEObjectPtrProperty&>(property),
                        propertyPtr,
                        activeUndoContext);
                }
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

        // Assign path already delivers semantic PostEdit (Setter or notifyPostEdit).
        // In-place paths (ObjectPtr / nested Object / fallback) still need virtual PostEdit when no Setter.
        if (valueChanged && mutableOwner != nullptr && !appliedViaAssign && !property.HasPropertySetter())
        {
            mutableOwner->PostEditChangeProperty(
                Reflection::PropertyChangedEvent{std::string_view(property.GetName())});
        }

        ImGui::PopID();
        return valueChanged;
    }

    bool SceneEditorInspectorSource::DrawLeafPropertyViaAssign(MEObject* owner,
                                                               const Reflection::MEProperty& property)
    {
        const size_t storageSize = property.GetStorageSize();
        if (owner == nullptr || storageSize == 0)
        {
            return false;
        }

        std::vector<uint8_t> editBuffer(storageSize);
        const bool needsLifetime = property.GetValueConstructFn() != nullptr;
        if (needsLifetime)
        {
            property.ConstructValue(editBuffer.data());
        }

        struct ScopedPropertyValue
        {
            const Reflection::MEProperty* Property = nullptr;
            void* Storage = nullptr;
            bool Live = false;

            ~ScopedPropertyValue()
            {
                if (Live && Property != nullptr && Storage != nullptr)
                {
                    Property->DestructValue(Storage);
                }
            }
        } scoped{&property, editBuffer.data(), needsLifetime};

        if (!Reflection::GetPropertyValue(owner, property, editBuffer.data()))
        {
            return false;
        }

        if (!PropertyValueWidget::Draw(property, editBuffer.data(), -FLT_MIN))
        {
            return false;
        }

        Reflection::PropertyAssignOptions options;
        options.notifyPostEdit = !property.HasPropertySetter();
        options.postEditObject = owner;
        if (!Reflection::AssignProperty(owner, property, editBuffer.data(), options))
        {
            return false;
        }

        m_SceneEditor.MarkSceneDirty();
        return true;
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
        const bool editingTempBuffer =
            owner != nullptr
            && objectPtrProperty.GetMutableAccessor() != nullptr
            && objectPtrProperty.GetMutable(const_cast<MEObject*>(owner)) != propertyPtr;

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
        hooks.TryApplySelection = [owner, ownerClass, &objectPtrProperty, propertyPtr, editingTempBuffer](
                                      const PropertyRefCandidate& selected) -> bool
        {
            // When drawing into an Assign temp buffer, always write the shared_ptr slot so
            // AssignProperty can invoke the reflected Setter. Special-cases mutate the owner
            // in place and would leave the temp buffer stale.
            if (editingTempBuffer)
            {
                return false;
            }

            if (selected.Kind != PropertyRefCandidateKind::AssetMeta || selected.Meta == nullptr)
            {
                return false;
            }

            std::string errorMessage;
            const std::shared_ptr<Asset> asset =
                AssetManager::Get().LoadAssetByPath(selected.Meta->AssetPath, errorMessage);
            if (!asset)
            {
                ME_LOG(LogEditor, Error, 
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
                        if (asset->GetClass() == nullptr
                            || !asset->GetClass()->IsA(MaterialInterface::StaticClass()))
                        {
                            return false;
                        }

                        meshComponent->SetMaterial(std::static_pointer_cast<MaterialInterface>(asset));
                        return true;
                    }
                }

                if (SkeletalMeshComponent* skeletalMeshComponent =
                        dynamic_cast<SkeletalMeshComponent*>(const_cast<MEObject*>(owner)))
                {
                    if (objectPtrProperty.GetName() == "m_Mesh")
                    {
                        skeletalMeshComponent->SetMesh(std::static_pointer_cast<SkeletalMesh>(asset));
                        return true;
                    }

                    if (objectPtrProperty.GetName() == "m_Material")
                    {
                        if (asset->GetClass() == nullptr
                            || !asset->GetClass()->IsA(MaterialInterface::StaticClass()))
                        {
                            return false;
                        }

                        skeletalMeshComponent->SetMaterial(std::static_pointer_cast<MaterialInterface>(asset));
                        return true;
                    }

                    if (objectPtrProperty.GetName() == "m_AnimationClip")
                    {
                        skeletalMeshComponent->SetAnimationClip(
                            std::static_pointer_cast<AnimationClip>(asset));
                        return true;
                    }
                }
            }

            return false;
        };
        hooks.OnSelectionCommitted = [this, undoContext, assetUndoKey, editingTempBuffer](bool selectionChanged)
        {
            // Assign path commits undo after AssignProperty (owner still stale here).
            if (editingTempBuffer || !selectionChanged || undoContext == nullptr || !undoContext->IsValid())
            {
                if (editingTempBuffer && undoContext != nullptr && undoContext->IsValid())
                {
                    // Keep before blob for LeafViaAssign; do not erase yet.
                    return;
                }
                if (!selectionChanged && undoContext != nullptr && undoContext->IsValid())
                {
                    m_AssetPropertyUndoBeforeByKey.erase(assetUndoKey);
                }
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
                    // UI already applied the ObjectPtr; record undo without re-deserializing/reloading.
                    TryPropertyUndoCommitImmediate(*undoContext, beforeBlob, afterBlob, false);
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

    bool SceneEditorInspectorSource::DrawObjectPtrLeafViaAssign(
        MEObject* owner,
        const Reflection::MEClass* ownerClass,
        const Reflection::MEObjectPtrProperty& objectPtrProperty,
        const PropertyUndoCaptureContext* undoContext)
    {
        const size_t storageSize = objectPtrProperty.GetStorageSize();
        if (owner == nullptr || storageSize == 0)
        {
            return false;
        }

        const std::string assetUndoKey = MakeAssetPropertyUndoKey(
            (undoContext && undoContext->IsValid()) ? undoContext->ownerGuid : GUID::Zero(),
            objectPtrProperty.GetName());

        std::vector<uint8_t> editBuffer(storageSize);
        const bool needsLifetime = objectPtrProperty.GetValueConstructFn() != nullptr;
        if (needsLifetime)
        {
            objectPtrProperty.ConstructValue(editBuffer.data());
        }

        struct ScopedPropertyValue
        {
            const Reflection::MEProperty* Property = nullptr;
            void* Storage = nullptr;
            bool Live = false;

            ~ScopedPropertyValue()
            {
                if (Live && Property != nullptr && Storage != nullptr)
                {
                    Property->DestructValue(Storage);
                }
            }
        } scoped{&objectPtrProperty, editBuffer.data(), needsLifetime};

        if (!Reflection::GetPropertyValue(owner, objectPtrProperty, editBuffer.data()))
        {
            return false;
        }

        if (!DrawObjectPtrProperty(owner, ownerClass, objectPtrProperty, editBuffer.data(), undoContext))
        {
            return false;
        }

        Reflection::PropertyAssignOptions options;
        options.notifyPostEdit = !objectPtrProperty.HasPropertySetter();
        options.postEditObject = owner;
        if (!Reflection::AssignProperty(owner, objectPtrProperty, editBuffer.data(), options))
        {
            return false;
        }

        if (undoContext != nullptr && undoContext->IsValid())
        {
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
                TryPropertyUndoCommitImmediate(*undoContext, beforeBlob, afterBlob, false);
            }
            m_AssetPropertyUndoBeforeByKey.erase(assetUndoKey);
        }

        m_SceneEditor.MarkSceneDirty();
        return true;
    }

    bool SceneEditorInspectorSource::DrawArrayProperty(const Reflection::MEArrayProperty& arrayProperty, void* propertyPtr)
    {
        ImGui::TextUnformatted("Array properties are not supported in this version.");
        return false;
    }

    void SceneEditorInspectorSource::BeginComponentRename(Component& component, uint64_t ownerGameObjectId)
    {
        m_IsRenamingComponent = true;
        m_RequestComponentRenameFocus = true;
        m_RenamingComponentGuid = component.GetGuid();
        m_RenamingComponentOwnerId = ownerGameObjectId;
        std::memset(m_ComponentRenameBuffer, 0, sizeof(m_ComponentRenameBuffer));
        std::strncpy(
            m_ComponentRenameBuffer, component.GetName().c_str(), sizeof(m_ComponentRenameBuffer) - 1);
    }

    bool SceneEditorInspectorSource::DrawComponentHeaderRow(GameObject& gameObject,
                                                            Component& component,
                                                            const Reflection::MEClass* classInfo,
                                                            EditorAppearance* appearance,
                                                            bool& outComponentOpen,
                                                            bool& inOutOpenHeaderContextMenu)
    {
        outComponentOpen = false;
        if (classInfo == nullptr)
        {
            return false;
        }

        auto noteRightClick = [&inOutOpenHeaderContextMenu]()
        {
            if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Right))
            {
                inOutOpenHeaderContextMenu = true;
            }
        };

        std::unique_ptr<EditorTypographyScope> componentHeaderTypography;
        if (appearance != nullptr)
        {
            componentHeaderTypography = std::make_unique<EditorTypographyScope>(
                *appearance, EditorTypographyRole::Subheading);
        }

        // Do not wrap TreeNode in BeginGroup/EndGroup — that breaks Indent and
        // shifts subsequent component rows into the left clip edge.

        if (appearance != nullptr)
        {
            EditorThemeScope componentSectionTheme = EditorWindowTheme::SectionHeader(*appearance);
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8.0f, 3.0f));
            const ImGuiTreeNodeFlags treeFlags = ImGuiTreeNodeFlags_DefaultOpen
                | ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_AllowOverlap
                | ImGuiTreeNodeFlags_FramePadding;
            outComponentOpen = ImGui::TreeNodeEx("##component_tree", treeFlags);
            ImGui::PopStyleVar();
        }
        else
        {
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8.0f, 3.0f));
            const ImGuiTreeNodeFlags treeFlags = ImGuiTreeNodeFlags_DefaultOpen
                | ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_AllowOverlap
                | ImGuiTreeNodeFlags_FramePadding;
            outComponentOpen = ImGui::TreeNodeEx("##component_tree", treeFlags);
            ImGui::PopStyleVar();
        }
        noteRightClick();

        ImGui::SameLine();

        if (appearance != nullptr)
        {
            ComponentTypeUiCatalog::DrawIcon(*appearance, classInfo);
            noteRightClick();
            ImGui::SameLine();
        }

        const std::string typeDisplay = ComponentTypeUiCatalog::MakeTypeDisplayName(classInfo);
        const bool isRenamingThis = m_IsRenamingComponent && m_RenamingComponentGuid == component.GetGuid();

        if (isRenamingThis)
        {
            ImGui::SetNextItemWidth(160.0f);
            const InlineRenameField::Result renameResult = InlineRenameField::Draw(
                m_ComponentRenameBuffer, sizeof(m_ComponentRenameBuffer), m_RequestComponentRenameFocus);
            if (renameResult == InlineRenameField::Result::Commit)
            {
                if (IEditorContext* context = m_SceneEditor.GetEditorContext())
                {
                    m_SceneEditor.SubmitRenameComponent(
                        *context, gameObject.GetID(), component.GetGuid(), m_ComponentRenameBuffer);
                }
                m_IsRenamingComponent = false;
            }
            else if (renameResult == InlineRenameField::Result::Cancel)
            {
                m_IsRenamingComponent = false;
            }
        }
        else
        {
            std::string instanceName = component.GetName();
            if (instanceName.empty())
            {
                instanceName = typeDisplay;
            }

            ImGui::TextUnformatted(instanceName.c_str());
            noteRightClick();
            if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
            {
                BeginComponentRename(component, gameObject.GetID());
            }
        }

        ImGui::SameLine();
        ImGui::TextDisabled("%s", typeDisplay.c_str());
        noteRightClick();

        const SceneComponent* rootComponent = gameObject.GetRootComponent();
        const bool isRoot = rootComponent != nullptr && &component == rootComponent;
        if (!isRoot)
        {
            const size_t index = gameObject.FindComponentIndex(component);
            const size_t count = gameObject.GetAllComponents().size();
            size_t minMovableIndex = 0;
            if (rootComponent != nullptr)
            {
                const size_t rootIndex = gameObject.FindComponentIndex(*rootComponent);
                if (rootIndex < count)
                {
                    minMovableIndex = rootIndex + 1;
                }
            }
            const bool canMoveUp = index > minMovableIndex && index < count;
            const bool canMoveDown = index + 1 < count;

            ImGui::SameLine();
            ImGui::BeginDisabled(!canMoveUp);
            if (ImGui::SmallButton("^##MoveUp") && canMoveUp)
            {
                if (IEditorContext* context = m_SceneEditor.GetEditorContext())
                {
                    m_SceneEditor.SubmitMoveComponent(
                        *context, gameObject.GetID(), component.GetGuid(), index - 1);
                }
            }
            ImGui::EndDisabled();

            ImGui::SameLine();
            ImGui::BeginDisabled(!canMoveDown);
            if (ImGui::SmallButton("v##MoveDown") && canMoveDown)
            {
                if (IEditorContext* context = m_SceneEditor.GetEditorContext())
                {
                    m_SceneEditor.SubmitMoveComponent(
                        *context, gameObject.GetID(), component.GetGuid(), index + 1);
                }
            }
            ImGui::EndDisabled();
        }

        return outComponentOpen;
    }

    void SceneEditorInspectorSource::StartInlineRename(const GameObject& gameObject)
    {
        m_IsRenamingSelectedGameObject = true;
        m_RenameTargetGameObjectId = gameObject.GetID();
        std::memset(m_RenameBuffer, 0, sizeof(m_RenameBuffer));
        std::strncpy(m_RenameBuffer, gameObject.GetName().c_str(), sizeof(m_RenameBuffer) - 1);
        m_RequestRenameFocus = true;
    }

    void SceneEditorInspectorSource::DrawGameObjectStatusSwitches(GameObject& gameObject)
    {
        const Reflection::MEClass* gameObjectClass = gameObject.GetClass();

        // Both boxes write the AUTHORED flag, never the effective one: a node whose ancestor is
        // off must still store what the author asked for, so switching it back on is one click.
        auto drawAuthoredSwitch = [this, &gameObject, gameObjectClass](const char* label,
                                                                     const char* propertyPath,
                                                                     bool selfValue,
                                                                     auto&& applyValue)
        {
            const PropertyUndoCaptureContext undoContext =
                MakePropertyUndoCaptureContext(&gameObject, gameObjectClass, propertyPath);
            std::vector<uint8_t> beforeBlob;
            const bool capturedBefore = SerializePropertyUndoBlob(undoContext, beforeBlob);

            bool checked = selfValue;
            if (ImGui::Checkbox(label, &checked))
            {
                applyValue(checked);
                m_SceneEditor.MarkSceneDirty();

                std::vector<uint8_t> afterBlob;
                if (capturedBefore && SerializePropertyUndoBlob(undoContext, afterBlob))
                {
                    TryPropertyUndoCommitImmediate(undoContext, beforeBlob, afterBlob);
                }
            }
        };

        auto drawHierarchyBadge = [](const char* text)
        {
            ImGui::SameLine();
            ImGui::TextDisabled("%s", text);
        };

        ImGui::PushID("GameObjectStatusSwitches");
        drawAuthoredSwitch("Active", "m_bActive", gameObject.IsActiveSelf(),
                           [&gameObject](bool value) { gameObject.SetActive(value); });
        ImGui::SameLine();
        drawAuthoredSwitch("Visible", "m_bVisible", gameObject.IsVisibleSelf(),
                           [&gameObject](bool value) { gameObject.SetVisible(value); });
        ImGui::PopID();

        // Read-only badges: they explain why the node or its subtree behaves as off while the
        // switch above still shows the author's own value. Testing the parent rather than the
        // effective state keeps the badge visible when the node itself is off as well.
        const GameObject* parentGameObject = gameObject.GetParent();
        if (parentGameObject != nullptr && !parentGameObject->IsActive())
        {
            drawHierarchyBadge("Inactive in Hierarchy");
        }
        if (parentGameObject != nullptr && !parentGameObject->IsVisible())
        {
            drawHierarchyBadge("Hidden in Hierarchy");
        }
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