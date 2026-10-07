---@type SceneScript
-- CharacterSheet: pushed over the Town with I. A tab per party member along the bottom
-- (Prefabs/PartyTabs.xml) picks whose sheet it is, and the mode button beside them swaps
-- between two modes:
--   character  Prefabs/CharacterSheet.xml: the inventory grid (empty for now), the model in
--              the middle panel, and its attributes
--   deck       Prefabs/DeckBuilder.xml: the deck (Run.Deck) as a list of rows, where hovering
--              a row pops up the whole card, and the collection as pages of large cards.
--              Dragging a row off the list takes the card out of the deck; dragging a card
--              from the collection onto the list puts it in (up to DECK_MAX).
-- I, Esc, or a click outside the sheet closes it. The Town says who to open on through the
-- "CharacterSheet" binding: { selected = index into Run.PARTY }.
local Cards = require("Scripts/Battler/Cards")
local Run = require("Scripts/Battler/Run")
local Units = require("Scripts/Battler/Units")
local Widgets = require("Scripts/Components/Widgets")
local Sfx = require("Scripts/Menu/Sfx")

local CharacterSheet = {}

local GRID_COLS, GRID_ROWS = 5, 7
local CELL, CELL_GAP = 56, 8
local GRID_TOP = 52              -- below the Inventory title
local TAB_W, TAB_GAP = 200, 12   -- Prefabs/CharacterTab.xml's width
local TAB_INSET = 8
local STATS = { "Strength", "Intellect", "Agility", "Stamina", "Health", "Mana" }
local LABEL_LEFT = 28

-- Deck mode.
local DECK_MAX = 8               -- as many rows as the list has room for
local ROW_H, ROW_GAP = 44, 6     -- Prefabs/DeckRow.xml's height
local ROW_TOP = 52               -- below the Deck title
local ROW_TITLE_LEFT = 70
local PAGE_COLS, PAGE_ROWS = 4, 2
local CARD_W, CARD_H = 140, 200  -- Prefabs/Card.xml's size
local PAGE_TOP, PAGE_ROW_GAP = 16, 16
local PREVIEW_SCALE = 1.4        -- the card popped up beside a hovered row
local PREVIEW_GAP = 12
local DRAG_SCALE = 0.8           -- the card held under the pointer while dragging
local ROW_FILL = {r = 13, g = 13, b = 20, a = 217}
local ROW_HOVER = {r = 46, g = 28, b = 5, a = 235}

