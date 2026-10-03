---@type EntityScript
---@class Checkbox
-- Prefabs/Checkbox.xml: a box that toggles its binding (Widgets.Bind with the placement id) on
-- click, showing the Check child while it's on. Clicking the label toggles it too.
local Widgets = require("Scripts/Components/Widgets")

local Checkbox = {}

local LABEL_LEFT = 48    -- where the label starts, right of the box
local LABEL_WIDTH = 360  -- how far right of the box a click still counts

function Checkbox:Initialize(entity)
    self.id = Widgets.PlacementId(entity)
    self.check = Widgets.Child(entity, "Check")
    self.label = Widgets.Child(entity, "Label")
end

function Checkbox:Update(entity, dt)
    local binding = self.id and Widgets.Binding(self.id)
    local on = binding and binding.get() or false

    local x, y, w, h = Widgets.Rect(entity)
    local hover = Widgets.Inside(GetMousePosition(), x, y, w + LABEL_WIDTH, h)
    if hover and binding and IsMouseButtonPressed(MOUSE_LEFT) then
        on = not on
        binding.set(on)
    end

    Widgets.AlignLeft(self.label, LABEL_LEFT)
    local layer = self.check and GetComponent(self.check, "Layer")
    if layer then layer.isVisible = on end
    local mat = GetComponent(entity, "Material")
    local stroke = mat and mat:Layer("Stroke")
    if stroke then stroke:Set("uColor", hover and Widgets.HOVER or Widgets.EDGE) end
end

return Checkbox
