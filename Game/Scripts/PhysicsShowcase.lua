local Script = {}

function Script.OnCreate(self)
    self.role = self.entity:name()
    self.collisions, self.enters, self.exits = 0, 0, 0
end

function Script.OnUpdate(self)
    if self.role == "Platform" then
        local tick = Physics.stats()
        self.entity:set_velocity(0, math.cos(tick / 60) * 0.5)
        return
    end
    if self.role ~= "Player" then return end
    if Input.was_key_pressed("Space") then self.entity:apply_impulse(0, 5) end
    if Input.was_key_pressed("Enter") then
        self.entity:set_position(-2, 6)
        self.entity:set_velocity(0, 0)
    end
    local tick, dropped, bodies = Physics.stats()
    local x, y = self.entity:get_position()
    local vx, vy = self.entity:get_velocity()
    local hit = Physics.raycast(7, 6, 7, -3)
    -- Update observes the last completed tick; callbacks below count this tick's events.
    Diagnostics.publish_snapshot({tick=tick, droppedSeconds=dropped, bodies=bodies,
        x=x, y=y, vx=vx, vy=vy, collisions=self.collisions,
        triggerEnters=self.enters, triggerExits=self.exits,
        rayFloor=hit ~= nil and hit.entity:name() == "Floor"})
end

function Script.OnCollisionEnter(self)
    self.collisions = self.collisions + 1
end

function Script.OnTriggerEnter(self, other)
    self.enters = self.enters + 1
    if self.role == "Collector" and other:name() == "Debris" then other:destroy() end
end

function Script.OnTriggerExit(self)
    self.exits = self.exits + 1
end

return Script
