# Render thread state boundary: proxies carry the draw gate

## Problem

`ForwardRenderer` decided what to draw by dereferencing game-thread components during the render
stage:

- `ContributesLight()` asked `lightProxy->m_LightComponent->IsActiveAndVisible()`;
- the primitive collection loop asked `primitiveProxy->m_PrimitiveComponent->IsActiveAndVisible()`;
- the ScreenUI queue asked the widget component and its owning canvas component.

Nothing synchronized those reads with the game thread, and every pass that needed the answer
re-derived it. The same decision already had a home in the codebase: `SkyBoxSceneProxy::m_Enabled`
is computed by `RenderScene` and only read by `SkyBoxPass` / `ManualRenderer`.

## Boundary

| Side | Owns | May do |
| --- | --- | --- |
| Game thread (`RenderScene`) | component lifetime, active/visible state | write proxy fields |
| Render thread (`RenderPipeline/**`) | passes, queues, graph | read proxy fields only |

Registration stays where it was: `RenderScene::UpdatePrimitive` / `UpdateLight` / `UpdateWidget`
still gate proxy creation and update on the component being active, because that is proxy lifetime,
not draw state. A component switched off stops being registered; a component switched invisible keeps
its proxy and only stops contributing.

## Changes

- `PrimitiveSceneProxy::m_Enabled` and `LightSceneProxy::m_Enabled` (default true) hold
  active-and-visible, named like `SkyBoxSceneProxy::m_Enabled`.
- `RenderScene::UpdatePrimitive` and `RenderScene::UpdateLight` write the bit once per update, after
  the create/sync branch, so the creation path and the sync path cannot disagree.
- `RenderScene::UpdateWidget` publishes the widget's participation: the widget's assembled draw bit
  is ANDed with its owning canvas being active. `WidgetComponent::FillSceneProxy` already folds the
  widget's own active-and-visible state into `WidgetSceneProxy::m_bVisible`, so this is the one term
  that was still missing.
- `ForwardRenderer` reads the proxy bits in all three places; no component state query remains under
  `RenderPipeline/`.

## Why the game thread updates are sufficient

`SceneComponent::SetVisible`, `GameObject::SetVisible` (through `ApplyEffectiveStateToComponents`)
and `Component::SetActive` (through `NotifyOwnerWidgetContentChanged`) all mark the affected
components' render state dirty, and end-of-frame update runs before the frame is rendered. The proxy
bit is therefore refreshed in the same frame in which the component's state changed.

## Mechanical gate

`scripts/audit_render_thread_state.py`, wired into `scripts/verify.ps1`:

- rule 1: no `->IsActive` / `->IsActiveAndVisible` / `->IsVisible` (and their `*Self` /
  `*InHierarchy` spellings) anywhere under `Runtime/Function/Render/RenderPipeline/**`;
- rule 2: the only whitelisted file is `RenderScene.cpp`; a missing whitelist file fails;
- rule 3: an empty scan fails. The audit asserts its own pattern still matches the game-thread
  publish sites and that the pipeline still reads a proxy draw bit, so a renamed path or a changed
  spelling cannot turn the gate into a vacuous pass.

## Known limitation

The canvas term is published when a widget is updated. Deactivating a `CanvasComponent` does not by
itself dirty the widgets under it (they live on descendant GameObjects, while
`NotifyOwnerWidgetContentChanged` reaches siblings only), so that widget keeps its last assembled
picture until something else updates it. Closing the gap means letting canvas activation dirty its
descendant widgets, which is a component-side change and out of scope here. The same shape of delay
already exists for the sky box proxy.

Also unchanged: `BuildScreenUIQueue` still resolves the owning canvas to read the sort order, scale
mode and reference resolution. Those are canvas settings, not draw state, but they are read on the
render thread for the same reason and would follow the same treatment.
