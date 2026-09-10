# Active work (agent backlog)

Last updated: 2026-09-10锛圗D-F06 Done锛涘噯澶?commit锛?Purpose: **short, human-maintained** list of what matters now. Agents use this for planning instead of old roadmaps or unchecked design checkboxes.

> **Agent:** Treat this file as the primary backlog. Do not infer mandatory tasks from `*_ROADMAP.md`, `*_PLAN.md`, or Snapshot/Archived docs unless the user points to them for the current task.

---

## 本波已合入（master）

| 轨 | 内容 |
|----|------|
| core | CORE-F08–F12 序列化/反射 + TEST-F04 |
| editor | ED-F05 Inspector / Component UX |
| gameplay | GP-F01 Tag + GP-F02 Event |
| animation | ANIM-F01–F04 / ASSET-F01–F02 / Parameter→CORE-F13 / SM Canvas→ED-F06–F07 |

> **ID：** 见 `FEATURE_REGISTRY.md` §Merge-wave ID remaps。

---

## 褰撳墠鐒︾偣锛坄feat/animation`锛?
### ED-F06 鈥?Anim SM Canvas Polish 鈫?**Done**

| 椤?| 閾炬帴 / 璇存槑 |
|----|-------------|
| Design / Impl | [Design](./Editor/ED-F06_ANIM_SM_CANVAS_POLISH_DESIGN.md) 路 [Impl](./Editor/ED-F06_ANIM_SM_CANVAS_POLISH_IMPLEMENTATION.md) |
| 杩涘害 | Entry / AnyState / 杈逛笌 State 鑿滃崟锛涚┖ Clip hold-last锛涚淮鎶よ€?smoke OK |
| Next | 鍑嗗 commit锛涚画 ANIM-F04锛堥敊宄帮級 |

### ANIM-F04 鈥?Blend Tree 1D 鈫?**Review锛堝緟瀹℃壒锛?*

| 椤?| 閾炬帴 / 璇存槑 |
|----|-------------|
| Design / Impl | [Design](./Animation/ANIM-F04_BLEND_TREE_DESIGN.md) 路 [Impl](./Animation/ANIM-F04_BLEND_TREE_IMPLEMENTATION.md) |
| 鐩爣 | State 鍙寕 1D BlendTree锛坒loat 鍙傛暟 + 闃堝€?Clip锛?|
| Out | 2D锛汵ested SM锛堚啋 **ANIM-F05** backlog锛夛紱AnimBP VM |
| Next | 瀹℃壒鍚庡疄鐜帮紱寤鸿涓?ED-F06 **閿欏嘲** |

### ED-F05 鈥?State Machine Graph Canvas 鈫?**Done**

| 椤?| 閾炬帴 / 璇存槑 |
|----|-------------|
| Design / Impl | [Design](./Editor/ED-F05_STATE_MACHINE_CANVAS_DESIGN.md) 路 [Impl](./Editor/ED-F05_STATE_MACHINE_CANVAS_IMPLEMENTATION.md) |
| 杩涘害 | 鍚堝叆 `53e99f6`锛涘悗缁敾甯冭涔?鈫?**ED-F06** |

### ANIM-F03 鈥?Animation Graph MVP 鈫?**In Progress**

| 椤?| 閾炬帴 / 璇存槑 |
|----|-------------|
| Design / Impl | [Design](./Animation/ANIM-F03_ANIMATION_GRAPH_DESIGN.md) 路 [Impl](./Animation/ANIM-F03_ANIMATION_GRAPH_IMPLEMENTATION.md) |
| 鐩爣 | Unity 寮?FSM + Params + Pose Blend锛汦ditor+SmGraph 宸插彲鐢?|
| Next | ED-F06 鏀跺彛鐢诲竷璇箟鍚庯紝浜哄瀷闂幆 smoke 鈫?鍙爣 Done |

### 鍔ㄧ敾鍚庣画 backlog锛堝厛璁颁竴绗旓紝鏈紑 Feature锛?
| 椤?| 澶囨敞 |
|----|------|
| **ANIM-F05** Nested State Machine | 瀛愮姸鎬佹満閽诲叆锛涘緟 F04 鍚庡啀绔?Design |
| Preview 绐?| AnimGraph 棰勮瑙嗗彛锛涘彟 Feature |
| Play 鏃舵椿璺?State/杈归珮浜?| Editor 灏忓垏鐗?|
| Inspector 鏉′欢缂栬緫 UX | ED-F06 鍚?|
| Undo/Redo 鍥剧紪杈?| 鐥涙劅澶熷啀寮€ |
| Exit Time | 鏇?Deferred |
| 浜哄瀷 Idle鈫擶alk 闂幆璧勪骇 | F03 鏀跺彛楠屾敹 |


