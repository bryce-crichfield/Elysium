local ResourceNode = {}

function ResourceNode.init(self, entity)
    Log("Initializing ResourceNode for entity " .. entity)

    -- Visuals: 40x40 cyan rectangle
    AddComponent(entity, "Rectangle")
    local rect = GetComponent(entity, "Rectangle")
    rect.width = 40
    rect.height = 40
    rect.background = Color.new(0, 200, 220, 255)  -- Cyan
    rect.border = Color.new(0, 150, 170, 255)

    AddComponent(entity, "Bounds")

    -- Resource data
    AddComponent(entity, "Resource")
    local res = GetComponent(entity, "Resource")
    res.resourceType = "Minerals"
    res.amount = 1500
    res.maxAmount = 1500
    res.gatherRate = 15
end

function ResourceNode.update(self, entity, dt)
    -- Dim the node as it gets depleted
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

function ResourceNode.onEvent(self, entity, event)
end

return ResourceNode
