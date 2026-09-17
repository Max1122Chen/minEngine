#include "AssetManagerTest.h"
#include "Access/SceneManagerTestAccess.h"
#include "Access/AssetManagerTestAccess.h"
#include "Access/ObjectManagerTestAccess.h"

#include "AssetManager.h"
#include "AssetRegistryTypes.h"
#include "Runtime/Core/Log/LogSystem.h"
#include "Runtime/Core/Object/ObjectManager.h"
#include "Runtime/Core/Paths/PathRegistry.h"
#include "Runtime/Core/Reflection/Reflection.h"
#include "Runtime/EngineConfig.h"
#include "Runtime/Function/Framework/Scene/SceneManager.h"
#include "Runtime/Function/Render/Material.h"

#include <filesystem>
#include <fstream>
#include <string_view>
#include <vector>

namespace minEngine
{
    class AssetManagerTestScope
    {
    public:
        AssetManagerTestScope()
        {
            Testing::TestAccess<ObjectManager>::SetInstance(&m_ObjectManager);
            m_ObjectManager.Initialize();

            Testing::TestAccess<SceneManager>::SetInstance(&m_SceneManager);
            m_SceneManager.Initialize();

            Testing::TestAccess<AssetManager>::SetInstance(&m_AssetManager);
            m_AssetManager.Initialize();
        }

        ~AssetManagerTestScope()
        {
            m_AssetManager.Shutdown();
            Testing::TestAccess<AssetManager>::SetInstance(nullptr);

            m_SceneManager.Shutdown();
            Testing::TestAccess<SceneManager>::SetInstance(nullptr);

            m_ObjectManager.Shutdown();
            Testing::TestAccess<ObjectManager>::SetInstance(nullptr);
        }

    private:
        ObjectManager m_ObjectManager;
        AssetManager m_AssetManager;
        SceneManager m_SceneManager;
    };

    namespace
    {

        bool EnsureEnginePathsReady(int argc, char** argv)
        {
            EngineConfig engineConfig;
            if (PathRegistry::Get().LoadEngineConfiguration(argc, argv, engineConfig))
            {
                return true;
            }

            ME_LOG(LogTest, Error, 
                "AssetManagerTest: failed to load engine configuration. "
                "Run from build bin with EngineConfig.meconfig nearby or pass --engine-root=.");
            return false;
        }

        class AssetManagerTestProject
        {
        public:
            AssetManagerTestProject()
            {
                const std::filesystem::path tempRoot =
                    std::filesystem::temp_directory_path() / "minEngine_AssetManagerP2Test";
                std::error_code removeError;
                std::filesystem::remove_all(tempRoot, removeError);

                m_ProjectRoot = tempRoot / "Project";
                const std::filesystem::path contentRoot = m_ProjectRoot / "Assets" / "_P2UnitTest";
                std::filesystem::create_directories(contentRoot);
                PathRegistry::Get().SetProjectRoots(m_ProjectRoot);
                m_TestContentDirectory = contentRoot;
            }

            ~AssetManagerTestProject()
            {
                PathRegistry::Get().ClearProjectRoots();
                std::error_code removeError;
                std::filesystem::remove_all(
                    std::filesystem::temp_directory_path() / "minEngine_AssetManagerP2Test",
                    removeError);
            }

            const std::filesystem::path& GetTestContentDirectory() const
            {
                return m_TestContentDirectory;
            }

            std::filesystem::path GetEngineCubeSource() const
            {
                return PathRegistry::Get().GetEngineDefaultAssetsRoot() / "Meshes" / "BasicShapes" /
                       "cube.obj";
            }

        private:
            std::filesystem::path m_ProjectRoot;
            std::filesystem::path m_TestContentDirectory;
        };

