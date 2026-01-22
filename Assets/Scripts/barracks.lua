local script = {}

-- State
script.queue = {}
script.spawnTimer = 0
script.selected = false

function script.init(self, entity)
    Log("Barracks Init: " .. entity)
    
    -- Ensure components
    AddComponent(entity, "Position")
    AddComponent(entity, "Rectangle")
    AddComponent(entity, "Bounds")
    AddComponent(entity, "Name")
    
    SetComponent(entity, "Name", "Barracks")
    SetComponent(entity, "Rectangle", {
        width = 80, 
        height = 80, 
        background = {r = 80, g = 80, b = 100, a = 255},
        border = {r = 150, g = 150, b = 150, a = 255}
    })
end

function script.update(self, entity, dt)
    if #self.queue > 0 then
        self.spawnTimer = self.spawnTimer - dt
        
        -- Visual feedback: change color based on progress
        local rect = GetComponent(entity, "Rectangle")
        if rect then
            local progress = 1.0 - (self.spawnTimer / 2.0)
            rect.background.g = 80 + (progress * 100)
        end

        if self.spawnTimer <= 0 then
            self:spawnUnit(entity)
            table.remove(self.queue, 1)
            
            if #self.queue > 0 then 
                self.spawnTimer = 2.0 
            else
                -- Reset color when done
                local rect = GetComponent(entity, "Rectangle")
                if rect then rect.background.g = 80 end
            end
        end
    end
end

function script.spawnUnit(self, entity)
    local pos = GetComponent(entity, "Position")
    local newUnit = CreateEntity()
    
    Log("Barracks: Spawning unit " .. newUnit)
    
    -- Setup the new unit
    AddComponent(newUnit, "Position")
    SetComponent(newUnit, "Position", {x = pos.x + 60, y = pos.y + 60})
    
    -- Important: Assign the unit script
    AddComponent(newUnit, "Script")
    SetComponent(newUnit, "Script", "Assets/Scripts/test.lua")
end

function script.onEvent(self, entity, event)
    -- Selection
    if event.type == "PickPress" then
        self.selected = not self.selected
        Log("Barracks Selected: " .. tostring(self.selected))
        
        local rect = GetComponent(entity, "Rectangle")
        if rect then
            rect.border = self.selected and {r=255, g=255, b=0, a=255} or {r=150, g=150, b=150, a=255}
        end
    end

    -- Hotkey 'U' to queue unit
    if self.selected and event.type == "KeyPressed" and event.key == KEY_U then
        Log("Barracks: Queuing Soldier...")
        table.insert(self.queue, "soldier")
        if #self.queue == 1 then self.spawnTimer = 2.0 end
    end
end

return script
