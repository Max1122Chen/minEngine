#include "AssetManager.h"

#include "Runtime/Core/Log/LogSystem.h"
#include "Runtime/Core/Object/ObjectCast.h"
#include "Runtime/Core/Object/ObjectManager.h"
#include "Runtime/Core/Reflection/Reflection.h"
#include "Runtime/Core/Serialization/Serializer.h"
#include "Runtime/Core/Tasks/TaskSystem.h"
#include "Runtime/Resource/AssetSerializeHelper.h"
#include "Runtime/Resource/DataAsset.h"

#include <thread>
#include <utility>

namespace minEngine
{
    struct AssetManager::AsyncDataAssetWorkerResult
    {
        Json json;
        std::string error;
        bool ok = false;
    };

    struct AssetManager::AsyncDataAssetLoadRecord
    {
        std::string registryKey;
        AssetMeta meta;
        TaskHandle workerHandle{};
        std::shared_ptr<AsyncDataAssetWorkerResult> workerResult;
        std::vector<AsyncLoadAssetCallback> waiters;
        std::shared_ptr<Asset> assembled;
        std::string assembleError;
        bool bAssembleAttempted = false;
        bool bFailed = false;
        bool bCallbacksDispatched = false;
    };

    void AssetManager::FlushAsyncLoadsForShutdown()
    {
        if (m_InFlightDataAssetLoads.empty())
        {
            return;
        }

        TaskSystem* taskSystem = TaskSystem::HasInstance() ? &TaskSystem::Get() : nullptr;
        for (auto& [key, record] : m_InFlightDataAssetLoads)
        {
            (void)key;
            if (record == nullptr)
            {
                continue;
            }

            if (taskSystem != nullptr && taskSystem->IsRunning() && record->workerHandle.IsValid())
            {
                taskSystem->Wait(record->workerHandle);
            }

            if (!record->bCallbacksDispatched && !record->waiters.empty())
            {
                ME_LOG(
                    LogAsset,
                    Warn,
                    "FlushAsyncLoadsForShutdown: dropping {} undispatched callback(s) for '{}'.",
                    record->waiters.size(),
                    record->registryKey);
                record->waiters.clear();
                record->bCallbacksDispatched = true;
            }
        }

        m_InFlightDataAssetLoads.clear();
    }

    std::shared_ptr<Asset> AssetManager::AssembleDataAssetFromJson(
        const AssetMeta& meta,
        const Json& root,
        std::string& outErrorMessage)
    {
        outErrorMessage.clear();

        if (meta.RuntimeClassName.empty())
        {
            outErrorMessage = "DataAsset load requires RuntimeClassName in meta";
            return nullptr;
        }

        const Reflection::MEClass* classInfo =
            Reflection::ReflectionSystem::Get().FindClass(meta.RuntimeClassName);
        if (classInfo == nullptr)
        {
            outErrorMessage = "DataAsset RuntimeClassName not found: " + meta.RuntimeClassName;
            return nullptr;
        }

        if (!classInfo->IsA(DataAsset::StaticClass()))
        {
            outErrorMessage = "RuntimeClassName is not a DataAsset: " + meta.RuntimeClassName;
            return nullptr;
        }

        std::shared_ptr<MEObject> created =
            NewObject(classInfo, meta.AssetName, nullptr, meta.Guid);
        std::shared_ptr<DataAsset> asset = Cast<DataAsset>(created);
        if (!asset)
        {
            outErrorMessage = "failed to construct DataAsset instance";
            return nullptr;
        }

        if (!AssetSerializeHelper::ReadObjectFromJson(root, *asset, &outErrorMessage))
        {
            return nullptr;
        }

        ApplyMetaIdentity(*asset, meta);
        return asset;
    }

    void AssetManager::TryAssembleInFlightDataAsset(const std::shared_ptr<AsyncDataAssetLoadRecord>& record)
    {
        if (record == nullptr || record->bAssembleAttempted)
        {
            return;
        }

        record->bAssembleAttempted = true;

        if (record->workerResult == nullptr || !record->workerResult->ok)
        {
            record->bFailed = true;
            record->assembleError = record->workerResult != nullptr
                ? record->workerResult->error
                : "async DataAsset worker result missing";
            return;
        }

        std::string assembleError;
        std::shared_ptr<Asset> asset =
            AssembleDataAssetFromJson(record->meta, record->workerResult->json, assembleError);
        if (!asset)
        {
            record->bFailed = true;
            record->assembleError = assembleError.empty() ? "assemble failed" : assembleError;
            return;
        }

        if (!ValidateOrHealRuntimeClassNameAfterLoad(record->meta, *asset, assembleError))
        {
            record->bFailed = true;
            record->assembleError = assembleError;
            return;
        }

        if (!record->registryKey.empty())
        {
            m_LoadedAssetCache[record->registryKey] = asset;
            const AssetMeta* registryMeta = FindAssetMetaByPath(record->registryKey);
            if (registryMeta != nullptr)
            {
                asset->SetMeta(const_cast<AssetMeta*>(registryMeta));
            }
        }

        record->assembled = std::move(asset);
        record->bFailed = false;
    }

