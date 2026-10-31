# Sprite asset fixtures

## legacy_baked_sprite.mesprite

Verbatim output of `AssetSerializeHelper::WriteObjectToFile` for a sprite that had just been
baked by `SpriteAtlas::Rebuild`, captured before the baked pair became runtime-only state. It is
committed as bytes rather than rebuilt by the test because the skip-unknown-field path is only
meaningful against a file some build actually wrote: a hand-written one would encode the author's
guess about the writer instead of the writer's output.

The load test that reads it recreates the source texture under the guid the file names, so the
baked page reference is the only one that cannot resolve. Keep the file as it is; regenerating it
with the current writer would drop `m_BakedTexture` and `m_BakedRectPx` and the test would lose
its subject.
