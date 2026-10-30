# BUG-TEST-002 — task-system 用例偶发失败并级联成整轮 smoke 红

- **Status:** Open
- **First seen:** 2026-10-30
- **Area:** Tests / TaskSystem

## 现象

偶发：`test smoke` 在用例
`task-system: a game-role render write with a frame in flight is counted and recorded`
失败，报 `inFlightAfter 1 != 0`；由于该套件配置了失败即中止，中止点留下 FrameThreading 注册状态，
级联导致同轮 `frame-threading`、`rhi-command-list` 也失败，整轮以退出码 132 结束。

同一提交下：
- 单独跑该套件 25/25 通过；
- 前后两次完整 `test smoke` 也是 34 套件全绿。

## 判断

**既有抖动，与当时改动的路径无关**（改动不涉及 role 启动/停止与 in-flight 计数）。
最可能的成因是"帧释放与计数的时序窗口"：用例在释放帧之后立刻读取在飞计数，而结算点可能仍在
另一个角色线程上未完成。

## 待办

1. 定住这条用例的等待条件（等待 in-flight 归零而不是读一次）；
2. 或者把"该套件失败即中止"的行为改成"记录失败后继续"，避免一个用例把整轮 smoke 弄红；
3. 复现方式：连续跑 `test smoke` 若干轮，记录失败轮次与 `inFlightAfter` 值。
