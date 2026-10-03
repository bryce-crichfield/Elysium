---@type EntityScript
---@class HeroFrame
-- A unit's portrait frame on the HUD. The battle binds a unit to it (HeroFrame.Bind) with its
-- team's colors; the frame then shows that unit's name and health, and lights its Selection
-- ring whenever the unit's own ring is lit. Unbound frames are hidden. HeroFrame.At finds the
-- unit whose frame is under a screen point, for clicks.
local HeroFrame = {}

-- Shared across every frame (one Lua state): frame root entity -> { unit, lit, glow, fill }.
HeroFrames = HeroFrames or { bindings = {}, frames = {} }

local DRAIN = 0.8  -- of the bar per second
local SIZE = 256   -- the prefab's frame, before the placement's scale
local DEFAULT_FILL = {r = 80, g = 220, b = 110, a = 255}

-- Spawned names are namespaced per placement, so match the end of the name.
local function FindChild(root, name)
    for _, e in ipairs(GetChildren(root)) do
        local n = GetComponent(e, "Name")
        if n and n.name:sub(-#name) == name then return e end
    end
    return nil
end

local function SetVisible(entity, visible)
    local layer = entity and GetComponent(entity, "Layer")
    if layer then layer.isVisible = visible end
end

function HeroFrame:Initialize(entity)
    self.portrait = FindChild(entity, "Portrait")
    self.frame = FindChild(entity, "Frame")
    self.selection = FindChild(entity, "Selection")
    self.nameText = FindChild(entity, "NameText")
    self.healthBar = FindChild(entity, "HealthBar")
    self.shown = nil
    self.time = 0
    HeroFrames.bindings[entity] = nil
    for _, e in ipairs(HeroFrames.frames) do if e == entity then return end end
    HeroFrames.frames[#HeroFrames.frames + 1] = entity
end

function HeroFrame:Update(entity, dt)
    self.time = self.time + dt
    local binding = HeroFrames.bindings[entity]
    local u = binding and binding.unit
    for _, e in ipairs({ self.portrait, self.frame, self.nameText, self.healthBar }) do SetVisible(e, u ~= nil) end
    SetVisible(self.selection, u ~= nil)
    if not u then return end

    local text = self.nameText and GetComponent(self.nameText, "Text")
    if text then text.content = u.name end

    -- Health: snap up on heals, drain down on hits.
    local fraction = u.maxHp > 0 and math.max(0, math.min(1, (u.alive and u.hp or 0) / u.maxHp)) or 0
    if not self.shown or fraction > self.shown then
        self.shown = fraction
    else
        self.shown = math.max(fraction, self.shown - DRAIN * dt)
    end
    local mat = self.healthBar and GetComponent(self.healthBar, "Material")
    local meter = mat and mat:Layer("Meter")
    if meter then
        meter:Set("uProgress", self.shown)
        meter:Set("uFillColor", binding.fill or DEFAULT_FILL)
    end

    -- Selection: the frame's fire burns while the unit's ring is lit, in the team's color.
    local sel = self.selection and GetComponent(self.selection, "Material")
    if sel then
        sel.enabled = binding.lit and u.alive
        local fire = sel:Layer("Fire")
        if fire and sel.enabled then
            if binding.glow then fire:Set("uGlowColor", binding.glow) end
            fire:Set("uIntensity", 2.4 + 0.5 * math.sin(self.time * 6))
        end
    end
end

-- Binds `unit` (a Units table, or nil to clear) to the frame rooted at `frame`; `glow` (an
-- {x, y, z} color) tints its Selection fire and `fill` (an {r, g, b, a}) its health bar.
function HeroFrame.Bind(frame, unit, glow, fill)
    HeroFrames.bindings[frame] = unit and { unit = unit, lit = false, glow = glow, fill = fill } or nil
end

-- The frame's rectangle on screen: x, y, w, h.
function HeroFrame.Rect(frame)
    local t = GetComponent(frame, "Transform")
    return t.worldX, t.worldY, SIZE * t.worldScaleX, SIZE * t.worldScaleY
end

-- The unit whose frame is under the screen point (x, y), if any.
function HeroFrame.At(x, y)
    for frame, binding in pairs(HeroFrames.bindings) do
        if binding.unit and binding.unit.alive and HasComponent(frame, "Transform") then
            local fx, fy, fw, fh = HeroFrame.Rect(frame)
            if x >= fx and x <= fx + fw and y >= fy and y <= fy + fh then return binding.unit end
        end
    end
    return nil
end

-- The frames on screen, top to bottom.
function HeroFrame.Frames()
    local list = {}
    for _, e in ipairs(HeroFrames.frames) do
        if HasComponent(e, "Transform") then list[#list + 1] = e end
    end
    table.sort(list, function(a, b) return GetComponent(a, "Transform").localY < GetComponent(b, "Transform").localY end)
    return list
end

HeroFrames.Bind, HeroFrames.Frames = HeroFrame.Bind, HeroFrame.Frames
HeroFrames.Rect, HeroFrames.At = HeroFrame.Rect, HeroFrame.At

return HeroFrame