        std::string ImportTestMeshCopy(
            AssetManager& assetManager,
            const AssetManagerTestProject& project,
            const std::string& destFileName)
        {
            const std::filesystem::path sourcePath = project.GetEngineCubeSource();
            if (!std::filesystem::exists(sourcePath))
            {
                ME_LOG(LogTest, Error, 
                    "AssetManagerTest: engine cube source missing at '{}'.",
                    sourcePath.string());
                return {};
            }

            const std::filesystem::path destFile =
                project.GetTestContentDirectory() / destFileName;
            std::error_code copyError;
            std::filesystem::remove(destFile, copyError);
            copyError.clear();
            std::filesystem::copy_file(
                sourcePath,
                destFile,
                std::filesystem::copy_options::none,
                copyError);
            if (copyError)
            {
                ME_LOG(LogTest, Error, 
                    "AssetManagerTest: failed to copy test mesh: {}",
                    copyError.message());
                return {};
            }

            const AssetMeta meta = assetManager.RegisterAsset(destFile.string(), "StaticMesh");
            if (meta.AssetPath.empty())
            {
                ME_LOG(LogTest, Error, "AssetManagerTest: RegisterAsset failed for test mesh.");
                return {};
            }

            return meta.AssetPath;
        }

        bool TestCreateAssetKeepsSingleObjectIdentity()
        {
            AssetManagerTestProject project;
            AssetManager& assetManager = AssetManager::Get();

            const std::string directoryRel = "Assets/_P2UnitTest";
            std::shared_ptr<Material> created =
                assetManager.CreateAsset<Material>("CreateIdentityMat", directoryRel);
            if (!created)
            {
                ME_LOG(LogTest, Error, "AssetManagerTest: CreateAsset<Material> returned null.");
                return false;
            }

            const GUID createdGuid = created->GetGuid();
            const AssetMeta* meta = assetManager.FindAssetMetaByPath(
                created->GetMeta() != nullptr ? created->GetMeta()->AssetPath : std::string());
            if (meta == nullptr)
            {
                // Fall back: search by type+name under test dir.
                const std::vector<const AssetMeta*> materials =
                    assetManager.FindAssetMetasByType("Material");
                for (const AssetMeta* candidate : materials)
                {
                    if (candidate != nullptr && candidate->Guid == createdGuid)
                    {
                        meta = candidate;
                        break;
                    }
                }
            }

            if (meta == nullptr)
            {
                ME_LOG(LogTest, Error, "AssetManagerTest: CreateAsset meta not found.");
                return false;
            }

            if (meta->Guid != createdGuid)
            {
                ME_LOG(LogTest, Error, 
                    "AssetManagerTest: CreateAsset Guid mismatch object='{}' meta='{}'.",
                    createdGuid.ToString(),
                    meta->Guid.ToString());
                return false;
            }

            std::shared_ptr<Material> loaded = assetManager.LoadAsset<Material>(meta->AssetPath);
            if (loaded.get() != created.get())
            {
                ME_LOG(LogTest, Error, 
                    "AssetManagerTest: LoadAsset after Create returned a different object (Load swap).");
                return false;
            }

            return true;
        }

