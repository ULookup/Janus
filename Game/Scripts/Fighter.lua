-- Expanded prefab instances share assets, but keep independent native bodies and cursors.
local Script = {}
function Script.OnCreate(self)
    self.role = self.entity:name()
    if JanusCombat and (self.role == "Hero" or self.role == "Enemy") then
        JanusCombat.actors = JanusCombat.actors or {}
        JanusCombat.actors[self.role] = self.entity
    end
end
function Script.OnCollisionEnter(self, other)
    if JanusCombat and other:name() == "Floor" then
        JanusCombat.landings = (JanusCombat.landings or 0) + 1
    end
end
return Script
