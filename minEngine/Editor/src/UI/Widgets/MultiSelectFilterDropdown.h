#pragma once

#include "imgui.h"

#include <string>
#include <vector>

namespace minEngine::UI
{
    struct FilterOptionRef
    {
        const char* label = "";
        bool* enabled = nullptr;
    };

    struct FilterSection
    {
        const char* title = "";
        std::vector<FilterOptionRef> options;
    };

    inline int CountEnabledOptions(const std::vector<FilterSection>& sections)
    {
        int count = 0;
        for (const FilterSection& section : sections)
        {
            for (const FilterOptionRef& option : section.options)
            {
                if (option.enabled != nullptr && *option.enabled)
                {
                    ++count;
                }
            }
        }
        return count;
    }

    inline int CountTotalOptions(const std::vector<FilterSection>& sections)
    {
        int total = 0;
        for (const FilterSection& section : sections)
        {
            total += static_cast<int>(section.options.size());
        }
        return total;
    }

    inline std::string BuildFilterSummary(const char* prefix, const std::vector<FilterSection>& sections)
    {
        const int enabledCount = CountEnabledOptions(sections);
        const int totalCount = CountTotalOptions(sections);
        return std::string(prefix) + " (" + std::to_string(enabledCount) + "/" + std::to_string(totalCount) + ")";
    }

    inline void SetAllFilterOptions(std::vector<FilterSection>& sections, bool enabled)
    {
        for (FilterSection& section : sections)
        {
            for (FilterOptionRef& option : section.options)
            {
                if (option.enabled != nullptr)
                {
                    *option.enabled = enabled;
                }
            }
        }
    }

    inline void DrawFilterDropdown(const char* comboId, std::vector<FilterSection>& sections)
    {
        const std::string summary = BuildFilterSummary("Filters", sections);
        if (!ImGui::BeginCombo(comboId, summary.c_str(), ImGuiComboFlags_HeightLarge))
        {
            return;
        }

        for (size_t sectionIndex = 0; sectionIndex < sections.size(); ++sectionIndex)
        {
            FilterSection& section = sections[sectionIndex];
            ImGui::TextUnformatted(section.title);
            ImGui::Separator();

            for (FilterOptionRef& option : section.options)
            {
                if (option.enabled == nullptr)
                {
                    continue;
                }

                if (ImGui::Selectable(option.label, *option.enabled, ImGuiSelectableFlags_DontClosePopups))
                {
                    *option.enabled = !*option.enabled;
                }
            }

            if (sectionIndex + 1 < sections.size())
            {
                ImGui::Spacing();
            }
        }

        ImGui::Spacing();
        if (ImGui::Button("All", ImVec2(78.0f, 0.0f)))
        {
            SetAllFilterOptions(sections, true);
        }
        ImGui::SameLine();
        if (ImGui::Button("None", ImVec2(78.0f, 0.0f)))
        {
            SetAllFilterOptions(sections, false);
        }

        ImGui::EndCombo();
    }
}
