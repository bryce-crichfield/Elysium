local script = {}

function script.init(self, entity)
    Log("Event Script Init: " .. entity)
    AddComponent(entity, "Position")
    SetComponent(entity, "Position", {x = 100, y = 100})
end

function script.update(self, entity, dt)
    -- Do nothing, wait for events
end

function script.onEvent(self, entity, event)
    if event.type == "KeyPressed" then
        Log("Key Pressed: " .. event.key)
        
        if event.key == KEY_SPACE then
            Log("Spacebar! Moving entity.")
            local pos = GetComponent(entity, "Position")
            if pos then
                SetComponent(entity, "Position", {x = pos.x + 10, y = pos.y})
            end
        end
    elseif event.type == "MouseButtonPressed" then
        -- Use World Coordinates (wx, wy) if available
        if event.wx and event.wy then
            Log("Mouse Clicked at World: " .. event.wx .. ", " .. event.wy)
            SetComponent(entity, "Position", {x = event.wx, y = event.wy})
        else
            Log("Mouse Clicked at Screen: " .. event.x .. ", " .. event.y)
            SetComponent(entity, "Position", {x = event.x, y = event.y})
        end
    end
end

return script