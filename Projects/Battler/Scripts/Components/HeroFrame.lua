---@type EntityScript
---@class HeroFrame
-- Prefabs/HeroFrame.xml: a unit's frame on the HUD. The battle binds a unit to it
-- (HeroFrame.Bind) with its team's colors; the frame then shows that unit's name, health,
-- mana crystals (Prefabs/Crystal.xml) and piles (cards in hand, left in the deck, discarded), and burns whenever the unit's own ring is lit. Unbound frames are
-- hidden. HeroFrame.At finds the unit whose frame is under a screen point, for clicks.
--
-- Tucked away it shows only the portrait; it opens out while its binding says `open` (the
-- battle keeps that to one frame at a time) or `flash` (just hurt, even fatally), and tucks back EXPAND_HOLD seconds after. A frame on the right half of
-- the screen opens leftward, so it stays against its edge.
--
-- It reads the unit's hp/maxHp, mana/manaMax (crystals grown so far) and
-- piles ({ hand, draw, discard }, if it has them).
--
-- The mana sequence (HeroFrames.Recharge, then HeroFrames.StartRecharges): the frame shows the
-- crystals as they were until it's started, then opens, a grown crystal fades in big and slams
-- down into its place, dark, and then the dark ones relight one by one, left to right. HeroFrames.Recharging() says
-- whether any frame is still at it.
local Widgets = require("Scripts/Components/Widgets")

local HeroFrame = {}

-- Shared across every frame (one Lua state): frame root entity -> { unit, lit, glow, fill }.
HeroFrames = HeroFrames or { bindings = {}, frames = {} }

local DRAIN = 0.8   -- of a bar per second
local CRYSTALS = 10
local NAME_LEFT = 10
local DEFAULT_FILL = {r = 255, g = 77, b = 77, a = 255}
local OPEN_W, TUCKED_W = 236, 74   -- the panel's width, open and tucked (just the portrait)
local EXPAND_HOLD = 0.12           -- seconds it stays open after it's told to close (no flicker)
-- The mana sequence, in seconds.
local FADE, DROP = 0.22, 0.14      -- a grown crystal fading in big, then slamming down
local BIG = 4                      -- its scale before the slam
local SETTLE = 0.08                -- after the slam, before the relighting
local STEP_FEW, STEP_MANY = 0.3, 0.08  -- between relit crystals, with 1 grown and with CRYSTALS
local LINGER = 0.2                 -- all lit, before the frame lets go
local GEM = 14                     -- the crystal's size (Prefabs/Crystal.xml), to scale about its middle
local DETAIL = { "HealthBar",
                 "HandIcon", "HandCount", "DeckIcon", "DeckCount", "DiscardIcon", "DiscardCount" }

