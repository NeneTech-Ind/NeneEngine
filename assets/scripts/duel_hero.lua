local Fighter = nene.Behaviour:extend()

Fighter.fields = {
	max_hp = 30,
	max_mana = 6,
	attack_damage = 5,
	fire_damage = 9,
	fire_mana_cost = 3,
}

return Fighter
