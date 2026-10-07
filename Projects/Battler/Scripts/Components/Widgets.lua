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

-- Anything else a prefab reads from its scene, as a table it defines (a Hand's show/onPlay,
-- a Card's view). The binder may keep changing the table's fields; the prefab reads them live.
function Widgets.BindView(id, view)
    bindings[id] = view
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

-- --- Anchoring ----------------------------------------------------------------------------
-- Scenes and prefabs lay the UI out for the layout size (GetLayoutSize: Config <Screen>), but
-- the game screen is that or bigger (GetScreenSize: a wider or taller window). Anchoring keeps
-- a widget where it was placed relative to an edge or the middle of the screen: across it's
-- "left" (as placed), "center" or "right", down "top" (as placed), "middle" or "bottom", and
-- "stretch" keeps both edges by growing its Rectangle. Call it every frame: the window can
-- change. What was placed is remembered per entity, and taken again if anything else moved it.
-- A bound button places itself every frame (its press squash), so it's handed its anchor
-- instead (binding.anchor) and adds Widgets.Shift itself.
local anchors = {}
local SHIFT = { left = 0, top = 0, center = 0.5, middle = 0.5, right = 1, bottom = 1, stretch = 0 }

-- How far an anchor moves what was placed for the layout size: dx, dy.
function Widgets.Shift(across, down)
    local sw, sh = GetScreenSize()
    local lw, lh = GetLayoutSize()
    return (sw - lw) * (SHIFT[across or "left"] or 0), (sh - lh) * (SHIFT[down or "top"] or 0)
end

local function Anchored(rec, key, current, shifted)
    -- (Components hold floats: what comes back is only near what was set.)
    local applied = rec[key .. "Applied"]
    if rec[key] == nil or math.abs(applied - current) > 0.01 then rec[key] = current end
    rec[key .. "Applied"] = rec[key] + shifted
    return rec[key .. "Applied"]
end

function Widgets.Anchor(entity, across, down)
    local t = entity and GetComponent(entity, "Transform")
    if not t then return end
    local id = Widgets.PlacementId(entity)
    local button = id and bindings[id]
    if button and button.onClick then
        button.anchor = { across, down }
        return
    end
    local sw, sh = GetScreenSize()
    local lw, lh = GetLayoutSize()
    local dw, dh = sw - lw, sh - lh
    local rec = anchors[entity] or {}
    anchors[entity] = rec
    t.localX = Anchored(rec, "x", t.localX, dw * (SHIFT[across or "left"] or 0))
    t.localY = Anchored(rec, "y", t.localY, dh * (SHIFT[down or "top"] or 0))
    local r = (across == "stretch" or down == "stretch") and GetComponent(entity, "Rectangle")
    if r and across == "stretch" then r.width = Anchored(rec, "w", r.width, dw) end
    if r and down == "stretch" then r.height = Anchored(rec, "h", r.height, dh) end
end

-- A menu's whole layout, kept in the middle of the screen: every top-level entity is anchored
-- center/middle, except `backdrop` (a placement id), which stretches over the whole screen.
function Widgets.CenterLayout(backdrop)
    local back = backdrop and Widgets.Find(backdrop)
    for _, e in ipairs(GetEntities()) do
        if not HasComponent(e, "Parent") and HasComponent(e, "Transform") and not HasComponent(e, "Camera") then
            if e == back then Widgets.Anchor(e, "stretch", "stretch") else Widgets.Anchor(e, "center", "middle") end
        end
    end
end

Widgets.EDGE ={r = 90, g = 90, b = 100, a = 255}
Widgets.HOVER = {r = 255, g = 210, b = 110, a = 255}

return Widgets
