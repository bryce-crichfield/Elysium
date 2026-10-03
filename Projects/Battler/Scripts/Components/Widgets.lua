-- How a scene hooks its values to the widget prefabs it places (Prefabs/Checkbox.xml,
-- Prefabs/Slider.xml): by the placement's id.
--
--   <PrefabInstance src="../Prefabs/Checkbox.xml" id="Msaa"> ... </PrefabInstance>
--   Widgets.Bind("Msaa", IsMsaaEnabled, SetMsaaEnabled)
--
-- A widget reads its binding every frame, so it doesn't matter whether the scene or the widget
-- initializes first. A checkbox's value is a boolean, a slider's a number from 0 to 1.
local Widgets = {}

local bindings = {}

function Widgets.Bind(id, get, set)
    bindings[id] = { get = get, set = set }
end

function Widgets.Binding(id) return bindings[id] end

-- The placement id a spawned entity belongs to: its name is namespaced "<id>::<name>".
function Widgets.PlacementId(entity)
    local n = GetComponent(entity, "Name")
    return n and n.name:match("^(.-)::") or nil
end

-- The child whose (namespaced) name ends with `name`.
function Widgets.Child(root, name)
    for _, e in ipairs(GetChildren(root)) do
        local n = GetComponent(e, "Name")
        if n and n.name:sub(-#name) == name then return e end
    end
    return nil
end

-- The root's rectangle on screen: x, y, w, h.
function Widgets.Rect(entity)
    local t = GetComponent(entity, "Transform")
    local r = GetComponent(entity, "Rectangle")
    if not t or not r then return 0, 0, 0, 0 end
    return t.worldX, t.worldY, r.width * t.worldScaleX, r.height * t.worldScaleY
end

function Widgets.Inside(m, x, y, w, h) return m.x >= x and m.x <= x + w and m.y >= y and m.y <= y + h end

-- Text is drawn centered on its position; this puts a text child's left edge at local x `left`.
function Widgets.AlignLeft(textEntity, left)
    local text = textEntity and GetComponent(textEntity, "Text")
    local t = textEntity and GetComponent(textEntity, "Transform")
    if text and t then t.localX = left + MeasureText(text.content, text.fontSize, text.font) / 2 end
end

Widgets.EDGE = {r = 90, g = 90, b = 100, a = 255}
Widgets.HOVER = {r = 255, g = 210, b = 110, a = 255}

return Widgets
