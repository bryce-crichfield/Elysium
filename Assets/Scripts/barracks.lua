local Component = require("Scripts/Component")

local Barracks = {
    spawnTimer = 0.0,
    spawnInterval = 5.0
}

function Barracks.Initialize(self, entity)
    Log("Initializing Barracks for entity " .. entity)

    -- Visuals
    Component.Add(entity, "Position", { x = 0, y = 0 })
    Component.Add(entity, "Rectangle", {
        width = 64,
        height = 64,
        background = Color.new(100, 100, 100, 255), -- Grey building
        border = Color.new(50, 50, 50, 255)
    })

    Component.Add(entity, "Bounds")
    Component.Add(entity, "Health",  { max = 500, current = 500 })
    Component.Add(entity, "Faction", { name = "Player" })
end

function Barracks.Update(self, entity, dt)
    self.spawnTimer = self.spawnTimer + dt
    
    if self.spawnTimer >= self.spawnInterval then
        self.spawnTimer = 0.0
        self.SpawnSoldier(entity)
    end
end

function Barracks.SpawnSoldier(buildingEntity)
    local worldPos = GetComponent(buildingEntity, "Position")
    
    local soldierId = CreateEntity()
    Log("Barracks spawning soldier: " .. soldierId)
    
    -- Set initial position near barracks
    Component.Add(soldierId, "Position", {
        x = worldPos.x + Random(-50, 50),
        y = worldPos.y + 70
    })
    
    -- Attach Soldier script
    Component.Add(soldierId, "Script", {
        scriptName = "Scripts/soldier.lua",
        isActive = true
    })
end

function Barracks.OnEvent(entity, event)
end

return Barracks