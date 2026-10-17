#include "DataAssetTest.h"

#include "Access/AssetManagerTestAccess.h"
#include "Access/ObjectManagerTestAccess.h"
#include "Access/SceneManagerTestAccess.h"

#include "AssetManager.h"
#include "Runtime/Core/Log/LogSystem.h"
#include "Runtime/Core/Object/ObjectCast.h"
#include "Runtime/Core/Object/ObjectManager.h"
#include "Runtime/Core/Paths/PathRegistry.h"
#include "Runtime/Core/Reflection/ReflectionUtils.h"
#include "Runtime/EngineConfig.h"
#include "Runtime/Function/Framework/Scene/SceneManager.h"
#include "Runtime/Resource/DataAsset.h"

#if MINENGINE_LINK_GAME_MODULE_STUB
#include "SampleTableDataAsset.h"
#endif

#include <filesystem>
#include <string>

namespace minEngine
{
    class DataAssetTestScope
    {
    public:
        DataAssetTestScope()
        {
            Testing::TestAccess<ObjectManager>::SetInstance(&m_ObjectManager);
            Testing::TestAccess<SceneManager>::SetInstance(&m_SceneManager);
            Testing::TestAccess<AssetManager>::SetInstance(&m_AssetManager);
            m_AssetManager.Initialize();
        }

        ~DataAssetTestScope()
        {
            m_AssetManager.Shutdown();
            Testing::TestAccess<AssetManager>::SetInstance(nullptr);
            Testing::TestAccess<SceneManager>::SetInstance(nullptr);
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

            ME_LOG(LogTest, Error, "DataAssetTest: failed to load engine configuration.");
            return false;
        }

        class DataAssetTestProject
        {
        public:
            DataAssetTestProject()
            {
                const std::filesystem::path tempRoot =
                    std::filesystem::temp_directory_path() / "minEngine_DataAssetTest";
                std::error_code removeError;
                std::filesystem::remove_all(tempRoot, removeError);

                m_ProjectRoot = tempRoot / "Project";
                std::filesystem::create_directories(m_ProjectRoot / "Assets" / "_DataAssetUnitTest");
                PathRegistry::Get().SetProjectRoots(m_ProjectRoot);
            }

            ~DataAssetTestProject()
            {
                PathRegistry::Get().ClearProjectRoots();
                std::error_code removeError;
                std::filesystem::remove_all(
                    std::filesystem::temp_directory_path() / "minEngine_DataAssetTest",
                    removeError);
            }

        private:
            std::filesystem::path m_ProjectRoot;
        };

        bool TestCreateSaveReloadNotesRoundtrip()
        {
            DataAssetTestProject project;
            AssetManager& assetManager = AssetManager::Get();

            const std::string directoryRel = "Assets/_DataAssetUnitTest";
            std::string createError;
            std::shared_ptr<Asset> createdBase = assetManager.CreateAssetByTypeId(
                "DataAsset", "RoundtripNotes", directoryRel, createError);
            std::shared_ptr<DataAsset> created = Cast<DataAsset>(createdBase);
            if (!created)
            {
                ME_LOG(LogTest, Error,
                    "DataAssetTest: CreateAssetByTypeId failed ({})",
                    createError);
                return false;
            }

            const AssetMeta* meta = created->GetMeta();
            if (meta == nullptr || meta->AssetPath.empty())
            {
                ME_LOG(LogTest, Error, "DataAssetTest: created asset missing meta.");
                return false;
            }

            if (meta->AssetType != "DataAsset")
            {
                ME_LOG(LogTest, Error, "DataAssetTest: unexpected AssetType '{}'.", meta->AssetType);
                return false;
            }

            const std::string expectedClassName = Reflection::GetClassName<DataAsset>();
            if (meta->RuntimeClassName != expectedClassName)
            {
                ME_LOG(
                    LogTest,
                    Error,
                    "DataAssetTest: RuntimeClassName after create got='{}' expected='{}'.",
                    meta->RuntimeClassName,
                    expectedClassName);
                return false;
            }

            const std::string assetPath = meta->AssetPath;
            const GUID createdGuid = created->GetGuid();
            const std::string expectedNotes = "f07-roundtrip-notes";
            created->SetNotes(expectedNotes);
            if (!assetManager.SaveAsset(assetPath, *created))
            {
                ME_LOG(LogTest, Error, "DataAssetTest: SaveAsset failed.");
                return false;
            }

            created.reset();
            createdBase.reset();

            std::string loadError;
            std::shared_ptr<DataAsset> reloaded = Cast<DataAsset>(
                assetManager.LoadAssetByPath(assetPath, loadError));
            if (!reloaded)
            {
                ME_LOG(LogTest, Error, "DataAssetTest: reload failed ({})", loadError);
                return false;
            }

            if (reloaded->GetNotes() != expectedNotes)
            {
                ME_LOG(LogTest, Error,
                    "DataAssetTest: notes mismatch got='{}' expected='{}'.",
                    reloaded->GetNotes(),
                    expectedNotes);
                return false;
            }

            if (reloaded->GetGuid() != createdGuid)
            {
                ME_LOG(LogTest, Error, "DataAssetTest: Guid changed across reload.");
                return false;
            }

            return true;
        }

