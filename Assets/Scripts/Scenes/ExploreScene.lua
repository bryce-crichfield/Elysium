---@type SceneScript
---@class ExploreScene
local ExploreScene = {}

local SELECTION_RADIUS = 32.0

function ExploreScene:Initialize()
    self.selected = {}   -- entity -> true
    Log("ExploreScene initialized")
end

function ExploreScene:Update(dt)
end

function ExploreScene:Render()
    for entity, _ in pairs(self.selected) do
        local pos = GetComponent(entity, "Position")
        if pos then
            -- draw isometric oval around the entity tileWidth = 64, tileHeight = 32, draw with ellipse radius of 32x16
            local centerX = pos.x
            local centerY = pos.y
            local radiusX = 32
            local radiusY = 16

            DrawEllipse(centerX, centerY, radiusX, radiusY, {r= 255, g = 255, b = 0, a = 255}, "entity")
        end
    end
end

function ExploreScene:OnEvent(event)
    if event.type == "MouseButtonPressed" then
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