        bool TestDeleteAssetRemovesDiskAndRegistry()
        {
            AssetManagerTestProject project;
            AssetManager& assetManager = AssetManager::Get();

            const std::string relativePath =
                ImportTestMeshCopy(assetManager, project, "p2_delete_me.obj");
            if (relativePath.empty())
            {
                return false;
            }

            const AssetMeta* metaBefore = assetManager.FindAssetMetaByPath(relativePath);
            if (metaBefore == nullptr)
            {
                ME_LOG(LogTest, Error, "AssetManagerTest: imported mesh not registered.");
                return false;
            }

            const GUID guid = metaBefore->Guid;
            bool sawUnregistered = false;
            const uint32_t subscriptionId = assetManager.Subscribe(
                [&](const AssetRegistryChange& change)
                {
                    if (change.Kind == AssetRegistryChangeKind::Unregistered &&
                        change.OldPath == relativePath && change.Guid == guid)
                    {
                        sawUnregistered = true;
                    }
                });

            std::string deleteError;
            if (!assetManager.DeleteAsset(relativePath, deleteError))
            {
                ME_LOG(LogTest, Error, "AssetManagerTest: DeleteAsset failed: {}", deleteError);
                assetManager.Unsubscribe(subscriptionId);
                return false;
            }

            assetManager.Unsubscribe(subscriptionId);

            if (!sawUnregistered)
            {
                ME_LOG(LogTest, Error, "AssetManagerTest: expected Unregistered event.");
                return false;
            }

            if (assetManager.FindAssetMetaByPath(relativePath) != nullptr ||
                assetManager.FindAssetMetaByGuid(guid) != nullptr)
            {
                ME_LOG(LogTest, Error, "AssetManagerTest: registry still contains deleted asset.");
                return false;
            }

            const std::filesystem::path absolutePath =
                assetManager.ResolveAssetAbsolutePath(relativePath);
            const std::filesystem::path metaPath = absolutePath.string() + ".meta";
            if (std::filesystem::exists(absolutePath) || std::filesystem::exists(metaPath))
            {
                ME_LOG(LogTest, Error, "AssetManagerTest: deleted asset files still on disk.");
                return false;
            }

            return true;
        }

        bool TestMoveAndRenamePreserveGuid()
        {
            AssetManagerTestProject project;
            AssetManager& assetManager = AssetManager::Get();

            const std::string relativePath =
                ImportTestMeshCopy(assetManager, project, "p2_move_a.obj");
            if (relativePath.empty())
            {
                return false;
            }

            const AssetMeta* metaBefore = assetManager.FindAssetMetaByPath(relativePath);
            if (metaBefore == nullptr)
            {
                return false;
            }

            const GUID guid = metaBefore->Guid;
            const std::string movedPath = "_P2UnitTest/p2_move_b.obj";

            bool sawMoved = false;
            const uint32_t subscriptionId = assetManager.Subscribe(
                [&](const AssetRegistryChange& change)
                {
                    if (change.Kind == AssetRegistryChangeKind::Moved && change.Guid == guid &&
                        change.OldPath == relativePath && change.NewPath == movedPath)
                    {
                        sawMoved = true;
                    }
                });

            std::string moveError;
            if (!assetManager.MoveAsset(relativePath, movedPath, moveError))
            {
                ME_LOG(LogTest, Error, "AssetManagerTest: MoveAsset failed: {}", moveError);
                assetManager.Unsubscribe(subscriptionId);
                return false;
            }

            assetManager.Unsubscribe(subscriptionId);

            if (!sawMoved)
            {
                ME_LOG(LogTest, Error, "AssetManagerTest: expected Moved event.");
                return false;
            }

            const AssetMeta* metaAfter = assetManager.FindAssetMetaByGuid(guid);
            if (metaAfter == nullptr || metaAfter->AssetPath != movedPath)
            {
                ME_LOG(LogTest, Error, "AssetManagerTest: GUID lookup failed after move.");
                return false;
            }

            const std::string renamedPath = "_P2UnitTest/p2_move_renamed.obj";
            std::string renameError;
            if (!assetManager.RenameAsset(movedPath, "p2_move_renamed.obj", renameError))
            {
                ME_LOG(LogTest, Error, "AssetManagerTest: RenameAsset failed: {}", renameError);
                return false;
            }

            if (assetManager.FindAssetMetaByGuid(guid) == nullptr ||
                assetManager.FindAssetMetaByGuid(guid)->AssetPath != renamedPath)
            {
                ME_LOG(LogTest, Error, "AssetManagerTest: GUID lookup failed after rename.");
                return false;
            }

            return true;
        }

        bool TestMoveRejectsExtensionChange()
        {
            AssetManagerTestProject project;
            AssetManager& assetManager = AssetManager::Get();

            const std::string relativePath =
                ImportTestMeshCopy(assetManager, project, "p2_ext_guard.obj");
            if (relativePath.empty())
            {
                return false;
            }

            std::string moveError;
            if (assetManager.MoveAsset(relativePath, "_P2UnitTest/p2_ext_guard.txt", moveError))
            {
                ME_LOG(LogTest, Error, "AssetManagerTest: extension change move should fail.");
                return false;
            }

            if (moveError.empty())
            {
                ME_LOG(LogTest, Error, "AssetManagerTest: expected error message for extension change.");
                return false;
            }

            return true;
        }

