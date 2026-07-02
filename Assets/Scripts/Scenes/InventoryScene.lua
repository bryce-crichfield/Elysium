---@type SceneScript
---@class InventoryScene
local InventoryScene = {}

-- ─────────────────────────────────────────────────────────────────────────────
-- Panel constants  (must match RectangleComponent sizes in InventoryScene.xml)
-- ─────────────────────────────────────────────────────────────────────────────
local PANEL_W    = 290
local HEADER_H   = 26   -- height of INV_HEADER / CHAR_HEADER

-- Inventory grid config
local GRID_COLS  = 5
local GRID_ROWS  = 5
local SLOT_SIZE  = 48
local SLOT_GAP   = 4

-- Centre x inside the right panel (relative, used for paper-doll layout)
local CHAR_CX    = math.floor(PANEL_W / 2)   -- 145

-- Equipment slots defined as (dx, dy) offsets from the panel's top-left corner.
-- Positions are relative so they stay correct regardless of where UiSystem places the panel.
local EQUIP_SLOTS = {
    --           dx    dy    w    h
    { dx = 120, dy =  38, w = 50, h = 50 },  -- Head
    { dx = 117, dy =  93, w = 56, h = 66 },  -- Body
    { dx =  66, dy =  93, w = 46, h = 56 },  -- L. Hand
    { dx = 178, dy =  93, w = 46, h = 56 },  -- R. Hand
    { dx = 117, dy = 164, w = 56, h = 56 },  -- Legs
    { dx = 117, dy = 225, w = 56, h = 40 },  -- Boots
    { dx = 244, dy =  38, w = 34, h = 34 },  -- Ring
    { dx = 244, dy =  77, w = 34, h = 34 },  -- Neck
}

-- ─────────────────────────────────────────────────────────────────────────────
-- Colours
-- ─────────────────────────────────────────────────────────────────────────────
local C_SLOT_FILL    = {r = 40,  g = 40,  b = 46,  a = 255}
local C_SLOT_BORDER  = {r = 80,  g = 80,  b = 90,  a = 200}
local C_EQUIP_FILL   = {r = 28,  g = 34,  b = 52,  a = 255}
local C_EQUIP_BORDER = {r = 70,  g = 100, b = 170, a = 220}

-- ─────────────────────────────────────────────────────────────────────────────
-- Helpers
-- ─────────────────────────────────────────────────────────────────────────────
local function drawFilledRect(x, y, w, h, fill, border, layer)
    FillRect(x, y, w, h, fill, layer)
    DrawLine(x,     y,     x + w, y,     border, layer)
    DrawLine(x + w, y,     x + w, y + h, border, layer)
    DrawLine(x + w, y + h, x,     y + h, border, layer)
    DrawLine(x,     y + h, x,     y,     border, layer)
end

-- ─────────────────────────────────────────────────────────────────────────────
-- Scene lifecycle
-- ─────────────────────────────────────────────────────────────────────────────
function InventoryScene:Initialize()
    -- Cache entity handles so Render() can read their UiSystem-resolved positions
    self.invPanel  = GetEntityByName("INV_PANEL")
    self.charPanel = GetEntityByName("CHAR_PANEL")
    Log("InventoryScene opened")
end

function InventoryScene:Update(dt)
end

function InventoryScene:Render()
    local layer = "ui_fg"

    -- ── Inventory grid ────────────────────────────────────────────────────────
    local invPos = self.invPanel ~= 0 and GetComponent(self.invPanel, "Transform") or nil
    if invPos then
        local gridW   = GRID_COLS * SLOT_SIZE + (GRID_COLS - 1) * SLOT_GAP   -- 256
        local startX  = invPos.worldX + math.floor((PANEL_W - gridW) / 2)    -- centred
        local startY  = invPos.worldY + HEADER_H + 8                          -- below header

        for row = 0, GRID_ROWS - 1 do
            for col = 0, GRID_COLS - 1 do
                local sx = startX + col * (SLOT_SIZE + SLOT_GAP)
                local sy = startY + row * (SLOT_SIZE + SLOT_GAP)
                drawFilledRect(sx, sy, SLOT_SIZE, SLOT_SIZE, C_SLOT_FILL, C_SLOT_BORDER, layer)
            end
        end
    end

    -- ── Character sheet equipment slots ──────────────────────────────────────
    local charPos = self.charPanel ~= 0 and GetComponent(self.charPanel, "Transform") or nil
    if charPos then
        for _, slot in ipairs(EQUIP_SLOTS) do
            drawFilledRect(
                charPos.worldX + slot.dx,
                charPos.worldY + slot.dy,
                slot.w, slot.h,
                C_EQUIP_FILL, C_EQUIP_BORDER, layer)
        end
    end
end

function InventoryScene:OnEvent(event)
    if event.type == "KeyPressed" then
        if event.key == KEY_I or event.key == KEY_ESCAPE then
            ScenePop()
            return true  -- consume the event; prevents ExploreScene from seeing KEY_I
        end
    end
end

return InventoryScene
