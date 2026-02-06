-- ComponentHelpers.lua
local Component = {}

-- Adds a component and optionally initializes it with a table of values
function Component.Add(entity, name, props)
    AddComponent(entity, name)
    if props then
        local c = GetComponent(entity, name)
        for k, v in pairs(props) do
            c[k] = v
        end
        SetComponent(entity, name, c)
    end
end

-- For updating components that already exist
function Component.Update(entity, name, func)
    local c = GetComponent(entity, name)
    func(c)
    SetComponent(entity, name, c)
end

return Component