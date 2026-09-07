-- Game-only composition: physics is visible feedback; Combat.lua owns damage and turns.
local Script = {}
function Script.OnCreate(self)
    local b = JanusCombat
    self.frame = 0
    self.victory, self.defeat = false, false
    b.hits, b.landings = 0, 0
    b.resetFeedback = function()
        b.hits, b.landings = 0, 0
        for role, entity in pairs(b.actors) do
            entity:set_position(role == "Hero" and -3 or 3, 1.5)
            entity:set_velocity(0, 0)
            entity:play_animation()
        end
    end
    b.feedback = function()
        local function hit(entity)
            entity:set_velocity(0, 0)
            entity:apply_impulse(0, 3)
            entity:play_animation("ad100000-0000-4000-8000-000000000004")
            b.hits = b.hits + 1
        end
        if b.lastDamage > 0 then hit(b.actors.Enemy) end
        if b.lastRetaliation > 0 then hit(b.actors.Hero) end
    end
    b.decorateSnapshot = function(fields)
        local tick, dropped, bodies = Physics.stats()
        local _, heroY = b.actors.Hero:get_position()
        local _, enemyY = b.actors.Enemy:get_position()
        local playing, frame = b.actors.Enemy:animation_state()
        fields.physicsTick, fields.droppedSeconds, fields.bodies = tick, dropped, bodies
        fields.heroY, fields.enemyY = heroY, enemyY
        fields.hits, fields.landings = b.hits, b.landings
        fields.enemyAnimating, fields.enemyFrame = playing, frame
        fields.verifiedVictory, fields.verifiedDefeat = self.victory, self.defeat
    end
    b.refresh()
end
function Script.OnUpdate(self)
    local b = JanusCombat
    self.frame = self.frame + 1
    if self.entity:name() == "ArenaVerification" then
        local sequence = {[1]="Start", [60]="Strike", [61]="Play",
            [120]="Strike", [121]="Play", [180]="Strike", [181]="Play",
            [240]="Restart", [241]="Start", [300]="Wait", [301]="Play",
            [360]="Wait", [361]="Play", [420]="Wait", [421]="Play"}
        if sequence[self.frame] then b.act(sequence[self.frame]) end
    else
        for _, binding in ipairs({{"StartBattle","Start"}, {"SelectStrike","Strike"},
            {"SelectWait","Wait"}, {"PlayCard","Play"}, {"RestartBattle","Restart"}}) do
            if Input.was_action_pressed(binding[1]) then b.act(binding[2]) end
        end
    end
    self.victory = self.victory or b.phase == "victory"
    self.defeat = self.defeat or b.phase == "defeat"
    -- Publish after input/feedback; physics poses observe the preceding completed tick.
    b.refresh()
end
return Script
