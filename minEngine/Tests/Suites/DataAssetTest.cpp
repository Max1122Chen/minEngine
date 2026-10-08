#include "DataAssetTest.h"

#include "Access/AssetManagerTestAccess.h"
#include "Access/ObjectManagerTestAccess.h"
#include "Access/SceneManagerTestAccess.h"

#include "AssetManager.h"
#include "Runtime/Core/Log/LogSystem.h"
#include "Runtime/Core/Object/ObjectCast.h"
#include "Runtime/Core/Object/ObjectManager.h"
#include "Runtime/Core/Paths/PathRegistry.h"
#include "Runtime/EngineConfig.h"
#include "Runtime/Function/Framework/Scene/SceneManager.h"
#include "Runtime/Resource/DataAsset.h"

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

            const std::string assetPath = meta->AssetPath;
            const GUID createdGuid = created->GetGuid();
            const std::string expectedNotes = "f07-roundtrip-notes";
            created->SetNotes(expectedNotes);
            if (!assetManager.SaveAsset(assetPath, *created))
            {
                ME_LOG(LogTest, Error, "DataAssetTest: SaveAsset failed.");
                return false;
            }

            // Drop strong refs so weak cache expires and Load re-reads disk.
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
