# BUG-RENDER-018 - A command enqueued inside the frame publish window was judged stale and dropped

## Meta
- **ID:** BUG-RENDER-018
- **Status:** Fixed
- **Severity:** S2
- **Owner:** project maintainer
- **Found:** 2026-10-29
- **Last updated:** 2026-10-29
- **Affects:** `RenderCommandPipe` (`PublishGameFrame`, `Enqueue`, `SubmitViewIntent`, `Shutdown`)
- **Related Feature/Slice:** game/render command pipe (`Runtime/Core/Threading/RenderCommandPipe`)

## TL;DR
`PublishGameFrame` stored the drain target before the tag new commands carry. A producer that sampled
the tag between the two stores read the frame the render role had just been allowed to drain, so its
command was enqueued at or behind the drain and dropped as stale on the spot - one scene change
silently missing while the game object it belonged to stayed. The view-intent queue had the same rule
and the same hole. Shutdown discarded both queues without counting anything, feed-forward.

## Symptoms
- A `RenderScene` add/remove command disappears and is counted in `DroppedStaleCommands` with a Warn
  naming a frame the render role had "already drained", even though the game role had only just
  finished that frame.
- The scene proxy and its game object disagree from then on: a destroyed component's proxy stays, or
  a created one never appears, with no further error.
- A view intent submitted in the same window is filed under a frame `TakeViewIntents` has already
  taken, so the renderer keeps drawing the previous camera until the next intent arrives.
- `Shutdown` emptied both queues with no counter and no log, which is where scene-teardown commands
  go: the work is lost with nothing in the counters to show it existed.

## Expected
- A producer can never take a tag the render role may already have drained.
- Whatever the pipe throws away is counted, and the depth is reported once.
- A call into a pipe that is no longer running is refused and counted, not applied out of band.

## Reproduction
1. Drive the pipe directly and construct the pair a producer samples mid-publish: the drain target
   already moved on (`completedFrame` = N) while the tag still reads N.
2. Let the render role drain the completed frame.
3. Enqueue a command / submit a view intent.
4. Before the fix: the command is dropped as stale at enqueue (counter +1) and the intent is dropped
   on take; after it: both are kept and applied by the next drain.

## Root cause
`PublishGameFrame` wrote `m_CompletedGameFrame` first and `m_CurrentGameFrame` second, with a comment
that justified the order by the opposite danger ("a producer must never end up ahead of the frame the
render role may drain"). Ahead is the harmless direction: an entry is discarded only when its frame is
*at or behind* the frame already drained, and a frame that has not been published as complete cannot
have been drained. Publishing the drain target first created exactly the state the drop rule punishes.
Neither store took a queue lock, although the header claimed the tag was read "under the same lock as
the queue". `SubmitViewIntent` / `TakeViewIntents` repeated the same rule against
`m_TakenIntentFrame`, so the intent queue carried the same defect. `Shutdown` cleared both deques
without reading their depth.

## Fix
- `PublishGameFrame` publishes the tag first and the drain target second, both under
  `std::scoped_lock(m_CommandMutex, m_IntentMutex)`, so a producer inside either queue sees one whole
  published pair. A lock-free reader outside those locks can now only see a tag ahead of the drain
  target; such an entry waits in FIFO order for the drain that reaches it rather than being dropped.
- `Enqueue` and `SubmitViewIntent` derive the tag with `NewEntryFrame`: the newer of the two
  published counters. When the pair agrees - every state `PublishGameFrame` leaves behind - it is
  exactly the announced tag; if a producer ever samples a half-published pair, it lands on the newer
  frame instead of one the drain may have passed. The tag, the drain comparison and the queue insert
  stay one step under the queue lock.
- `Shutdown` adds the discarded depth to `DroppedStaleCommands` / `DroppedStaleViewIntents` and
  reports it once; `Enqueue` and `SubmitViewIntent` refuse and count on a pipe that is no longer
  running instead of asserting or running the callable on the caller's thread.
- The inline pass-through in `Enqueue` (`Multi, later`) is gone: it applied a command outside the
  frame tag, the drain comparison and the FIFO order the queue exists for.

## Regression verification
- `render-command-pipe` suite (13 cases):
  - `a producer that samples the tag mid-publish keeps its command`
  - `a view intent filed mid-publish is taken by the next frame`
  - `shutdown counts the queued work it discards`
  - `enqueue and intent submission are refused after shutdown`
- Command: `minEngine/bin/minEngineTests.exe test render-command-pipe`
- Unchanged and still green: `frame-threading`, `scene-retirement`, `test smoke` (24 suites).

- [x] Red before the fix (the mid-publish command was dropped as stale; the mid-publish intent was
      discarded on take)
- [x] Green after it

## Residual risk
- `EnqueueRenderCommand` with no pipe in the process still runs the callable inline. That fallback is
  what headless callers and suites without an engine rely on, and it cannot distinguish "never had a
  pipe" from "the engine has shut down"; the refusal lives one level down, in the pipe that still
  exists.

## Links
- `minEngine/minEngine/src/Runtime/Core/Threading/RenderCommandPipe.cpp`
- `minEngine/minEngine/src/Runtime/Core/Threading/RenderCommandPipe.h`
- `minEngine/Tests/Suites/RenderCommandPipeTest.cpp`
- `minEngine/Tests/Access/RenderCommandPipeTestAccess.h`

---

## Change log

| Date | Note |
|------|------|
| 2026-10-29 | Recorded on fix; publish order, tag repair, shutdown accounting, inline pass-through removed |