function CharacterSheet:Initialize()
    self.sheet = Widgets.Find("Sheet")
    self.inventory = Widgets.Child(self.sheet, "InventoryPanel")
    self.modelPanel = Widgets.Child(self.sheet, "ModelPanel")
    self.stats = Widgets.Child(self.sheet, "StatsPanel")
    self.deck = Widgets.Find("Deck")
    self.deckPanel = Widgets.Child(self.deck, "DeckPanel")
    self.collection = Widgets.Child(self.deck, "CollectionPanel")
    self.tabBar = Widgets.Find("Tabs")
    self.model = GetEntityByName("Model")
    self.closing = false
    self.page = 1
    self.pages = math.max(1, math.ceil(#Cards.Collection / (PAGE_COLS * PAGE_ROWS)))
    for _, stat in ipairs(STATS) do Widgets.ChildText(self.stats, stat .. "Label", nil, nil, LABEL_LEFT) end

    -- Bound before the first Update anchors them: a bound button takes its anchor itself.
    Widgets.BindButton("ModeButton", function() self:SetMode(self.mode == "deck" and "character" or "deck") end)
    Widgets.BindButton("PrevPage", function() self.page = self.page - 1 end, function() return self.page > 1 end)
    Widgets.BindButton("NextPage", function() self.page = self.page + 1 end, function() return self.page < self.pages end)

    self.cells = {}
    for k = 1, GRID_COLS * GRID_ROWS do self.cells[k] = SpawnPrefab("Prefabs/ItemSlot.xml") end

    self.tabs = {}
    for k, name in ipairs(Run.PARTY) do
        local tab = SpawnPrefab("Prefabs/CharacterTab.xml")
        if tab then
            local c = Units.Classes[name]
            Widgets.ChildText(tab, "NameText", c.label)
            self:SetTexture(Widgets.Child(tab, "Portrait"), c.portrait)
            self.tabs[k] = tab
        end
    end

    self.rows = {}
    for k = 1, DECK_MAX do
        local row = SpawnPrefab("Prefabs/DeckRow.xml")
        if row then self.rows[#self.rows + 1] = { entity = row } end
    end

    self.cards = {}
    for k = 1, PAGE_COLS * PAGE_ROWS do self.cards[k] = self:SpawnCard() end
    self.preview = self:SpawnCard()
    if self.preview then self.preview.view.top = true end
    self.held = self:SpawnCard()
    if self.held then self.held.view.top = true end
    self.drag = nil   -- { card, index }: the card being dragged, and its row if it came off the list

    local binding = Widgets.Binding("CharacterSheet")
    self:Show(binding and binding.selected or 1)
    self:SetMode("character")
end

-- A Card.xml and the view it reads (Scripts/Components/Card.lua), hidden until it's laid out.
function CharacterSheet:SpawnCard()
    local e = SpawnPrefab("Prefabs/Card.xml")
    if not e then return nil end
    local slot = { entity = e, view = { hidden = true, faceUp = true, playable = true } }
    Widgets.BindView(Widgets.PlacementId(e), slot.view)
    return slot
end

function CharacterSheet:SetTexture(entity, path, tint)
    local mat = entity and GetComponent(entity, "Material")
    local tex = mat and mat:Layer("Texture")
    if not tex or not path then return end
    LoadTexture(path)
    tex.texture = path
    if tint then tex:Set("uTint", tint) end
end

-- The k-th party member's sheet: its model, name and attributes.
function CharacterSheet:Show(k)
    local name = Run.PARTY[k]
    local c = name and Units.Classes[name]
    if not c then return end
    self.selected = k

    local rig = Units.Rigs[c.rig]
    local model = self.model and GetComponent(self.model, "Model")
    if model and rig then
        model.model = rig.model
        model.scale = rig.scale
    end
    local anim = self.model and GetComponent(self.model, "Animation")
    if anim and rig then
        anim.clip = rig.clips.Idle
        anim.loop = true
        anim.speed = 1
        anim.playing = true
        anim.time = 0
    end

    Widgets.ChildText(self.modelPanel, "NameText", c.label)
    Widgets.ChildText(self.stats, "RoleText", c.role)
    self.drag = nil
    self.deckIds = Run.Deck(name)
    self:RefreshRows()
    local v = Units.Vitals(c)
    local values = { Strength = c.str, Intellect = c.int, Agility = c.agi, Stamina = c.stamina,
                     Health = v.hp, Mana = v.manaCap }
    for _, stat in ipairs(STATS) do Widgets.ChildText(self.stats, stat .. "Value", tostring(values[stat])) end
    for j, tab in pairs(self.tabs) do Widgets.SetGlow(tab, j == k and 1 or 0) end
end

-- The rows show the deck, one card each; the rest of the rows are hidden.
function CharacterSheet:RefreshRows()
    local c = Units.Classes[Run.PARTY[self.selected]]
    Widgets.ChildText(self.deckPanel, "DeckTitle", string.format("%s's deck  %d / %d", c.label, #self.deckIds, DECK_MAX))
    for k, row in ipairs(self.rows) do
        local card = Cards.Catalog[self.deckIds[k]]
        row.card = card
        if card then
            Widgets.ChildText(row.entity, "Title", card.name, nil, ROW_TITLE_LEFT)
            Widgets.ChildText(row.entity, "Cost", tostring(card.cost or 0))
            self:SetTexture(Widgets.Child(row.entity, "Art"), card.art, card.tint)
        end
        Widgets.SetVisible(row.entity, self.mode == "deck" and card ~= nil)
    end
end

function CharacterSheet:SetMode(mode)
    self.mode = mode
    local deck = mode == "deck"
    Widgets.SetVisible(self.sheet, not deck)
    Widgets.SetVisible(self.model, not deck)
    for _, cell in pairs(self.cells) do Widgets.SetVisible(cell, not deck) end
    Widgets.SetVisible(self.deck, deck)
    self.drag = nil
    self:RefreshRows()
    Widgets.SetVisible(Widgets.Find("PrevPage"), deck)
    Widgets.SetVisible(Widgets.Find("NextPage"), deck)
    Widgets.ChildText(Widgets.Find("ModeButton"), "Label", deck and "Character" or "Deck")
end

-- What's spawned on its own (the grid, tabs, rows and cards) follows its panel every frame.
function CharacterSheet:Update(dt)
    Widgets.Anchor(Widgets.Find("Backdrop"), "stretch", "stretch")
    Widgets.Anchor(self.sheet, "center", "middle")
    Widgets.Anchor(self.deck, "center", "middle")
    Widgets.Anchor(self.tabBar, "center", "middle")
    for _, id in ipairs({ "ModeButton", "PrevPage", "NextPage" }) do Widgets.Anchor(Widgets.Find(id), "center", "middle") end

    local ix, iy, iw = Widgets.Rect(self.inventory)
    local left = ix + (iw - (GRID_COLS * CELL + (GRID_COLS - 1) * CELL_GAP)) / 2
    for k, cell in pairs(self.cells) do
        local t = GetComponent(cell, "Transform")
        local col, row = (k - 1) % GRID_COLS, math.floor((k - 1) / GRID_COLS)
        t.localX, t.localY = left + col * (CELL + CELL_GAP), iy + GRID_TOP + row * (CELL + CELL_GAP)
    end

    local bx, by = Widgets.Rect(self.tabBar)
    for k, tab in pairs(self.tabs) do
        local t = GetComponent(tab, "Transform")
        t.localX, t.localY = bx + TAB_INSET + (k - 1) * (TAB_W + TAB_GAP), by + TAB_INSET
    end

    self:UpdateDeck()
end

-- The rows (and the card popped up beside the hovered one) and the collection's page.
function CharacterSheet:UpdateDeck()
    local deck = self.mode == "deck"
    local m = GetMousePosition()

    local dx, dy, dw = Widgets.Rect(self.deckPanel)
    local hovered
    for k, row in ipairs(self.rows) do
        local t = GetComponent(row.entity, "Transform")
        local r = GetComponent(row.entity, "Rectangle")
        t.localX, t.localY = dx + (dw - r.width) / 2, dy + ROW_TOP + (k - 1) * (ROW_H + ROW_GAP)
        local over = deck and row.card ~= nil and not self.drag and Widgets.Inside(m, t.localX, t.localY, r.width, r.height)
        if over then hovered = row end
        Widgets.SetFill(row.entity, over and ROW_HOVER or ROW_FILL)
    end

    local preview = self.preview
    if preview then
        preview.view.card = hovered and hovered.card
        preview.view.hidden = not hovered
        if hovered then
            local rt = GetComponent(hovered.entity, "Transform")
            local rr = GetComponent(hovered.entity, "Rectangle")
            local h = CARD_H * PREVIEW_SCALE
            local _, sh = GetScreenSize()
            local t = GetComponent(preview.entity, "Transform")
            t.localScaleX, t.localScaleY = PREVIEW_SCALE, PREVIEW_SCALE
            t.localX = rt.localX + rr.width + PREVIEW_GAP
            t.localY = math.max(8, math.min(sh - h - 8, rt.localY + rr.height / 2 - h / 2))
        end
    end

    local cx, cy, cw = Widgets.Rect(self.collection)
    local gap = (cw - 2 * PAGE_TOP - PAGE_COLS * CARD_W) / (PAGE_COLS - 1)
    local first = (self.page - 1) * PAGE_COLS * PAGE_ROWS
    for k, slot in pairs(self.cards) do
        local card = Cards.Collection[first + k]
        local col, row = (k - 1) % PAGE_COLS, math.floor((k - 1) / PAGE_COLS)
        local t = GetComponent(slot.entity, "Transform")
        t.localX = cx + PAGE_TOP + col * (CARD_W + gap)
        t.localY = cy + PAGE_TOP + row * (CARD_H + PAGE_ROW_GAP)
        slot.view.card = card
        slot.view.hidden = not deck or not card
        slot.view.hover = deck and card ~= nil and not hovered and not self.drag and Widgets.Inside(m, t.localX, t.localY, CARD_W, CARD_H)
    end

    local held = self.held
    if held then
        held.view.card = self.drag and self.drag.card
        held.view.hidden = not self.drag
        local t = GetComponent(held.entity, "Transform")
        t.localScaleX, t.localScaleY = DRAG_SCALE, DRAG_SCALE
        t.localX, t.localY = m.x - CARD_W * DRAG_SCALE / 2, m.y - CARD_H * DRAG_SCALE / 2
    end
    Widgets.ChildText(self.collection, "PageText", string.format("Page %d / %d", self.page, self.pages))
end

function CharacterSheet:Render() end

function CharacterSheet:Close()
    if self.closing then return end
    self.closing = true
    Sfx.Play(Sfx.CANCEL)
    ScenePop()
end

-- What's under the pointer to pick up: a row's card (and its index in the deck), or a card
-- in the collection.
function CharacterSheet:Pickup(m)
    for k, row in ipairs(self.rows) do
        if row.card and Widgets.Inside(m, Widgets.Rect(row.entity)) then return { card = row.card, index = k } end
    end
    for _, slot in pairs(self.cards) do
        if slot.view.card and Widgets.Inside(m, Widgets.Rect(slot.entity)) then return { card = slot.view.card } end
    end
    return nil
end

-- Dropped on the list, a collection card goes into the deck; dropped off it, a row's card
-- comes out.
function CharacterSheet:Drop(m)
    local drag = self.drag
    self.drag = nil
    local onList = Widgets.Inside(m, Widgets.Rect(self.deckPanel))
    if drag.index and not onList then
        table.remove(self.deckIds, drag.index)
        Sfx.Play(Sfx.CANCEL)
    elseif not drag.index and onList and #self.deckIds < DECK_MAX then
        self.deckIds[#self.deckIds + 1] = drag.card.id
        Sfx.Play(Sfx.SELECT)
    end
    self:RefreshRows()
end

-- Whether a screen point is on the sheet: its panels (the same in either mode) and the tab bar.
function CharacterSheet:OnSheet(m)
    local _, top = Widgets.Rect(self.inventory)
    local bx, by, bw, bh = Widgets.Rect(self.tabBar)
    return Widgets.Inside(m, bx, top, bw, by + bh - top)
end

-- Takes every event, so nothing reaches the Town underneath while the sheet is up.
function CharacterSheet:OnEvent(event)
    if event.type == "KeyPressed" and (event.key == KEY_I or event.key == KEY_ESCAPE) then
        self:Close()
    elseif event.type == "MouseButtonPressed" and event.button == MOUSE_LEFT then
        local m = GetMousePosition()
        if not self:OnSheet(m) then
            self:Close()
            return true
        end
        for k, tab in pairs(self.tabs) do
            if k ~= self.selected and Widgets.Inside(m, Widgets.Rect(tab)) then
                Sfx.Play(Sfx.SELECT)
                self:Show(k)
            end
        end
        if self.mode == "deck" then
            self.drag = self:Pickup(m)
            if self.drag then Sfx.PlayAny(Sfx.CARD, 0.4) end
        end
    elseif event.type == "MouseButtonReleased" and event.button == MOUSE_LEFT and self.drag then
        self:Drop(GetMousePosition())
    end
    return true
end

return CharacterSheet
