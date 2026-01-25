local CommandCenter = {}

CommandCenter.spawnTimer = 0.0
CommandCenter.spawnInterval = 10.0

function CommandCenter.init(self, entity)
    Log("Initializing CommandCenter for entity " .. entity)

    AddComponent(entity, "Position")
    local pos = GetComponent(entity, "Position")
    pos.x = 0
    pos.y = 0
    SetComponent(entity, "Position", pos)

    -- Visuals: 96x96 dark green rectangle
    AddComponent(entity, "Rectangle")
    local rect = GetComponent(entity, "Rectangle")
    rect.width = 96
    rect.height = 96
    rect.background = Color.new(34, 85, 51, 255)  -- Dark green
    rect.border = Color.new(17, 51, 26, 255)
    SetComponent(entity, "Rectangle", rect)

     -- Bounds
    AddComponent(entity, "Bounds")

    -- Health
    AddComponent(entity, "Health")
    local health = GetComponent(entity, "Health")
    health.max = 1000
    health.current = 1000
    SetComponent(entity, "Health", health)

    -- Faction
    AddComponent(entity, "Faction")
    local faction = GetComponent(entity, "Faction")
    faction.name = "Player"
    SetComponent(entity, "Faction", faction)

    -- Storage for deposited resources
    AddComponent(entity, "Storage")
    local storage = GetComponent(entity, "Storage")
    storage.canAcceptDeposits = true
    SetComponent(entity, "Storage", storage)
end

function CommandCenter.update(self, entity, dt)
    self.spawnTimer = self.spawnTimer + dt

    if self.spawnTimer >= self.spawnInterval then
        self.spawnTimer = 0.0
        CommandCenter.SpawnWorker(entity)
    end
end

function CommandCenter.SpawnWorker(buildingEntity)
    local worldPos = GetComponent(buildingEntity, "Position")

    local workerId = CreateEntity()
    Log("CommandCenter spawning worker: " .. workerId)

    -- Set initial position near command center
    AddComponent(workerId, "Position")
    local pos = GetComponent(workerId, "Position")
    pos.x = worldPos.x + Random(-30, 30)
    pos.y = worldPos.y + 80  -- Spawn below building

    -- Attach Worker script
    AddComponent(workerId, "Script")
    local script = GetComponent(workerId, "Script")
    script.scriptName = "Scripts/worker.lua"
    script.isActive = true
end

function CommandCenter.onEvent(self, entity, event)
end

return CommandCenter
