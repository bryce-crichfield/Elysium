local Component = require("Scripts/Component")

local CommandCenter = {
    spawnTimer = 0.0,
    spawnInterval = 10.0
}

function CommandCenter.Initialize(self, entity)
    Log("Initializing CommandCenter: " .. entity)

    Component.Add(entity, "Position", { x = 0, y = 0 })
    
    Component.Add(entity, "Rectangle", {
        width = 96,
        height = 96,
        background = Color.new(34, 85, 51, 255),
        border = Color.new(17, 51, 26, 255)
    })

    Component.Add(entity, "Bounds")
    Component.Add(entity, "Health",  { max = 1000, current = 1000 })
    Component.Add(entity, "Faction", { name = "Player" })
    Component.Add(entity, "Storage", { canAcceptDeposits = true })
end

function CommandCenter.Update(self, entity, deltaTime)
    self.spawnTimer = self.spawnTimer + deltaTime
    if self.spawnTimer >= self.spawnInterval then
        self.spawnTimer = self.spawnTimer - self.spawnInterval
        CommandCenter.SpawnWorker(entity)
    end
end

function CommandCenter.OnEvent(self, entity, event)
    -- We can do logic set rally point or other interactions here
end

function CommandCenter.SpawnWorker(buildingEntity)
    local worldPos = GetComponent(buildingEntity, "Position")
    local workerId = CreateEntity()

    Component.Add(workerId, "Position", {
        x = worldPos.x + Random(-30, 30),
        y = worldPos.y + 80
    })

    Component.Add(workerId, "Script", {
        scriptName = "Scripts/worker.lua",
        isActive = true
    })
end

return CommandCenter