        bool TestMissingRuntimeClassNameFailsForDataAsset()
        {
            DataAssetTestProject project;
            AssetManager& assetManager = AssetManager::Get();

            std::string createError;
            std::shared_ptr<Asset> created = assetManager.CreateAssetByTypeId(
                "DataAsset", "MissingClassName", "Assets/_DataAssetUnitTest", createError);
            if (!created)
            {
                ME_LOG(LogTest, Error, "DataAssetTest: create failed ({})", createError);
                return false;
            }

            const std::string assetPath = created->GetMeta()->AssetPath;
            created.reset();
            assetManager.EvictLoadedAssetCache(assetPath);

            AssetMeta* liveMeta = const_cast<AssetMeta*>(assetManager.FindAssetMetaByPath(assetPath));
            if (liveMeta == nullptr)
            {
                ME_LOG(LogTest, Error, "DataAssetTest: meta missing for missing-class-name case.");
                return false;
            }

            liveMeta->RuntimeClassName.clear();
            if (!assetManager.WriteOrUpdateMetaFile(*liveMeta))
            {
                ME_LOG(LogTest, Error, "DataAssetTest: failed to clear RuntimeClassName on disk.");
                return false;
            }

            std::string loadError;
            std::shared_ptr<Asset> reloaded = assetManager.LoadAssetByPath(assetPath, loadError);
            if (reloaded != nullptr)
            {
                ME_LOG(LogTest, Error, "DataAssetTest: expected load failure when RuntimeClassName missing.");
                return false;
            }

            if (loadError.empty())
            {
                ME_LOG(LogTest, Error, "DataAssetTest: expected error message for missing RuntimeClassName.");
                return false;
            }

            return true;
        }

        bool TestRecoverableAssetHealsMissingRuntimeClassName()
        {
            DataAssetTestProject project;
            AssetManager& assetManager = AssetManager::Get();

            std::string createError;
            std::shared_ptr<Asset> created = assetManager.CreateAssetByTypeId(
                "Prefab", "HealClassName", "Assets/_DataAssetUnitTest", createError);
            if (!created)
            {
                ME_LOG(LogTest, Error, "DataAssetTest: Prefab create failed ({})", createError);
                return false;
            }

            const AssetMeta* meta = created->GetMeta();
            if (meta == nullptr || meta->RuntimeClassName.empty())
            {
                ME_LOG(LogTest, Error, "DataAssetTest: Prefab create did not write RuntimeClassName.");
                return false;
            }

            const std::string assetPath = meta->AssetPath;
            const std::string expectedClassName = meta->RuntimeClassName;
            created.reset();
            assetManager.EvictLoadedAssetCache(assetPath);

            AssetMeta* liveMeta = const_cast<AssetMeta*>(assetManager.FindAssetMetaByPath(assetPath));
            if (liveMeta == nullptr)
            {
                ME_LOG(LogTest, Error, "DataAssetTest: Prefab meta missing.");
                return false;
            }

            liveMeta->RuntimeClassName.clear();
            if (!assetManager.WriteOrUpdateMetaFile(*liveMeta))
            {
                ME_LOG(LogTest, Error, "DataAssetTest: failed to clear Prefab RuntimeClassName.");
                return false;
            }

            std::string loadError;
            std::shared_ptr<Asset> reloaded = assetManager.LoadAssetByPath(assetPath, loadError);
            if (!reloaded)
            {
                ME_LOG(LogTest, Error, "DataAssetTest: Prefab heal load failed ({})", loadError);
                return false;
            }

            const AssetMeta* healedMeta = assetManager.FindAssetMetaByPath(assetPath);
            if (healedMeta == nullptr || healedMeta->RuntimeClassName != expectedClassName)
            {
                ME_LOG(
                    LogTest,
                    Error,
                    "DataAssetTest: Prefab RuntimeClassName not healed got='{}'.",
                    healedMeta != nullptr ? healedMeta->RuntimeClassName : "<null>");
                return false;
            }

            return true;
        }

