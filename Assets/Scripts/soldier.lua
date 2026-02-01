local Component = require("Scripts/Component")

local Soldier = {}

function Soldier.Initialize(self, entity)
    Log("Initializing Soldier for entity " .. entity)

    Component.Add(entity, "Position")
    Component.Add(entity, "Bounds")

    Component.Add(entity, "Rectangle", {
        width = 32,
        height = 48,
        background = Color.new(0, 0, 255, 255),
        border = Color.new(0, 0, 150, 255)
    })

    Component.Add(entity, "Movement")

    Component.Add(entity, "Kinematics", {
        maxSpeed = 150.0,
        friction = 5.0
    })

    Component.Add(entity, "Health", {
        max = 100,
        current = 100
    })

    Component.Add(entity, "Faction", {
        name = "Enemy"
    })

    Component.Add(entity, "Attack", {
        range = 100.0,
        damage = 10.0,
        cooldown = 1.0,
        timer = 0.0,
        isAttacking = false,
        targetId = -1
    })
end

function Soldier.Update(self, entity, dt)
    local kin = GetComponent(entity, "Kinematics")
    if kin and (math.abs(kin.velocity.x) > 10 or math.abs(kin.velocity.y) > 10) then
        -- Moving state
    else
        -- Idle state
    end
end

function Soldier.OnEvent(self, entity, event)
end

return Soldier