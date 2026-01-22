local script = {}

function script.init(self, entity)
    Log("Event Script Init: " .. entity)
    AddComponent(entity, "Position")
    SetPosition(entity, 100, 100)
end

function script.update(self, entity, dt)
    -- Do nothing, wait for events
end

function script.onEvent(self, entity, event)
    if event.type == "KeyPressed" then
        Log("Key Pressed: " .. event.key)
        
        if event.key == KEY_SPACE then
            Log("Spacebar! Moving entity.")
            local x, y = GetPosition(entity)
            SetPosition(entity, x + 10, y)
        end
    elseif event.type == "MouseButtonPressed" then
        -- Use World Coordinates (wx, wy) if available
        if event.wx and event.wy then
            Log("Mouse Clicked at World: " .. event.wx .. ", " .. event.wy)
            SetPosition(entity, event.wx, event.wy)
        else
            Log("Mouse Clicked at Screen: " .. event.x .. ", " .. event.y)
            SetPosition(entity, event.x, event.y)
        end
    end
end

return script