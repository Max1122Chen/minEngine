#pragma once

#include "UI/Property/PropertyEditSession.h"
#include "UI/Property/PropertyRefTypes.h"

#include "Runtime/Core/Reflection/MEProperties.h"

#include <functional>

namespace minEngine
{
    struct ObjectPtrWidgetHooks
    {
        std::function<void()> OnComboActivated;
        std::function<void()> OnComboDeactivatedAfterEdit;
        std::function<bool(const PropertyRefCandidate& selected)> TryApplySelection;
        std::function<void(bool selectionChanged)> OnSelectionCommitted;
        std::function<void()> OnMarkDirty;
        std::function<void()> OnRenderStateDirty;
    };

    class ObjectPtrWidget
    {
    public:
        static bool Draw(const Reflection::MEProperty& property,
                         void* propertyPtr,
                         const PropertyEditSession& session,
                         const ObjectPtrWidgetHooks& hooks = {},
                         float itemWidth = -1.0f);

    private:
        static void CollectAssetCandidates(const Reflection::MEClass* allowedClass,
                                           std::vector<PropertyRefCandidate>& outCandidates);
        static void CollectObjectCandidates(const Reflection::MEClass* allowedClass,
                                            std::vector<PropertyRefCandidate>& outCandidates);
        static void AppendCandidatesDedupe(std::vector<PropertyRefCandidate>& candidates,
                                           const std::vector<PropertyRefCandidate>& toAppend);
        static void SortCandidatesByDisplayName(std::vector<PropertyRefCandidate>& candidates);

        static AllowedClasses ResolveDefaultAllowedClasses(const Reflection::MEClass* valueClass);
        static GUID ReadCurrentGuid(const Reflection::MEObjectPtrProperty& objectPtrProperty, void* propertyPtr);
        static bool ApplySelection(const Reflection::MEObjectPtrProperty& objectPtrProperty,
                                   void* propertyPtr,
                                   const Reflection::MEClass* valueClass,
                                   const PropertyRefCandidate* selected,
                                   const ObjectPtrWidgetHooks& hooks);
        static std::string MakeObjectDisplayLabel(const MEObject& object);
    };
}
