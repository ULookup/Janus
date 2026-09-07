-- Fixed 10-05b rules live entirely in this game project. Each Runtime has its own Lua VM.
local Script = {}

local function reset()
    JanusCombat.phase = "menu"
    JanusCombat.enemyHp = 12
    JanusCombat.playerHp = 6
    JanusCombat.turn = 0
    JanusCombat.lastDamage = 0
    JanusCombat.lastRetaliation = 0
    JanusCombat.selectedCard = "none"
end

local function refresh()
    local b = JanusCombat
    local hints = {
        menu = "START A BATTLE - SELECT A CARD - PLAY YOUR CARD",
        battle = "SELECT STRIKE OR WAIT, THEN PLAY YOUR CARD",
        victory = "VICTORY! THREE STRIKES. RESTART TO PLAY AGAIN",
        defeat = "DEFEAT. WAITING LETS THE ENEMY ATTACK. RESTART"
    }
    b.status:set_text(string.format(
        "%s\nENEMY HP %d / 12     YOUR HP %d / 6     TURN %d\nSELECTED %s     DAMAGE %d     RETURN DAMAGE %d",
        hints[b.phase], b.enemyHp, b.playerHp, b.turn,
        string.upper(b.selectedCard), b.lastDamage, b.lastRetaliation))
    Diagnostics.publish_snapshot({
        phase = b.phase, enemyHp = b.enemyHp, playerHp = b.playerHp,
        turn = b.turn, lastDamage = b.lastDamage, lastRetaliation = b.lastRetaliation,
        selectedCard = b.selectedCard,
        canPlay = b.phase == "battle" and b.selectedCard ~= "none"
    })
end

-- Both Button.OnClick and the neutral-step verification scene use this one rule entry.
local function act(action)
    local b = JanusCombat
    if action == "Restart" then
        reset()
    elseif action == "Start" and b.phase == "menu" then
        b.phase = "battle"
    elseif b.phase == "battle" then
        if action == "Strike" then
            b.selectedCard = "strike"
        elseif action == "Wait" then
            b.selectedCard = "wait"
        elseif action == "Play" and b.selectedCard ~= "none" then
            b.lastDamage = b.selectedCard == "strike" and 4 or 0
            b.enemyHp = math.max(0, b.enemyHp - b.lastDamage)
            b.turn = b.turn + 1
            b.selectedCard = "none"
            b.lastRetaliation = b.enemyHp > 0 and 2 or 0
            b.playerHp = math.max(0, b.playerHp - b.lastRetaliation)
            if b.enemyHp == 0 then b.phase = "victory"
            elseif b.playerHp == 0 then b.phase = "defeat" end
        end
    end
    refresh()
end

function Script.OnCreate(self)
    local name = self.entity:name()
    -- The controller has the smallest scripted UUID; startup order is explicit in the scene.
    if name == "Status" or name == "Verification" then
        JanusCombat = {status = self.entity}
        reset()
        self.verificationStep = 0
        refresh()
    end
end

function Script.OnClick(self)
    act(self.entity:name())
end

function Script.OnUpdate(self)
    local name = self.entity:name()
    if name == "Verification" then
        local sequence = {"Start", "Strike", "Play", "Strike", "Play", "Strike", "Play"}
        self.verificationStep = self.verificationStep + 1
        local action = sequence[self.verificationStep]
        if action then act(action) end
    elseif name == "Status" then
        refresh()
    end
end

return Script
