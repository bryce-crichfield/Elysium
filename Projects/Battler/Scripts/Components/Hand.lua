---@type EntityScript
---@class Hand
-- Prefabs/Hand.xml: a hand of cards on a wheel along the bottom of the screen. It spawns its
-- own cards (Prefabs/Card.xml) and binds each one's view; the scene only says what's in it:
--
--   Widgets.BindView("Hand", {
--       show = function() return { cards = {...}, faceUp = true, owner = "Knight" } end,
--                                                    -- or nil: no hand shown
--       playable = function(slot) return true end,   -- (optional) can be paid for now
--       chosen = function() return slot end,         -- (optional) the card being played
--       hidden = function() return true end,         -- (optional) the hand slides out of view
--       peek = function() return true end,           -- (optional) it sinks to just its top edges
--       onPlay = function(slot) end,                 -- a face-up card clicked
--       flights = {},                                -- see below
--   })
--
-- Up to VISIBLE cards sit on the wheel; the mouse wheel over the hand turns it to the rest.
-- Hovering a card lifts it out, full size, over the others. The chosen card flies out of the
-- hand to wait off to the side (HELD); put back (chosen goes nil) it flies home. Played, the
-- scene removes it from the cards and pushes { slot = i, faceDown = bool } onto `flights`: that
-- card flies to the middle of the screen, jiggles, turns over (if it was face down) as it
-- grows for a look, then burns away in a burst. The hand sets the flight's `released` once it
-- starts burning (or at once, if it had no such card), which is when the play should land.
local Widgets = require("Scripts/Components/Widgets")

local Hand = {}

local CARD = "Prefabs/Card.xml"
local W, H = 140, 200        -- the card prefab's size
local VISIBLE = 7
local SPACING = 108          -- between neighbours on the wheel
local ARC = 7                -- how far a card drops per step from the middle, squared
local SHRINK = 0.04          -- how much smaller per step from the middle
local PREVIEW = 1.4          -- the hovered card's scale
local MARGIN = 8
local HELD = { x = 1060, y = 380, s = 1.2 }   -- where the chosen card waits for its target
local DROP = 330             -- how far the hand slides down out of view
local PEEK = 36              -- how much of the cards' tops shows while peeking
-- A played card's show, in seconds per step.
local TRAVEL, JIGGLE, REVEAL, LOOK, BURN, SPARKS = 0.35, 0.28, 0.3, 0.45, 0.7, 0.6
local SWELL = 0.25  -- how much bigger it grows as it burns away
local TRAVEL_SCALE, LOOK_SCALE = 1.15, 1.6

local function Clamp(v, lo, hi) return math.max(lo, math.min(hi, v)) end

function Hand:Initialize(entity)
    self.id = Widgets.PlacementId(entity)
    self.slots = {}   -- { entity, id, view, x, y, s }: the spawned cards, reused as the hand changes
    self.scroll, self.scrollTarget = nil, nil
    self.owner = nil
    self.drop = 0
    self.flying = {}  -- { slot, t, x0, y0, s0, x, y }: played cards on their way out
end

