-- Editor smoke: LuaScript asset + CORE-F02 ScriptBinding (value types + scene entry).
-- Assign to a LuaComponent on a GO that has a SceneComponent root (typical Editor GO).

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

-- Value-type binding self-check (isolated Transform; does not touch the scene).
local function verify_value_bindings()
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

-- Scene entry: self → Owner → write-back Translate (needs Root SceneComponent).
local function verify_scene_entry()
  expect(self ~= nil, "env self (Component*) was not injected")
  local owner = self:GetOwner()
  expect(owner ~= nil, "self:GetOwner() returned nil — is LuaComponent attached to a GameObject?")

  owner:SetPosition(Vector3.new(0, 0, 0))
  owner:Translate(Vector3.new(1, 0, 0))
  local p = owner:GetPosition()
  expect(nearly_equal(p.x, 1) and nearly_equal(p.y, 0) and nearly_equal(p.z, 0),
    "GameObject Translate did not write back Root position (missing SceneComponent root?)")

  me.log(string.format(
    "HelloTick: SceneEntry OK OwnerPos=(%.1f,%.1f,%.1f)",
    p.x, p.y, p.z))
end

function tick(dt)
  frames = frames + 1
  elapsed = elapsed + dt

  if not bindingVerified then
    verify_value_bindings()
    verify_scene_entry()
    bindingVerified = true
  end

  -- Slow drift so Editor viewport can show script-driven motion.
  if self ~= nil then
    local owner = self:GetOwner()
    if owner ~= nil then
      owner:Translate(Vector3.new(0, 0, dt * 0.25))
    end
  end

  if elapsed >= 1.0 then
    local posStr = "?"
    if self ~= nil then
      local owner = self:GetOwner()
      if owner ~= nil then
        local p = owner:GetPosition()
        posStr = string.format("(%.2f,%.2f,%.2f)", p.x, p.y, p.z)
      end
    end
    me.log(string.format("HelloTick: alive frames=%d dt=%.4f ownerPos=%s", frames, dt, posStr))
    elapsed = 0.0
  end
end
