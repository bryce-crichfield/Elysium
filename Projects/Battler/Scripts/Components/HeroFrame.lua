---@type EntityScript
---@class HeroFrame
-- A hero's portrait frame on the HUD. The battle binds a unit to it (HeroFrame.Bind); the
-- frame then shows that unit's name and health, and lights its Selection ring while the
-- unit is selected. Unbound frames are hidden.
local HeroFrame = {}

-- Shared across every frame (one Lua state): frame root entity -> { unit, selected }.
HeroFrames = HeroFrames or { bindings = {}, frames = {} }

local DRAIN = 0.8  -- of the bar per second
local HEALTH_COLORS = {
    high = {r = 80, g = 220, b = 110, a = 255},
    mid  = {r = 235, g = 190, b = 60, a = 255},
    low  = {r = 235, g = 70, b = 55, a = 255},
}

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
        meter:Set("uFillColor", fraction > 0.5 and HEALTH_COLORS.high or (fraction > 0.25 and HEALTH_COLORS.mid or HEALTH_COLORS.low))
    end

    -- Selection: the ring's fire burns while the unit is selected.
    local sel = self.selection and GetComponent(self.selection, "Material")
    if sel then
        sel.enabled = binding.selected and u.alive
        local fire = sel:Layer("Fire")
        if fire and sel.enabled then fire:Set("uIntensity", 2.4 + 0.5 * math.sin(self.time * 6)) end
    end
end

-- Binds `unit` (a Units table, or nil to clear) to the frame rooted at `frame`.
function HeroFrame.Bind(frame, unit)
    HeroFrames.bindings[frame] = unit and { unit = unit, selected = false } or nil
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

return HeroFrame
