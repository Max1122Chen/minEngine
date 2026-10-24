#include "RenderCommandPipeTest.h"

#include "Runtime/Core/Threading/RenderCommandPipe.h"
#include "Runtime/Function/Render/SceneDrawDesc.h"

#include "doctest.h"

using minEngine::RenderCommandPipe;
using minEngine::SceneDrawDesc;

namespace
{
    struct RenderCommandPipeScope
    {
        RenderCommandPipe Pipe;

        RenderCommandPipeScope()
        {
            Pipe.Initialize();
        }

        ~RenderCommandPipeScope()
        {
            Pipe.Shutdown();
        }
    };
}

TEST_CASE("render-command-pipe: enqueue flush by frame [smoke]")
{
    RenderCommandPipeScope scope;
    RenderCommandPipe& pipe = scope.Pipe;

    int value = 0;
    pipe.Enqueue([&value]() { value = 1; });
    CHECK(value == 0);

    pipe.FlushApplyForFrame(0);
    CHECK(value == 1);

    // Still on logic frame 0: this command is tagged 0.
    pipe.Enqueue([&value]() { value = 2; });
    pipe.AdvanceLogicFrame();
    // Now logic frame 1.
    pipe.Enqueue([&value]() { value = 3; });

    pipe.FlushApplyForFrame(0);
    CHECK(value == 2);

    pipe.FlushApplyForFrame(1);
    CHECK(value == 3);
}

TEST_CASE("render-command-pipe: view intent take by frame [smoke]")
{
    RenderCommandPipeScope scope;
    RenderCommandPipe& pipe = scope.Pipe;

    SceneDrawDesc a{};
    a.ViewId = 11;
    pipe.SubmitViewIntent(a);
    pipe.AdvanceLogicFrame();
    SceneDrawDesc b{};
    b.ViewId = 22;
    pipe.SubmitViewIntent(b);

    std::vector<SceneDrawDesc> taken;
    pipe.TakeViewIntents(0, taken);
    REQUIRE(taken.size() == 1);
    CHECK(taken[0].ViewId == 11);

    pipe.TakeViewIntents(1, taken);
    REQUIRE(taken.size() == 1);
    CHECK(taken[0].ViewId == 22);
}

TEST_CASE("render-command-pipe: published world gen advances [smoke]")
{
    RenderCommandPipeScope scope;
    RenderCommandPipe& pipe = scope.Pipe;

    CHECK(pipe.GetPublishedWorldGen() == UINT64_MAX);
    CHECK(pipe.GetLogicFrameIndex() == 0);

    pipe.SetPublishedWorldGen(0);
    pipe.AdvanceLogicFrame();
    CHECK(pipe.GetPublishedWorldGen() == 0);
    CHECK(pipe.GetLogicFrameIndex() == 1);
}
