---@type SceneScript
---@class ExploreScene
local ExploreScene = {}

local SELECTION_RADIUS = 32.0

function ExploreScene:Initialize()
    self.selected    = {}   -- entity -> true
    self.moveTargets = {}   -- entity -> { x, y }
    Log("ExploreScene initialized")
end

function ExploreScene:Update(dt)
    -- Steer selected units toward their move targets
    for entity, target in pairs(self.moveTargets) do
        local pos = GetComponent(entity, "Position")
        local kin = GetComponent(entity, "Kinematics")
        if pos and kin then
            local dx = target.x - pos.x
            local dy = target.y - pos.y
            local dist = math.sqrt(dx * dx + dy * dy)
            if dist > 5.0 then
                local speed = kin.maxSpeed or 150.0
                kin.velocity.x = (dx / dist) * speed
                kin.velocity.y = (dy / dist) * speed
                SetComponent(entity, "Kinematics", kin)
            else
                -- Arrived
                kin.velocity.x = 0
                kin.velocity.y = 0
                SetComponent(entity, "Kinematics", kin)
                self.moveTargets[entity] = nil
            end
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
    -- Deselect all current selections
    for entity, _ in pairs(self.selected) do
        if HasComponent(entity, "Selection") then
            RemoveComponent(entity, "Selection")
        end
    end
    self.selected = {}

    -- Find nearest unit and select if within radius
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
        self.moveTargets[entity] = { x = wx, y = wy }
        count = count + 1
    end
    if count > 0 then
        Log("Move order: " .. count .. " unit(s) -> (" .. math.floor(wx) .. ", " .. math.floor(wy) .. ")")
    end
end

return ExploreScene
