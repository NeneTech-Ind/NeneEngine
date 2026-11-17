local rules = require("duel_rules")
local fighters = require("duel_fighters")
local Controller = nene.Behaviour:extend()

local responses = {
	charging = "The guardian strikes and prepares a heavy attack.",
	charged_attack = "The guardian lands its heavy attack.",
	interrupted = "Fire interrupts the guardian's heavy attack.",
	victory = "Victory!",
	defeat = "Defeat!",
}

local function log_status(state)
	nene.log.info(("Hero HP: %d | Mana: %d | Guardian HP: %d")
		:format(state.hero_hp, state.hero_mana, state.guardian_hp))
end

function Controller:on_start()
	self.hero = nene.world.spawn("duel_hero", nene.vec3(-2, 0, 0))
	self.guardian = nene.world.spawn("ice_guardian", nene.vec3(2, 0, 0))
	if not self.hero or not self.guardian then
		nene.log.error("Could not spawn both duel fighters.")
	end
end

function Controller:on_update()
	if not self.hero or not self.hero:is_valid() or not self.guardian or not self.guardian:is_valid() then
		return
	end

	if not self.state then
		if not fighters.hero or not fighters.guardian then
			return
		end
		self.state = rules.new(fighters.hero, fighters.guardian)
		nene.log.info("Duel started. J: attack. K: fire.")
		log_status(self.state)
	end
	if self.state.outcome ~= "ongoing" then
		return
	end

	local action
	if nene.input.is_action_pressed("DuelFire") then
		action = "fire"
	elseif nene.input.is_action_pressed("DuelAttack") then
		action = "attack"
	else
		return
	end

	local accepted, result = rules.take_turn(self.state, action)
	if not accepted then
		nene.log.warn("Insufficient mana to cast fire.")
		return
	end
	nene.log.info(responses[result])
	log_status(self.state)
end

function Controller:on_destroy()
	if self.hero and self.hero:is_valid() then
		nene.world.destroy(self.hero)
	end
	if self.guardian and self.guardian:is_valid() then
		nene.world.destroy(self.guardian)
	end
end

return Controller
