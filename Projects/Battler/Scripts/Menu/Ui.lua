-- Immediate-mode menu widgets (buttons, text fields) for the Screen2D "ui" layer; the checkbox
-- and slider are prefabs instead (Prefabs/Checkbox.xml, Slider.xml). A scene script builds its
-- widgets once, calls Ui.Update(widgets) from Update and Ui.Draw(widgets) from Render. Coordinates are
-- game-screen pixels (1280 x 720).
local Sfx = require("Scripts/Menu/Sfx")

local Ui = {}

Ui.W, Ui.H = 1280, 720
Ui.FONT = "Fonts/EnchantedLand-Regular.ttf"

Ui.COLORS = {
    bg = {r = 10, g = 10, b = 16, a = 255},
    panel = {r = 22, g = 22, b = 34, a = 235},
    button = {r = 34, g = 34, b = 52, a = 255},
    hover = {r = 58, g = 52, b = 34, a = 255},
    edge = {r = 255, g = 210, b = 110, a = 255},
    text = {r = 235, g = 235, b = 240, a = 255},
    dim = {r = 160, g = 160, b = 175, a = 255},
    gold = {r = 255, g = 210, b = 110, a = 255},
    off = {r = 90, g = 90, b = 100, a = 255},
}
local C = Ui.COLORS

function Ui.Text(text, x, y, size, color)
    DrawText(text, x, y, size, color or C.text, "ui", Ui.FONT)
end

function Ui.Measure(text, size) return MeasureText(text, size, Ui.FONT) end

-- Centered on x.
function Ui.Title(text, x, y, size, color)
    Ui.Text(text, x - Ui.Measure(text, size) / 2, y, size, color)
end

-- On its own layer under "ui": draw commands land on top of a layer's entities, so a backdrop
-- drawn on "ui" would cover prefab widgets.
function Ui.Background()
    FillRect(0, 0, Ui.W, Ui.H, C.bg, "background")
end

local function Inside(w, m) return m.x >= w.x and m.x <= w.x + w.w and m.y >= w.y and m.y <= w.y + w.h end

local function Outline(x, y, w, h, color)
    DrawLine(x, y, x + w, y, color, "ui")
    DrawLine(x + w, y, x + w, y + h, color, "ui")
    DrawLine(x + w, y + h, x, y + h, color, "ui")
    DrawLine(x, y + h, x, y, color, "ui")
end

-- --- Widgets ------------------------------------------------------------------------------

-- onClick() runs on release over the button. enabled: a function, greyed out when it says no.
function Ui.Button(label, x, y, w, h, onClick, enabled)
    return { kind = "button", label = label, x = x, y = y, w = w, h = h, onClick = onClick, enabled = enabled }
end

-- A line of typed text (letters, digits, . - :), focused by clicking it. get() / set(s) read
-- and write it.
function Ui.Field(label, x, y, w, get, set, maxLen)
    return { kind = "field", label = label, x = x, y = y, w = w, h = 40, get = get, set = set, maxLen = maxLen or 32 }
end

-- A column of buttons centered on cx, starting at y.
function Ui.Column(cx, y, w, h, gap, items)
    local list = {}
    for i, item in ipairs(items) do
        list[i] = Ui.Button(item[1], cx - w / 2, y + (i - 1) * (h + gap), w, h, item[2], item[3])
    end
    return list
end

-- The characters the key codes typed (GLFW: printable keys are their ASCII codes).
local function KeyChar(key)
    if key >= 48 and key <= 57 then return string.char(key) end           -- 0-9
    if key >= 65 and key <= 90 then return string.char(key):lower() end   -- a-z
    if key == 46 then return "." end
    if key == 45 then return "-" end
    if key == 59 then return ":" end
    return nil
end

local function Enabled(w) return not w.enabled or w.enabled() end

-- --- Frame --------------------------------------------------------------------------------

function Ui.Update(widgets)
    local m = GetMousePosition()
    local pressed, released = IsMouseButtonPressed(MOUSE_LEFT), IsMouseButtonReleased(MOUSE_LEFT)
    for _, w in ipairs(widgets) do
        local wasHover = w.hover
        w.hover = Inside(w, m) and Enabled(w)
        if w.kind == "button" then
            if w.hover and not wasHover and wasHover ~= nil then Sfx.Play(Sfx.HOVER, 0.5) end
            if pressed and w.hover then w.armed = true end
            if released then
                if w.armed and w.hover and w.onClick then
                    Sfx.Play(Sfx.CLICK)
                    w.onClick()
                end
                w.armed = false
            end
        elseif w.kind == "field" then
            if pressed then w.focused = w.hover end
        end
    end
end

-- Feed KeyPressed events here; returns true if a focused field took the key.
function Ui.Key(widgets, key)
    for _, w in ipairs(widgets) do
        if w.kind == "field" and w.focused then
            local s = w.get()
            if key == KEY_BACKSPACE then
                w.set(s:sub(1, -2))
            elseif key == KEY_ENTER or key == KEY_ESCAPE then
                w.focused = false
            else
                local ch = KeyChar(key)
                if ch and #s < w.maxLen then w.set(s .. ch) end
            end
            return true
        end
    end
    return false
end

function Ui.Draw(widgets, time)
    for _, w in ipairs(widgets) do
        if w.kind == "button" then
            local on = Enabled(w)
            FillRect(w.x, w.y, w.w, w.h, w.hover and C.hover or C.button, "ui")
            if w.hover then Outline(w.x, w.y, w.w, w.h, C.edge) end
            local size = math.floor(w.h * 0.55)
            Ui.Text(w.label, w.x + w.w / 2 - Ui.Measure(w.label, size) / 2, w.y + (w.h - size) / 2 - 2, size,
                on and (w.hover and C.gold or C.text) or C.off)
        elseif w.kind == "field" then
            Ui.Text(w.label, w.x, w.y - 26, 22, C.dim)
            FillRect(w.x, w.y, w.w, w.h, C.button, "ui")
            Outline(w.x, w.y, w.w, w.h, w.focused and C.edge or C.off)
            local caret = w.focused and (math.floor((time or 0) * 2) % 2 == 0) and "_" or ""
            Ui.Text(w.get() .. caret, w.x + 12, w.y + 8, 24, C.text)
        end
    end
end

return Ui
