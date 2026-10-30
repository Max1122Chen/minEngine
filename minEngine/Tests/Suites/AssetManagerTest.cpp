#include "AssetManagerTest.h"
#include "Access/SceneManagerTestAccess.h"
#include "Access/AssetManagerTestAccess.h"
#include "Access/ObjectManagerTestAccess.h"

#include "AssetManager.h"
#include "AssetRegistryTypes.h"
#include "Runtime/Core/Assert/Assert.h"
#include "Runtime/Core/Log/LogSystem.h"
#include "Runtime/Core/Object/ObjectManager.h"
#include "Runtime/Core/Paths/PathRegistry.h"
#include "Runtime/Core/Threading/FrameThreading.h"
#include "Runtime/Core/Reflection/Reflection.h"
#include "Runtime/EngineConfig.h"
#include "Runtime/Function/Framework/Scene/SceneManager.h"
#include "Runtime/Function/Render/Material.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <string_view>
#include <system_error>
#include <thread>
#include <vector>

namespace minEngine
{
    class AssetManagerTestScope
    {
    public:
        AssetManagerTestScope()
        {
            Testing::TestAccess<ObjectManager>::SetInstance(&m_ObjectManager);
            ObjectManager::Initialize();

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

            ObjectManager::Shutdown();
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

            const std::filesystem::path& GetProjectRoot() const
            {
                return m_ProjectRoot;
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
                ([&]() -> std::shared_ptr<Material> { std::string _createAssetError; return std::dynamic_pointer_cast<Material>(assetManager.CreateAssetByTypeId("Material", "CreateIdentityMat", directoryRel, _createAssetError)); })();
            if (!created)
            {
                ME_LOG(LogTest, Error, "AssetManagerTest: CreateAssetByTypeId(Material) returned null.");
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

            std::string loadError;
            std::shared_ptr<Material> loaded = std::dynamic_pointer_cast<Material>(
                assetManager.LoadAssetByPath(meta->AssetPath, loadError));
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

TEST_CASE("asset-manager: a sync load off the game role is counted and still loads [full]")
{
    using namespace minEngine;

    EngineReflectionFixture fixture;
    REQUIRE(fixture.IsReflectionReady());
    REQUIRE(EnsureEnginePathsReady(fixture.GetArgc(), fixture.GetArgv()));

    AssetManagerTestScope scope;
    AssetManagerTestProject project;

    AssetManager& assetManager = AssetManager::Get();
    std::string createError;
    std::shared_ptr<Material> created = std::dynamic_pointer_cast<Material>(
        assetManager.CreateAssetByTypeId("Material", "OffRoleSyncLoadMat", "Assets/_P2UnitTest", createError));
    REQUIRE(created != nullptr);

    const AssetMeta* meta = assetManager.FindAssetMetaByPath(
        created->GetMeta() != nullptr ? created->GetMeta()->AssetPath : std::string());
    REQUIRE(meta != nullptr);

    // Multi, so this thread owns the Game role and the worker below owns nothing: the wrong-thread
    // caller is a real one, not a simulated role.
    FrameThreading threading;
    threading.Initialize(FrameThreadMode::Multi);

    const uint64_t loadsBefore =
        ObjectManager::GetOffRoleCoreServiceCallCount(ECoreServiceEntry::SyncAssetLoad);
    const uint64_t recordsBefore = GetContractViolationCounters().Total();

    std::shared_ptr<Asset> loaded;
    std::string loadError;
    std::thread worker([&assetManager, &loaded, &loadError, path = meta->AssetPath]() {
        loaded = assetManager.LoadAssetByPath(path, loadError);
    });
    worker.join();

    CHECK(ObjectManager::GetOffRoleCoreServiceCallCount(ECoreServiceEntry::SyncAssetLoad)
        == loadsBefore + 1u);
    CHECK(GetContractViolationCounters().Total() > recordsBefore);

    // Behaviour unchanged: reported, not refused, so the load happened on the thread that asked.
    REQUIRE(loaded != nullptr);
    CHECK(loaded.get() == created.get());

    threading.Shutdown();
}


namespace minEngine
{
    namespace
    {
        /**
         * The algorithm the manager used before the cache existed, kept verbatim (only the log
         * lines are dropped). The equivalence test compares the cached implementation against
         * this copy, so a cache that silently changes case, separators, '..' folding or symlink
         * resolution fails instead of agreeing with itself.
         */
        std::string ReferenceNormalizeProjectRelativeAssetPath(const std::string& path)
        {
            if (path.empty())
            {
                return std::string();
            }

            const PathRegistry& paths = PathRegistry::Get();
            const std::filesystem::path& contentRoot = paths.GetProjectContentRoot();
            const std::filesystem::path input(path);

            std::filesystem::path absolutePath;
            if (input.is_absolute())
            {
                absolutePath = std::filesystem::weakly_canonical(input);
            }
            else if (!contentRoot.empty())
            {
                absolutePath = std::filesystem::weakly_canonical(contentRoot / input);
            }
            else
            {
                absolutePath = std::filesystem::weakly_canonical(std::filesystem::absolute(input));
            }

            const auto isInsideDirectory =
                [](const std::filesystem::path& absolutePathToTest,
                   const std::filesystem::path& rootDirectory)
            {
                if (rootDirectory.empty() || absolutePathToTest.empty())
                {
                    return false;
                }

                const std::filesystem::path canonicalRoot =
                    std::filesystem::weakly_canonical(rootDirectory);
                const std::filesystem::path canonicalPath =
                    std::filesystem::weakly_canonical(absolutePathToTest);
                const auto mismatchPair = std::mismatch(
                    canonicalRoot.begin(), canonicalRoot.end(), canonicalPath.begin());
                return mismatchPair.first == canonicalRoot.end();
            };

            if (isInsideDirectory(absolutePath, paths.GetEngineDefaultAssetsRoot()))
            {
                return std::string();
            }

            if (!contentRoot.empty() && isInsideDirectory(absolutePath, contentRoot))
            {
                std::error_code errorCode;
                const std::filesystem::path relativePath = std::filesystem::relative(
                    absolutePath,
                    std::filesystem::weakly_canonical(contentRoot),
                    errorCode);
                if (!errorCode)
                {
                    std::string result = relativePath.lexically_normal().generic_string();
                    if (result.size() >= 2 && result[0] == '.' && result[1] == '.')
                    {
                        return std::string();
                    }

                    return result;
                }
            }

            return std::string();
        }

        /** One path per shape the normaliser has to fold: separators, '..', case, tails, roots. */
        std::vector<std::string> BuildProjectRelativePathTable()
        {
            const PathRegistry& paths = PathRegistry::Get();
            const std::filesystem::path contentRoot = paths.GetProjectContentRoot();
            const std::filesystem::path engineDefaultRoot = paths.GetEngineDefaultAssetsRoot();

            const std::filesystem::path tableFile =
                contentRoot / "_P2UnitTest" / "NormalizeTable.mat";
            std::filesystem::create_directories(tableFile.parent_path());
            std::ofstream tableDummy(tableFile, std::ios::binary | std::ios::trunc);
            tableDummy.put('x');
            tableDummy.close();

            std::vector<std::string> table;
            table.push_back(std::string());
            table.push_back("Assets");
            table.push_back("_P2UnitTest");
            table.push_back("_P2UnitTest/NormalizeTable.mat");
            table.push_back("_P2UnitTest\\NormalizeTable.mat");
            table.push_back("./_P2UnitTest/NormalizeTable.mat");
            table.push_back("_P2UnitTest//NormalizeTable.mat");
            table.push_back("_P2UnitTest/./NormalizeTable.mat");
            table.push_back("_P2UnitTest/../_P2UnitTest/NormalizeTable.mat");
            table.push_back("_P2UnitTest/NormalizeTable.mat/");
            table.push_back("_P2UnitTest\\NormalizeTable.mat\\");
            table.push_back("_p2unittest/normalizetable.mat");
            table.push_back("_P2UnitTest/NORMALIZETABLE.MAT");
            table.push_back("_P2UnitTest/Missing/Deep.mat");
            table.push_back("../../outside.mat");
            table.push_back(tableFile.generic_string());
            table.push_back(tableFile.string());
            table.push_back((contentRoot / ".." / "_P2UnitTest" / "NormalizeTable.mat").string());
            table.push_back((engineDefaultRoot / "Meshes" / "BasicShapes" / "cube.obj").string());
            table.push_back(engineDefaultRoot.string());
            table.push_back("C:/Windows/not_an_asset.mat");
            return table;
        }

        std::string NormalizePathThroughManager(AssetManager& assetManager, const std::string& path)
        {
            return Testing::TestAccess<AssetManager>::NormalizeProjectRelativeAssetPath(
                assetManager,
                path);
        }

        Testing::AssetPathNormalizationStats GetNormalizationStats(const AssetManager& assetManager)
        {
            return Testing::TestAccess<AssetManager>::GetNormalizationStats(assetManager);
        }
    }
}

TEST_CASE("asset-manager: repeated normalisation resolves a path on disk once [full]")
{
    using namespace minEngine;

    EngineReflectionFixture fixture;
    REQUIRE(fixture.IsReflectionReady());
    REQUIRE(EnsureEnginePathsReady(fixture.GetArgc(), fixture.GetArgv()));

    AssetManagerTestScope scope;
    AssetManagerTestProject project;

    AssetManager& assetManager = AssetManager::Get();
    std::string createError;
    std::shared_ptr<Material> created = std::dynamic_pointer_cast<Material>(
        assetManager.CreateAssetByTypeId("Material", "NormalizeCachedMat", "Assets/_P2UnitTest", createError));
    REQUIRE(created != nullptr);
    REQUIRE(created->GetMeta() != nullptr);

    const std::string path = created->GetMeta()->AssetPath;
    REQUIRE(!path.empty());

    // Another spelling of the same file: creating the asset already normalised `path`, so the
    // cold call has to be one the memo has never seen.
    const size_t slashPosition = path.find('/');
    REQUIRE(slashPosition != std::string::npos);
    const std::string coldSpelling =
        path.substr(0, slashPosition + 1) + "./" + path.substr(slashPosition + 1);

    const Testing::AssetPathNormalizationStats before = GetNormalizationStats(assetManager);
    for (int repeat = 0; repeat < 33; ++repeat)
    {
        INFO("repeat: " << repeat);
        CHECK(NormalizePathThroughManager(assetManager, coldSpelling) == path);
    }
    const Testing::AssetPathNormalizationStats after = GetNormalizationStats(assetManager);

    INFO("input canonicalisations: "
        << (after.InputCanonicalizations - before.InputCanonicalizations)
        << ", root refreshes: " << (after.RootRefreshes - before.RootRefreshes)
        << ", hits: " << (after.CacheHits - before.CacheHits)
        << ", misses: " << (after.CacheMisses - before.CacheMisses));
    // 33 calls, one resolution against the disk, and no root resolved per call.
    CHECK(after.InputCanonicalizations - before.InputCanonicalizations == 1u);
    CHECK(after.RootRefreshes - before.RootRefreshes <= 1u);
    CHECK(after.CacheHits - before.CacheHits == 32u);
    CHECK(after.CacheMisses - before.CacheMisses == 1u);
    CHECK(after.CachedEntries == before.CachedEntries + 1u);
}

TEST_CASE("asset-manager: normalisation cache follows project root changes [full]")
{
    using namespace minEngine;

    EngineReflectionFixture fixture;
    REQUIRE(fixture.IsReflectionReady());
    REQUIRE(EnsureEnginePathsReady(fixture.GetArgc(), fixture.GetArgv()));

    AssetManagerTestScope scope;
    AssetManagerTestProject project;

    AssetManager& assetManager = AssetManager::Get();
    const std::filesystem::path projectRoot = project.GetProjectRoot();
    const std::filesystem::path otherRoot = projectRoot.parent_path() / "ProjectOther";
    std::filesystem::create_directories(otherRoot / "Assets");

    const std::filesystem::path probeFile = project.GetTestContentDirectory() / "RootSwitch.mat";
    std::ofstream probeStream(probeFile, std::ios::binary | std::ios::trunc);
    probeStream.put('x');
    probeStream.close();

    const std::string probeAbsolute = probeFile.string();
    const std::string expectedRelative = "_P2UnitTest/RootSwitch.mat";

    REQUIRE(NormalizePathThroughManager(assetManager, probeAbsolute) == expectedRelative);
    const Testing::AssetPathNormalizationStats before = GetNormalizationStats(assetManager);

    CHECK(NormalizePathThroughManager(assetManager, probeAbsolute) == expectedRelative);

    // The same absolute path is no longer inside the content root: a cached answer would still
    // report the old relative path.
    PathRegistry::Get().SetProjectRoots(otherRoot);
    CHECK(NormalizePathThroughManager(assetManager, probeAbsolute).empty());
    CHECK(NormalizePathThroughManager(assetManager, probeAbsolute).empty());

    // Switching back must recompute as well, not resurrect the answer from before the switch.
    PathRegistry::Get().SetProjectRoots(projectRoot);
    CHECK(NormalizePathThroughManager(assetManager, probeAbsolute) == expectedRelative);

    const Testing::AssetPathNormalizationStats after = GetNormalizationStats(assetManager);
    INFO("input canonicalisations: "
        << (after.InputCanonicalizations - before.InputCanonicalizations)
        << ", root refreshes: " << (after.RootRefreshes - before.RootRefreshes)
        << ", hits: " << (after.CacheHits - before.CacheHits)
        << ", misses: " << (after.CacheMisses - before.CacheMisses));
    // Two of the four calls are real misses (one per root set), two are memo hits, and each root
    // swap refreshes the roots exactly once.
    CHECK(after.InputCanonicalizations - before.InputCanonicalizations == 2u);
    CHECK(after.RootRefreshes - before.RootRefreshes == 2u);
    CHECK(after.CacheHits - before.CacheHits == 2u);
    CHECK(after.CacheMisses - before.CacheMisses == 2u);
}

TEST_CASE("asset-manager: normalised paths match the pre-cache algorithm [full]")
{
    using namespace minEngine;

    EngineReflectionFixture fixture;
    REQUIRE(fixture.IsReflectionReady());
    REQUIRE(EnsureEnginePathsReady(fixture.GetArgc(), fixture.GetArgv()));

    AssetManagerTestScope scope;
    AssetManagerTestProject project;

    AssetManager& assetManager = AssetManager::Get();
    const std::vector<std::string> pathTable = BuildProjectRelativePathTable();
    REQUIRE(pathTable.size() >= 20u);

    for (const std::string& path : pathTable)
    {
        const std::string expected = ReferenceNormalizeProjectRelativeAssetPath(path);
        const std::string actual = NormalizePathThroughManager(assetManager, path);
        INFO("path: '" << path << "'");
        CHECK(actual == expected);

        if (!actual.empty())
        {
            INFO("normalised: '" << actual << "'");
            CHECK(NormalizePathThroughManager(assetManager, actual) == actual);
        }
    }

    // Fixed anchors: the table above only proves the two implementations agree, so pin the
    // shapes the cache must fold exactly.
    CHECK(NormalizePathThroughManager(assetManager, "_P2UnitTest/NormalizeTable.mat")
        == "_P2UnitTest/NormalizeTable.mat");
    CHECK(NormalizePathThroughManager(assetManager, "_P2UnitTest\\NormalizeTable.mat")
        == "_P2UnitTest/NormalizeTable.mat");
    CHECK(NormalizePathThroughManager(assetManager, "_P2UnitTest/../_P2UnitTest/./NormalizeTable.mat")
        == "_P2UnitTest/NormalizeTable.mat");
    CHECK(NormalizePathThroughManager(assetManager, "_P2UnitTest/NormalizeTable.mat/")
        == "_P2UnitTest/NormalizeTable.mat");
    CHECK(NormalizePathThroughManager(assetManager, "../outside.mat").empty());
    CHECK(NormalizePathThroughManager(assetManager, "").empty());
}

TEST_CASE("asset-manager: a sync load normalises the registry path once [full]")
{
    using namespace minEngine;

    EngineReflectionFixture fixture;
    REQUIRE(fixture.IsReflectionReady());
    REQUIRE(EnsureEnginePathsReady(fixture.GetArgc(), fixture.GetArgv()));

    AssetManagerTestScope scope;
    AssetManagerTestProject project;

    AssetManager& assetManager = AssetManager::Get();
    const std::filesystem::path projectRoot = project.GetProjectRoot();
    const std::filesystem::path otherRoot = projectRoot.parent_path() / "ProjectLoadOther";
    std::filesystem::create_directories(otherRoot / "Assets" / "_P2UnitTest");

    std::string createError;
    std::shared_ptr<Asset> created = assetManager.CreateAssetByTypeId(
        "Material", "SingleNormalizeMat", "Assets/_P2UnitTest", createError);
    REQUIRE(created != nullptr);
    REQUIRE(created->GetMeta() != nullptr);

    const std::string path = created->GetMeta()->AssetPath;
    REQUIRE(!path.empty());

    // Resolve once under another root set, so the memo belongs to that root when the load below
    // starts: switching back then forces a real normalisation instead of a warm entry.
    PathRegistry::Get().SetProjectRoots(otherRoot);
    REQUIRE(NormalizePathThroughManager(assetManager, path) == path);
    PathRegistry::Get().SetProjectRoots(projectRoot);

    const Testing::AssetPathNormalizationStats before = GetNormalizationStats(assetManager);
    std::string loadError;
    std::shared_ptr<Asset> loaded = assetManager.LoadAssetByPath(path, loadError);
    REQUIRE_MESSAGE(loaded != nullptr, loadError);
    const Testing::AssetPathNormalizationStats after = GetNormalizationStats(assetManager);

    INFO("input canonicalisations: "
        << (after.InputCanonicalizations - before.InputCanonicalizations)
        << ", root refreshes: " << (after.RootRefreshes - before.RootRefreshes));
    CHECK(after.InputCanonicalizations - before.InputCanonicalizations == 1u);

    // Warm memos answer the second load without touching the filesystem again.
    const std::shared_ptr<Asset> reloaded = assetManager.LoadAssetByPath(path, loadError);
    REQUIRE(reloaded != nullptr);
    CHECK(reloaded.get() == loaded.get());
    CHECK(GetNormalizationStats(assetManager).InputCanonicalizations
        == after.InputCanonicalizations);
}

