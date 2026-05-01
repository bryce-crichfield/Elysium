local PortraitBar = {}
PortraitBar.__index = PortraitBar

local MAX_PORTRAITS = 5

local PORTRAITS = {
    Archer  = "Textures/Portraits/Archer128.jpg",
    Knight  = "Textures/Portraits/Knight128.jpg",
    Bishop  = "Textures/Portraits/Bishop128.jpg",
    Militia = "Textures/Portraits/Militia128.jpg",
    Worker  = "Textures/Portraits/Worker128.jpg",
}

local function getTexture(entity)
    local nameComp = GetComponent(entity, "Name")
    if not nameComp then return "" end
    for unitType, tex in pairs(PORTRAITS) do
        if nameComp.name:find(unitType) then return tex end
    end
    return ""
end

function PortraitBar.new()
    return setmetatable({}, PortraitBar)
end

function PortraitBar:Refresh(selected)
    local units = {}
    for entity in pairs(selected) do
        table.insert(units, entity)
    end
    for i = 1, MAX_PORTRAITS do
        local slot = PortraitSlotRegistry and PortraitSlotRegistry[i]
        if slot then
            local unit = units[i]
            if unit then slot:Show(getTexture(unit)) else slot:Hide() end
        end
    end
end

return PortraitBar