### CORE-F08 鈥?Parameter Schema / Layout / Store 鈫?**Done**

| 椤?| 閾炬帴 / 璇存槑 |
|----|-------------|
| Design / Impl | [Design](./Platform/Core/CORE-F08_PARAMETER_STORAGE_DESIGN.md) 路 [Impl](./Platform/Core/CORE-F08_PARAMETER_STORAGE_IMPLEMENTATION.md) |
| 浠ｇ爜鐩綍 | `Runtime/Function/Framework/Parameters/` |
| 杩涘害 | S00鈥揝02 **Done**锛堝惈 Schema JSON round-trip锛夛紱`test parameter-store` 7 cases PASS |
| Next | 鍙噯澶?commit锛堜粎 Parameters + 娴嬭瘯 + F08 鏂囨。锛涘嬁甯︽湰鍦?Animations/`build_*.log`锛?|

### ASSET-F02 鈥?Formal Import/Load 娉ㄥ唽寮忕绾?+ ImportDialog 鈫?**Done**

| 椤?| 閾炬帴 / 璇存槑 |
|----|-------------|
| Design / Impl | [Design](./Asset/ASSET-F02_IMPORT_SERVICE_DESIGN.md) 路 [Impl](./Asset/ASSET-F02_IMPORT_SERVICE_IMPLEMENTATION.md) |
| 鐩爣 | **Register 寮?* Load/Import锛涘叡鐢?ImportDialog锛汣lip 鏄惧紡 Skeleton |
| 杩涘害 | S00鈥揝05 **Done**锛堣嚜鍔ㄥ寲 + 鎵嬪姩楠?PASS锛?|
| Out | Import Settings 妗嗘灦锛沗.memesh`锛汻etarget锛涘ぇ铏氬熀绫绘彃浠朵綋绯?|
| Next | 宸插悎鍏?`06b4d74` |

### ANIM-F02 鈥?Clip Playback 鈫?**Done**

| 椤?| 閾炬帴 / 璇存槑 |
|----|-------------|
| Design / Impl | [Design](./Animation/ANIM-F02_CLIP_PLAYBACK_DESIGN.md) 路 [Impl](./Animation/ANIM-F02_CLIP_PLAYBACK_IMPLEMENTATION.md) |
| 鐩爣 | MVP锛歚AnimationTrack`锛堥 TRS锛? Player鈯廠MC锛汭mport `.meaclip`锛沗TryGetNamedFloat` 澹?|
| 楠岃瘉 | 浜哄瀷 Walking Clip Editor 鐩 PASS锛沗animation-clip` / smoke |
| Next | 鍚堝叆鍚庡彟璁紙ANIM-F03 Graph 鎸夐渶锛涘嬁榛樿寮€骞诧級 |

### ANIM-F01 鈥?Skeletal Mesh Pipeline 鈫?**Done**

