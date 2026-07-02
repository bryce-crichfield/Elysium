---@type SceneScript
---@class TestScene
local TestScene = {}

local TWO_PI = math.pi * 2

function TestScene:Initialize()
    for i = 1, 5 do
        self:CreateCircle()
    end

    self.timeSinceLastCircle = 0
end

function TestScene:Update(dt)
    self.timeSinceLastCircle = self.timeSinceLastCircle + dt
    if self.timeSinceLastCircle >= .05 then
        self:CreateCircle()
        self.timeSinceLastCircle = 0
    end

    for i, entity in ipairs(GetEntities()) do
        local pos = GetComponent(entity, "Transform")
        local kinematics = GetComponent(entity, "Kinematics")
        if pos.localX < -400 or pos.localX > 400 then
            kinematics.velocity.x = -kinematics.velocity.x
        end
        if pos.localY < -200 or pos.localY > 200 then
            kinematics.velocity.y = -kinematics.velocity.y
        end

        SetComponent(entity, "Kinematics", kinematics)
        SetComponent(entity, "Transform", pos)
    end
end

function TestScene:Render()
    local count = 0
    for _, entity in ipairs(GetEntities()) do
        local pos = GetComponent(entity, "Transform")
        if pos then
            DrawCircle(pos.worldX, pos.worldY, 20, {r = 255, g = 0, b = 0, a = 255}, "default")
        end
        count = count + 1
    end

    DrawText("Circles: " .. count, 0, 0, 16, {r = 255, g = 255, b = 255, a = 255}, "ui")
end

function TestScene:OnEvent(event)
    if event.type == "MouseButtonPressed" then
        self:CreateCircle()
    end
end

function TestScene:CreateCircle()
    local entity = CreateEntity()

    AddComponent(entity, "Transform")
    local pos = GetComponent(entity, "Transform")
    pos.localX = math.random(-300, 300)
    pos.localY = math.random(-200, 200)
    SetComponent(entity, "Transform", pos)

    AddComponent(entity, "Kinematics")
    local kinematics = GetComponent(entity, "Kinematics")
    local angle = math.random() * TWO_PI
    kinematics.velocity.x = math.cos(angle) * 150
    kinematics.velocity.y = math.sin(angle) * 150
    kinematics.friction = 0
    SetComponent(entity, "Kinematics", kinematics)
end

return TestScene
