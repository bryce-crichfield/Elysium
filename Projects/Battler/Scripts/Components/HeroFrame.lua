---@type EntityScript
---@class HeroFrame
-- Prefabs/HeroFrame.xml: a unit's frame on the HUD. The battle binds a unit to it
-- (HeroFrame.Bind) with its team's colors; the frame then shows that unit's name, health,
-- stamina, mana crystals (Prefabs/Crystal.xml) and piles (cards in hand, left in the deck, discarded), and burns whenever the unit's own ring is lit. Unbound frames are
-- hidden. HeroFrame.At finds the unit whose frame is under a screen point, for clicks.
--
-- Tucked away it shows only the portrait; it opens out while its binding says `open` (the
-- battle keeps that to one frame at a time) or `flash` (just hurt, even fatally), and tucks back EXPAND_HOLD seconds after. A frame on the right half of
-- the screen opens leftward, so it stays against its edge.
--
-- It reads the unit's hp/maxHp, stamina/staminaMax, mana/manaMax (crystals grown so far) and
-- piles ({ hand, draw, discard }, if it has them).
--
-- A bar eases to a change (filling up, draining down) while the chunk gained or lost flashes
-- white and fades, and a +N / -N pops off it and floats away, bigger the more of the bar it is.
--
-- The mana sequence (HeroFrames.Recharge, then HeroFrames.StartRecharges): the frame shows the
-- crystals as they were until it's started, then opens, a grown crystal fades in big and slams
-- down into its place, dark, and then the dark ones the unit regained relight one by one, left to right, while the
-- stamina bar refills. Every frame runs it at once, so the grow sound plays once for them all,
-- and the recharge sound once per relit crystal index. HeroFrames.Recharging() says whether any
-- frame is still at it.
--
-- A binding's `preview` ({ hp, stamina, mana }, each optional) is what the unit would be left
-- with by the play being aimed: the part of a bar it would lose pulses white, and so do the
-- crystals it would spend (Crystal.lua's preview).
--
-- Spent mana goes out a crystal at a time, right to left, SPEND_STEP apart: each pops (Crystal.lua)
-- with its own spend sound.
local Widgets = require("Scripts/Components/Widgets")
local Sfx = require("Scripts/Menu/Sfx")

local HeroFrame = {}

-- Shared across every frame (one Lua state): frame root entity -> { unit, lit, glow, fill }.
-- cued: the mana sequence's sounds already played this round.
HeroFrames = HeroFrames or { bindings = {}, frames = {}, cued = {} }

local DRAIN = 0.8   -- of a bar per second
local FILL = 1.2    -- of a bar per second
local CHUNK = 0.7   -- seconds a changed chunk flashes white and fades
local FLOAT = 1.0   -- seconds a +N / -N floats off its bar
local FLOAT_RISE = 34                -- pixels it rises
local FLOAT_MIN, FLOAT_MAX = 16, 40  -- its font size, for a sliver of the bar and for all of it
local FONT = "Fonts/EnchantedLand-Regular.ttf"
local CRYSTALS = 10
local NAME_LEFT = 10
local DEFAULT_FILL = {r = 255, g = 77, b = 77, a = 255}
local STAMINA_FILL = {r = 90, g = 200, b = 90, a = 255}
local OPEN_W, TUCKED_W = 236, 74   -- the panel's width, open and tucked (just the portrait)
local EXPAND_HOLD = 0.12           -- seconds it stays open after it's told to close (no flicker)
-- The mana sequence, in seconds.
local FADE, DROP = 0.22, 0.14      -- a grown crystal fading in big, then slamming down
local BIG = 4                      -- its scale before the slam
local SETTLE = 0.08                -- after the slam, before the relighting
local STEP_FEW, STEP_MANY = 0.3, 0.08  -- between relit crystals, with 1 grown and with CRYSTALS
local LINGER = 0.2                 -- all lit, before the frame lets go
local PREVIEW_PULSE = 7            -- radians a second of the preview's white pulse
local SPEND_STEP = 0.35            -- seconds between crystals going out when mana is spent
local GEM = 14                     -- the crystal's size (Prefabs/Crystal.xml), to scale about its middle
local DETAIL = { "HealthBar", "StaminaBar",
                 "HandIcon", "HandCount", "DeckIcon", "DeckCount", "DiscardIcon", "DiscardCount" }

function HeroFrame:Initialize(entity)
    self.health = Widgets.Child(entity, "HealthBar")
    self.stamina = Widgets.Child(entity, "StaminaBar")
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
    self.bars, self.floats = {}, {}
    self.time = 0
    self.detail = {}
    for _, name in ipairs(DETAIL) do self.detail[#self.detail + 1] = Widgets.Child(entity, name) end
    self.open, self.hold, self.homeX, self.homeY = 0, 0, nil, nil
    HeroFrames.bindings[entity] = nil
    for _, e in ipairs(HeroFrames.frames) do if e == entity then return end end
    HeroFrames.frames[#HeroFrames.frames + 1] = entity
end

-- A bar's shown fraction: fills up on gains, drains down on losses, the chunk between flashing.
-- A change pops its number off the bar (HeroFrame:Pop). With a `preview` below the value, the
-- part it would lose pulses white instead (while no change is flashing).
function HeroFrame:Bar(key, bar, value, max, dt, fill, preview)
    local fraction = max > 0 and math.max(0, math.min(1, value / max)) or 0
    local state = self.bars[key]
    if not state then
        state = { value = value, shown = fraction }
        self.bars[key] = state
    end
    if value ~= state.value then
        local before = max > 0 and math.max(0, math.min(1, state.value / max)) or 0
        state.chunk = { from = math.min(before, fraction), to = math.max(before, fraction), t = 0 }
        self:Pop(bar, value - state.value, max, fraction, fill)
        state.value = value
    end
    if fraction > state.shown then state.shown = math.min(fraction, state.shown + FILL * dt)
    else state.shown = math.max(fraction, state.shown - DRAIN * dt) end
    local flash = 0
    if state.chunk then
        state.chunk.t = state.chunk.t + dt
        flash = math.max(0, 1 - state.chunk.t / CHUNK)
        if flash == 0 then state.chunk = nil end
    end
    local mat = bar and GetComponent(bar, "Material")
    local meter = mat and mat:Layer("Meter")
    if meter then
        meter:Set("uProgress", state.shown)
        if fill then meter:Set("uFillColor", fill) end
        if state.chunk then
            meter:Set("uFlash", flash * flash)
            meter:Set("uFlashFrom", state.chunk.from)
            meter:Set("uFlashTo", state.chunk.to)
        elseif preview and preview < value and max > 0 then
            meter:Set("uFlash", 0.45 + 0.35 * math.sin(self.time * PREVIEW_PULSE))
            meter:Set("uFlashFrom", math.max(0, preview / max))
            meter:Set("uFlashTo", fraction)
        else
            meter:Set("uFlash", 0)
        end
    end
end

-- A +N / -N popping off `bar` where its fill now ends, sized by how much of the bar it is.
function HeroFrame:Pop(bar, delta, max, fraction, fill)
    if delta == 0 or not bar then return end
    local share = max > 0 and math.min(1, math.abs(delta) / max) or 0
    self.floats[#self.floats + 1] = {
        bar = bar, at = fraction, t = 0,
        text = (delta > 0 and "+" or "-") .. tostring(math.abs(delta)),
        size = FLOAT_MIN + (FLOAT_MAX - FLOAT_MIN) * math.sqrt(share),
        color = delta > 0 and (fill or DEFAULT_FILL) or {r = 255, g = 255, b = 255, a = 255},
    }
end

-- The numbers floating off the bars: each pops in big, settles, then rises and fades.
function HeroFrame:DrawFloats(dt, shown)
    for i = #self.floats, 1, -1 do
        local f = self.floats[i]
        f.t = f.t + dt
        if f.t >= FLOAT then
            table.remove(self.floats, i)
        elseif shown then
            local k = f.t / FLOAT
            local pop = 1 + 0.6 * math.max(0, 1 - k * 5) ^ 2
            local size = math.floor(f.size * pop + 0.5)
            local x, y, w, h = Widgets.Rect(f.bar)
            local tw = MeasureText(f.text, size, FONT)
            local a = math.floor(255 * math.min(1, (1 - k) * 2.5))
            local c = f.color
            DrawText(f.text, x + w * f.at - tw / 2, y + h / 2 - size / 2 - FLOAT_RISE * k * k, size,
                     {r = c.r, g = c.g, b = c.b, a = a}, "ui", FONT)
        end
    end
end

function HeroFrame:Update(entity, dt)
    self.time = self.time + dt
    local binding = HeroFrames.bindings[entity]
    local u = binding and binding.unit
    Widgets.SetVisible(entity, u ~= nil)
    if u ~= self.unit then self.unit, self.bars, self.floats, self.mana = u, {}, {}, nil end
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
    -- Each bar's x/X sits in it, shown only while the pointer is on that bar (just a look: the
    -- frame's own hover is untouched).
    local m = GetMousePosition()
    for _, bar in ipairs({ self.health, self.stamina }) do
        local vitals = Widgets.Child(bar, "Vitals")
        local vlayer = vitals and GetComponent(vitals, "Layer")
        if vlayer then vlayer.isVisible = detailed and Widgets.Inside(m, Widgets.Rect(bar)) end
    end

    Widgets.ChildText(entity, "NameText", u.name, nil, NAME_LEFT)
    Widgets.ChildText(self.health, "Vitals", string.format("%d/%d", u.alive and u.hp or 0, u.maxHp))
    local preview = binding.preview or {}
    self:Bar("health", self.health, u.alive and u.hp or 0, u.maxHp, dt, binding.fill or DEFAULT_FILL, preview.hp)
    if self.stamina then
        -- Through the mana sequence it shows what it had until the sequence starts, then refills.
        local seq = binding.recharge
        local stamina = (seq and not (seq.started and detailed)) and seq.stamina or u.stamina or 0
        Widgets.ChildText(self.stamina, "Vitals", string.format("%d/%d", stamina, u.staminaMax or 0))
        self:Bar("stamina", self.stamina, stamina, u.staminaMax or 0, dt, STAMINA_FILL, preview.stamina)
    end
    self:DrawFloats(dt, detailed)
    local piles = u.piles or {}
    Widgets.ChildText(entity, "HandCount", tostring(#(piles.hand or {})), nil, 92)
    Widgets.ChildText(entity, "DeckCount", tostring(#(piles.draw or {})), nil, 142)
    Widgets.ChildText(entity, "DiscardCount", tostring(#(piles.discard or {})), nil, 192)

    -- One crystal per crystal grown; the first `mana` of them full (or as the sequence has them).
    local shake = 0
    if binding.recharge then
        shake = self:ManaSequence(binding, u, detailed, dt)
    else
        self:SpendStep(u, dt)
        for k, crystal in ipairs(self.crystals) do
            Widgets.SetVisible(crystal.entity, k <= (u.manaMax or 0) and detailed)
            crystal.view.charged = k <= self.mana
            crystal.view.preview = preview.mana ~= nil and k > preview.mana and k <= self.mana
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

-- Steps the lit crystals (self.mana) toward the unit's mana: gains light at once, spending puts
-- them out one at a time, each with a spend sound.
function HeroFrame:SpendStep(u, dt)
    local mana = u.mana or 0
    if not self.mana or mana >= self.mana then
        self.mana, self.spendWait = mana, 0
        return
    end
    self.spendWait = math.max(0, (self.spendWait or 0) - dt)
    if self.spendWait == 0 then
        self.mana = self.mana - 1
        self.spendWait = SPEND_STEP
        Sfx.Play(Sfx.MANA_SPEND)
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
            if not lit and k <= (u.mana or 0) then
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
    if grown and t >= slam then HeroFrame.Cue(Sfx.MANA_GROW, "grow") end
    local relit = dead > 0 and math.min(dead, math.floor((t - first) / STEP) + 1) or 0
    for n = 1, relit do HeroFrame.Cue(Sfx.MANA_RECHARGE, "recharge" .. n) end
    if t >= first + dead * STEP + LINGER then binding.recharge = nil end
    -- The slam's jolt: a quick dip that settles.
    local since = grown and t - slam or -1
    if since >= 0 and since < 0.18 then return math.sin(since / 0.18 * math.pi) * 3 * (1 - since / 0.18) end
    return 0
end

-- Starts `unit`'s mana sequence on its frame: it had `mana` lit, crystals grown up to `max` and
-- `stamina` before this phase's growth and refills. It waits for HeroFrames.StartRecharges.
function HeroFrame.Recharge(unit, mana, max, stamina)
    for _, binding in pairs(HeroFrames.bindings) do
        if binding.unit == unit then
            binding.recharge = { from = mana or 0, grew = (unit.manaMax or 0) > (max or 0), t = 0, started = false,
                                 stamina = stamina or unit.stamina or 0 }
        end
    end
end

-- Plays `sound` for the step `key` of this round's mana sequence, unless some frame already has.
function HeroFrame.Cue(sound, key)
    if HeroFrames.cued[key] then return end
    HeroFrames.cued[key] = true
    Sfx.Play(sound)
end

function HeroFrame.StartRecharges()
    HeroFrames.cued = {}
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
