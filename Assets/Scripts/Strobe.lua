local Strobe = {}

function Strobe.Initialize(self, entity)
    self.timeAccumulator = 0.0
    self.cycleDuration = 21 -- Total time for one complete up/down cycle
    self.minIntensity = 0.0
    self.maxIntensity = 2.0
end

function Strobe.Update(self, entity, dt)
    local light = GetComponent(entity, "Light")
    if not light then
        return
    end

    self.timeAccumulator = self.timeAccumulator + dt
    
    -- Normalize time to 0-1 range within the cycle
    local normalizedTime = (self.timeAccumulator % self.cycleDuration) / self.cycleDuration
    
    -- Create triangle wave: ramp up 0->1, then down 1->0
    local triangleWave
    if normalizedTime < 0.5 then
        triangleWave = normalizedTime * 2.0  -- 0 to 1
    else
        triangleWave = 2.0 - (normalizedTime * 2.0)  -- 1 to 0
    end
    
    -- Map to intensity range
    light.intensity = self.minIntensity + (triangleWave * (self.maxIntensity - self.minIntensity))
end


return Strobe