#include "Runtime/Function/Framework/Transform/Transform.h"
#include "Runtime/Function/Framework/Scene/Scene.h"
#include "Runtime/Function/Framework/GameObject/GameObject.h"
#include "Runtime/Function/Framework/Components/DirectionalLightComponent.h"
#include "Runtime/Function/Framework/Components/StaticMeshComponent.h"
#include "Runtime/Function/Render/OpenGL/OpenGLShader.h"
#include "Runtime/Function/Render/Material.h"
#include "Runtime/Function/Render/StaticMesh.h"
#include "Runtime/Function/Render/Texture.h"
#include "Runtime/Resource/AssetManager.h"

#include "Runtime/Function/Render/RHI/RHIShader.h"
#include "Runtime/Function/Render/RHI/RHITexture.h"
#include "Runtime/Function/Render/RHI/RHIBuffers.h"

#include <array>
#include <filesystem>
#include <initializer_list>

namespace minEngine
{
    void PopulateEditorDefaultScene(Scene& scene)
    {
        AssetManager& assetManager = AssetManager::Get();

        
    }
}