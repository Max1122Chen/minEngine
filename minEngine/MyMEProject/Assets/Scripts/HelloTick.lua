-- Editor smoke: LuaScript asset + CORE-F02 generated ScriptBinding (Transform / Vector3).
-- Assign to a LuaComponent; first tick runs a compact binding self-check, then a once-per-second heartbeat.

local frames = 0
local elapsed = 0.0
local bindingVerified = false

local function nearly_equal(a, b)
  return math.abs(a - b) < 1e-4
end

local function expect(cond, message)
  if not cond then
    error(message, 2)
  end
end

-- Sufficient coverage, not a full matrix: construct, property R/W, ScriptCallable, Translate.
local function verify_generated_bindings()
  local t = Transform.new()
  expect(nearly_equal(t.Position.x, 0) and nearly_equal(t.Position.y, 0) and nearly_equal(t.Position.z, 0),
    "Transform.new() should start at origin")

  t.Position = Vector3.new(1.5, -2.0, 3.25)
  expect(nearly_equal(t.Position.x, 1.5) and nearly_equal(t.Position.y, -2.0) and nearly_equal(t.Position.z, 3.25),
    "ScriptReadWrite Position assignment failed")

  t:SetPosition(Vector3.new(10, 20, 30))
  expect(nearly_equal(t.Position.x, 10) and nearly_equal(t.Position.y, 20) and nearly_equal(t.Position.z, 30),
    "ScriptCallable SetPosition failed")

  t:Translate(Vector3.new(1, 2, 3))
  expect(nearly_equal(t.Position.x, 11) and nearly_equal(t.Position.y, 22) and nearly_equal(t.Position.z, 33),
    "ScriptCallable Translate failed")

  me.log(string.format(
    "HelloTick: ScriptBinding OK Position=(%.1f,%.1f,%.1f)",
    t.Position.x, t.Position.y, t.Position.z))
end

function tick(dt)
  frames = frames + 1
  elapsed = elapsed + dt

  if not bindingVerified then
    verify_generated_bindings()
    bindingVerified = true
  end

  if elapsed >= 1.0 then
    me.log(string.format("HelloTick: alive frames=%d dt=%.4f", frames, dt))
    elapsed = 0.0
  end
end