        bool TestUnregisterAssetWithoutDeletingAssetFile()
        {
            AssetManagerTestProject project;
            AssetManager& assetManager = AssetManager::Get();

            const std::string relativePath =
                ImportTestMeshCopy(assetManager, project, "p2_unregister_me.obj");
            if (relativePath.empty())
            {
                return false;
            }

            const std::filesystem::path absolutePath =
                assetManager.ResolveAssetAbsolutePath(relativePath);
            const std::filesystem::path metaPath = absolutePath.string() + ".meta";

            std::string unregisterError;
            if (!assetManager.UnregisterAsset(relativePath, unregisterError))
            {
                ME_LOG(LogTest, Error, "AssetManagerTest: UnregisterAsset failed: {}", unregisterError);
                return false;
            }

            if (assetManager.FindAssetMetaByPath(relativePath) != nullptr)
            {
                ME_LOG(LogTest, Error, "AssetManagerTest: asset still registered after UnregisterAsset.");
                return false;
            }

            if (!std::filesystem::exists(absolutePath))
            {
                ME_LOG(LogTest, Error, "AssetManagerTest: UnregisterAsset should not delete the asset file.");
                return false;
            }

            if (std::filesystem::exists(metaPath))
            {
                ME_LOG(LogTest, Error, "AssetManagerTest: UnregisterAsset should remove the meta file.");
                return false;
            }

            std::error_code removeError;
            std::filesystem::remove(absolutePath, removeError);
            return true;
        }

        bool TestClearProjectRegistry()
        {
            AssetManagerTestProject project;
            AssetManager& assetManager = AssetManager::Get();

            const std::string relativePath =
                ImportTestMeshCopy(assetManager, project, "p2_clear_me.obj");
            if (relativePath.empty())
            {
                return false;
            }

            bool subscriberStillWorks = false;
            const uint32_t subscriptionId = assetManager.Subscribe(
                [&](const AssetRegistryChange&)
                {
                    subscriberStillWorks = true;
                });

            assetManager.ClearProjectRegistry();

            if (assetManager.FindAssetMetaByPath(relativePath) != nullptr)
            {
                ME_LOG(LogTest, Error, "AssetManagerTest: registry not empty after ClearProjectRegistry.");
                assetManager.Unsubscribe(subscriptionId);
                return false;
            }

            const std::filesystem::path reimportFile =
                project.GetTestContentDirectory() / "p2_after_clear.obj";
            std::error_code copyError;
            std::filesystem::copy_file(
                project.GetEngineCubeSource(),
                reimportFile,
                std::filesystem::copy_options::overwrite_existing,
                copyError);
            if (copyError)
            {
                ME_LOG(LogTest, Error, 
                    "AssetManagerTest: failed to stage mesh after clear: {}",
                    copyError.message());
                assetManager.Unsubscribe(subscriptionId);
                return false;
            }

            const AssetMeta reimportMeta =
                assetManager.RegisterAsset(reimportFile.string(), "StaticMesh");
            if (reimportMeta.AssetPath.empty())
            {
                ME_LOG(LogTest, Error, "AssetManagerTest: re-register after clear failed.");
                assetManager.Unsubscribe(subscriptionId);
                return false;
            }

            assetManager.Unsubscribe(subscriptionId);

            if (!subscriberStillWorks)
            {
                ME_LOG(LogTest, Error, "AssetManagerTest: subscriber was not invoked after clear.");
                return false;
            }

            std::error_code removeError;
            const std::filesystem::path absolutePath =
                assetManager.ResolveAssetAbsolutePath(reimportMeta.AssetPath);
            std::filesystem::remove(absolutePath, removeError);
            std::filesystem::remove(absolutePath.string() + ".meta", removeError);
            return true;
        }

