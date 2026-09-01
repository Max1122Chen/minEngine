-- Editor ear-test: call AudioComponent:Play() on the first script tick.
-- Setup on the same GameObject:
--   - SceneComponent root (default when creating a GO)
--   - AudioComponent with Clip assigned in Inspector
--   - LuaComponent with this script asset
-- Alternative: skip this script and enable Play On Awake on AudioComponent only.

local played = false

function tick(dt)
  if played then
    return
  end
  played = true

  local owner = self:GetOwner()
  if owner == nil then
    me.log("AudioPlayOnStart: self:GetOwner() returned nil")
    return
  end

  local component = owner:FindComponentByClassName("AudioComponent")
  local audio = AsAudioComponent(component)
  if audio == nil then
    me.log("AudioPlayOnStart: AudioComponent missing on owner")
    return
  end

  audio:Play()
  me.log("AudioPlayOnStart: Play() called")
end
