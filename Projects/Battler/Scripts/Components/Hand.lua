---@type EntityScript
---@class Hand
-- Prefabs/Hand.xml: a hand of cards on a wheel along the bottom of the screen. It spawns its
-- own cards (Prefabs/Card.xml) and binds each one's view; the scene only says what's in it:
--
--   Widgets.BindView("Hand", {
--       show = function() return { cards = {...}, faceUp = true, owner = "Marsh",
--                                  deck = 23, discard = 4 } end,   -- or nil: no hand shown
--       playable = function(slot) return true end,   -- (optional) can be paid for now
--       chosen = function() return slot end,         -- (optional) the card being played
--       hidden = function() return true end,         -- (optional) the hand slides out of view
--       onPlay = function(slot) end,                 -- a face-up card clicked
--       flights = {},                                -- see below
--   })
--
-- Up to VISIBLE cards sit on the wheel; the mouse wheel over the hand turns it to the rest.
-- Hovering a card lifts it out, full size, over the others. The chosen card flies out of the
-- hand to wait off to the side (HELD); put back (chosen goes nil) it flies home. Played, the
-- scene removes it from the cards and pushes { slot = i, x = sx, y = sy } onto `flights`: that
-- card flies from where it is into the screen point and burns out.
local Widgets = require("Scripts/Components/Widgets")

local Hand = {}

local CARD = "Prefabs/Card.xml"
local W, H = 140, 200        -- the card prefab's size
local VISIBLE = 7
local SPACING = 108          -- between neighbours on the wheel
local ARC = 7                -- how far a card drops per step from the middle, squared
local SHRINK = 0.04          -- how much smaller per step from the middle
local PREVIEW = 1.4          -- the hovered card's scale
local SCREEN_H, MARGIN = 720, 8
local HELD = { x = 1060, y = 380, s = 1.2 }   -- where the chosen card waits for its target
local DROP = 330             -- how far the hand slides down out of view
local FLIGHT = 0.38          -- seconds a played card takes to reach its target

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
-- the cards) and flies to (x, y), shrinking and burning; then it's a spare slot again.
function Hand:Launch(flights)
    while #flights > 0 do
        local f = table.remove(flights, 1)
        local slot = table.remove(self.slots, f.slot)
        if slot then
            slot.view.chosen, slot.view.top, slot.view.hover, slot.view.hidden = true, true, false, false
            self.flying[#self.flying + 1] = { slot = slot, t = 0, x0 = slot.x, y0 = slot.y, s0 = slot.s, x = f.x, y = f.y }
        end
    end
end

function Hand:Fly(dt)
    for i = #self.flying, 1, -1 do
        local f = self.flying[i]
        f.t = f.t + dt
        local k = math.min(1, f.t / FLIGHT)
        local e = k * k  -- speeds up into the target
        local s = f.s0 + (0.3 - f.s0) * e
        local slot = f.slot
        slot.s = s
        -- Its middle travels to the point.
        local cx0, cy0 = f.x0 + W * f.s0 / 2, f.y0 + H * f.s0 / 2
        local cx, cy = cx0 + (f.x - cx0) * e, cy0 + (f.y - cy0) * e - math.sin(k * math.pi) * 60
        slot.x, slot.y = cx - W * s / 2, cy - H * s / 2
        local ct = GetComponent(slot.entity, "Transform")
        if ct then ct.localX, ct.localY, ct.localScaleX, ct.localScaleY = slot.x, slot.y, s, s end
        if k >= 1 then
            slot.view.hidden, slot.view.card = true, nil
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
    slot = { entity = e, id = Widgets.PlacementId(e), view = { hidden = true }, x = ax - W / 2, y = SCREEN_H, s = 1 }
    Widgets.BindView(slot.id, slot.view)
    self.slots[i] = slot
    return slot
end

function Hand:Update(entity, dt)
    local binding = self.id and Widgets.Binding(self.id)
    local info = binding and binding.show and binding.show()
    local t = GetComponent(entity, "Transform")
    local ax, ay = t.worldX, t.worldY
    local cards = info and info.cards or {}
    local n = #cards
    if binding and binding.flights then self:Launch(binding.flights) end
    self:Fly(dt)
    local hidden = binding and binding.hidden and binding.hidden() or false
    self.drop = Widgets.Ease(self.drop, hidden and DROP or 0, 10, dt)

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
                    tx = Clamp(b.cx - W * ts / 2, MARGIN, 1280 - W * ts - MARGIN)
                    ty = SCREEN_H - H * ts - MARGIN
                elseif i == chosen then
                    tx, ty, ts = HELD.x, HELD.y, HELD.s
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
        Widgets.ChildText(entity, "Owner", info.owner or "", nil, -520)
        Widgets.ChildText(entity, "Piles", string.format("Deck %d   Discard %d", info.deck or 0, info.discard or 0), nil, -520)
        local left = math.max(0, math.ceil(self.scroll - (VISIBLE + 1) / 2 - 0.05))
        local right = math.max(0, math.ceil(n - self.scroll - (VISIBLE - 1) / 2 - 0.05))
        Widgets.ChildText(entity, "LeftMore", left > 0 and ("< " .. left) or "")
        Widgets.ChildText(entity, "RightMore", right > 0 and (right .. " >") or "")
    end
end

return Hand
