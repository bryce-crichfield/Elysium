---@type EntityScript
---
--- Reveals the entity's text content one character at a time.
--- Attach to any entity with a TextComponent.
local TypeWriter = {}

local CHARS_PER_SEC = 10   -- reveal speed
local START_DELAY   = 0.15 -- brief pause before typing begins

function TypeWriter:Initialize(entity)
    local text = GetComponent(entity, "Text")
    if not text then return end

    self.fullText = text.content
    self.elapsed  = 0.0
    self.shown    = 0
    self.done     = false

    text.content = ""

end

function TypeWriter:Update(entity, dt)
    if self.done then return end

    self.elapsed = self.elapsed + dt
    local t = self.elapsed - START_DELAY
    if t < 0.0 then return end

    local target = math.floor(t * CHARS_PER_SEC)
    if target <= self.shown then return end

    self.shown = math.min(target, #self.fullText)

    local text = GetComponent(entity, "Text")
    if text then
        text.content = self.fullText:sub(1, self.shown)
    end

    if self.shown >= #self.fullText then
        self.done = true
    end
end

return TypeWriter
