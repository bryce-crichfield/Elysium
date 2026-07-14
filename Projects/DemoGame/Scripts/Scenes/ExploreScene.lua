---@type SceneScript
---@class ExploreScene
local ExploreScene = {}

local TileMap    = require("Scripts/Elysium/TileMap")
local Selection  = require("Scripts/Elysium/Selection")
local PortraitBar = require("Scripts/Elysium/PortraitBar")

local CAMERA_SPEED = 400

function ExploreScene:Initialize()
    self.time      = 0
    self.debugDraw = false

    self.portraitBar = PortraitBar.new()
    self.selection   = Selection.new(function(e) return HasComponent(e, "Kinematics") end)

    local cam = GetEntityByName("CAMERA")
    self.cameraEntity = (cam ~= 0) and cam or nil

    Log("ExploreScene initialized")
end

function ExploreScene:Update(dt)
    self.time = self.time + dt
    self.portraitBar:Refresh(self.selection.selected)

    local dx, dy = 0, 0
    if IsKeyDown(KEY_W) then dy = dy - 1 end
    if IsKeyDown(KEY_S) then dy = dy + 1 end
    if IsKeyDown(KEY_A) then dx = dx - 1 end
    if IsKeyDown(KEY_D) then dx = dx + 1 end

    if (dx ~= 0 or dy ~= 0) and self.cameraEntity then
        if dx ~= 0 and dy ~= 0 then
            local inv = 1 / math.sqrt(2)
            dx, dy = dx * inv, dy * inv
        end
        local pos = GetComponent(self.cameraEntity, "Transform")
        if pos then
            pos.localX = pos.localX + dx * CAMERA_SPEED * dt
            pos.localY = pos.localY + dy * CAMERA_SPEED * dt
        end
    end
end

function ExploreScene:Render()
    self.selection:Render(self.time)

    -- Path waypoint highlights for selected units
    local pathColor = {r=255, g=140, b=0, a=180}
    for entity in pairs(self.selection.selected) do
        local mv = GetComponent(entity, "Movement")
        if mv then
            for i = 0, mv:GetWaypointCount() - 1 do
                local wp = mv:GetWaypoint(i)
                local wtx, wty = TileMap.worldToTile(wp.x, wp.y)
                local wcx, wcy = TileMap.tileToWorld(wtx, wty)
                DrawPolygon({
                    {x = wcx,                  y = wcy - TileMap.HALF_H},
                    {x = wcx + TileMap.HALF_W, y = wcy},
                    {x = wcx,                  y = wcy + TileMap.HALF_H},
                    {x = wcx - TileMap.HALF_W, y = wcy},
                }, pathColor, "selection")
            end
        end
    end

    -- Hovered tile outline (suppressed while drag-selecting)
    if not self.selection.isDragging then
        local mp = ScreenToWorld(GetMousePosition())
        local tx, ty = TileMap.worldToTile(mp.x, mp.y)
        local cx, cy = TileMap.tileToWorld(tx, ty)
        local t = 0.5 + 0.5 * math.sin(self.time * (2 * math.pi / 1.5))
        TileMap.drawTileOutline(cx, cy,
            TileMap.HALF_W * (1.0 + 0.3 * t),
            TileMap.HALF_H * (1.0 + 0.3 * t),
            {r=255, g=0, b=0, a=160}, "selection")

        local cursor = GetEntityByName("CURSOR")
        if cursor and cursor ~= 0 then
            local pos = GetComponent(cursor, "Transform")
            if pos then pos.localX = cx; pos.localY = cy end
        end
    end

    -- Collider debug outlines
    if self.debugDraw then
        local colColor = {r=0, g=255, b=200, a=180}
        for _, entity in ipairs(GetEntities()) do
            local pos = GetComponent(entity, "Transform")
            local col = GetComponent(entity, "Collider")
            if pos and col then
                local rx = pos.worldX + col.offsetX - col.width  * 0.5
                local ry = pos.worldY + col.offsetY - col.height * 0.5
                DrawLine(rx,             ry,             rx + col.width, ry,              colColor, "selection")
                DrawLine(rx + col.width, ry,             rx + col.width, ry + col.height, colColor, "selection")
                DrawLine(rx + col.width, ry + col.height, rx,            ry + col.height, colColor, "selection")
                DrawLine(rx,             ry + col.height, rx,            ry,              colColor, "selection")
            end
        end
    end
end

function ExploreScene:OnEvent(event)
    self.selection:OnEvent(event)

    if event.type == "KeyPressed" then
        if event.key == KEY_ESCAPE then
            ScenePush("OptionsScene")
            return true
        elseif event.key == KEY_I then
            ScenePush("InventoryScene")
            return true
        elseif event.key == KEY_1 then
            self.debugDraw = not self.debugDraw
            Log("Collider debug draw: " .. tostring(self.debugDraw))
        end

    elseif event.type == "MouseButtonPressed" and event.button == MOUSE_RIGHT then
        self:_handleMoveOrder(event.wx, event.wy)
    end
end

function ExploreScene:_handleMoveOrder(wx, wy)
    local baseTX, baseTY = TileMap.worldToTile(wx, wy)
    local i, count = 1, 0
    for entity in pairs(self.selection.selected) do
        local off = TileMap.SPREAD_OFFSETS[i] or {i - 1, 0}
        local twx, twy = TileMap.tileToWorld(baseTX + off[1], baseTY + off[2])
        IssueMoveCommand(entity, twx, twy)
        i = i + 1
        count = count + 1
    end
    if count > 0 then
        Log("Move order: " .. count .. " unit(s) -> tile (" .. baseTX .. ", " .. baseTY .. ")")
    end
end

return ExploreScene
