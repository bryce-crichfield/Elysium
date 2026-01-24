local Barracks = {}

Barracks.spawnTimer = 0.0
Barracks.spawnInterval = 5.0

function Barracks.init(self, entity)
    Log("Initializing Barracks for entity " .. entity)

    -- Visuals
    AddComponent(entity, "Rectangle")
    local rect = GetComponent(entity, "Rectangle")
    rect.width = 64
    rect.height = 64
    rect.background = Color.new(100, 100, 100, 255) -- Grey building
    rect.border = Color.new(50, 50, 50, 255)
    
    AddComponent(entity, "Bounds")
    
    -- Health
    AddComponent(entity, "Health")
    local health = GetComponent(entity, "Health")
    health.max = 500
    health.current = 500
    
    -- Faction
    AddComponent(entity, "Faction")
    local faction = GetComponent(entity, "Faction")
    faction.name = "Player"
end

function Barracks.update(self, entity, dt)
    Barracks.spawnTimer = Barracks.spawnTimer + dt
    
    if Barracks.spawnTimer >= Barracks.spawnInterval then
        Barracks.spawnTimer = 0.0
        Barracks.SpawnSoldier(entity)
    end
end

function Barracks.SpawnSoldier(buildingEntity)
    local worldPos = GetComponent(buildingEntity, "Position")
    
    local soldierId = CreateEntity()
    Log("Barracks spawning soldier: " .. soldierId)
    
    -- Set initial position near barracks
    AddComponent(soldierId, "Position")
    local pos = GetComponent(soldierId, "Position")
    pos.x = worldPos.x + Random(-50, 50)
    pos.y = worldPos.y + 70 -- Spawn slightly below
    
    -- Attach Soldier script
    AddComponent(soldierId, "Script")
    local script = GetComponent(soldierId, "Script")
    script.scriptName = "Scripts/soldier.lua"
    script.isActive = true
    -- Note: ScriptSystem will initialize the script on next frame/update
end

function Barracks.onEvent(entity, event)
end

return Barracks