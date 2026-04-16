---@type SceneScript
---@class ExploreScene
local ExploreScene = {}

-- Portrait bar ---------------------------------------------------------------
-- Portrait bar ---------------------------------------------------------------
local MAX_PORTRAITS = 5

local UNIT_PORTRAITS = {
    Archer = "Textures/Portraits/Archer128.jpg",
    Knight = "Textures/Portraits/Knight128.jpg",
    Bishop = "Textures/Portraits/Bishop128.jpg",
    Militia = "Textures/Portraits/Militia128.jpg",
    Worker  = "Textures/Portraits/Worker128.jpg",
}

local function getPortraitTexture(entity)
    local nameComp = GetComponent(entity, "Name")
    if not nameComp then return "" end
    for unitType, tex in pairs(UNIT_PORTRAITS) do
        if nameComp.name:find(unitType) then return tex end
    end
    return ""
end

local function refreshPortraits(selected)
    local units = {}
    for entity in pairs(selected) do
        table.insert(units, entity)
    end
    for i = 1, MAX_PORTRAITS do
        local slot = PortraitSlotRegistry and PortraitSlotRegistry[i]
        if slot then
            local unit = units[i]
            if unit then
                slot:Show(getPortraitTexture(unit))
            else
                slot:Hide()
            end
        end
    end
end
------------------------------------------------------------------------------
------------------------------------------------------------------------------

local SELECTION_RADIUS = 32.0
local SELECTION_Y_OFFSET = 0  -- pos is feet (bottom-anchor sprites), ring goes at pos
local DRAG_THRESHOLD   = 6    -- world-px; under this = click, over = drag

-- Isometric tile dimensions (must match ExploreScene.xml Tilemap attributes)
local TILE_W, TILE_H = 64, 32
local HALF_W, HALF_H = TILE_W / 2, TILE_H / 2

-- Ordered tile offsets for spreading units around a target.
-- Ring 0: the exact tile. Rings expand outward so units fan out naturally.
local SPREAD_OFFSETS = {
    { 0,  0},
    { 1,  0}, { 0,  1}, {-1,  0}, { 0, -1},
    { 1,  1}, {-1,  1}, { 1, -1}, {-1, -1},
    { 2,  0}, { 0,  2}, {-2,  0}, { 0, -2},
    { 2,  1}, { 1,  2}, {-1,  2}, {-2,  1},
    {-2, -1}, {-1, -2}, { 1, -2}, { 2, -1},
}

-- Convert world position to tile grid coordinates.
-- Uses round (floor+0.5) so points in the upper/left half of a diamond snap correctly.
local function worldToTile(wx, wy)
    local a = wx / HALF_W
    local b = wy / HALF_H
    return math.floor((a + b) / 2 + 0.5), math.floor((b - a) / 2 + 0.5)
end
-- Convert tile grid coordinates to world center position
local function tileToWorld(tx, ty)
    return (tx - ty) * HALF_W, (tx + ty) * HALF_H
end

-- Draw an isometric diamond outline centred at (cx, cy)
local function drawTileOutline(cx, cy, width, height, color, layer)
    local hw, hh = width, height
    DrawLine(cx,      cy - hh, cx + hw, cy,      color, layer)
    DrawLine(cx + hw, cy,      cx,      cy + hh, color, layer)
    DrawLine(cx,      cy + hh, cx - hw, cy,      color, layer)
    DrawLine(cx - hw, cy,      cx,      cy - hh, color, layer)
end

function ExploreScene:Initialize()
    self.selected   = {}     -- entity -> true
    self.mouseWX    = 0
    self.mouseWY    = 0
    self.dragStart  = nil    -- {x, y} in world coords when LMB goes down
    self.dragEnd    = nil    -- current mouse world pos while LMB held
    self.isDragging = false  -- true once pointer moved past DRAG_THRESHOLD
    self.debugDraw  = false  -- toggle with D key

    -- Cache portrait slot entity handles
    self.portraits = {}
    for i = 0, MAX_PORTRAITS - 1 do
        local e = GetEntityByName("PORTRAIT_" .. i)
        self.portraits[i + 1] = (e ~= 0) and e or nil
    end

    -- Cache camera entity for WASD pan
    local cam = GetEntityByName("CAMERA")
    self.cameraEntity = (cam ~= 0) and cam or nil

    Log("ExploreScene initialized")
end

local CAMERA_SPEED = 400  -- world units per second

