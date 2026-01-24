local Soldier = {}

function Soldier.init(self, entity)
    Log("Initializing Soldier for entity " .. entity)

    -- Ensure it has Position (usually already added by scene loader or spawner)
    AddComponent(entity, "Position")
    

    -- Add Bounds
    AddComponent(entity, "Bounds")

    AddComponent(entity, "Rectangle")
    local rect = GetComponent(entity, "Rectangle")
    rect.width = 32
    rect.height = 48
    rect.background = Color.new(0, 0, 255, 255) --
    rect.border = Color.new(0, 0, 150, 255)
    SetComponent(entity, "Rectangle", rect)

    -- Add Movement/Physics
    AddComponent(entity, "Movement")
    AddComponent(entity, "Kinematics")
    local kin = GetComponent(entity, "Kinematics")
    kin.maxSpeed = 150.0
    kin.friction = 5.0
    SetComponent(entity, "Kinematics", kin)

    -- Add Stats
    AddComponent(entity, "Health")
    local health = GetComponent(entity, "Health")
    health.max = 100
    health.current = 100
    SetComponent(entity, "Health", health)

    -- Add Faction (Default to Player/0, but can be overridden)
    AddComponent(entity, "Faction")
    local faction = GetComponent(entity, "Faction")
    faction.name = "Enemy"
    SetComponent(entity, "Faction", faction)

    -- Add Attack Capability
    AddComponent(entity, "Attack")
    local attack = GetComponent(entity, "Attack")
    attack.range = 100.0
    attack.damage = 10.0
    attack.cooldown = 1.0
    attack.timer = 0.0
    attack.isAttacking = false
    attack.targetId = -1
    SetComponent(entity, "Attack", attack)
end

function Soldier.update(self, entity, dt)
    -- Simple state machine could go here (e.g. animation switching)
    local kin = GetComponent(entity, "Kinematics")
    local sprite = GetComponent(entity, "Sprite")
    
    -- Very basic animation state logic
    if math.abs(kin.velocity.x) > 10 or math.abs(kin.velocity.y) > 10 then
        -- Moving
        -- sprite.marker = "run/down" (if we had run anims)
    else
        -- Idle
        -- sprite.marker = "idle/down"
    end
end

function Soldier.onEvent(entity, event)
    -- Handle specific events if needed
end

return Soldier