        bool TestRuntimeClassNameMismatchFails()
        {
            DataAssetTestProject project;
            AssetManager& assetManager = AssetManager::Get();

            std::string createError;
            std::shared_ptr<Asset> created = assetManager.CreateAssetByTypeId(
                "Prefab", "MismatchClassName", "Assets/_DataAssetUnitTest", createError);
            if (!created)
            {
                ME_LOG(LogTest, Error, "DataAssetTest: Prefab create failed ({})", createError);
                return false;
            }

            const std::string assetPath = created->GetMeta()->AssetPath;
            created.reset();
            assetManager.EvictLoadedAssetCache(assetPath);

            AssetMeta* liveMeta = const_cast<AssetMeta*>(assetManager.FindAssetMetaByPath(assetPath));
            if (liveMeta == nullptr)
            {
                ME_LOG(LogTest, Error, "DataAssetTest: Prefab meta missing for mismatch case.");
                return false;
            }

            liveMeta->RuntimeClassName = "NotARealAssetClass";
            if (!assetManager.WriteOrUpdateMetaFile(*liveMeta))
            {
                ME_LOG(LogTest, Error, "DataAssetTest: failed to write mismatched RuntimeClassName.");
                return false;
            }

            std::string loadError;
            std::shared_ptr<Asset> reloaded = assetManager.LoadAssetByPath(assetPath, loadError);
            if (reloaded != nullptr)
            {
                ME_LOG(LogTest, Error, "DataAssetTest: expected load failure on RuntimeClassName mismatch.");
                return false;
            }

            return true;
        }

#if MINENGINE_LINK_GAME_MODULE_STUB
        bool TestDerivedTableRoundtrip()
        {
            DataAssetTestProject project;
            AssetManager& assetManager = AssetManager::Get();

            (void)GameModuleStub::SampleTableDataAsset::StaticClass();

            std::string createError;
            std::shared_ptr<Asset> createdBase = assetManager.CreateDataAsset(
                GameModuleStub::SampleTableDataAsset::StaticClass(),
                "SampleTable",
                "Assets/_DataAssetUnitTest",
                createError);
            std::shared_ptr<GameModuleStub::SampleTableDataAsset> created =
                Cast<GameModuleStub::SampleTableDataAsset>(createdBase);
            if (!created)
            {
                ME_LOG(LogTest, Error, "DataAssetTest: CreateDataAsset failed ({})", createError);
                return false;
            }

            const AssetMeta* meta = created->GetMeta();
            if (meta == nullptr)
            {
                ME_LOG(LogTest, Error, "DataAssetTest: derived create missing meta.");
                return false;
            }

            const std::string expectedClassName =
                Reflection::GetClassName<GameModuleStub::SampleTableDataAsset>();
            if (meta->AssetType != "DataAsset" || meta->RuntimeClassName != expectedClassName)
            {
                ME_LOG(
                    LogTest,
                    Error,
                    "DataAssetTest: derived meta AssetType='{}' RuntimeClassName='{}'.",
                    meta->AssetType,
                    meta->RuntimeClassName);
                return false;
            }

            GameModuleStub::SampleTableRow row;
            row.Id = "stone";
            row.Value = 42;
            created->SetRows({row});

            const std::string assetPath = meta->AssetPath;
            if (!assetManager.SaveAsset(assetPath, *created))
            {
                ME_LOG(LogTest, Error, "DataAssetTest: derived SaveAsset failed.");
                return false;
            }

            created.reset();
            createdBase.reset();
            assetManager.EvictLoadedAssetCache(assetPath);

            std::string loadError;
            std::shared_ptr<GameModuleStub::SampleTableDataAsset> reloaded =
                Cast<GameModuleStub::SampleTableDataAsset>(
                    assetManager.LoadAssetByPath(assetPath, loadError));
            if (!reloaded)
            {
                ME_LOG(LogTest, Error, "DataAssetTest: derived reload/cast failed ({})", loadError);
                return false;
            }

            if (reloaded->GetRows().size() != 1
                || reloaded->GetRows()[0].Id != "stone"
                || reloaded->GetRows()[0].Value != 42)
            {
                ME_LOG(LogTest, Error, "DataAssetTest: derived rows mismatch after reload.");
                return false;
            }

            return true;
        }
#endif

        bool RunDataAssetSmokeTestsImpl(int argc, char** argv)
        {
            if (!EnsureEnginePathsReady(argc, argv))
            {
                return false;
            }

            DataAssetTestScope scope;
            if (!TestCreateSaveReloadNotesRoundtrip())
            {
                return false;
            }
            if (!TestMissingRuntimeClassNameFailsForDataAsset())
            {
                return false;
            }
            if (!TestRecoverableAssetHealsMissingRuntimeClassName())
            {
                return false;
            }
            if (!TestRuntimeClassNameMismatchFails())
            {
                return false;
            }
#if MINENGINE_LINK_GAME_MODULE_STUB
            if (!TestDerivedTableRoundtrip())
            {
                return false;
            }
#endif

            ME_LOG(LogTest, Info, "DataAssetTest: smoke tests passed.");
            return true;
        }
    }

    bool RunDataAssetSmokeTests(int argc, char** argv)
    {
        return RunDataAssetSmokeTestsImpl(argc, argv);
    }
}

#include "doctest.h"

#include "EngineTestFixture.h"

TEST_CASE("data-asset: create save reload notes [smoke]")
{
    minEngine::EngineReflectionFixture fixture;
    REQUIRE(fixture.IsReflectionReady());
    CHECK(minEngine::RunDataAssetSmokeTests(fixture.GetArgc(), fixture.GetArgv()));
}
