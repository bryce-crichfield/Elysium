local script = {}

-- Configuration
script.colorIndex = 1
script.colors = {
    {r = 255, g = 0, b = 0, a = 255},   -- Red
    {r = 0, g = 255, b = 255, a = 255}, -- Cyan
    {r = 255, g = 0, b = 255, a = 255}, -- Magenta
    {r = 0, g = 255, b = 0, a = 255}    -- Green
}

function script.init(self, entity)
    Log("Script Init: Color Cycle Entity " .. entity)
    
    -- Setup Components
    AddComponent(entity, "Position")
    AddComponent(entity, "Rectangle")
    AddComponent(entity, "Bounds") -- Required for PickPress to work
    
    -- Initial State
    SetComponent(entity, "Position", {x = 400, y = 300})
    SetComponent(entity, "Rectangle", {
        width = 100, 
        height = 100, 
        border = self.colors[self.colorIndex]
    })
end

function script.update(self, entity, dt)
    -- Nothing needed here for a static clickable object
end

function script.onEvent(self, entity, event)
    if event.type == "PickPress" then
        Log("Rectangle Clicked! Cycling color.")

        -- 1. Increment the index (wrap around using modulo)
        self.colorIndex = (self.colorIndex % #self.colors) + 1
        
        -- 2. Get the current rectangle component
        local rect = GetComponent(entity, "Rectangle")
        
        if rect then
            -- 3. Update the color and apply it back
            rect.border = self.colors[self.colorIndex]
            SetComponent(entity, "Rectangle", rect)
        end
    end
end

return script