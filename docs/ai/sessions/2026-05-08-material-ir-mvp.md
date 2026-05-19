# 2026-05-08 - Material IR MVP

- Background
  - User requested a simple, learnable MVP pipeline to translate a MaterialEdGraph into GLSL.
  - Goal includes logging an IR dump and GLSL output on editor startup.

- Work done
  - Added MIR graph/value/node structures and literal value representation for float/vector constants.
  - Implemented MIRBuilder with caching, binary ops, texture parameter, and texture sampling builders.
  - Added node defs for constant2, multiply, texture parameter/sample and fixed Add node IR emission.
  - Extended GLSL material compiler for texture uniforms and texture() emission.
  - Implemented MaterialIR test graph creation, IR dump, GLSL compilation helpers, and editor startup logging.
  - Fixed editor graph pin ownership so ConnectNodes works.
  - Added a MaterialOutput node and wired Albedo into MIRGraph outputs.
  - Enforced MaterialOutput-only compilation and updated the MVP test graph to end in that node.

- Open issues
  - No optimization passes (constant folding, DCE) and no stage separation yet.
  - Texture sampling uses constant UVs and a single sampler2D uniform in the test.
  - Diagnostics are minimal; builder returns null on invalid inputs without rich error context.
  - Only Albedo is emitted; other material properties are not yet supported.

- First next action
  - Add Emissive/Opacity outputs or defaults for a fuller unlit MVP.
