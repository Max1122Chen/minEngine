#pragma once

#include <string>
#include <vector>

namespace minEngine
{
    struct EditorState
    {
        bool isPlaying = false;
        bool showDemoWindow = false;
        float lastDeltaTime = 0.0f;

        std::vector<std::string> hierarchyItems {"MainCamera", "DirectionalLight", "Cube_01", "Plane_01"};
        int selectedHierarchyIndex = 0;
        std::string inspectorName = "MainCamera";

        float inspectorPosition[3] = {0.0f, 0.0f, 0.0f};
        float inspectorRotation[3] = {0.0f, 0.0f, 0.0f};
        float inspectorScale[3] = {1.0f, 1.0f, 1.0f};
        float inspectorTint[3] = {1.0f, 1.0f, 1.0f};
    };
}
