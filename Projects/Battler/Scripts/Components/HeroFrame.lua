---@type EntityScript
---@class HeroFrame
-- Prefabs/HeroFrame.xml: a unit's frame on the HUD. The battle binds a unit to it
-- (HeroFrame.Bind) with its team's colors; the frame then shows that unit's name, health,
-- stamina, mana crystals and piles (cards in hand, left in the deck, discarded), and burns whenever the unit's own ring is lit. Unbound frames are
-- hidden. HeroFrame.At finds the unit whose frame is under a screen point, for clicks.
--
-- Tucked away it shows only the portrait; it opens out while its binding says `open` (the
-- battle keeps that to one frame at a time) or `flash` (just hurt, even fatally), and tucks back EXPAND_HOLD seconds after. A frame on the right half of
-- the screen opens leftward, so it stays against its edge.
--
-- It reads the unit's hp/maxHp, stamina/maxStamina, mana/manaMax (crystals grown so far) and
-- piles ({ hand, draw, discard }, if it has them).
local Widgets = require("Scripts/Components/Widgets")

local HeroFrame = {}

-- Shared across every frame (one Lua state): frame root entity -> { unit, lit, glow, fill }.
HeroFrames = HeroFrames or { bindings = {}, frames = {} }

local DRAIN = 0.8   -- of a bar per second
local CRYSTALS = 10
local NAME_LEFT = 78
local DEFAULT_FILL = {r = 255, g = 77, b = 77, a = 255}
local CRYSTAL_FULL = {r = 80, g = 150, b = 255, a = 255}
local CRYSTAL_EMPTY = {r = 20, g = 30, b = 60, a = 255}
local OPEN_W, TUCKED_W = 236, 74   -- the panel's width, open and tucked (just the portrait)
local EXPAND_HOLD = 0.12           -- seconds it stays open after it's told to close (no flicker)
local DETAIL = { "NameText", "Vitals", "HealthBar", "StaminaBar",
                 "HandIcon", "HandCount", "DeckIcon", "DeckCount", "DiscardIcon", "DiscardCount" }

function HeroFrame:Initialize(entity)
    self.health = Widgets.Child(entity, "HealthBar")
    self.stamina = Widgets.Child(entity, "StaminaBar")
    self.crystals = {}
    for k = 1, CRYSTALS do self.crystals[k] = Widgets.Child(entity, "Crystal" .. k) end
    self.shown = {}
    self.time = 0
    self.detail = {}
    for _, name in ipairs(DETAIL) do self.detail[#self.detail + 1] = Widgets.Child(entity, name) end
    self.open, self.hold, self.homeX = 0, 0, nil
    HeroFrames.bindings[entity] = nil
    for _, e in ipairs(HeroFrames.frames) do if e == entity then return end end
    HeroFrames.frames[#HeroFrames.frames + 1] = entity
end

-- A bar's shown fraction: snaps up on gains, drains down on losses.
function HeroFrame:Bar(key, bar, value, max, dt, fill)
    local fraction = max > 0 and math.max(0, math.min(1, value / max)) or 0
    local shown = self.shown[key]
    if not shown or fraction > shown then shown = fraction else shown = math.max(fraction, shown - DRAIN * dt) end
    self.shown[key] = shown
    local mat = bar and GetComponent(bar, "Material")
    local meter = mat and mat:Layer("Meter")
    if meter then
        meter:Set("uProgress", shown)
        if fill then meter:Set("uFillColor", fill) end
    end
end

function HeroFrame:Update(entity, dt)
    self.time = self.time + dt
    local binding = HeroFrames.bindings[entity]
    local u = binding and binding.unit
    Widgets.SetVisible(entity, u ~= nil)
    if not u then return end

    -- Open while hovered or lit, and for a moment after.
    local t = GetComponent(entity, "Transform")
    local rect = GetComponent(entity, "Rectangle")
    self.homeX = self.homeX or t.localX
    if (binding.open and u.alive) or binding.flash then self.hold = EXPAND_HOLD
    else self.hold = math.max(0, self.hold - dt) end
    self.open = Widgets.Ease(self.open, self.hold > 0 and 1 or 0, 16, dt)
    local width = TUCKED_W + (OPEN_W - TUCKED_W) * self.open
    rect.width = width
    if self.homeX + OPEN_W / 2 > 640 then t.localX = self.homeX + OPEN_W - width end
    local detailed = self.open > 0.85
    for _, e in ipairs(self.detail) do
        local layer = GetComponent(e, "Layer")
        if layer then layer.isVisible = detailed end
    end

    Widgets.ChildText(entity, "NameText", u.name, nil, NAME_LEFT)
    Widgets.ChildText(entity, "Vitals", string.format("%d/%d", u.alive and u.hp or 0, u.maxHp))
    self:Bar("health", self.health, u.alive and u.hp or 0, u.maxHp, dt, binding.fill or DEFAULT_FILL)
    self:Bar("stamina", self.stamina, u.stamina or 0, u.maxStamina or 0, dt)
    local piles = u.piles or {}
    Widgets.ChildText(entity, "HandCount", tostring(#(piles.hand or {})), nil, 92)
    Widgets.ChildText(entity, "DeckCount", tostring(#(piles.draw or {})), nil, 142)
    Widgets.ChildText(entity, "DiscardCount", tostring(#(piles.discard or {})), nil, 192)

    -- One crystal per crystal grown; the first `mana` of them full.
    for k, crystal in ipairs(self.crystals) do
        local grown = k <= (u.manaMax or 0)
        local layer = GetComponent(crystal, "Layer")
        if layer then layer.isVisible = grown and detailed end
        if grown then Widgets.SetFill(crystal, k <= (u.mana or 0) and CRYSTAL_FULL or CRYSTAL_EMPTY) end
    end

    -- The root burns while the unit's ring is lit, in the team's color.
    local mat = GetComponent(entity, "Material")
    local fire = mat and mat:Layer("Fire")
    if fire then
        if binding.glow then fire:Set("uGlowColor", binding.glow) end
        fire:Set("uIntensity", (binding.lit and u.alive) and (1.4 + 0.3 * math.sin(self.time * 6)) or 0)
    end
end

-- Binds `unit` (a Units table, or nil to clear) to the frame rooted at `frame`; `glow` (an
-- {x, y, z} color) tints its fire and `fill` (an {r, g, b, a}) its health bar.
function HeroFrame.Bind(frame, unit, glow, fill)
    HeroFrames.bindings[frame] = unit and { unit = unit, lit = false, open = false, glow = glow, fill = fill } or nil
end

-- The frame's rectangle on screen: x, y, w, h.
function HeroFrame.Rect(frame) return Widgets.Rect(frame) end

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
