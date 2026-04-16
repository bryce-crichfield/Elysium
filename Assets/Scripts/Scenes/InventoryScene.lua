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

-- Equipment slots with a `slot` name matching Character.inventory slot fields
local EQUIP_SLOTS = {
    --           dx    dy    w    h     slot
    { dx = 120, dy =  38, w = 50, h = 50, slot = "head"    },  -- Head
    { dx = 117, dy =  93, w = 56, h = 66, slot = "body"    },  -- Body
    { dx =  66, dy =  93, w = 46, h = 56, slot = "offhand" },  -- L. Hand
    { dx = 178, dy =  93, w = 46, h = 56, slot = "weapon"  },  -- R. Hand
    { dx = 117, dy = 164, w = 56, h = 56, slot = "legs"    },  -- Legs
    { dx = 117, dy = 225, w = 56, h = 40, slot = "boots"   },  -- Boots
    { dx = 244, dy =  38, w = 34, h = 34, slot = "ring"    },  -- Ring
    { dx = 244, dy =  77, w = 34, h = 34, slot = "neck"    },  -- Neck
    { dx =  28, dy =  38, w = 34, h = 34, slot = "ammo"    },  -- Ammo (top-left corner)
}

-- Slot names that belong to the paperdoll (all others → backpack grid)
local EQUIP_SLOT_NAMES = {
    head=true, body=true, offhand=true, weapon=true,
    legs=true, boots=true, ring=true, neck=true, ammo=true,
}

-- ─────────────────────────────────────────────────────────────────────────────
-- Colours
-- ─────────────────────────────────────────────────────────────────────────────
local C_SLOT_FILL     = {r = 40,  g = 40,  b = 46,  a = 255}
local C_SLOT_BORDER   = {r = 80,  g = 80,  b = 90,  a = 200}
local C_EQUIP_FILL    = {r = 28,  g = 34,  b = 52,  a = 255}
local C_EQUIP_BORDER  = {r = 70,  g = 100, b = 170, a = 220}
local C_EQUIP_FILLED  = {r = 40,  g = 60,  b = 100, a = 255}   -- slot has an item
local C_TEXT_BRIGHT   = {r = 220, g = 220, b = 230, a = 255}
local C_TEXT_DIM      = {r = 140, g = 140, b = 155, a = 200}
local C_STAT_LABEL    = {r = 110, g = 120, b = 140, a = 220}
local C_STAT_VALUE    = {r = 200, g = 210, b = 225, a = 255}
local C_HP            = {r = 200, g = 70,  b = 70,  a = 255}
local C_MP            = {r = 70,  g = 120, b = 210, a = 255}

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

-- Truncate a string to at most `maxLen` characters + "…" if needed
local function trunc(s, maxLen)
    if #s <= maxLen then return s end
    return s:sub(1, maxLen - 1) .. "\xE2\x80\xA6"  -- UTF-8 ellipsis
end

-- ─────────────────────────────────────────────────────────────────────────────
-- Scene lifecycle
-- ─────────────────────────────────────────────────────────────────────────────
function InventoryScene:Initialize(params)
    -- Log Params
    if params then
        local paramStrs = {}
        for k, v in pairs(params) do
            table.insert(paramStrs, tostring(k) .. "=" .. tostring(v))
        end
        Log("InventoryScene opened with params: " .. table.concat(paramStrs, ", "))
    else
        Log("InventoryScene opened with no params")
    end
    -- Cache panel entity handles
    self.invPanel  = GetEntityByName("INV_PANEL")
    self.charPanel = GetEntityByName("CHAR_PANEL")

    -- Character data
    self.character      = nil
    self.equippedBySlot = {}   -- slot name -> CharacterItem
    self.backpackItems  = {}   -- array of CharacterItem not in paperdoll slots

    local characterId = params and params.characterId
    if characterId then
        local charComp = HasComponent(characterId, "Character") and GetComponent(characterId, "Character") or nil
        if charComp and charComp.characterPath ~= "" then
            self.character = GetCharacter(charComp.characterPath)
            self.characterPath = charComp.characterPath
        end
    end

    if self.character then
        -- Partition inventory into paperdoll slots and backpack
        for _, item in ipairs(self.character.inventory) do
            if EQUIP_SLOT_NAMES[item.slot] then
                self.equippedBySlot[item.slot] = item
            else
                table.insert(self.backpackItems, item)
            end
        end

        -- Update entity text components to show the character's name
        local titleEnt = GetEntityByName("MODAL_TITLE")
        if titleEnt and titleEnt ~= 0 then
            local t = GetComponent(titleEnt, "Text")
            if t then t.text = self.character.name .. "  —  Inventory" end
        end
        local charHeaderEnt = GetEntityByName("CHAR_HEADER")
        if charHeaderEnt and charHeaderEnt ~= 0 then
            local t = GetComponent(charHeaderEnt, "Text")
            if t then t.text = self.character.name end
        end

        Log("InventoryScene: loaded character '" .. self.character.name .. "'")
    else
        Log("InventoryScene opened (no character selected)")
    end