function HeroFrame:Initialize(entity)
    self.health = Widgets.Child(entity, "HealthBar")
    -- Each crystal reads its view by its root entity (see Crystal.lua); Crystal1..10 run left to right.
    self.crystals = {}
    for k = 1, CRYSTALS do
        local e = Widgets.Child(entity, "Crystal" .. k)
        if e then
            local view = { charged = true }
            Widgets.BindView(e, view)
            self.crystals[#self.crystals + 1] = { entity = e, view = view }
        end
    end
    self.shown = {}
    self.time = 0
    self.detail = {}
    for _, name in ipairs(DETAIL) do self.detail[#self.detail + 1] = Widgets.Child(entity, name) end
    self.open, self.hold, self.homeX, self.homeY = 0, 0, nil, nil
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
    self.homeY = self.homeY or t.localY
    local seq = binding.recharge
    if seq and not seq.started then seq = nil end
    if (binding.open and u.alive) or binding.flash or seq then self.hold = EXPAND_HOLD
    else self.hold = math.max(0, self.hold - dt) end
    self.open = Widgets.Ease(self.open, self.hold > 0 and 1 or 0, 16, dt)
    local width = TUCKED_W + (OPEN_W - TUCKED_W) * self.open
    rect.width = width
    -- A frame placed on the right half of the layout keeps to the screen's right edge.
    local sw = GetScreenSize()
    local lw = GetLayoutSize()
    if self.homeX + OPEN_W / 2 > lw / 2 then
        t.localX = self.homeX + (sw - lw) + OPEN_W - width
    else
        t.localX = self.homeX
    end
    local detailed = self.open > 0.85
    for _, e in ipairs(self.detail) do
        local layer = GetComponent(e, "Layer")
        if layer then layer.isVisible = detailed end
    end
    -- The x/X sits in the health bar, shown only while the pointer is on the bar (just a look:
    -- the frame's own hover is untouched).
    local vitals = self.health and Widgets.Child(self.health, "Vitals")
    local vlayer = vitals and GetComponent(vitals, "Layer")
    if vlayer then
        local m = GetMousePosition()
        vlayer.isVisible = detailed and Widgets.Inside(m, Widgets.Rect(self.health))
    end

    Widgets.ChildText(entity, "NameText", u.name, nil, NAME_LEFT)
    Widgets.ChildText(self.health, "Vitals", string.format("%d/%d", u.alive and u.hp or 0, u.maxHp))
    self:Bar("health", self.health, u.alive and u.hp or 0, u.maxHp, dt, binding.fill or DEFAULT_FILL)
    local piles = u.piles or {}
    Widgets.ChildText(entity, "HandCount", tostring(#(piles.hand or {})), nil, 92)
    Widgets.ChildText(entity, "DeckCount", tostring(#(piles.draw or {})), nil, 142)
    Widgets.ChildText(entity, "DiscardCount", tostring(#(piles.discard or {})), nil, 192)

    -- One crystal per crystal grown; the first `mana` of them full (or as the sequence has them).
    local shake = 0
    if binding.recharge then
        shake = self:ManaSequence(binding, u, detailed, dt)
    else
        for k, crystal in ipairs(self.crystals) do
            Widgets.SetVisible(crystal.entity, k <= (u.manaMax or 0) and detailed)
            crystal.view.charged = k <= (u.mana or 0)
        end
    end
    t.localY = self.homeY + shake

    -- The root burns while the unit's ring is lit, in the team's color.
    local mat = GetComponent(entity, "Material")
    local fire = mat and mat:Layer("Fire")
    if fire then
        if binding.glow then fire:Set("uGlowColor", binding.glow) end
        fire:Set("uIntensity", (binding.lit and u.alive) and (1.4 + 0.3 * math.sin(self.time * 6)) or 0)
    end
end

-- The mana sequence's frame: which crystals show and are lit, and the grown one's slam.
-- Returns how far the frame jolts down (the slam landing). Its clock runs once it's started
-- and the frame has opened; at the end it clears binding.recharge.
function HeroFrame:ManaSequence(binding, u, detailed, dt)
    local seq = binding.recharge
    if seq.started and detailed then seq.t = seq.t + dt end
    local t, max = seq.t, u.manaMax or 0
    local grown = seq.grew and max or nil
    local slam = grown and FADE + DROP or 0
    local first = grown and slam + SETTLE or 0.1
    local STEP = STEP_FEW + (STEP_MANY - STEP_FEW) * math.max(0, max - 1) / (CRYSTALS - 1)
    local dead = 0
    for k = 1, max do
        local crystal = self.crystals[k]
        if crystal then
            local lit = k <= seq.from
            local shown = detailed
            local scale, opacity = 1, 1
            if k == grown then
                -- It comes in dark; the relighting reaches it last, being the rightmost.
                lit = false
                shown = shown and t > 0
                if t < FADE then
                    scale, opacity = BIG, t / FADE
                elseif t < slam then
                    local d = (t - FADE) / DROP
                    scale = BIG + (1 - BIG) * d * d * d  -- speeds up into the slam
                end
            end
            if not lit then
                dead = dead + 1
                lit = t >= first + (dead - 1) * STEP
            end
            Widgets.SetVisible(crystal.entity, shown)
            crystal.view.charged = lit
            local ct = GetComponent(crystal.entity, "Transform")
            if ct and not crystal.x then crystal.x, crystal.y = ct.localX, ct.localY end
            if ct then
                ct.localScaleX, ct.localScaleY = scale, scale
                ct.localX, ct.localY = crystal.x - GEM * (scale - 1) / 2, crystal.y - GEM * (scale - 1) / 2
            end
            local layer = GetComponent(crystal.entity, "Layer")
            if layer then layer.opacity = opacity end
        end
    end
    for k = max + 1, CRYSTALS do
        if self.crystals[k] then Widgets.SetVisible(self.crystals[k].entity, false) end
    end
    if t >= first + dead * STEP + LINGER then binding.recharge = nil end
    -- The slam's jolt: a quick dip that settles.
    local since = grown and t - slam or -1
    if since >= 0 and since < 0.18 then return math.sin(since / 0.18 * math.pi) * 3 * (1 - since / 0.18) end
    return 0
end

-- Starts `unit`'s mana sequence on its frame: it had `mana` lit and had crystals grown up to
-- `max` before this phase's growth and refill. It waits for HeroFrames.StartRecharges.
function HeroFrame.Recharge(unit, mana, max)
    for _, binding in pairs(HeroFrames.bindings) do
        if binding.unit == unit then
            binding.recharge = { from = mana or 0, grew = (unit.manaMax or 0) > (max or 0), t = 0, started = false }
        end
    end
end

function HeroFrame.StartRecharges()
    for _, binding in pairs(HeroFrames.bindings) do
        if binding.recharge then binding.recharge.started = true end
    end
end

function HeroFrame.Recharging()
    for _, binding in pairs(HeroFrames.bindings) do
        if binding.recharge then return true end
    end
    return false
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
HeroFrames.Recharge, HeroFrames.StartRecharges = HeroFrame.Recharge, HeroFrame.StartRecharges
HeroFrames.Recharging = HeroFrame.Recharging

return HeroFrame
