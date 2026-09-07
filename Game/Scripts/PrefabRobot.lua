local Script = {}

function Script.OnCreate(self)
    self.originX, self.originY = self.entity:get_position()
    self.elapsed = 0
end

function Script.OnUpdate(self, dt)
    self.elapsed = self.elapsed + dt
    self.entity:set_position(self.originX + math.sin(self.elapsed) * 60, self.originY)
    local playing, frame = self.entity:animation_state()
    Diagnostics.publish_snapshot({prefabRobot = self.entity:id(), elapsed = self.elapsed,
        playing = playing, animationFrame = frame})
end

return Script
