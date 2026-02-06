local Component = require("Scripts/Component")

local ResourceNode = {}

function ResourceNode.Initialize(self, entity)
    Log("Initializing ResourceNode for entity " .. entity)

    Component.Add(entity, "Rectangle", {
        width = 40,
        height = 40,
        background = Color.new(0, 200, 220, 255),
        border = Color.new(0, 150, 170, 255)
    })

    Component.Add(entity, "Bounds")

    Component.Add(entity, "Resource", {
        resourceType = "Minerals",
        amount = 1500,
        maxAmount = 1500,
        gatherRate = 15
    })
end

function ResourceNode.Update(self, entity, dt)
    local res = GetComponent(entity, "Resource")
    local rect = GetComponent(entity, "Rectangle")

    if res and rect then
        local ratio = res.amount / res.maxAmount
        local alpha = math.max(80, math.floor(255 * ratio))
        local baseG = math.floor(200 * ratio)
        local baseB = math.floor(220 * ratio)

        rect.background = Color.new(0, baseG, baseB, alpha)
    end
end

function ResourceNode.OnEvent(self, entity, event)
end

return ResourceNode