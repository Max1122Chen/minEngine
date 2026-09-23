# CORE-F30 — Play main Camera (BeginPlay policy) — Implementation Plan

## Meta
- **ID:** CORE-F30
- **Status:** Done
- **Last updated:** 2026-09-23
- **Related:** [Design](./CORE-F30_PLAY_MAIN_CAMERA_BINDING_DESIGN.md)
- **Depends:** CORE-F31 Done

## Done
- \CameraComponent::m_bSetAsMainCameraOnBeginPlay\ (EditAnywhere)
- \OnBeginPlay\: bool **or** sole active camera → \SetSelfAsMainCamera()- \SetSelfAsMainCamera\: clear other Mains in Scene; Edit viewport push only when \!IsPIEPlayActive- suite \play-main-camera\ PASS
