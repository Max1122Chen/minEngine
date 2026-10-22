#pragma once

#include "Core.h"
#include "Runtime/Core/Object/MEObject.h"
#include "Runtime/Function/Framework/Scene/SceneTypes.h"

namespace minEngine
{
    class SubsystemCollectionBase;
    class Scene;

    /**
     * Abstract base for scoped engine extensions (Engine / GameInstance / World).
     * Discovered via reflection; instances owned by SubsystemCollection.
     */
    ME_CLASS(Abstract)
    class MINENGINE_API Subsystem : public MEObject
    {
        ME_GENERATED_BODY()

    public:
        Subsystem() = default;
        ~Subsystem() override = default;

        /** Called after construction and Outer assignment; may call InitializeDependency. */
        virtual void Initialize(SubsystemCollectionBase& collection) { (void)collection; }

        /** Called before the instance is destroyed / removed from the collection. */
        virtual void Deinitialize() {}

        /**
         * CDO / type gate: return false to skip creating this type for the given outer.
         * Default: create.
         */
        virtual bool ShouldCreateSubsystem(MEObject* outer) const
        {
            (void)outer;
            return true;
        }
    };

    ME_CLASS(Abstract)
    class MINENGINE_API EngineSubsystem : public Subsystem
    {
        ME_GENERATED_BODY()
    };

    ME_CLASS(Abstract)
    class MINENGINE_API GameInstanceSubsystem : public Subsystem
    {
        ME_GENERATED_BODY()
    };

    ME_CLASS(Abstract)
    class MINENGINE_API WorldSubsystem : public Subsystem
    {
        ME_GENERATED_BODY()

    public:
        Scene* GetScene() const;

        bool ShouldCreateSubsystem(MEObject* outer) const override;

        /** Filter by SceneType (Editor / PIE). Default: all types. */
        virtual bool DoesSupportWorldType(ESceneType sceneType) const
        {
            (void)sceneType;
            return true;
        }

        virtual void PostInitialize() {}
        virtual void OnWorldBeginPlay(Scene& scene) { (void)scene; }
        virtual void OnWorldEndPlay(Scene& scene, EEndPlayReason reason)
        {
            (void)scene;
            (void)reason;
        }
    };

    ME_CLASS(Abstract)
    class MINENGINE_API TickableGameInstanceSubsystem : public GameInstanceSubsystem
    {
        ME_GENERATED_BODY()

    public:
        virtual void Tick(float deltaTime) { (void)deltaTime; }
        virtual bool IsTickable() const { return true; }

        /**
         * Only called when a derived GameInstance::OnFixedTick invokes it.
         * Engine does not ForEach GI subsystems for FixedTick.
         */
        virtual void FixedTick(float fixedDeltaSeconds) { (void)fixedDeltaSeconds; }
    };

    ME_CLASS(Abstract)
    class MINENGINE_API TickableWorldSubsystem : public WorldSubsystem
    {
        ME_GENERATED_BODY()

    public:
        virtual void Tick(float deltaTime) { (void)deltaTime; }
        virtual bool IsTickable() const { return true; }

        /** Opt into engine FixedTick dispatch (creation / m_Instances order). */
        virtual bool WantsFixedTick() const { return false; }
        virtual void FixedTick(float fixedDeltaSeconds) { (void)fixedDeltaSeconds; }
    };
}

#include "Subsystem.gen.h"