end

function InventoryScene:Update(dt)
end

function InventoryScene:Render()
    local layer = "ui_fg"

    -- ── Backpack grid (left panel) ────────────────────────────────────────────
    local invPos = self.invPanel ~= 0 and GetComponent(self.invPanel, "Position") or nil
    if invPos then
        local gridW  = GRID_COLS * SLOT_SIZE + (GRID_COLS - 1) * SLOT_GAP   -- 256
        local startX = invPos.x + math.floor((PANEL_W - gridW) / 2)
        local startY = invPos.y + HEADER_H + 8

        local idx = 1
        for row = 0, GRID_ROWS - 1 do
            for col = 0, GRID_COLS - 1 do
                local sx = startX + col * (SLOT_SIZE + SLOT_GAP)
                local sy = startY + row * (SLOT_SIZE + SLOT_GAP)

                local item = self.backpackItems[idx]
                if item then
                    drawFilledRect(sx, sy, SLOT_SIZE, SLOT_SIZE, C_EQUIP_FILLED, C_EQUIP_BORDER, layer)
                    DrawText(trunc(item.id, 6),    sx + 3, sy + 4,  8, C_TEXT_BRIGHT, layer)
                    if item.quantity > 1 then
                        DrawText("x" .. item.quantity, sx + 3, sy + 34, 8, C_TEXT_DIM,   layer)
                    end
                    idx = idx + 1
                else
                    drawFilledRect(sx, sy, SLOT_SIZE, SLOT_SIZE, C_SLOT_FILL, C_SLOT_BORDER, layer)
                end
            end
        end
    end

    -- ── Character sheet (right panel) ─────────────────────────────────────────
    local charPos = self.charPanel ~= 0 and GetComponent(self.charPanel, "Position") or nil
    if charPos then
        -- Equipment slots / paperdoll
        for _, slotDef in ipairs(EQUIP_SLOTS) do
            local sx = charPos.x + slotDef.dx
            local sy = charPos.y + slotDef.dy
            local item = self.equippedBySlot[slotDef.slot]
            if item then
                drawFilledRect(sx, sy, slotDef.w, slotDef.h, C_EQUIP_FILLED, C_EQUIP_BORDER, layer)
                DrawText(trunc(item.id, 7), sx + 2, sy + 3, 8, C_TEXT_BRIGHT, layer)
            else
                drawFilledRect(sx, sy, slotDef.w, slotDef.h, C_EQUIP_FILL, C_EQUIP_BORDER, layer)
            end
        end

        -- Stats block below the paperdoll
        if self.character then
            local stats = self.character.stats
            local bx    = charPos.x + 8
            local by    = charPos.y + HEADER_H + 275   -- below boots

            -- Helper: draw "label   value / max" on one line
            local function statLine(label, statName, y, col)
                local s = stats[statName]
                if not s then return end
                local lx = bx + col * 138
                DrawText(label .. ":", lx,      y, 9, C_STAT_LABEL, layer)
                local valStr = tostring(s.value)
                if s.max ~= s.value then
                    valStr = valStr .. "/" .. tostring(s.max)
                end
                DrawText(valStr, lx + 42, y, 9, C_STAT_VALUE, layer)
            end

            -- Row 0: HP (red) and Level
            local s_hp    = stats["hp"]
            local s_level = stats["level"]
            if s_hp then
                DrawText("HP:", bx,       by,      9, C_HP,         layer)
                DrawText(s_hp.value .. "/" .. s_hp.max, bx + 28, by, 9, C_STAT_VALUE, layer)
            end
            if s_level then
                DrawText("Lv.", bx + 138, by,      9, C_STAT_LABEL, layer)
                DrawText(tostring(s_level.value), bx + 166, by, 9, C_STAT_VALUE, layer)
            end

            -- Row 1: MP (blue)
            local s_mp = stats["mp"]
            if s_mp then
                DrawText("MP:", bx,       by + 14, 9, C_MP,         layer)
                DrawText(s_mp.value .. "/" .. s_mp.max, bx + 28, by + 14, 9, C_STAT_VALUE, layer)
            end

            -- Rows 2–4: STR / DEX / INT
            statLine("STR", "str", by + 30, 0)
            statLine("DEX", "dex", by + 44, 0)
            statLine("INT", "int", by + 58, 0)
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
