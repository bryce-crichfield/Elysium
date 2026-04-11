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
        local pos = GetComponent(entity, "Position")
        local kinematics = GetComponent(entity, "Kinematics")
        if pos.x < -400 or pos.x > 400 then
            kinematics.velocity.x = -kinematics.velocity.x
        end
        if pos.y < -200 or pos.y > 200 then
            kinematics.velocity.y = -kinematics.velocity.y
        end

        SetComponent(entity, "Kinematics", kinematics)
        SetComponent(entity, "Position", pos)
    end
end

function TestScene:Render()
    local count = 0
    for _, entity in ipairs(GetEntities()) do
        local pos = GetComponent(entity, "Position")
        if pos then
            DrawCircle(pos.x, pos.y, 20, {r = 255, g = 0, b = 0, a = 255}, "default")
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

    AddComponent(entity, "Position")
    local pos = GetComponent(entity, "Position")
    pos.x = math.random(-300, 300)
    pos.y = math.random(-200, 200)
    SetComponent(entity, "Position", pos)

    AddComponent(entity, "Kinematics")
    local kinematics = GetComponent(entity, "Kinematics")
    kinematics.velocity.x = math.cos(TWO_PI / 5) * 100
    kinematics.velocity.y = math.sin(TWO_PI / 5) * 100
    kinematics.friction = 0
    SetComponent(entity, "Kinematics", kinematics)
end

return TestScene
