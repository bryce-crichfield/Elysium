local Component = require("Scripts/Component")

local Unit = {
    selected = false,
    colors = {
        normal = {r = 0, g = 255, b = 255, a = 255},
        selected = {r = 255, g = 255, b = 0, a = 255}
    }
}

function Unit.Initialize(self, entity)
    Log("RTS Unit Initialized: " .. entity)

    Component.Add(entity, "Position")
    Component.Add(entity, "Bounds")
    Component.Add(entity, "Movement")
    Component.Add(entity, "Unit")

    Component.Add(entity, "Rectangle", {
        width = 40,
        height = 40,
        border = self.colors.normal,
        background = {r = 50, g = 50, b = 50, a = 200},
        layerName = "default"
    })

    Component.Update(entity, "Movement", function(move)
        move.speed = 200.0
        move.loop = false
    end)
end

function Unit.Update(self, entity, dt)
    if IsKeyPressed(KEY_S) then
        local mousePos = GetMousePosition()
        local newUnit = CreateEntity()
        Log("Spawning unit " .. newUnit .. " at " .. mousePos.x .. ", " .. mousePos.y)

        Component.Add(newUnit, "Position", { x = mousePos.x, y = mousePos.y })
    end

    if self.selected and IsKeyPressed(KEY_X) then
        Log("Destroying unit " .. entity)
        DestroyEntity(entity)
    end
end

function Unit.OnEvent(self, entity, event)
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

    if event.type == "PickPress" then
        self.selected = not self.selected
        Log("Unit " .. entity .. " selected: " .. tostring(self.selected))

        local rect = GetComponent(entity, "Rectangle")
        if rect then
            rect.border = self.selected and self.colors.selected or self.colors.normal
        end
    end
end

return Unit