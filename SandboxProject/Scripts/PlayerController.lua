local PlayerController = {}

function PlayerController.OnCreate(self)
    self.speed = 180.0
end

function PlayerController.OnUpdate(self, dt)
    local x, y = self.entity:get_position()
    local distance = self.speed * dt

    if Input.is_key_down("D") or Input.is_key_down("ArrowRight") then
        x = x + distance
    end
    if Input.is_key_down("A") or Input.is_key_down("ArrowLeft") then
        x = x - distance
    end
    if Input.is_key_down("W") or Input.is_key_down("ArrowUp") then
        y = y + distance
    end
    if Input.is_key_down("S") or Input.is_key_down("ArrowDown") then
        y = y - distance
    end

    self.entity:set_position(x, y)
end

return PlayerController
