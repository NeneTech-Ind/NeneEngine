local Duel = {}

function Duel.new(hero, guardian)
	return {
		hero_hp = hero.max_hp,
		hero_mana = hero.max_mana,
		guardian_hp = guardian.max_hp,
		guardian_charging = false,
		outcome = "ongoing",
		hero = hero,
		guardian = guardian,
	}
end

function Duel.take_turn(state, action)
	if state.outcome ~= "ongoing" then
		return false, "duel_over"
	end

	local damage
	if action == "attack" then
		damage = state.hero.attack_damage
	elseif action == "fire" then
		local mana_cost = state.hero.fire_mana_cost
		if state.hero_mana < mana_cost then
			return false, "insufficient_mana"
		end
		state.hero_mana = state.hero_mana - mana_cost
		damage = state.hero.fire_damage
	else
		return false, "unknown_action"
	end

	state.guardian_hp = math.max(0, state.guardian_hp - damage)
	if state.guardian_hp == 0 then
		state.guardian_charging = false
		state.outcome = "victory"
		return true, state.outcome
	end

	local response
	if state.guardian_charging then
		state.guardian_charging = false
		if action == "fire" then
			return true, "interrupted"
		end
		state.hero_hp = math.max(0, state.hero_hp - state.guardian.charged_damage)
		response = "charged_attack"
	else
		state.guardian_charging = true
		state.hero_hp = math.max(0, state.hero_hp - state.guardian.attack_damage)
		response = "charging"
	end

	if state.hero_hp == 0 then
		state.guardian_charging = false
		state.outcome = "defeat"
		return true, state.outcome
	end
	return true, response
end

return Duel
