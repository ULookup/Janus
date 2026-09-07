-- Fixed 10-05b rules live entirely in this game project. Each Runtime has its own Lua VM.
local Script = {}

local function reset()
    if JanusCombat.play then
        JanusCombat.play:stop_animation()
        JanusCombat.play:stop_audio()
    end
    if JanusCombat.music then JanusCombat.music:play_audio() end
    JanusCombat.phase = "menu"
    JanusCombat.enemyHp = 12
    JanusCombat.playerHp = 6
    JanusCombat.turn = 0
    JanusCombat.lastDamage = 0
    JanusCombat.lastRetaliation = 0
    JanusCombat.selectedCard = "none"
    if JanusCombat.resetFeedback then JanusCombat.resetFeedback() end
end

local function refresh()
    local b = JanusCombat
    local cardStatus, cardCursor = "Stopped", 0
    if b.play then cardStatus, cardCursor = b.play:audio_state() end
    local musicStatus, musicCursor = b.status:audio_state()
    local audioAvailable, audioError = b.status:audio_output()
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
    local fields = {
        phase = b.phase, enemyHp = b.enemyHp, playerHp = b.playerHp,
        turn = b.turn, lastDamage = b.lastDamage, lastRetaliation = b.lastRetaliation,
        selectedCard = b.selectedCard,
        canPlay = b.phase == "battle" and b.selectedCard ~= "none",
        cardAudioStatus = cardStatus, cardAudioCursor = cardCursor,
        musicAudioStatus = musicStatus, musicAudioCursor = musicCursor,
        audioAvailable = audioAvailable, audioError = audioError
    }
    if b.decorateSnapshot then b.decorateSnapshot(fields) end
    Diagnostics.publish_snapshot(fields)
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
            b.play:play_animation()
            b.play:play_audio()
            if b.enemyHp == 0 then b.phase = "victory"
            elseif b.playerHp == 0 then b.phase = "defeat" end
            if b.feedback then b.feedback() end
        end
    end
    refresh()
end

function Script.OnCreate(self)
    local name = self.entity:name()
    -- The controller has the smallest scripted UUID; startup order is explicit in the scene.
    if name == "Status" or name == "Verification" then
        -- Arena may still hold this table when only Combat.lua is hot reloaded.
        JanusCombat = JanusCombat or {}
        JanusCombat.status, JanusCombat.music = self.entity, self.entity
        JanusCombat.act, JanusCombat.refresh = act, refresh
        reset()
        self.verificationStep = 0
        refresh()
    elseif name == "Play" then
        JanusCombat.play = self.entity
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
