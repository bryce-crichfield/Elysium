---@type EntityScript
---@class Card
-- Prefabs/Card.xml: shows one card, as its placement's binding says (Widgets.Bind(id, view)):
--   card      the card (Scripts/Battler/Cards.lua), or nil for an empty slot
--   faceUp    the face, or the back (an enemy's hand)
--   hover     the pointer is on it: it glows and plays the hover sound
--   chosen    it's being played (waiting on a target): it burns
--   playable  its owner can pay for it now; a card that can't be paid for is dimmed
--   hidden    not shown at all
--   top       drawn over the other cards (the "uiTop" layer)
-- Where it sits and how big is whoever binds it's business (Transform); this only draws.
local Widgets = require("Scripts/Components/Widgets")
local Sfx = require("Scripts/Menu/Sfx")

local Card = {}

local WHITE = {r = 255, g = 255, b = 255, a = 255}
local DIM = {r = 120, g = 120, b = 130, a = 255}
local TITLE = {r = 255, g = 225, b = 160, a = 255}
local BODY = {r = 225, g = 225, b = 232, a = 255}
local FACE = { "Title", "Art", "Body", "BlueCost" }
local BACK = { "Back", "Emblem" }

local function SetLayerName(entity, name)
    local layer = GetComponent(entity, "Layer")
    if layer and layer.name ~= name then layer.name = name end
    for _, child in ipairs(GetChildren(entity)) do SetLayerName(child, name) end
end

local function Show(entity, on)
    local layer = entity and GetComponent(entity, "Layer")
    if layer then layer.isVisible = on end
end

function Card:Initialize(entity)
    self.id = Widgets.PlacementId(entity)
    self.parts = {}
    for _, name in ipairs(FACE) do self.parts[name] = Widgets.Child(entity, name) end
    for _, name in ipairs(BACK) do self.parts[name] = Widgets.Child(entity, name) end
    self.frame = Widgets.Child(entity, "Frame")  -- card_frame.png, over the art; carries the glow
    self.art = nil                               -- the texture the Art layer shows
    self.glow, self.hover, self.time = 0, false, 0
end

function Card:Update(entity, dt)
    self.time = self.time + dt
    local view = self.id and Widgets.Binding(self.id)
    if not view or view.hidden or not view.card then
        Widgets.SetVisible(entity, false)
        self.hover = false
        return
    end

    SetLayerName(entity, view.top and "uiTop" or "ui")
    Show(entity, true)
    Show(self.frame, true)
    for _, name in ipairs(FACE) do Show(self.parts[name], view.faceUp) end
    for _, name in ipairs(BACK) do Show(self.parts[name], not view.faceUp) end

    if view.hover and not self.hover then Sfx.Play(Sfx.HOVER, 0.4) end
    self.hover = view.hover

    -- Glow: a steady burn while it's being played, a lift on hover.
    local target = 0
    if view.chosen then target = 1.6 + 0.3 * math.sin(self.time * 6)
    elseif view.hover then target = view.faceUp and view.playable and 1.2 or 0.6 end
    self.glow = Widgets.Ease(self.glow, target, 12, dt)
    Widgets.SetGlow(self.frame, self.glow)
    local mat = self.frame and GetComponent(self.frame, "Material")
    local stroke = mat and mat:Layer("Stroke")
    if stroke then stroke:Set("uColor", (view.hover or view.chosen) and Widgets.HOVER or Widgets.EDGE) end

    if not view.faceUp then return end
    local card = view.card
    local usable = view.playable ~= false
    Widgets.ChildText(entity, "Title", card.name, usable and TITLE or DIM)
    Widgets.ChildText(entity, "Body", card.text or "", usable and BODY or DIM)
    self:SetArt(card.art, usable)
    local cost = card.cost or 0
    Widgets.ChildText(entity, "BlueCost", tostring(cost), cost > 0 and WHITE or DIM)
end

-- The Art child's picture (a texture path, or nil for none), dimmed when the card can't be paid for.
function Card:SetArt(path, usable)
    local mat = self.parts.Art and GetComponent(self.parts.Art, "Material")
    local tex = mat and mat:Layer("Texture")
    if not tex then return end
    if path ~= self.art then
        self.art = path
        if path then LoadTexture(path) end
        tex.texture = path or ""
    end
    tex:Set("uTint", usable and WHITE or DIM)
end

return Card