function ExploreScene:Update(dt)
    self.time = (self.time or 0) + dt
    refreshPortraits(self.selected)

    -- WASD camera pan
    local dx, dy = 0, 0
    if IsKeyDown(KEY_W) then dy = dy - 1 end
    if IsKeyDown(KEY_S) then dy = dy + 1 end
    if IsKeyDown(KEY_A) then dx = dx - 1 end
    if IsKeyDown(KEY_D) then dx = dx + 1 end

    if (dx ~= 0 or dy ~= 0) and self.cameraEntity then
        -- Normalize diagonal so speed is consistent in all directions
        if dx ~= 0 and dy ~= 0 then
            local inv = 1 / math.sqrt(2)
            dx = dx * inv
            dy = dy * inv
        end
        local pos = GetComponent(self.cameraEntity, "Position")
        if pos then
            pos.x = pos.x + dx * CAMERA_SPEED * dt
            pos.y = pos.y + dy * CAMERA_SPEED * dt
        end
    end
end

function ExploreScene:Render()
    -- Selection ring at the unit's feet
    for entity, _ in pairs(self.selected) do
        local pos = GetComponent(entity, "Position")
        if pos then
            -- Smooth sine pulse: ~1.5s cycle, subtle 10% size variation
            local pulse = 1.0 + 0.1 * math.sin(self.time * (2 * math.pi / 1.5))
            local width  = HALF_W * pulse
            local height = width * 0.5
            DrawEllipse(pos.x, pos.y + SELECTION_Y_OFFSET, width, height,
                        {r = 0, g = 255, b = 0, a = 255}, "selection")
        end
    end

    -- Active path highlights: filled orange diamonds, only for selected units
    local pathColor = {r = 255, g = 140, b = 0, a = 180}
    for entity, _ in pairs(self.selected) do
        local mv = GetComponent(entity, "Movement")
        if mv then
            local count = mv:GetWaypointCount()
            for i = 0, count - 1 do
                local wp  = mv:GetWaypoint(i)
                local wtx, wty = worldToTile(wp.x, wp.y)
                local wcx, wcy = tileToWorld(wtx, wty)
                DrawPolygon({
                    {x = wcx,          y = wcy - HALF_H},
                    {x = wcx + HALF_W, y = wcy},
                    {x = wcx,          y = wcy + HALF_H},
                    {x = wcx - HALF_W, y = wcy},
                }, pathColor, "selection")
            end
        end
    end

    -- Hovered tile outline (suppressed while drag-selecting)
    if not self.isDragging then
        local screenMp = GetMousePosition()
        local worldMp = ScreenToWorld(screenMp)
        local tx, ty = worldToTile(worldMp.x, worldMp.y)
        local cx, cy = tileToWorld(tx, ty)
        local color = {r = 255, g = 0, b = 0, a = 160}
        local t = 0.5 + 0.5 * math.sin(self.time * (2 * math.pi / 1.5))
        -- pulse the width and height
        local width, height = HALF_W, HALF_H
        width  = width  * (1.0 + 0.3 * t)
        height = height * (1.0 + 0.3 * t)
        drawTileOutline(cx, cy,width,height,color, "selection")

        -- Set the cursor entity's position to the hovered tile center
        local cursor = GetEntityByName("CURSOR")
        if cursor and cursor ~= 0 then
            local pos = GetComponent(cursor, "Position")
            if pos then
                pos.x = cx
                pos.y = cy
            end
        end
    end

    -- Collider debug outlines (toggle with D)
    if self.debugDraw then
        local colColor = {r = 0, g = 255, b = 200, a = 180}
        for _, entity in ipairs(GetEntities()) do
            local pos = GetComponent(entity, "Position")
            local col = GetComponent(entity, "Collider")
            if pos and col then
                local rx = pos.x + col.offsetX - col.width  * 0.5
                local ry = pos.y + col.offsetY - col.height * 0.5
                local rw = col.width
                local rh = col.height
                DrawLine(rx,      ry,      rx + rw, ry,      colColor, "selection")
                DrawLine(rx + rw, ry,      rx + rw, ry + rh, colColor, "selection")
                DrawLine(rx + rw, ry + rh, rx,      ry + rh, colColor, "selection")
                DrawLine(rx,      ry + rh, rx,      ry,      colColor, "selection")
            end
        end
    end

    -- Drag-select box (drawn last so it sits on top)
    if self.isDragging and self.dragStart and self.dragEnd then
        local x1, y1 = self.dragStart.x, self.dragStart.y
        local x2, y2 = self.dragEnd.x,   self.dragEnd.y
        local bx = math.min(x1, x2)
        local by = math.min(y1, y2)
        local bw = math.abs(x2 - x1)
        local bh = math.abs(y2 - y1)
        FillRect(bx, by, bw, bh, {r = 100, g = 180, b = 255, a = 40},  "selection")
        DrawLine(x1, y1, x2, y1, {r = 100, g = 180, b = 255, a = 220}, "selection")
        DrawLine(x2, y1, x2, y2, {r = 100, g = 180, b = 255, a = 220}, "selection")
        DrawLine(x2, y2, x1, y2, {r = 100, g = 180, b = 255, a = 220}, "selection")
        DrawLine(x1, y2, x1, y1, {r = 100, g = 180, b = 255, a = 220}, "selection")
    end
