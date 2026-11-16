#pragma once

#include "Scripting/ScriptFieldSchema.h"

#include <map>
#include <string>

namespace NeneEngine::ECS
{

	using ScriptFieldValue = NeneEngine::ScriptFieldValue;

	struct ScriptComponent
	{
		std::string scriptPath;
		std::map<std::string, ScriptFieldValue> fields;
	};

}
