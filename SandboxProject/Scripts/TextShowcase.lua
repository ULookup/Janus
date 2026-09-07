-- Presentation-only demo; combat and Button events belong to 10-05.
local Script = {}
function Script.OnCreate(self)
    self.elapsed = 0
end
function Script.OnUpdate(self, dt)
    self.elapsed = self.elapsed + dt
    local hp = 12 - (math.floor(self.elapsed) % 4) * 4
    self.entity:set_text(string.format("HP %d / 12\nRUNTIME VALUE", hp))
end
return Script
