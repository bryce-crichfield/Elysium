local Selection = {}
Selection.__index = Selection

local SELECTION_RADIUS = 32.0
local DRAG_THRESHOLD   = 6
local RING_W           = 32
local RING_H           = 16
local SELECTION_Y_OFFSET = 0

function Selection.new(filter)
    return setmetatable({
        selected    = {},
        isDragging  = false,
        _dragStart  = nil,
        _dragEnd    = nil,
        _filter     = filter or function(e) return HasComponent(e, "Kinematics") end,
    }, Selection)
end

function Selection:_clearOnly()
    for entity in pairs(self.selected) do
        if HasComponent(entity, "Selection") then
            RemoveComponent(entity, "Selection")
        end
    end
    self.selected = {}
end

function Selection:Clear()
    self:_clearOnly()
end

function Selection:Render(time)
    for entity in pairs(self.selected) do
        local pos = GetComponent(entity, "Transform")
        if pos then
            local pulse = 1.0 + 0.1 * math.sin(time * (2 * math.pi / 1.5))
            DrawEllipse(pos.worldX, pos.worldY + SELECTION_Y_OFFSET,
                        RING_W * pulse, RING_H * pulse,
                        {r=0, g=255, b=0, a=255}, "selection")
        end
    end

    if self.isDragging and self._dragStart and self._dragEnd then
        local x1, y1 = self._dragStart.x, self._dragStart.y
        local x2, y2 = self._dragEnd.x,   self._dragEnd.y
        FillRect(math.min(x1,x2), math.min(y1,y2),
                 math.abs(x2-x1), math.abs(y2-y1),
                 {r=100, g=180, b=255, a=40}, "selection")
        DrawLine(x1, y1, x2, y1, {r=100, g=180, b=255, a=220}, "selection")
        DrawLine(x2, y1, x2, y2, {r=100, g=180, b=255, a=220}, "selection")
        DrawLine(x2, y2, x1, y2, {r=100, g=180, b=255, a=220}, "selection")
        DrawLine(x1, y2, x1, y1, {r=100, g=180, b=255, a=220}, "selection")
    end
end

function Selection:OnEvent(event)
    if event.type == "MouseMoved" then
        if self._dragStart then
            self._dragEnd = {x = event.wx, y = event.wy}
            if not self.isDragging then
                local dx = event.wx - self._dragStart.x
                local dy = event.wy - self._dragStart.y
                if math.abs(dx) > DRAG_THRESHOLD or math.abs(dy) > DRAG_THRESHOLD then
                    self.isDragging = true
                end
            end
        end

    elseif event.type == "MouseButtonPressed" and event.button == MOUSE_LEFT then
        self._dragStart = {x = event.wx, y = event.wy}
        self._dragEnd   = {x = event.wx, y = event.wy}
        self.isDragging = false

    elseif event.type == "MouseButtonReleased" and event.button == MOUSE_LEFT then
        if self.isDragging then
            self:_dragSelect()
        else
            self:_clickSelect(event.wx, event.wy)
        end
        self._dragStart = nil
        self._dragEnd   = nil
        self.isDragging = false
    end
end

function Selection:_clickSelect(wx, wy)
    self:_clearOnly()
    local nearest = FindNearestEntity(wx, wy, "Kinematics")
    if nearest ~= 0 and self._filter(nearest) then
        local pos = GetComponent(nearest, "Transform")
        if pos and Distance(wx, wy, pos.worldX, pos.worldY) <= SELECTION_RADIUS then
            self.selected[nearest] = true
            AddComponent(nearest, "Selection")
            Log("Selected entity " .. nearest)
        end
    end
end

function Selection:_dragSelect()
    self:_clearOnly()
    local x1 = math.min(self._dragStart.x, self._dragEnd.x)
    local x2 = math.max(self._dragStart.x, self._dragEnd.x)
    local y1 = math.min(self._dragStart.y, self._dragEnd.y)
    local y2 = math.max(self._dragStart.y, self._dragEnd.y)
    local count = 0
    for _, entity in ipairs(GetEntities()) do
        if HasComponent(entity, "Kinematics") and self._filter(entity) then
            local pos = GetComponent(entity, "Transform")
            if pos and pos.worldX >= x1 and pos.worldX <= x2 and pos.worldY >= y1 and pos.worldY <= y2 then
                self.selected[entity] = true
                AddComponent(entity, "Selection")
                count = count + 1
            end
        end
    end
    Log("Drag-selected " .. count .. " unit(s)")
end

return Selection
