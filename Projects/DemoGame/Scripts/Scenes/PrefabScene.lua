---@type SceneScript
---@class PrefabScene
-- Tile-free POC scene. Selection, move orders and a path-line toggle; everything spatial is
-- authored in the editor (colliders/occluders on prefabs, nav areas painted in navmesh mode).
local PrefabScene = {}

local Selection = require("Scripts/Elysium/Selection")

local CAMERA_SPEED = 300
local ZOOM_STEP = 1.15   -- per wheel notch
local ZOOM_MIN, ZOOM_MAX = 0.6, 2.5
local ZOOM_SMOOTH = 14   -- how fast zoom converges on the target, per second

function PrefabScene:Initialize()
    self.time = 0
    self.navDebug = true
    self.selection = Selection.new(function(e) return HasComponent(e, "Kinematics") end)

    self.cameraEntity = nil
    self.panAnchor = nil
    self.zoomTarget = nil
    self.zoomOffset = nil
    Log("PrefabScene: N = path lines, right-click = move, wheel = zoom, middle-drag = pan, Esc = back")
end

-- Wheel zooms about the cursor and middle-drag pans, matching the editor viewport's feel. The
-- wheel sets a target and the zoom eases toward it over several frames, so a notch glides rather
-- than snapping; the cursor's world point is held fixed throughout by arithmetic on the stored
-- screen offset, since ScreenToWorld reports the projection the last frame rendered with.

-- Resolved lazily rather than cached once, so the camera can also arrive from a prefab or be
-- spawned at runtime.
function PrefabScene:Camera()
    if self.cameraEntity and HasComponent(self.cameraEntity, "Camera") then return self.cameraEntity end

    -- nil, not 0, means "no such entity": ids start at 0, so a scene whose camera is authored
    -- first has camera entity 0.
    local cam = GetEntityByName("CAMERA")
    if not cam then
        local found = FindEntitiesWithComponent("Camera")
        cam = found and found[1]
    end
    if not cam then return nil end

    self.cameraEntity = cam
    return cam
end

function PrefabScene:UpdateCamera(dt)
    self.cameraEntity = self:Camera()
    if not self.cameraEntity then return end
    local transform = GetComponent(self.cameraEntity, "Transform")
    local camera = GetComponent(self.cameraEntity, "Camera")
    if not transform or not camera then return end

    self.zoomTarget = self.zoomTarget or camera.zoom

    local wheel = GetMouseWheelMove()
    if wheel ~= 0 then
        self.zoomTarget = math.max(ZOOM_MIN, math.min(ZOOM_MAX, self.zoomTarget * ZOOM_STEP ^ wheel))
        -- Where the cursor sits relative to the screen centre, in pixels: (mouse - centre) is what
        -- the projection scales by zoom, so recovering it once per notch lets every smoothing step
        -- below re-derive the world point under the cursor without another ScreenToWorld call
        -- (which would report the projection the *last* frame rendered with, not this zoom).
        local under = ScreenToWorld(GetMousePosition())
        self.zoomOffset = { x = (under.x - transform.localX) * camera.zoom,
                            y = (under.y - transform.localY) * camera.zoom }
    end

    if math.abs(self.zoomTarget - camera.zoom) > 0.0005 then
        -- Ease toward the target instead of jumping a whole notch per wheel event, which reads as
        -- the camera snapping. Framerate-independent: the same fraction of the gap closes per
        -- second however long the frame was.
        local zoom = camera.zoom + (self.zoomTarget - camera.zoom) * (1 - math.exp(-ZOOM_SMOOTH * dt))
        local offset = self.zoomOffset
        if offset then
            -- Hold the cursor's world point still: it is camera + offset/zoom before the change, so
            -- the camera has to move to leave it where it was after.
            local worldX = transform.localX + offset.x / camera.zoom
            local worldY = transform.localY + offset.y / camera.zoom
            transform.localX = worldX - offset.x / zoom
            transform.localY = worldY - offset.y / zoom
        end
        camera.zoom = zoom
    else
        camera.zoom = self.zoomTarget
        self.zoomOffset = nil
    end

    local mouse = GetMousePosition()
    if IsMouseButtonDown(MOUSE_MIDDLE) then
        if self.panAnchor then
            -- Drag the world with the cursor, so the camera moves the other way.
            transform.localX = transform.localX - (mouse.x - self.panAnchor.x) / camera.zoom
            transform.localY = transform.localY - (mouse.y - self.panAnchor.y) / camera.zoom
        end
        self.panAnchor = { x = mouse.x, y = mouse.y }
    else
        self.panAnchor = nil
    end
end

function PrefabScene:Update(dt)
    self.time = self.time + dt

    local dx, dy = 0, 0
    if IsKeyDown(KEY_W) then dy = dy - 1 end
    if IsKeyDown(KEY_S) then dy = dy + 1 end
    if IsKeyDown(KEY_A) then dx = dx - 1 end
    if IsKeyDown(KEY_D) then dx = dx + 1 end
    if (dx ~= 0 or dy ~= 0) and self:Camera() then
        local pos = GetComponent(self.cameraEntity, "Transform")
        if pos then
            pos.localX = pos.localX + dx * CAMERA_SPEED * dt
            pos.localY = pos.localY + dy * CAMERA_SPEED * dt
        end
    end

    self:UpdateCamera(dt)
end

function PrefabScene:Render()
    self.selection:Render(self.time)

    -- Mouse probe: green dot when hovering walkable ground, red otherwise.
    local mp = ScreenToWorld(GetMousePosition())
    local ok = NavIsWalkable(mp.x, mp.y)
    local c = ok and {r=0, g=255, b=80, a=220} or {r=255, g=40, b=40, a=220}
    DrawLine(mp.x - 6, mp.y, mp.x + 6, mp.y, c, "selection")
    DrawLine(mp.x, mp.y - 3, mp.x, mp.y + 3, c, "selection")
end

function PrefabScene:OnEvent(event)
    self.selection:OnEvent(event)

    if event.type == "KeyPressed" then
        if event.key == KEY_ESCAPE then
            SceneReplace("MenuScene")
            return true
        elseif event.key == KEY_N then
            self.navDebug = not self.navDebug
            NavSetDebugDraw(self.navDebug)
            Log("Path lines: " .. tostring(self.navDebug))
        end
    elseif event.type == "MouseButtonPressed" and event.button == MOUSE_RIGHT then
        local i = 0
        for entity in pairs(self.selection.selected) do
            -- Fan the group out a little so they don't all target one cell.
            IssueMoveCommand(entity, event.wx + (i % 2) * 20, event.wy + math.floor(i / 2) * 12)
            i = i + 1
        end
    end
end

return PrefabScene