-- Played cards: the card in `slot` leaves the hand's slots (so the rest still line up with
-- the cards) and puts on its show (Hand:Fly); then it's a spare slot again.
function Hand:Launch(flights)
    while #flights > 0 do
        local f = table.remove(flights, 1)
        local slot = table.remove(self.slots, f.slot)
        if slot and slot.view.card then
            slot.view.chosen, slot.view.top, slot.view.hover, slot.view.hidden = false, true, false, false
            slot.view.faceUp = not f.faceDown
            slot.view.dissolve, slot.view.burst = 0, nil
            self.flying[#self.flying + 1] = { slot = slot, flight = f, t = 0, x0 = slot.x, y0 = slot.y, s0 = slot.s }
        else
            if slot then self.slots[#self.slots + 1] = slot end
            f.released = true
        end
    end
end

local function Smooth(k) k = math.max(0, math.min(1, k)) return k * k * (3 - 2 * k) end

-- A played card at `t` seconds into its show: its middle (x, y) and its scale across and down.
local function Pose(f, t, mx, my)
    local cx0, cy0 = f.x0 + W * f.s0 / 2, f.y0 + H * f.s0 / 2
    if t < TRAVEL then
        local k = Smooth(t / TRAVEL)
        local s = f.s0 + (TRAVEL_SCALE - f.s0) * k
        return cx0 + (mx - cx0) * k, cy0 + (my - cy0) * k - math.sin(k * math.pi) * 40, s, s
    end
    t = t - TRAVEL
    if t < JIGGLE then
        -- Anticipation: a shiver that dies out, with a little squash.
        local k = t / JIGGLE
        local shake = math.sin(k * math.pi * 6) * 7 * (1 - k)
        local squash = 1 + math.sin(k * math.pi * 3) * 0.05 * (1 - k)
        return mx + shake, my, TRAVEL_SCALE * squash, TRAVEL_SCALE / squash
    end
    t = t - JIGGLE
    local k = Smooth(math.min(1, t / REVEAL))
    local s = TRAVEL_SCALE + (LOOK_SCALE - TRAVEL_SCALE) * k
    if f.flight.faceDown and t < REVEAL then
        -- Turns over: it narrows to an edge face down and widens back face up.
        return mx, my, s * math.abs(math.cos(k * math.pi)), s
    end
    return mx, my, s, s
end

function Hand:Fly(dt)
    local sw, sh = GetScreenSize()
    local mx, my = sw / 2, sh / 2 - 30
    for i = #self.flying, 1, -1 do
        local f = self.flying[i]
        f.t = f.t + dt
        local slot, view = f.slot, f.slot.view
        local x, y, sx, sy = Pose(f, f.t, mx, my)
        local burning = f.t - (TRAVEL + JIGGLE + REVEAL + LOOK)
        if f.flight.faceDown and f.t > TRAVEL + JIGGLE + REVEAL / 2 then view.faceUp = true end
        if burning >= 0 then
            f.flight.released = true
            view.dissolve = math.min(1, burning / BURN)
            view.burst = burning < SPARKS and burning / SPARKS or nil
            -- Swells as it goes, fast then easing off: a pop that smoulders out.
            local k = math.min(1, burning / BURN)
            local swell = 1 + SWELL * (1 - (1 - k) * (1 - k))
            sx, sy = sx * swell, sy * swell
        end
        slot.s = sy
        slot.x, slot.y = x - W * sx / 2, y - H * sy / 2
        local ct = GetComponent(slot.entity, "Transform")
        if ct then ct.localX, ct.localY, ct.localScaleX, ct.localScaleY = slot.x, slot.y, sx, sy end
        if burning >= math.max(BURN, SPARKS) then
            view.hidden, view.card, view.dissolve, view.burst = true, nil, 0, nil
            if ct then ct.localScaleX = ct.localScaleY end
            table.remove(self.flying, i)
            self.slots[#self.slots + 1] = slot
        end
    end
end

-- The spawned card for slot i, spawning it on first use.
function Hand:Slot(i, ax, ay)
    local slot = self.slots[i]
    if slot then return slot end
    local e = SpawnPrefab(CARD, ax, ay)
    if not e then return nil end
    local _, sh = GetScreenSize()
    slot = { entity = e, id = Widgets.PlacementId(e), view = { hidden = true }, x = ax - W / 2, y = sh, s = 1 }
    Widgets.BindView(slot.id, slot.view)
    self.slots[i] = slot
    return slot
end

function Hand:Update(entity, dt)
    local SCREEN_W, SCREEN_H = GetScreenSize()
    local binding = self.id and Widgets.Binding(self.id)
    local info = binding and binding.show and binding.show()
    local t = GetComponent(entity, "Transform")
    local ax, ay = t.worldX, t.worldY
    local cards = info and info.cards or {}
    local n = #cards
    if binding and binding.flights then self:Launch(binding.flights) end
    self:Fly(dt)
    local hidden = binding and binding.hidden and binding.hidden() or false
    local peek = not hidden and binding and binding.peek and binding.peek() or false
    self.drop = Widgets.Ease(self.drop, hidden and DROP or peek and (SCREEN_H - PEEK - ay) or 0, 10, dt)
    hidden = hidden or peek  -- either way, nothing to hover or click

    for _, child in ipairs(GetChildren(entity)) do
        local layer = GetComponent(child, "Layer")
        if layer then layer.isVisible = info ~= nil end
    end

    -- A new owner starts with the wheel in the middle of their hand.
    local lo, hi = (n + 1) / 2, (n + 1) / 2
    if n > VISIBLE then lo, hi = (VISIBLE + 1) / 2, n - (VISIBLE - 1) / 2 end
    if not info or info.owner ~= self.owner then
        self.owner = info and info.owner
        self.scroll, self.scrollTarget = lo, lo
    end

    -- Where each card sits on the wheel, before any hover: the pointer and clicks test these,
    -- so a card lifting out doesn't change which one is under the pointer.
    local m = GetMousePosition()
    local base, over, hovered, hoveredDx = {}, false, nil, 1e9
    for i = 1, n do
        local d = i - self.scroll
        local s = 1 - math.min(math.abs(d), 4) * SHRINK
        local cx = ax + d * SPACING
        local b = { x = cx - W * s / 2, y = ay + d * d * ARC + self.drop, s = s, cx = cx, shown = math.abs(d) <= VISIBLE / 2 + 0.05 }
        base[i] = b
        if b.shown and not hidden and Widgets.Inside(m, b.x, b.y, W * s, H * s) then
            over = true
            if math.abs(m.x - cx) < hoveredDx then hovered, hoveredDx = i, math.abs(m.x - cx) end
        end
    end

    -- The mouse wheel over the hand turns it (and not the camera: the pointer is over the UI).
    Widgets.SetHovered(self.id, over)
    if over and n > VISIBLE then
        local wheel = GetMouseWheelMove()
        if wheel ~= 0 then self.scrollTarget = self.scrollTarget - wheel end
    end
    self.scrollTarget = Clamp(self.scrollTarget, lo, hi)
    self.scroll = Widgets.Ease(self.scroll, self.scrollTarget, 14, dt)

    local chosen = binding and binding.chosen and binding.chosen()
    -- Spawning fills self.slots in order; flown-out slots wait at the end as spares.
    for i = 1, math.max(n, #self.slots) do
        local slot = i <= n and self:Slot(i, ax, ay) or self.slots[i]
        if slot then
            local view, b = slot.view, base[i]
            if i > n then
                view.hidden, view.card = true, nil
            else
                local tx, ty, ts = b.x, b.y, b.s
                if i == hovered then
                    ts = PREVIEW
                    tx = Clamp(b.cx - W * ts / 2, MARGIN, SCREEN_W - W * ts - MARGIN)
                    ty = SCREEN_H - H * ts - MARGIN
                elseif i == chosen then
                    tx, ty, ts = HELD.x + SCREEN_W - (GetLayoutSize()), HELD.y, HELD.s  -- kept to the right edge
                end
                slot.x = Widgets.Ease(slot.x, tx, 16, dt)
                slot.y = Widgets.Ease(slot.y, ty, 16, dt)
                slot.s = Widgets.Ease(slot.s, ts, 16, dt)
                local ct = GetComponent(slot.entity, "Transform")
                if ct then ct.localX, ct.localY, ct.localScaleX, ct.localScaleY = slot.x, slot.y, slot.s, slot.s end

                view.card = cards[i]
                view.faceUp = info.faceUp
                view.hidden = not b.shown and i ~= chosen
                view.hover = i == hovered
                view.chosen = i == chosen
                view.top = i == hovered or i == chosen
                view.playable = not binding.playable or binding.playable(i)
            end
        end
    end

    if hovered and not hidden and info.faceUp and binding.onPlay and IsMouseButtonPressed(MOUSE_LEFT) then
        binding.onPlay(hovered)
    end

    if info then
        local left = math.max(0, math.ceil(self.scroll - (VISIBLE + 1) / 2 - 0.05))
        local right = math.max(0, math.ceil(n - self.scroll - (VISIBLE - 1) / 2 - 0.05))
        Widgets.ChildText(entity, "LeftMore", left > 0 and ("< " .. left) or "")
        Widgets.ChildText(entity, "RightMore", right > 0 and (right .. " >") or "")
    end
end

return Hand
