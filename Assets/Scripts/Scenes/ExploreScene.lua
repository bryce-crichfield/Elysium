---@type SceneScript
---@class ExploreScene
local ExploreScene = {}

local SELECTION_RADIUS = 32.0
-- How far below the sprite center the isometric ground ring sits.
-- Sprites are 128px tall and centered on pos, so feet are ~+48 below center.
local SELECTION_Y_OFFSET = 0  -- pos is feet (bottom-anchor sprites), ring goes at pos

-- Isometric tile dimensions (must match ExploreScene.xml Tilemap attributes)
local TILE_W, TILE_H   = 64, 32
local HALF_W, HALF_H   = TILE_W / 2, TILE_H / 2

-- Convert world position to tile grid coordinates (matches SceneLoader tile placement).
-- Uses round (floor + 0.5) rather than floor: isometric tile containment is determined
-- by which tile center is nearest in tile-coordinate space, not by floor-partitioning.
-- floor() fails for points in the upper/left half of any diamond.
local function worldToTile(wx, wy)
    local a = wx / HALF_W
    local b = wy / HALF_H
    return math.floor((a + b) / 2 + 0.5), math.floor((b - a) / 2 + 0.5)
end

-- Convert tile grid coordinates back to world center position
local function tileToWorld(tx, ty)
    return (tx - ty) * HALF_W, (tx + ty) * HALF_H
end

-- Draw an isometric diamond outline centred at (cx, cy)
local function drawTileOutline(cx, cy, color, layer)
    local hw, hh = HALF_W, HALF_H
    DrawLine(cx,      cy - hh, cx + hw, cy,      color, layer)
    DrawLine(cx + hw, cy,      cx,      cy + hh, color, layer)
    DrawLine(cx,      cy + hh, cx - hw, cy,      color, layer)
    DrawLine(cx - hw, cy,      cx,      cy - hh, color, layer)
end

function ExploreScene:Initialize()
    self.selected  = {}   -- entity -> true
    self.mouseWX   = 0
    self.mouseWY   = 0
    Log("ExploreScene initialized")
end

function ExploreScene:Update(dt)
end

function ExploreScene:Render()
    -- Selection ring at the unit's feet (ground level)
    for entity, _ in pairs(self.selected) do
        local pos = GetComponent(entity, "Position")
        if pos then
            DrawEllipse(pos.x, pos.y + SELECTION_Y_OFFSET, HALF_W, HALF_H,
                        {r = 255, g = 255, b = 0, a = 255}, "selection")
        end
    end

    -- Active path highlights (filled orange diamonds for each waypoint tile)
    local pathColor = {r = 255, g = 140, b = 0, a = 255}
    for _, entity in ipairs(GetEntities()) do
        local mv = GetComponent(entity, "Movement")
        if mv then
            local count = mv:GetWaypointCount()
            for i = 0, count - 1 do
                local wp = mv:GetWaypoint(i)
                local wtx, wty = worldToTile(wp.x, wp.y)
                local wcx, wcy = tileToWorld(wtx, wty)
                Log("Drawing path waypoint at tile (" .. wtx .. ", " .. wty .. ") -> world (" .. wcx .. ", " .. wcy .. ")")
                DrawPolygon({
                    {x = wcx,          y = wcy - HALF_H},
                    {x = wcx + HALF_W, y = wcy},
                    {x = wcx,          y = wcy + HALF_H},
                    {x = wcx - HALF_W, y = wcy},
                }, pathColor, "selection")
            end
        end
    end

    -- Hovered tile outline
    local tx, ty   = worldToTile(self.mouseWX, self.mouseWY)
    local cx, cy   = tileToWorld(tx, ty)
    drawTileOutline(cx, cy, {r = 255, g = 0, b = 0, a = 160}, "selection")
end

function ExploreScene:OnEvent(event)
    if event.type == "MouseMoved" then
        self.mouseWX = event.wx
        self.mouseWY = event.wy
    elseif event.type == "MouseButtonPressed" then
        if event.button == MOUSE_LEFT then
            self:HandleSelection(event.wx, event.wy)
        elseif event.button == MOUSE_RIGHT then
            self:HandleMoveOrder(event.wx, event.wy)
        end
    end
end

function ExploreScene:HandleSelection(wx, wy)
    for entity, _ in pairs(self.selected) do
        if HasComponent(entity, "Selection") then
            RemoveComponent(entity, "Selection")
        end
    end
    self.selected = {}

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

function ExploreScene:HandleMoveOrder(wx, wy)
    local count = 0
    for entity, _ in pairs(self.selected) do
        IssueMoveCommand(entity, wx, wy)
        count = count + 1
    end
    if count > 0 then
        Log("Move order: " .. count .. " unit(s) -> (" .. math.floor(wx) .. ", " .. math.floor(wy) .. ")")
    end
end

return ExploreScene