    void AssetManager::FinishInFlightDataAsset(const std::shared_ptr<AsyncDataAssetLoadRecord>& record)
    {
        if (record == nullptr || record->bCallbacksDispatched)
        {
            return;
        }

        TryAssembleInFlightDataAsset(record);
        record->bCallbacksDispatched = true;

        const bool bSucceeded = !record->bFailed && record->assembled != nullptr;
        const std::string_view error = bSucceeded
            ? std::string_view{}
            : std::string_view{record->assembleError};
        std::shared_ptr<Asset> asset = bSucceeded ? record->assembled : nullptr;

        std::vector<AsyncLoadAssetCallback> waiters = std::move(record->waiters);
        record->waiters.clear();

        for (AsyncLoadAssetCallback& waiter : waiters)
        {
            if (!waiter)
            {
                continue;
            }
            waiter(asset, bSucceeded, error);
        }

        if (!record->registryKey.empty())
        {
            m_InFlightDataAssetLoads.erase(record->registryKey);
        }
    }

    std::shared_ptr<Asset> AssetManager::JoinInFlightDataAssetLoad(
        const std::shared_ptr<AsyncDataAssetLoadRecord>& record,
        std::string& outErrorMessage)
    {
        outErrorMessage.clear();
        if (record == nullptr)
        {
            outErrorMessage = "in-flight DataAsset record missing";
            return nullptr;
        }

        TaskSystem* taskSystem = TaskSystem::HasInstance() ? &TaskSystem::Get() : nullptr;
        if (taskSystem != nullptr && taskSystem->IsRunning()
            && taskSystem->GetGameThreadId() != std::this_thread::get_id())
        {
            outErrorMessage =
                "sync Load joining in-flight async DataAsset must run on the game thread";
            return nullptr;
        }

        if (taskSystem != nullptr && taskSystem->IsRunning() && record->workerHandle.IsValid())
        {
            taskSystem->Wait(record->workerHandle);
        }

        TryAssembleInFlightDataAsset(record);
        if (record->bFailed || !record->assembled)
        {
            outErrorMessage = record->assembleError.empty()
                ? "async DataAsset assemble failed"
                : record->assembleError;
            return nullptr;
        }

        return record->assembled;
    }

    AsyncLoadHandle AssetManager::EnqueueAsyncLoadFailure(
        AsyncLoadAssetCallback onComplete,
        std::string errorMessage)
    {
        if (!TaskSystem::HasInstance() || !TaskSystem::Get().IsRunning())
        {
            return AsyncLoadHandle{};
        }

        TaskSystem& taskSystem = TaskSystem::Get();
        if (taskSystem.GetGameThreadId() != std::this_thread::get_id())
        {
            ME_LOG(LogAsset, Error, "AsyncLoadAsset rejected: not on the game thread.");
            return AsyncLoadHandle{};
        }

        const TaskHandle handle = taskSystem.Submit(
            []() {},
            [callback = std::move(onComplete), error = std::move(errorMessage)](bool /*bSucceeded*/) {
                if (callback)
                {
                    callback(nullptr, false, error);
                }
            });

        if (!handle.IsValid())
        {
            return AsyncLoadHandle{};
        }

        // Same id space as TaskHandle so callers can TaskSystem::Wait(TaskHandle{handle.Id}).
        return AsyncLoadHandle{handle.Id};
    }

    AsyncLoadHandle AssetManager::EnqueueAsyncLoadCacheHit(
        std::shared_ptr<Asset> asset,
        AsyncLoadAssetCallback onComplete)
    {
        if (!TaskSystem::HasInstance() || !TaskSystem::Get().IsRunning())
        {
            return AsyncLoadHandle{};
        }

        TaskSystem& taskSystem = TaskSystem::Get();
        if (taskSystem.GetGameThreadId() != std::this_thread::get_id())
        {
            ME_LOG(LogAsset, Error, "AsyncLoadAsset rejected: not on the game thread.");
            return AsyncLoadHandle{};
        }

        const TaskHandle handle = taskSystem.Submit(
            []() {},
            [callback = std::move(onComplete), asset = std::move(asset)](bool /*bSucceeded*/) {
                if (callback)
                {
                    callback(asset, asset != nullptr, asset != nullptr ? std::string_view{} : "cache hit was null");
                }
            });

        if (!handle.IsValid())
        {
            return AsyncLoadHandle{};
        }

        return AsyncLoadHandle{handle.Id};
    }

