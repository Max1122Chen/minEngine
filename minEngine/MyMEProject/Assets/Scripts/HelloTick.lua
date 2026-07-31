-- Easy editor smoke script for CORE-F01 / LuaScript asset.
-- Assign this asset to a LuaComponent on any GameObject, then watch the Editor log.

local frames = 0
local elapsed = 0.0

function tick(dt)
  frames = frames + 1
  elapsed = elapsed + dt

  if frames == 1 then
    me.log("HelloTick: first tick (LuaScript asset is running)")
  end

  -- Log about once per second so the console stays readable.
  if elapsed >= 1.0 then
    me.log(string.format("HelloTick: alive frames=%d dt=%.4f", frames, dt))
    elapsed = 0.0
  end
end
