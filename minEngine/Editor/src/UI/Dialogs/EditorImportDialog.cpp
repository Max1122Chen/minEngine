#include "UI/Dialogs/EditorImportDialog.h"

#include "Runtime/Resource/AssetManager.h"
#include "Runtime/Resource/AssetMeta.h"

#include <imgui.h>

#include <cstdio>

namespace minEngine
{
    void EditorImportDialog::Open(
        std::vector<std::filesystem::path> sourcePaths,
        std::filesystem::path destDirectory)
    {
        m_Open = true;
        m_SourcePaths = std::move(sourcePaths);
        m_DestDirectory = std::move(destDirectory);
        m_SelectedProductIndex = 0;
        m_SelectedProductId.clear();
        m_SkeletonAssetPath.clear();
        RebuildCompatibleProducts();
        ImGui::OpenPopup("Import Product");
    }

    void EditorImportDialog::Close()
    {
        m_Open = false;
        m_SourcePaths.clear();
        m_DestDirectory.clear();
        m_CompatibleProductIds.clear();
        m_CompatibleProductNames.clear();
        m_SelectedProductIndex = 0;
        m_SelectedProductId.clear();
        m_SkeletonAssetPath.clear();
    }

    void EditorImportDialog::RebuildCompatibleProducts()
    {
        m_CompatibleProductIds.clear();
        m_CompatibleProductNames.clear();

        if (m_SourcePaths.empty())
        {
            return;
        }

        const std::vector<ImportProductDescriptor>& products =
            AssetManager::Get().GetImportProducts();

        for (const ImportProductDescriptor& product : products)
        {
            bool acceptsAll = true;
            for (const std::filesystem::path& sourcePath : m_SourcePaths)
            {
                const std::string extension = sourcePath.extension().string();
                if (product.AcceptsSourceExtension == nullptr
                    || !product.AcceptsSourceExtension(extension))
                {
                    acceptsAll = false;
                    break;
                }
            }

            if (!acceptsAll)
            {
                continue;
            }

            m_CompatibleProductIds.push_back(product.ProductId);
            m_CompatibleProductNames.push_back(
                product.DisplayName.empty() ? product.ProductId : product.DisplayName);
        }

        if (m_CompatibleProductIds.empty())
        {
            m_SelectedProductIndex = 0;
            m_SelectedProductId.clear();
            return;
        }

        if (m_SelectedProductIndex < 0
            || m_SelectedProductIndex >= static_cast<int>(m_CompatibleProductIds.size()))
        {
            m_SelectedProductIndex = 0;
        }

        m_SelectedProductId = m_CompatibleProductIds[static_cast<size_t>(m_SelectedProductIndex)];
    }

    bool EditorImportDialog::SelectedProductNeedsSkeletonPicker() const
    {
        if (m_SelectedProductId.empty())
        {
            return false;
        }

        const ImportProductDescriptor* product =
            AssetManager::Get().FindImportProduct(m_SelectedProductId);
        return product != nullptr && product->bNeedsSkeletonPicker;
    }

    EditorImportDialogAction EditorImportDialog::Draw()
    {
        if (!m_Open)
        {
            return EditorImportDialogAction::None;
        }

        ImGui::OpenPopup("Import Product");

        const ImVec2 center = ImGui::GetMainViewport()->GetCenter();
        ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

        if (!ImGui::BeginPopupModal("Import Product", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        {
            return EditorImportDialogAction::None;
        }

        char message[256];
        std::snprintf(
            message,
            sizeof(message),
            "Import %d source file(s) as which product?",
            static_cast<int>(m_SourcePaths.size()));
        ImGui::TextUnformatted(message);
        ImGui::Separator();

        if (m_CompatibleProductIds.empty())
        {
            ImGui::TextWrapped("No registered import products accept these source extensions.");
        }
        else
        {
            const char* preview =
                m_CompatibleProductNames[static_cast<size_t>(m_SelectedProductIndex)].c_str();
            if (ImGui::BeginCombo("Product", preview))
            {
                for (int index = 0; index < static_cast<int>(m_CompatibleProductIds.size()); ++index)
                {
                    const bool selected = index == m_SelectedProductIndex;
                    if (ImGui::Selectable(
                            m_CompatibleProductNames[static_cast<size_t>(index)].c_str(),
                            selected))
                    {
                        m_SelectedProductIndex = index;
                        m_SelectedProductId =
                            m_CompatibleProductIds[static_cast<size_t>(index)];
                        m_SkeletonAssetPath.clear();
                    }

                    if (selected)
                    {
                        ImGui::SetItemDefaultFocus();
                    }
                }

                ImGui::EndCombo();
            }
        }

        if (SelectedProductNeedsSkeletonPicker())
        {
            ImGui::Separator();
            ImGui::TextUnformatted("Skeleton");
            ImGui::TextWrapped(
                "Required for AnimationClip. Leave empty to try "
                "{stem}_Skeleton.meskeleton next to the destination.");

            const std::vector<const AssetMeta*> skeletons =
                AssetManager::Get().FindAssetMetasByType("Skeleton");
            const char* skeletonPreview = m_SkeletonAssetPath.empty()
                ? "(none / runtime fallback)"
                : m_SkeletonAssetPath.c_str();
            if (ImGui::BeginCombo("##SkeletonPicker", skeletonPreview))
            {
                if (ImGui::Selectable("(none / runtime fallback)", m_SkeletonAssetPath.empty()))
                {
                    m_SkeletonAssetPath.clear();
                }

                for (const AssetMeta* skeletonMeta : skeletons)
                {
                    if (skeletonMeta == nullptr)
                    {
                        continue;
                    }

                    const bool selected = m_SkeletonAssetPath == skeletonMeta->AssetPath;
                    if (ImGui::Selectable(skeletonMeta->AssetPath.c_str(), selected))
                    {
                        m_SkeletonAssetPath = skeletonMeta->AssetPath;
                    }

                    if (selected)
                    {
                        ImGui::SetItemDefaultFocus();
                    }
                }

                ImGui::EndCombo();
            }
        }

        EditorImportDialogAction action = EditorImportDialogAction::None;
        const bool canConfirm = !m_CompatibleProductIds.empty() && !m_SelectedProductId.empty();

        ImGui::Separator();
        if (!canConfirm)
        {
            ImGui::BeginDisabled();
        }

        if (ImGui::Button("Import", ImVec2(120.0f, 0.0f)))
        {
            action = EditorImportDialogAction::Confirm;
        }

        if (!canConfirm)
        {
            ImGui::EndDisabled();
        }

        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120.0f, 0.0f)))
        {
            action = EditorImportDialogAction::Cancel;
        }

        if (action != EditorImportDialogAction::None)
        {
            ImGui::CloseCurrentPopup();
            m_Open = false;
        }

        ImGui::EndPopup();
        return action;
    }
}
