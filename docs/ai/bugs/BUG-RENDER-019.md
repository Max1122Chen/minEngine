# BUG-RENDER-019 — 绘制包以裸指针持有绑定集，材质重建后悬垂崩溃

- **Status:** Fixed
- **First seen:** 2026-10-30, Multi 拓扑
- **Area:** Render / DrawCommands / Material

## 现象

多线程拓扑下，挖爆一个方块后进程**直接消失**，编辑器日志里没有任何 Error/Warn。

崩溃记录（`ed_crash.log`）：

```
FaultThread: role=Render+RHI
ExceptionCode=0xC0000005  ExceptionAddress=0x0  FaultRIP=0x0
Stack[0]  OpenGLRHI::RHICmdSetShaderBindingSet   OpenGLRHI.cpp:762
Stack[16] OpenGLRHI::RHICmdBeginRenderPass       OpenGLRHI.cpp:711
```

## 根因

反汇编对位确认 `0xDAF18` 是 `call *%rdx`，`rdx = *(*bindingSet + 0x28)` 读成 0 —— 传入的
`RHIShaderBindingSet` **已是被释放对象**（内存仍可读，虚表槽已被清零）。并非空函数指针表。

链路：

- `MeshDrawPacket` 以 `RHIShaderBindingSet*` 裸指针记录每 draw 的绑定集；
- 录制期该裸指针被捕获进命令 lambda，Replay 在 RHI 角色上才真正使用；
- `SceneMeshDrawUtils` 在 **Prepare 阶段**就写入材质绑定集，而
  `MaterialRenderProxy::BindForDraw` 在**编译代次变化**时先释放旧集再重建；
- 新材质第一次真正可绘制的那一帧正好走这条路径（挖方块生成掉落物 → 首次使用 sprite/unlit 材质），
  于是 packet 里的旧指针悬垂。

延迟销毁机制只保证 native 句柄的销毁时机，**不保证 C++ 对象本身**；靠注释写下的寿命契约挡不住这条。

## 修复

- `MeshDrawPacket` 改持 `RHIShaderBindingSetRef`（类型上无法再塞进借来的地址）；
- `RHICommandList::SetShaderBindingSet` 增加接收 ref 的重载，未 Replay 的命令流自己握住 set；
- 删除 Prepare 期的材质集填充，材质槽只在 `BindForDraw` 之后写一次；
- set0/set1 同样以 ref 形式交出，集合在放弃持有时报告"仍被引用的数量"；
- `GetRecordedBindingSetHoldCount()` 让"被未回放命令握住的 set 数"可观测。

## 回归检查

- `draw-packet-lifetime`：断言「packet 生成后生产者替换自己的 set，packet 仍引用有效对象」，
  修复前以探针计数失败（`1 != 2`），修复后通过。
- 真机：Multi 下挖爆方块不应再崩溃；崩溃记录不应再出现 `FaultRIP=0x0`。

## 未完成

packet 的顶点/索引缓冲仍是借用（`RHIBuffer*`）。它们由场景代理持有、与 pass 的绘制命令同寿命、
帧内无重建路径，改成 ref 会连锁到 `MeshDrawCommand`/代理/场景，故本次未动——这是 packet 里最后一处借来的对象。
