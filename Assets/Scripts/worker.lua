local Worker = {}

-- States
Worker.STATE_IDLE = "IDLE"
Worker.STATE_MOVING_TO_RESOURCE = "MOVING_TO_RESOURCE"
Worker.STATE_GATHERING = "GATHERING"
Worker.STATE_RETURNING = "RETURNING"
Worker.STATE_DEPOSITING = "DEPOSITING"

Worker.state = Worker.STATE_IDLE
Worker.targetResource = 0
Worker.targetStorage = 0
Worker.gatherTimer = 0.0

function Worker.init(self, entity)
    Log("Initializing Worker for entity " .. entity)

    -- Ensure Position exists
    AddComponent(entity, "Position")

    -- Visuals: 24x24 yellow rectangle
    AddComponent(entity, "Rectangle")
    local rect = GetComponent(entity, "Rectangle")
    rect.width = 24
    rect.height = 24
    rect.background = Color.new(255, 220, 50, 255)  -- Yellow
    rect.border = Color.new(180, 150, 30, 255)

    AddComponent(entity, "Bounds")

    -- Movement
    AddComponent(entity, "Movement")

    AddComponent(entity, "Kinematics")
    local kin = GetComponent(entity, "Kinematics")
    kin.maxSpeed = 100.0
    kin.friction = 5.0

    -- Health
    AddComponent(entity, "Health")
    local health = GetComponent(entity, "Health")
    health.max = 50
    health.current = 50

    -- Faction
    AddComponent(entity, "Faction")
    local faction = GetComponent(entity, "Faction")
    faction.name = "Player"

    -- Carrying capacity
    AddComponent(entity, "Carry")
    local carry = GetComponent(entity, "Carry")
    carry.capacity = 50
    carry.amount = 0
    carry.resourceType = ""

    self.state = Worker.STATE_IDLE
    self.targetResource = 0
    self.targetStorage = 0
    self.gatherTimer = 0.0
end

function Worker.update(self, entity, dt)
    local pos = GetComponent(entity, "Position")

    if self.state == Worker.STATE_IDLE then
        self:FindResource(entity, pos)

    elseif self.state == Worker.STATE_MOVING_TO_RESOURCE then
        self:MoveToResource(entity, pos)

    elseif self.state == Worker.STATE_GATHERING then
        self:Gather(entity, dt)

    elseif self.state == Worker.STATE_RETURNING then
        self:ReturnToStorage(entity, pos)

    elseif self.state == Worker.STATE_DEPOSITING then
        self:Deposit(entity)
    end
end

function Worker:FindResource(entity, pos)
    -- Find nearest resource with amount > 0
    local resources = FindEntitiesWithComponent("Resource")
    local bestEntity = 0
    local bestDist = 999999

    for i, resEntity in ipairs(resources) do
        local res = GetComponent(resEntity, "Resource")
        if res and res.amount > 0 then
            local resPos = GetComponent(resEntity, "Position")
            if resPos then
                local dist = Distance(pos.x, pos.y, resPos.x, resPos.y)
                if dist < bestDist then
                    bestDist = dist
                    bestEntity = resEntity
                end
            end
        end
    end

    if bestEntity ~= 0 then
        self.targetResource = bestEntity
        local resPos = GetComponent(bestEntity, "Position")

        -- Request pathfinding
        AddComponent(entity, "PathRequest")
        local pathReq = GetComponent(entity, "PathRequest")
        pathReq.target = Vector2.new(resPos.x, resPos.y)

        self.state = Worker.STATE_MOVING_TO_RESOURCE
        Log("Worker " .. entity .. " moving to resource " .. bestEntity)
    end
end