end

function ExploreScene:OnEvent(event)
    if event.type == "MouseMoved" then
        self.mouseWX = event.wx
        self.mouseWY = event.wy
        -- Update drag end and check if we've crossed the drag threshold
        if self.dragStart then
            self.dragEnd = {x = event.wx, y = event.wy}
            if not self.isDragging then
                local dx = event.wx - self.dragStart.x
                local dy = event.wy - self.dragStart.y
                if math.abs(dx) > DRAG_THRESHOLD or math.abs(dy) > DRAG_THRESHOLD then
                    self.isDragging = true
                end
            end
        end

    elseif event.type == "KeyPressed" then
        if event.key == KEY_ESCAPE then
            ScenePush("OptionsScene")
            return true
        elseif event.key == KEY_I then
            -- Pass the first selected unit into the inventory screen (nil = no unit)
            local firstSelected = nil
            for entity, _ in pairs(self.selected) do
                firstSelected = entity
                break
            end
            ScenePush("InventoryScene", { characterId = firstSelected })
            return true
        elseif event.key == KEY_1 then
            self.debugDraw = not self.debugDraw
            Log("Collider debug draw: " .. tostring(self.debugDraw))
        end

    elseif event.type == "MouseButtonPressed" then
        if event.button == MOUSE_LEFT then
            -- Start potential drag; don't commit selection until release
            self.dragStart  = {x = event.wx, y = event.wy}
            self.dragEnd    = {x = event.wx, y = event.wy}
            self.isDragging = false
        elseif event.button == MOUSE_RIGHT then
            self:HandleMoveOrder(event.wx, event.wy)
        end

    elseif event.type == "MouseButtonReleased" then
        if event.button == MOUSE_LEFT then
            if self.isDragging then
                self:HandleDragSelection()
            else
                self:HandleClickSelection(event.wx, event.wy)
            end
            self.dragStart  = nil
            self.dragEnd    = nil
            self.isDragging = false
        end
    end
end

-- Clear current selection and apply a new one
local function clearSelection(selected)
    for entity, _ in pairs(selected) do
        if HasComponent(entity, "Selection") then
            RemoveComponent(entity, "Selection")
        end
    end
    return {}
end

-- Point-click selection: select the nearest eligible unit within SELECTION_RADIUS
function ExploreScene:HandleClickSelection(wx, wy)
    self.selected = clearSelection(self.selected)
    local nearest = FindNearestEntity(wx, wy, "Kinematics")
    if nearest ~= 0 then
        local pos = GetComponent(nearest, "Position")
        if pos and Distance(wx, wy, pos.x, pos.y) <= SELECTION_RADIUS then
            self.selected[nearest] = true
            AddComponent(nearest, "Selection")
            Log("Selected entity " .. nearest)
        end
    end
end

-- Box selection: select all units whose position falls inside the drag rectangle
function ExploreScene:HandleDragSelection()
    self.selected = clearSelection(self.selected)
    local x1 = math.min(self.dragStart.x, self.dragEnd.x)
    local x2 = math.max(self.dragStart.x, self.dragEnd.x)
    local y1 = math.min(self.dragStart.y, self.dragEnd.y)
    local y2 = math.max(self.dragStart.y, self.dragEnd.y)
    local count = 0
    for _, entity in ipairs(GetEntities()) do
        if HasComponent(entity, "Kinematics") then
            local pos = GetComponent(entity, "Position")
            if pos and pos.x >= x1 and pos.x <= x2 and pos.y >= y1 and pos.y <= y2 then
                self.selected[entity] = true
                AddComponent(entity, "Selection")
                count = count + 1
            end
        end
    end
    Log("Drag-selected " .. count .. " unit(s)")
end

-- Issue move commands, spreading units across ring-ordered tiles so they don't stack
function ExploreScene:HandleMoveOrder(wx, wy)
    local baseTX, baseTY = worldToTile(wx, wy)
    local i = 1
    local count = 0
    for entity, _ in pairs(self.selected) do
        local off = SPREAD_OFFSETS[i] or {i - 1, 0}  -- linear fallback for large groups
        local ttx = baseTX + off[1]
        local tty = baseTY + off[2]
        local twx, twy = tileToWorld(ttx, tty)
        IssueMoveCommand(entity, twx, twy)
        i = i + 1
        count = count + 1
    end
    if count > 0 then
        Log("Move order: " .. count .. " unit(s) -> tile (" .. baseTX .. ", " .. baseTY .. ")")
    end
end

return ExploreScene
