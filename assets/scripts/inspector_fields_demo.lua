-- A scene fixture for exercising the script field inspector.
local Demo = nene.Behaviour and nene.Behaviour:extend() or {}

Demo.fields = {
	enabled = true,
	count = 3,
	speed = 4.0,
	label = "example",
	offset = nene.vec3(1.0, 2.0, 3.0),
	mode = "walk",
}

Demo.field_metadata = {
	enabled = { type = "bool" },
	count = { type = "int", min = 0, max = 10 },
	speed = { type = "float", min = 0, max = 10 },
	label = { type = "string" },
	offset = { type = "vec3" },
	mode = { type = "enum", options = { "walk", "run", "jump" } },
}

return Demo
