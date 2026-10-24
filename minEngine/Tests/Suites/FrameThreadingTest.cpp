#include "FrameThreadingTest.h"

#include "Runtime/Core/Threading/FrameThreading.h"

#include "doctest.h"

using minEngine::FrameThreadKind;
using minEngine::FrameThreadMode;
using minEngine::FrameThreading;

TEST_CASE("frame-threading: parse mode tokens [smoke]")
{
    const auto single = FrameThreading::TryParseMode("single");
    REQUIRE(single.has_value());
    CHECK(*single == FrameThreadMode::Single);

    const auto singleUpper = FrameThreading::TryParseMode("Single");
    REQUIRE(singleUpper.has_value());
    CHECK(*singleUpper == FrameThreadMode::Single);

    const auto multi = FrameThreading::TryParseMode("multi");
    REQUIRE(multi.has_value());
    CHECK(*multi == FrameThreadMode::Multi);

    const auto multiUpper = FrameThreading::TryParseMode("MULTI");
    REQUIRE(multiUpper.has_value());
    CHECK(*multiUpper == FrameThreadMode::Multi);

    CHECK_FALSE(FrameThreading::TryParseMode("").has_value());
    CHECK_FALSE(FrameThreading::TryParseMode("parallel").has_value());
}

TEST_CASE("frame-threading: resolve mode defaults [smoke]")
{
    CHECK(FrameThreading::ResolveMode(std::nullopt) == FrameThreadMode::Single);
    CHECK(FrameThreading::ResolveMode(FrameThreadMode::Multi) == FrameThreadMode::Multi);
    CHECK(FrameThreading::ResolveMode(FrameThreadMode::Single) == FrameThreadMode::Single);
}

TEST_CASE("frame-threading: init binds game thread [smoke]")
{
    FrameThreading ft;
    CHECK_FALSE(FrameThreading::HasInstance());
    ft.Initialize(FrameThreadMode::Single);
    CHECK(FrameThreading::HasInstance());
    CHECK(ft.GetMode() == FrameThreadMode::Single);
    CHECK(ft.IsInGameThread());
    CHECK(ft.IsInRenderingThread());
    CHECK(ft.IsInRHIThread());
    ft.Shutdown();
    CHECK_FALSE(FrameThreading::HasInstance());
    ft.Shutdown();
    CHECK_FALSE(FrameThreading::HasInstance());
}

TEST_CASE("frame-threading: multi stores mode without second OS thread [smoke]")
{
    FrameThreading ft;
    ft.Initialize(FrameThreadMode::Multi);
    CHECK(ft.GetMode() == FrameThreadMode::Multi);
    // Until a dedicated render OS thread exists, all roles share the init thread.
    CHECK(ft.IsInGameThread());
    CHECK(ft.IsInRenderingThread());
    CHECK(ft.IsInRHIThread());
    CHECK(ft.GetThreadId(FrameThreadKind::Game) == ft.GetThreadId(FrameThreadKind::Render));
    ft.Shutdown();
}
