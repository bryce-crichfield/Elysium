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

-- A button's action (Prefabs/Button.xml): onClick() on a click; enabled() (optional) greys it
-- out while it says no; active() (optional) holds it lit, as the chosen one of a set.
function Widgets.BindButton(id, onClick, enabled, active)
    bindings[id] = { onClick = onClick, enabled = enabled, active = active }
end

-- Whether the pointer is over a shown button, so a scene doesn't also take the click as a
-- click on the world. Buttons report themselves each frame.
local hovered = {}
function Widgets.SetHovered(id, on) hovered[id] = on or nil end
function Widgets.PointerOverUi() return next(hovered) ~= nil end
-- Whether a text field has the keyboard, so a scene leaves keys (Esc, letters) to it.
local typing = {}
function Widgets.SetTyping(id, on) typing[id] = on or nil end
function Widgets.Typing() return next(typing) ~= nil end

-- For a scene's Initialize: a button or field left hovered or focused when the last scene
-- ended is gone now.
function Widgets.ResetHover() hovered, typing = {}, {} end

-- --- Driving a placed prefab from its scene ----------------------------------------------

-- A placement's root, by its id (or any entity by name).
function Widgets.Find(id) return GetEntityByName(id) end

-- Whether an entity is shown, from its Layer.
function Widgets.Visible(entity)
    local layer = entity and GetComponent(entity, "Layer")
    return layer == nil or layer.isVisible
end

-- Shows or hides an entity and everything under it.
function Widgets.SetVisible(entity, on)
    if not entity then return end
    local layer = GetComponent(entity, "Layer")
    if layer then layer.isVisible = on end
    for _, child in ipairs(GetChildren(entity)) do Widgets.SetVisible(child, on) end
end

-- A text child's content and, optionally, color; `left` puts its left edge there instead of
-- centering it (local x).
function Widgets.ChildText(root, name, content, color, left)
    local e = root and Widgets.Child(root, name)
    local text = e and GetComponent(e, "Text")
    if not text then return end
    if content then text.content = content end
    if color then text.color = color end
    if left then Widgets.AlignLeft(e, left) end
end

-- The color of an entity's Flat material layer (a panel's fill).
function Widgets.SetFill(entity, color)
    local mat = entity and GetComponent(entity, "Material")
    local flat = mat and mat:Layer("Flat")
    if flat then flat:Set("uColor", color) end
end

function Widgets.Binding(id) return bindings[id] end

-- A text entity's content, by entity name (a scene's own labels); ignored if it isn't there.
function Widgets.SetText(name, content, color)
    local e = GetEntityByName(name)
    local text = e and GetComponent(e, "Text")
    if not text then return end
    if content then text.content = content end
    if color then text.color = color end
end

-- The placement id a spawned entity belongs to. The placement's root is named the id itself;
-- the entities under it are namespaced "<id>::<name>".
function Widgets.PlacementId(entity)
    local n = GetComponent(entity, "Name")
    if not n then return nil end
    return n.name:match("^(.-)::") or n.name
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

-- Eases `from` toward `to` at `rate` per second (frame-rate independent).
function Widgets.Ease(from, to, rate, dt) return from + (to - from) * (1 - math.exp(-rate * dt)) end

-- An entity's Fire layer intensity (the hover glow the widget prefabs carry).
function Widgets.SetGlow(entity, intensity)
    local mat = entity and GetComponent(entity, "Material")
    local fire = mat and mat:Layer("Fire")
    if fire then fire:Set("uIntensity", intensity) end
end

Widgets.EDGE ={r = 90, g = 90, b = 100, a = 255}
Widgets.HOVER = {r = 255, g = 210, b = 110, a = 255}

return Widgets
