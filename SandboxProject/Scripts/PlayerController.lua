local PlayerController = {}

-- Keep older projects without project.json playable with the original key bindings.
local function is_down(action, key, alternate)
    if Input.has_action(action) then
        return Input.is_action_down(action)
    end
    return Input.is_key_down(key) or Input.is_key_down(alternate)
end

function PlayerController.OnCreate(self)
    self.speed = 180.0
end

function PlayerController.OnUpdate(self, dt)
    local x, y = self.entity:get_position()
    local distance = self.speed * dt

    if is_down("MoveRight", "D", "ArrowRight") then
        x = x + distance
    end
    if is_down("MoveLeft", "A", "ArrowLeft") then
        x = x - distance
    end
    if is_down("MoveUp", "W", "ArrowUp") then
        y = y + distance
    end
    if is_down("MoveDown", "S", "ArrowDown") then
        y = y - distance
    end

    self.entity:set_position(x, y)
end

return PlayerController
