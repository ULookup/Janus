local Script = {}
function Script.OnUpdate(self)
    if Input.was_key_pressed("Space") then self.entity:stop_animation() end
    if Input.was_key_pressed("Enter") then self.entity:play_animation() end
    if Input.was_key_pressed("D") then
        self.entity:play_animation("ad100000-0000-4000-8000-000000000004")
    end
    local playing, frame, elapsed = self.entity:animation_state()
    Diagnostics.publish_snapshot({playing=playing, animationFrame=frame, elapsed=elapsed})
end
return Script
