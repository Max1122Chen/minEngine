#pragma once

#include "Core.h"
#include "Runtime/Core/Math/Math.h"
#include "Runtime/Core/Reflection/MEClass.h"
#include "Runtime/Core/Reflection/Reflection.h"
#include "Runtime/Function/Framework/Components/Component.h"

namespace minEngine
{
    class Material;
    class RHIBuffer;
    class RHIVertexInputLayout;
    class Texture2D;

    /**
     * One ScreenUI draw, copied out of the graphic that produced it.
     * Plain data with no back pointer to a component: the proxy outlives this call and the render
     * thread reads it, so it must not be able to reach a game-thread object.
     */
    struct WidgetDrawData
    {
        /** True when the vertex buffer is a widget-local pixel mesh instead of the shared quad. */
        bool bLocalPixelMesh = false;
        RHIBuffer* VertexBuffer = nullptr;
        RHIBuffer* IndexBuffer = nullptr;
        RHIVertexInputLayout* VertexInputLayout = nullptr;
        Texture2D* Texture = nullptr;
        Vector4 Color{ 1.0f, 1.0f, 1.0f, 1.0f };
        Vector4 UVRect{ 0.0f, 0.0f, 1.0f, 1.0f };
    };

    /**
     * A visual source that a WidgetComponent beside it assembles into its scene proxy.
     * Abstract because a graphic only ever exists as a concrete kind: the reflection factory
     * refuses to build one directly, so it cannot be added as a component.
     */
    ME_CLASS(Abstract)
    class WidgetGraphicComponent : public Component
    {
        ME_GENERATED_BODY()
    public:
        virtual ~WidgetGraphicComponent() override = default;

        /**
         * Whether this graphic takes part in the widget's picture right now.
         * Includes the graphic's own switch: a graphic that is turned off hands the widget to the
         * next one instead of holding the slot with nothing to show.
         */
        virtual bool CanContribute() const = 0;

        /**
         * Produce one draw into a widget of the given size; false when nothing can be drawn.
         * A graphic that contributes may still fail here (a font that yields no quad), and the
         * widget then draws nothing rather than a stale picture from an older frame.
         */
        virtual bool BuildDrawData(const Vector2& widgetSizePx, WidgetDrawData& outData) = 0;

        /**
         * Write own parameters into the material instance the widget owns.
         * One graphic paints at a time, so one material instance is enough and it stays with the
         * widget.
         */
        virtual void ApplyMaterialParameters(Material& material) const = 0;

        /**
         * The reflected class of this base, resolved once from the registry.
         * StaticClass() is the usual spelling, but its definition is generated inside the engine
         * module and is not part of the export contract, so a header-inline caller has nothing to
         * link against. The registry lookup yields the same MEClass.
         */
        static const Reflection::MEClass* LookupClass()
        {
            static const Reflection::MEClass* s_Class =
                Reflection::ReflectionSystem::Get().FindClass("minEngine::WidgetGraphicComponent");
            return s_Class;
        }
    };
}

#include "Generated/Reflection/WidgetGraphicComponent.gen.h"