| 椤?| 閾炬帴 / 璇存槑 |
|----|-------------|
| Design / Impl | [Design](./Animation/ANIM-F01_SKELETAL_MESH_PIPELINE_DESIGN.md) 路 [Impl](./Animation/ANIM-F01_SKELETAL_MESH_PIPELINE_IMPLEMENTATION.md) |
| 楠岃瘉 | stick 鐩 + ASSET-F01 浜哄瀷 Import锛沗skeleton-pose` / smoke |
| Deferred | Shadow skinned锛涙壄楠ㄤ氦浜?UX |

### ASSET-F01 鈥?External Import Pipeline 鈫?**Done锛圡VP锛?*

| 椤?| 閾炬帴 / 璇存槑 |
|----|-------------|
| Design / Impl | [Design](./Asset/ASSET-F01_IMPORT_PIPELINE_DESIGN.md) 路 [Impl](./Asset/ASSET-F01_IMPORT_PIPELINE_IMPLEMENTATION.md) |
| S00鈥揝03 | **Done**锛堟墜鍔ㄩ獙 Static+Skeletal锛?|
| Deferred | S04 / `.memesh` 鈥?鍙敱 **ASSET-F02** 鍙€夊垏鐗囧惛鏀讹紱浜屾湡 `.memesh` 浠嶅彟鎺?|
| 杩佺Щ | 鍕挎彁浜ら敊璇?`.fbx.meta`锛涗汉鍨嬮獙璇佽祫浜х暀鏈湴涓嶅叆搴?|

**鏄庣‘涓嶆帓鏈燂紙鍔ㄧ敾鎵╁睍锛夛細** Animation Event銆両K銆丷oot Motion銆丷etarget銆佸畬鏁?AnimBP銆?
### `master` 鏃佽矾锛堥潪鏈?worktree 鐒︾偣锛?
| 椤?| 鐘舵€?|
|----|------|
| CORE-F05 Play Mode MVP | **Done** |
| ED-F02 Editor Workflow | Planned锛堝€欓€夛紱涓庡姩鐢昏建骞惰涓嶆姠锛?|
| ED-F04 Console | In Progress锛圡VP Done锛?|

---

## 褰撳墠绛栫暐锛?026-09-05锛?
| 杞?| 鍒嗘敮 | 鍚堝叆鐩爣 | 璇存槑 |
|----|------|----------|------|
| **鍔ㄧ敾 / 璧勪骇** | `feat/animation` | 绔栧垏鍚庡啀璁?| 鐒︾偣绌猴紙ASSET-F02 宸?commit锛夛紱ANIM-F01/F02 Done锛汚SSET-F01 MVP Done |
| **鍐呮牳 / 缂栬緫鍣?* | `master` | `master` | CORE-F05 Done锛汦D-F02 绛夊彲骞惰 |

**鏄庣‘ Defer锛?* `.memesh` 路 Animation Event锛堟殏涓嶇櫥璁帮級路 IK / Root Motion / Retarget 路 Import Settings 妗嗘灦锛團02 涔嬪悗锛壜?ED-F01 VK 闃村奖璐ㄩ噺 路 `RND-F12` 路 `PHYS-F03` 路 ED-F04 S10b 路 CORE-F05-S05 Pause/Step 路 ANIM Shadow skinned  
**涓嬩竴寮€骞诧細** ANIM-F03-S08 AnimationGraphEditor 涓夌獥锛圖esign 搂9锛?
---

## Worktrees

| 璺緞 | 鍒嗘敮 | 鐢ㄩ€?|
|------|------|------|
| `D:/Dev/GitRepo/minEngine` | `master` | 鍐呮牳 + 宸插悎鍏?editor 杞?|
| `D:/Dev/GitRepo/minEngine-animation` | `feat/animation` | **鍔ㄧ敾杞?*锛堝綋鍓嶏級 |
| `D:/Dev/GitRepo/minEngine-editor` | `feat/editor` | 鍙綊妗ｆ垨鐢ㄤ簬涓嬩竴 editor 鍒囩墖 |

鏃?`minEngine-physics` / `minEngine-audio` / `minEngine-launcher` worktree 鍙寜闇€淇濈暀鎴栧垹闄ゃ€?
---

## In focus

> 鏈?worktree锛坄minEngine-animation` / `feat/animation`锛変互鏂囬 **ASSET-F02** 涓哄噯銆備笅鍒?A鈥揊 涓?`master` 杞ㄥ巻鍙蹭笌鏃佽矾 backlog銆?
### A. `master` 鈥?灏忎慨澶嶏紙鏀跺熬锛?
| 椤?| 鐘舵€?|
|----|------|
| ~~BUG-RENDER-014~~ 鐐瑰厜鍗婂緞/琛板噺 | Done锛坄f3c8200`锛?|
| ~~PHYS-F04~~ Collider 涓?Scale 瑙ｈ€?| **Done** 鈥?[Design](./Physics/PHYS-F04_COLLIDER_FIXES_DESIGN.md) 路 `c2c0893` |
| ~~BUG-PHYS-003~~ Add BoxCollider 闂存瓏宕╂簝 | **Fixed** 鈥?[Record](./bugs/BUG-PHYS-003.md)锛堟湭鍐嶅鐜帮級 |
| ~~BUG-PHYS-004~~ Collider 绂?鍒犲舰浣撳埛鏂?| **Fixed** 鈥?`c0a51ce` |

### B. `master` 鈥?鍐呮牳

| 椤?| 鐘舵€?|
|----|------|
| ~~CORE-F06~~ Component Activate | **Done** 鈥?`b07009e` |
| ~~CORE-F05~~ Play Mode MVP | **Done** 鈥?S00鈥揝04 + S06锛汼05 Deferred |
| ~~CORE-F07~~ 鍙嶅皠灞曠ず鍚?| **Done** 鈥?宸插悎鍏?`master` |

### C. `master` 鈥?ED-F02 Editor Workflow锛堟梺璺€欓€夛級

[Design](./Editor/ED-F02_EDITOR_WORKFLOW_DESIGN.md) 路 [Impl](./Editor/ED-F02_EDITOR_WORKFLOW_IMPLEMENTATION.md)

| 鍒囩墖 | 鍐呭 | 浼樺厛绾?|
|------|------|--------|
| S00 | Content Browser 鍙屽嚮 鈫?`TryOpenAsset` | 楂橈紙鎺ョ嚎锛?|
| S01 | 鎵撳紑 Scene锛團ile/Open銆佸垏鎹€乨irty锛?| 楂?|
| S02 | 鍒涘缓璧勪骇锛圫cene銆丮aterial銆佲€︼級 | 楂?|
| S03 | Material Editor SkyBox 淇 | 涓?|
| S04 | Viewport 榧犳爣绾︽潫 | 涓?|
| S05 | Abstract Component 杩囨护 + Component 涓嬫媺鍥炬爣 | 浣?|

### D. `master` 鈥?ED-F03 Viewport Play Toolbar

| ID | 鍐呭 | 鐘舵€?|
|----|------|------|
| **ED-F03** | Viewport 涓夎锛歍ab / Toolbar / 涓讳綋 | **Done** 鈥?[Design](./Editor/ED-F03_EDITOR_TOOLBAR_DESIGN.md) |

### E. `master` 鈥?ED-F04 Debug Console锛圡VP 宸叉敹鍙ｏ紝**闈?Done**锛?
| ID | 鍐呭 | 鐘舵€?|
|----|------|------|
| **ED-F04** | Debug Console & Unified Command System | **In Progress** 鈥?MVP S00鈥揝10a **Done**锛沎Design](./Editor/ED-F03_DEBUG_CONSOLE_COMMAND_SYSTEM_DESIGN.md) |

**Deferred锛?* S10b `activate`/`deactivate`锛汼07 ExportSchema锛涙瀬鐭竷灞€锛汣ommand Palette銆?
### F. `master` 鈥?CORE-F07锛堝凡瀹屾垚锛?
| ID | 鍐呭 | 鐘舵€?|
|----|------|------|
| **CORE-F07** | 鍙嶅皠灞曠ず鍚嶅幓 `m_`/`x_`/`b_` 鍓嶇紑 | **Done** |

---

## Done / 缁存姢

- ~~RND-F05 / RND-F11 / AUD-F01 / LAUN-F01 / CORE-F06 / PHYS-F04 / BUG-PHYS-003/004 / CORE-F07 / feat/editor merge / **CORE-F05 MVP**~~
- **ED-F01** 鈥?浠ｇ爜鍦?master锛沄K 闃村奖璐ㄩ噺 defer
- **WF-F02** handbook 鈥?楠ㄦ灦 Done锛屾鏂囨寜闇€

---

## 鎰挎櫙鍗犱綅锛圧egistry only锛屼笉鎺掓湡锛?
| ID | 鍒嗘敮锛堝皢鏉ワ級 | 鍓嶇疆 |
|----|--------------|------|
| `ANIM-F03` | `feat/animation` | **In Progress** runtime MVP锛汼08 Deferred |
| Animation Event / IK / Root Motion / Retarget | 鈥?| 鏈櫥璁帮紱Graph MVP 鍚庡啀璇勪及 |
| `UI-F01` | `feat/ui` | `RND-F16` Sprite 2D |
| `RND-F16` | `feat/sprite`锛堟湭寤猴級 | 鈥?|
| Gameplay 鎻掍欢鍖?/ 缃戠粶 | 鈥?| 浠呮枃妗ｅ崰浣嶏紝瑙?REGISTRY 澶囨敞 |

---

## Verification habit

| Check | Command |
|-------|---------|
| Local smoke | `.\scripts\verify.ps1` |
| Parameter store | `minEngineTests.exe test parameter-store` |
| Physics | `minEngineTests.exe test physics-shapes` / `physics-smoke` |
| GL Editor | `Editor.exe --rhi opengl --project 鈥 |

Record in `PROGRESS_LOG.md` after meaningful slices.

---

## How this relates to other docs

| File | Role |
|------|------|
| [FEATURE_REGISTRY.md](./FEATURE_REGISTRY.md) | IDs and status |
| [PROGRESS_LOG.md](./PROGRESS_LOG.md) | What landed and how it was verified |
| [TECH_DEBT.md](./TECH_DEBT.md) | Open debt rows only |