function Worker:MoveToResource(entity, pos)
    if self.targetResource == 0 then
        self.state = Worker.STATE_IDLE
        return
    end

    -- Check if resource still exists and has amount
    if not HasComponent(self.targetResource, "Resource") then
        self.state = Worker.STATE_IDLE
        self.targetResource = 0
        return
    end

    local res = GetComponent(self.targetResource, "Resource")
    if res.amount <= 0 then
        self.state = Worker.STATE_IDLE
        self.targetResource = 0
        return
    end

    local resPos = GetComponent(self.targetResource, "Position")
    local dist = Distance(pos.x, pos.y, resPos.x, resPos.y)

    if dist < 40 then
        -- Arrived at resource - remove path request to stop
        if HasComponent(entity, "PathRequest") then
            RemoveComponent(entity, "PathRequest")
        end
        self.gatherTimer = 0.0
        self.state = Worker.STATE_GATHERING
        Log("Worker " .. entity .. " arrived at resource, gathering")
    end
end

function Worker:Gather(entity, dt)
    if self.targetResource == 0 or not HasComponent(self.targetResource, "Resource") then
        self.state = Worker.STATE_IDLE
        return
    end

    local res = GetComponent(self.targetResource, "Resource")
    local carry = GetComponent(entity, "Carry")

    if res.amount <= 0 or carry.amount >= carry.capacity then
        -- Done gathering, return to storage
        self.state = Worker.STATE_RETURNING
        Log("Worker " .. entity .. " carrying " .. carry.amount .. " " .. carry.resourceType)
        return
    end

    -- Accumulate gathered resources
    self.gatherTimer = self.gatherTimer + dt
    local gatherAmount = res.gatherRate * dt
    local spaceLeft = carry.capacity - carry.amount
    local available = res.amount
    local gathered = math.min(gatherAmount, spaceLeft, available)

    res.amount = res.amount - gathered
    carry.amount = carry.amount + gathered
    carry.resourceType = res.resourceType
end

function Worker:ReturnToStorage(entity, pos)
    -- Find nearest storage building
    local storages = FindEntitiesWithComponent("Storage")
    local bestEntity = 0
    local bestDist = 999999

    for i, storageEntity in ipairs(storages) do
        local storage = GetComponent(storageEntity, "Storage")
        if storage and storage.canAcceptDeposits then
            local storagePos = GetComponent(storageEntity, "Position")
            if storagePos then
                local dist = Distance(pos.x, pos.y, storagePos.x, storagePos.y)
                if dist < bestDist then
                    bestDist = dist
                    bestEntity = storageEntity
                end
            end
        end
    end

    if bestEntity ~= 0 then
        self.targetStorage = bestEntity
        local storagePos = GetComponent(bestEntity, "Position")

        -- Request pathfinding
        AddComponent(entity, "PathRequest")
        local pathReq = GetComponent(entity, "PathRequest")
        pathReq.target = Vector2.new(storagePos.x, storagePos.y)

        -- Check arrival
        local dist = Distance(pos.x, pos.y, storagePos.x, storagePos.y)
        if dist < 60 then
            -- Arrived at storage
            if HasComponent(entity, "PathRequest") then
                RemoveComponent(entity, "PathRequest")
            end
            self.state = Worker.STATE_DEPOSITING
            Log("Worker " .. entity .. " arrived at storage")
        end
    else
        -- No storage found, wait
        self.state = Worker.STATE_IDLE
    end
end

function Worker:Deposit(entity)
    if self.targetStorage == 0 or not HasComponent(self.targetStorage, "Storage") then
        self.state = Worker.STATE_IDLE
        return
    end

    local carry = GetComponent(entity, "Carry")
    local storage = GetComponent(self.targetStorage, "Storage")

    if carry.amount > 0 then
        storage:AddResource(carry.resourceType, carry.amount)
        Log("Worker " .. entity .. " deposited " .. carry.amount .. " " .. carry.resourceType)
        carry.amount = 0
        carry.resourceType = ""
    end

    self.targetStorage = 0
    self.targetResource = 0
    self.state = Worker.STATE_IDLE
end

function Worker.onEvent(self, entity, event)
end

return Worker
