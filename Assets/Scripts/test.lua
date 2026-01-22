local script = {}

script.speed = 200
script.direction = 1
script.minX = 100
script.maxX = 700
script.pulseTime = 0

function script.init(self, entity)
    Log("Script Init: Entity " .. entity)
    
    -- Ensure we have a Position and Rectangle
    AddComponent(entity, "Position")
    AddComponent(entity, "Rectangle")
    
    -- Set initial size
    SetRectangle(entity, 50, 50)
    SetPosition(entity, 400, 300)
end

function script.update(self, entity, dt)
    -- Get current position
    local x, y = GetPosition(entity)
    
    if x then
        -- Update position (Ping Pong movement)
        -- Use 'self' to store per-entity state
        x = x + (self.speed * self.direction * dt)
        
        if x > self.maxX then
            x = self.maxX
            self.direction = -1
        elseif x < self.minX then
            x = self.minX
            self.direction = 1
        end
        
        SetPosition(entity, x, y)
        
        -- Pulse size
        self.pulseTime = self.pulseTime + dt
        local size = 50 + (math.sin(self.pulseTime * 5) * 10)
        SetRectangle(entity, size, size)
    end
end

function script.onEvent(self, entity, event)
    -- Log all events for this entity
    -- Log("PingPong Entity Event: " .. event.type)
end

return script