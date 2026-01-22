local script = {}

script.speed = 200
script.direction = 1
script.minX = 100
script.maxX = 700
script.pulseTime = 0
script.borderColorTime = 0
script.borderColorIndex = 2
script.borderColors = {
    {r = 255, g = 0, b = 0, a = 255},
    {r = 0, g = 255, b = 0, a = 255},
    {r = 0, g = 0, b = 255, a = 255}
}

function script.init(self, entity)
    Log("Script Init: Entity " .. entity)
    
    -- Ensure we have a Position and Rectangle
    AddComponent(entity, "Position")
    AddComponent(entity, "Rectangle")
    
    -- Set initial size and color
    local initialRect = {
        width = 50, 
        height = 50, 
        border = self.borderColors[self.borderColorIndex]
    }
    SetComponent(entity, "Rectangle", initialRect)
    SetComponent(entity, "Position", {x = 400, y = 300})
end

function script.update(self, entity, dt)
    -- Get current position
    local pos = GetComponent(entity, "Position")
    
    if pos then
        -- Update position (Ping Pong movement)
        local x = pos.x + (self.speed * self.direction * dt)
        if x > self.maxX then
            x = self.maxX
            self.direction = -1
        elseif x < self.minX then
            x = self.minX
            self.direction = 1
        end
        SetComponent(entity, "Position", {x = x, y = pos.y})
        
        -- Pulse size
        self.pulseTime = self.pulseTime + dt
        local size = 50 + (math.sin(self.pulseTime * 5) * 10)
        
        -- Cycle border color
        self.borderColorTime = self.borderColorTime + dt
        if self.borderColorTime > 1.0 then
            self.borderColorTime = 0
            self.borderColorIndex = (self.borderColorIndex % #self.borderColors) + 1
        end

        local rect = GetComponent(entity, "Rectangle")
        if rect then
            rect.width = size
            rect.height = size
            rect.border = self.borderColors[self.borderColorIndex]
            SetComponent(entity, "Rectangle", rect)
        end
    end
end

function script.onEvent(self, entity, event)
    -- Log all events for this entity
    -- Log("PingPong Entity Event: " .. event.type)
end

return script