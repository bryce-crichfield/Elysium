local script = {}

script.speed = 200
script.direction = 1
script.minX = 100
script.maxX = 700
script.pulseTime = 0

function script.init(entity)
    Log("Script Init: Entity " .. entity)
    
    -- Ensure we have a Position and Rectangle
    AddComponent(entity, "Position")
    AddComponent(entity, "Rectangle")
    
    -- Set initial size
    SetRectangle(entity, 50, 50)
    SetPosition(entity, 400, 300)
end

function script.update(entity, dt)
    -- Get current position
    local x, y = GetPosition(entity)
    
    if x then
        -- Update position (Ping Pong movement)
        x = x + (script.speed * script.direction * dt)
        
        if x > script.maxX then
            x = script.maxX
            script.direction = -1
        elseif x < script.minX then
            x = script.minX
            script.direction = 1
        end
        
        SetPosition(entity, x, y)
        
        -- Pulse size
        script.pulseTime = script.pulseTime + dt
        local size = 50 + (math.sin(script.pulseTime * 5) * 10)
        SetRectangle(entity, size, size)
    end
end

return script