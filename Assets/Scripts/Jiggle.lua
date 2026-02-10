local Component = require("Scripts/Component")
local RandomWander = {}

function RandomWander.Initialize(self, entity)
    local position = GetComponent(entity, "Position")
    if not position then
        return
    end
    
    -- Store the origin point
    self.originX = position.x
    self.originY = position.y
    
    -- Configuration
    self.wanderRadius = 2.0  -- How far from origin the entity can wander
    self.maxSpeed = 1.5      -- Maximum movement speed
    self.changeDirectionTime = 2.0  -- Time between direction changes
    self.timeAccumulator = 0.0
    
    -- Initialize with random direction
    self:PickRandomDirection()
end

function RandomWander.PickRandomDirection(self)
    -- Random angle in radians
    local angle = math.random() * math.pi * 2.0
    
    -- Random speed (between 0.3 and max speed for variation)
    local speed = 0.3 + (math.random() * (self.maxSpeed - 0.3))
    
    -- Convert to velocity components
    self.targetVelocityX = math.cos(angle) * speed
    self.targetVelocityY = math.sin(angle) * speed
end

function RandomWander.Update(self, entity, dt)
    local position = GetComponent(entity, "Position")
    local kinematics = GetComponent(entity, "Kinematics")
    
    if not position or not kinematics then
        Log("Can't find position or kinematics")
        return
    end
    
    -- Update direction change timer
    self.timeAccumulator = self.timeAccumulator + dt
    if self.timeAccumulator >= self.changeDirectionTime then
        self:PickRandomDirection()
        self.timeAccumulator = 0.0
    end
    
    -- Calculate distance from origin
    local dx = position.x - self.originX
    local dy = position.y - self.originY
    local distanceFromOrigin = math.sqrt(dx * dx + dy * dy)
    
    -- If too far from origin, push back towards center
    if distanceFromOrigin > self.wanderRadius then
        local pushAngle = math.atan2(-dy, -dx)
        self.targetVelocityX = math.cos(pushAngle) * self.maxSpeed
        self.targetVelocityY = math.sin(pushAngle) * self.maxSpeed
        self.timeAccumulator = 0.0  -- Reset timer to get new direction soon
    end
    
    -- Apply velocity
    kinematics.velocity.x = self.targetVelocityX
    kinematics.velocity.y = self.targetVelocityY
    
    Component.Add(entity, "Kinematics", kinematics)
end

return RandomWander