        bool TestDeleteSceneAssetUnregistersScene()
        {
            AssetManagerTestProject project;
            AssetManager& assetManager = AssetManager::Get();
            SceneManager& sceneManager = SceneManager::Get();

            const std::filesystem::path scenePath =
                PathRegistry::Get().GetProjectContentRoot() / "Scenes";
            std::filesystem::create_directories(scenePath);

            const std::filesystem::path sceneFile = scenePath / "P2_DeleteMe.mescene";
            {
                std::ofstream sceneStream(sceneFile);
                sceneStream << "{\n    \"m_GameObjects\": []\n}\n";
            }

            AssetMeta sceneMeta =
                assetManager.RegisterAsset(sceneFile.string(), "Scene");
            if (sceneMeta.AssetPath.empty())
            {
                ME_LOG(LogTest, Error, "AssetManagerTest: failed to register scene asset.");
                return false;
            }

            sceneManager.RegisterScene(sceneMeta.AssetName, sceneMeta.AssetPath);
            if (!sceneManager.IsSceneRegistered(sceneMeta.AssetName))
            {
                ME_LOG(LogTest, Error, "AssetManagerTest: scene not registered in SceneManager.");
                return false;
            }

            std::string deleteError;
            if (!assetManager.DeleteAsset(sceneMeta.AssetPath, deleteError))
            {
                ME_LOG(LogTest, Error, "AssetManagerTest: scene DeleteAsset failed: {}", deleteError);
                return false;
            }

            if (sceneManager.IsSceneRegistered(sceneMeta.AssetName))
            {
                ME_LOG(LogTest, Error, "AssetManagerTest: scene still registered after delete.");
                return false;
            }

            return true;
        }

        bool RunAssetManagerSmokeTestsImpl(int argc, char** argv)
        {
            if (!EnsureEnginePathsReady(argc, argv))
            {
                return false;
            }

            AssetManagerTestScope scope;

            if (!TestCreateAssetKeepsSingleObjectIdentity())
            {
                return false;
            }

            if (!TestDeleteAssetRemovesDiskAndRegistry())
            {
                return false;
            }

            if (!TestUnregisterAssetWithoutDeletingAssetFile())
            {
                return false;
            }

            ME_LOG(LogTest, Info, "AssetManagerTest: smoke tests passed.");
            return true;
        }

        bool RunAssetManagerFullTestsImpl(int argc, char** argv)
        {
            if (!EnsureEnginePathsReady(argc, argv))
            {
                return false;
            }

            AssetManagerTestScope scope;

            if (!TestMoveAndRenamePreserveGuid())
            {
                return false;
            }

            if (!TestDeleteSceneAssetUnregistersScene())
            {
                return false;
            }

            ME_LOG(LogTest, Info, "AssetManagerTest: full tests passed.");
            return true;
        }
    }

    bool RunAssetManagerSmokeTests(int argc, char** argv)
    {
        return RunAssetManagerSmokeTestsImpl(argc, argv);
    }

    bool RunAssetManagerFullTests(int argc, char** argv)
    {
        return RunAssetManagerFullTestsImpl(argc, argv);
    }
}

#include "doctest.h"

#include "EngineTestFixture.h"

TEST_CASE("asset-manager: register delete unregister [smoke]")
{
    minEngine::EngineReflectionFixture fixture;
    REQUIRE(fixture.IsReflectionReady());
    CHECK(minEngine::RunAssetManagerSmokeTests(fixture.GetArgc(), fixture.GetArgv()));
}

TEST_CASE("asset-manager: move and scene delete [full]")
{
    minEngine::EngineReflectionFixture fixture;
    REQUIRE(fixture.IsReflectionReady());
    CHECK(minEngine::RunAssetManagerFullTests(fixture.GetArgc(), fixture.GetArgv()));
}