    AsyncLoadHandle AssetManager::AsyncLoadAssetByMeta_Internal(
        const AssetMeta& meta,
        AsyncLoadAssetCallback onComplete)
    {
        if (!TaskSystem::HasInstance() || !TaskSystem::Get().IsRunning())
        {
            ME_LOG(LogAsset, Error, "AsyncLoadAsset rejected: TaskSystem is not available/running.");
            return AsyncLoadHandle{};
        }

        TaskSystem& taskSystem = TaskSystem::Get();
        if (taskSystem.GetGameThreadId() != std::this_thread::get_id())
        {
            ME_LOG(LogAsset, Error, "AsyncLoadAsset rejected: not on the game thread.");
            return AsyncLoadHandle{};
        }

        if (meta.AssetType != "DataAsset")
        {
            return EnqueueAsyncLoadFailure(
                std::move(onComplete),
                "AsyncLoadAsset MVP accepts DataAsset only (got '" + meta.AssetType + "')");
        }

        const std::string registryKey = NormalizeProjectRelativeAssetPath(meta.AssetPath);
        if (registryKey.empty())
        {
            return EnqueueAsyncLoadFailure(std::move(onComplete), "AsyncLoadAsset: empty asset path");
        }

        if (!registryKey.empty())
        {
            const auto cacheIt = m_LoadedAssetCache.find(registryKey);
            if (cacheIt != m_LoadedAssetCache.end())
            {
                std::shared_ptr<MEObject> cachedObject = cacheIt->second.lock();
                if (cachedObject)
                {
                    std::shared_ptr<Asset> cachedAsset = Cast<Asset>(cachedObject);
                    if (cachedAsset)
                    {
                        return EnqueueAsyncLoadCacheHit(std::move(cachedAsset), std::move(onComplete));
                    }
                }
            }
        }

        const auto inFlightIt = m_InFlightDataAssetLoads.find(registryKey);
        if (inFlightIt != m_InFlightDataAssetLoads.end() && inFlightIt->second != nullptr)
        {
            if (onComplete)
            {
                inFlightIt->second->waiters.push_back(std::move(onComplete));
            }
            return AsyncLoadHandle{inFlightIt->second->workerHandle.Id};
        }

        if (meta.RuntimeClassName.empty())
        {
            return EnqueueAsyncLoadFailure(
                std::move(onComplete),
                "DataAsset load requires RuntimeClassName in meta");
        }

        auto record = std::make_shared<AsyncDataAssetLoadRecord>();
        record->registryKey = registryKey;
        record->meta = meta;
        record->meta.AssetPath = registryKey;
        record->workerResult = std::make_shared<AsyncDataAssetWorkerResult>();
        if (onComplete)
        {
            record->waiters.push_back(std::move(onComplete));
        }

        const std::string absoluteAssetPath = ResolveAssetAbsolutePath(registryKey).string();
        std::shared_ptr<AsyncDataAssetWorkerResult> workerResult = record->workerResult;

        record->workerHandle = taskSystem.Submit(
            [absoluteAssetPath, workerResult]() {
                if (!AssetSerializeHelper::ReadJsonFromFile(
                        absoluteAssetPath, workerResult->json, &workerResult->error))
                {
                    workerResult->ok = false;
                    if (workerResult->error.empty())
                    {
                        workerResult->error = "LoadJsonFromFile failed";
                    }
                    return;
                }
                workerResult->ok = true;
            },
            [this, registryKey](bool /*bWorkSucceeded*/) {
                const auto it = m_InFlightDataAssetLoads.find(registryKey);
                if (it == m_InFlightDataAssetLoads.end() || it->second == nullptr)
                {
                    return;
                }
                FinishInFlightDataAsset(it->second);
            });

        if (!record->workerHandle.IsValid())
        {
            ME_LOG(LogAsset, Error, "AsyncLoadAsset rejected: TaskSystem::Submit failed.");
            return AsyncLoadHandle{};
        }

        m_InFlightDataAssetLoads.emplace(registryKey, record);
        return AsyncLoadHandle{record->workerHandle.Id};
    }

    AsyncLoadHandle AssetManager::AsyncLoadAssetByPath(
        const std::string& path,
        AsyncLoadAssetCallback onComplete)
    {
        const AssetMeta* meta = FindAssetMetaByPath(path);
        if (meta == nullptr)
        {
            return EnqueueAsyncLoadFailure(
                std::move(onComplete),
                "asset meta not found for path: " + path);
        }
        return AsyncLoadAssetByMeta_Internal(*meta, std::move(onComplete));
    }

    AsyncLoadHandle AssetManager::AsyncLoadAssetByGUID(
        const GUID& guid,
        AsyncLoadAssetCallback onComplete)
    {
        const AssetMeta* meta = FindAssetMetaByGuid(guid);
        if (meta == nullptr)
        {
            return EnqueueAsyncLoadFailure(
                std::move(onComplete),
                "guid not found in asset registry");
        }
        return AsyncLoadAssetByMeta_Internal(*meta, std::move(onComplete));
    }

    AsyncLoadHandle AssetManager::AsyncLoadAssetByMeta(
        const AssetMeta& meta,
        AsyncLoadAssetCallback onComplete)
    {
        return AsyncLoadAssetByMeta_Internal(meta, std::move(onComplete));
    }
}
