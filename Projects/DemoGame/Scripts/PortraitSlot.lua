---@type EntityScript
---@class PortraitSlot
local PortraitSlot = {}

-- Registry so ExploreScene can reach slot instances by index (1-based)
PortraitSlotRegistry = PortraitSlotRegistry or {}

local SLIDE_DIST  = 48    -- px below resting position when hidden
local SLIDE_SPEED = 8.0   -- t units/sec  (~0.125 s full travel)
local BORDER_IDLE     = "#303030FF"
local BORDER_OCCUPIED = "#FFFFFFFF"

local function smoothstep(t)
    t = math.max(0, math.min(1, t))
    return t * t * (3 - 2 * t)
end

function PortraitSlot:Initialize(entity)
    -- Read slot index from the entity name "PORTRAIT_0", "PORTRAIT_1", …
    local nameComp = GetComponent(entity, "Name")
    local idx = nameComp and tonumber(nameComp.name:match("PORTRAIT_(%d+)"))
    self.slotIndex = idx and (idx + 1) or 0   -- convert to 1-based

    local pos = GetComponent(entity, "Transform")
    self.baseY       = pos and pos.localY or 0
    self.t           = 0        -- 0 = fully offscreen below, 1 = resting position
    self.visible     = false
    self.pendingTex  = ""       -- texture requested by scene script

    -- Cache the paired frame entity so we can sync its visibility
    local nameComp2 = GetComponent(entity, "Name")
    local frameName = nameComp2 and (nameComp2.name .. "_FRAME") or ""
    self.frameEntity = frameName ~= "" and GetEntityByName(frameName) or nil

    -- Register so ExploreScene can call :Show / :Hide on us
    if self.slotIndex > 0 then
        PortraitSlotRegistry[self.slotIndex] = self
    end
end

-- Called by ExploreScene when a unit is assigned to this slot
function PortraitSlot:Show(textureName)
    self.pendingTex = textureName
    self.visible    = true
end

-- Called by ExploreScene when this slot should be cleared
function PortraitSlot:Hide()
    self.visible = false
    -- keep pendingTex so portrait stays visible while sliding out
end

function PortraitSlot:Update(entity, dt)
    -- Advance t toward target
    local target = self.visible and 1 or 0
    if self.t ~= target then
        local dir = (target > self.t) and 1 or -1
        self.t = math.max(0, math.min(1, self.t + dir * SLIDE_SPEED * dt))
    end

    local ease   = smoothstep(self.t)
    local offset = (1.0 - ease) * SLIDE_DIST   -- 0 at rest, SLIDE_DIST when hidden

    local pos = GetComponent(entity, "Transform")
    local mat = GetComponent(entity, "Material")

    if pos then
        pos.localY = self.baseY + offset
    end

    if mat then
        local portrait = mat:Layer("Texture")
        local border   = mat:Layer("Stroke")
        -- Only show the portrait while partially or fully visible
        if portrait then
            portrait.texture = self.t > 0 and self.pendingTex or ""
            portrait.enabled = self.t > 0 and self.pendingTex ~= ""
        end
        if border then
            border:Set("uColor", self.t > 0 and (self.visible and BORDER_OCCUPIED or BORDER_IDLE) or "#00000000")
        end
    end
end

return PortraitSlot