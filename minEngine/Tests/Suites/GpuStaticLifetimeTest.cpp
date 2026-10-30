#include "GpuStaticLifetimeTest.h"

#include "Support/HeadlessTestRHI.h"

#include "Runtime/Function/Render/RHI/RHI.h"
#include "Runtime/Function/Render/RHI/RHIBuffers.h"
#include "Runtime/Function/UI/Font/TextMeshBuilder.h"

#include "doctest.h"

using minEngine::RHIVertexElement;
using minEngine::RHIVertexInputLayout;
using minEngine::Testing::GpuHandleProbe;
using minEngine::Testing::HeadlessTestRHI;

namespace
{
    void InstallProbeLayoutHook(HeadlessTestRHI& rhi, const std::shared_ptr<GpuHandleProbe>& probe)
    {
        rhi.CreateVertexInputLayoutHook =
            [probe](std::initializer_list<RHIVertexElement> elements)
        {
            return std::make_shared<minEngine::Testing::ProbeVertexInputLayout>(
                probe,
                std::vector<RHIVertexElement>(elements));
        };
    }
}

TEST_CASE("gpu-static-lifetime: the shared text vertex layout is gone before the device is [smoke]")
{
    const std::shared_ptr<GpuHandleProbe> probe = std::make_shared<GpuHandleProbe>();
    HeadlessTestRHI rhi(probe);
    InstallProbeLayoutHook(rhi, probe);

    const RHIVertexInputLayout* shared = minEngine::TextMeshBuilder::GetOrCreateVertexInputLayout(rhi);
    REQUIRE(shared != nullptr);
    CHECK(minEngine::TextMeshBuilder::HasSharedLayout());
    CHECK(probe->HandlesAlive.load() == 1);

    // The shutdown sequence RenderSystem runs: the process-level holder gives its reference back
    // while the device is still alive, the queue is drained, and only then does the device go.
    minEngine::TextMeshBuilder::ReleaseSharedLayout();
    CHECK_FALSE(minEngine::TextMeshBuilder::HasSharedLayout());

    const size_t destroyed = rhi.RHIDrainAllDeferredDestroys();

    // Nothing of this device is left holding a handle, and the one it handed out was destroyed by
    // the drain rather than by a static destructor that would have run after the device was gone.
    CHECK(probe->HandlesAlive.load() == 0);
    CHECK(destroyed == 1);
    CHECK(probe->NativesDestroyed.load() == 1);
    CHECK(probe->NativesRefused.load() == 0);

    rhi.Shutdown();
}

TEST_CASE("gpu-static-lifetime: the shared layout factory rebuilds after a release [smoke]")
{
    const std::shared_ptr<GpuHandleProbe> probe = std::make_shared<GpuHandleProbe>();
    HeadlessTestRHI rhi(probe);
    InstallProbeLayoutHook(rhi, probe);

    const RHIVertexInputLayout* first = minEngine::TextMeshBuilder::GetOrCreateVertexInputLayout(rhi);
    REQUIRE(first != nullptr);
    CHECK(probe->HandlesCreated.load() == 1);
    // The factory is a factory, not a per-call allocator: a second call reuses what it built.
    CHECK(minEngine::TextMeshBuilder::GetOrCreateVertexInputLayout(rhi) == first);
    CHECK(probe->HandlesCreated.load() == 1);

    minEngine::TextMeshBuilder::ReleaseSharedLayout();
    CHECK(rhi.RHIDrainAllDeferredDestroys() == 1);
    CHECK(probe->HandlesAlive.load() == 0);

    // Releasing is not retiring the factory: the next call builds a layout for the current device.
    const RHIVertexInputLayout* second = minEngine::TextMeshBuilder::GetOrCreateVertexInputLayout(rhi);
    REQUIRE(second != nullptr);
    CHECK(second != first);
    CHECK(probe->HandlesCreated.load() == 2);
    CHECK(probe->HandlesAlive.load() == 1);

    minEngine::TextMeshBuilder::ReleaseSharedLayout();
    CHECK(rhi.RHIDrainAllDeferredDestroys() == 1);
    CHECK(probe->NativesDestroyed.load() == 2);
    CHECK(probe->NativesRefused.load() == 0);

    rhi.Shutdown();
}
