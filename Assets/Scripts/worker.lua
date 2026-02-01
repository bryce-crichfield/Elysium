local Component = require("Scripts/Component")

local Worker = {
    STATE_IDLE = "IDLE",
    STATE_MOVING_TO_RESOURCE = "MOVING_TO_RESOURCE",
    STATE_GATHERING = "GATHERING",
    STATE_RETURNING = "RETURNING",
    STATE_DEPOSITING = "DEPOSITING",

    state = "IDLE",
    targetResource = 0,
    targetStorage = 0,
    gatherTimer = 0.0
}

function Worker.Initialize(self, entity)
    Log("Initializing Worker for entity " .. entity)

    Component.Add(entity, "Position")

    Component.Add(entity, "Rectangle", {
        width = 24,
        height = 24,
        background = Color.new(255, 220, 50, 255),
        border = Color.new(180, 150, 30, 255)
    })

    Component.Add(entity, "Bounds")
    Component.Add(entity, "Movement")

    Component.Add(entity, "Kinematics", {
        maxSpeed = 100.0,
        friction = 5.0
    })

    Component.Add(entity, "Health", {
        max = 50,
        current = 50
    })

    Component.Add(entity, "Faction", { name = "Player" })

    Component.Add(entity, "Carry", {
        capacity = 50,
        amount = 0,
        resourceType = ""
    })

    self.state = Worker.STATE_IDLE
    self.targetResource = 0
    self.targetStorage = 0
    self.gatherTimer = 0.0
end

function Worker.Update(self, entity, dt)
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
        self.state = Worker.STATE_RETURNING
        Log("Worker " .. entity .. " carrying " .. carry.amount .. " " .. carry.resourceType)
        return
    end

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

        AddComponent(entity, "PathRequest")
        local pathReq = GetComponent(entity, "PathRequest")
        pathReq.target = Vector2.new(storagePos.x, storagePos.y)

        local dist = Distance(pos.x, pos.y, storagePos.x, storagePos.y)
        if dist < 60 then
            if HasComponent(entity, "PathRequest") then
                RemoveComponent(entity, "PathRequest")
            end
            self.state = Worker.STATE_DEPOSITING
            Log("Worker " .. entity .. " arrived at storage")
        end
    else
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

function Worker.OnEvent(self, entity, event)
end

return Worker