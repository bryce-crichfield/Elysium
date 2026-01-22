local script = {}

-- Configuration
script.selected = false
script.colors = {
    normal = {r = 0, g = 255, b = 255, a = 255},   -- Cyan
    selected = {r = 255, g = 255, b = 0, a = 255}  -- Yellow
}

function script.init(self, entity)
    Log("RTS Unit Initialized: " .. entity)
    
    -- Ensure components exist
    AddComponent(entity, "Position")
    AddComponent(entity, "Rectangle")
    AddComponent(entity, "Bounds")
    AddComponent(entity, "Movement")
    AddComponent(entity, "Unit")
    
    -- Setup initial look
    SetComponent(entity, "Rectangle", {
        width = 40, 
        height = 40, 
        border = self.colors.normal,
        background = {r = 50, g = 50, b = 50, a = 200},
        layerName = "default"
    })

    local move = GetComponent(entity, "Movement")
    if move then
        move.speed = 200.0
        move.loop = false
    end
    SetComponent(entity, "Movement", move)
end

function script.update(self, entity, dt)
    -- 1. Spawning Logic: Press 'S' to spawn a new unit at mouse
    if IsKeyPressed(KEY_S) then
        local mousePos = GetMousePosition()
        local newUnit = CreateEntity()
        Log("Spawning unit " .. newUnit .. " at " .. mousePos.x .. ", " .. mousePos.y)
        
        -- Give it this script
        -- In our engine, AddComponent usually needs a call to set data or it uses defaults
        AddComponent(newUnit, "Position")
        SetComponent(newUnit, "Position", {x = mousePos.x, y = mousePos.y})
        
        -- Add the script component and set it
        -- Note: We might need a SetComponent for Script specifically or just rely on the fact that 
        -- the engine will see the scriptName if we could set it.
        -- Since ScriptComponent isn't fully bound with fields yet, let's assume AddComponent handles the basics
        -- and we might need to bind ScriptComponent fields in C++ if we want to change them here.
    end


    -- 3. Deletion Logic: Press 'X' to destroy selected unit
    if self.selected and IsKeyPressed(KEY_X) then
        Log("Destroying unit " .. entity)
        DestroyEntity(entity)
    end
end

function script.onEvent(self, entity, event)
    if event.type == "MouseButtonPressed" and event.button == MOUSE_RIGHT and self.selected then
        local targetX = event.wx or event.x
        local targetY = event.wy or event.y
        
        local move = GetComponent(entity, "Movement")
        if move then
            Log("Unit " .. entity .. " moving to: " .. targetX .. ", " .. targetY)
            move:ClearWaypoints()
            move:AddWaypoint(Vector2.new(targetX, targetY))
            move.isMoving = true
        end
    end
    -- Selection Logic
    if event.type == "PickPress" then
        self.selected = not self.selected
        Log("Unit " .. entity .. " selected: " .. tostring(self.selected))

        local rect = GetComponent(entity, "Rectangle")
        if rect then
            rect.border = self.selected and self.colors.selected or self.colors.normal
        end
    end
end

return script
