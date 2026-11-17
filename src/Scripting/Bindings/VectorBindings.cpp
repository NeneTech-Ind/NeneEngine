#include "Scripting/Bindings/VectorBindings.h"

#include <cmath>
#include <glm/glm.hpp>
#include <limits>
#include <sol/sol.hpp>
#include <stdexcept>

namespace NeneEngine
{

	namespace
	{
		glm::vec3 ToVector(const glm::dvec3& value)
		{
			for (int axis = 0; axis < 3; ++axis)
				if (!std::isfinite(value[axis]) || std::abs(value[axis]) > (std::numeric_limits<float>::max)())
					throw std::invalid_argument("vec3 components must be finite float values");
			return glm::vec3(value);
		}

		double Length(const glm::vec3& value)
		{
			const double length = glm::length(glm::dvec3(value));
			if (!std::isfinite(length)) throw std::invalid_argument("vec3 components must be finite");
			return length;
		}

		auto Coordinate(int axis)
		{
			return sol::property([axis](const glm::vec3& value) { return value[axis]; },
			                     [axis](glm::vec3& value, const sol::object& component)
			                     {
				                     if (component.get_type() != sol::type::number)
					                     throw std::invalid_argument("vec3 components must be numbers");
				                     auto updated = glm::dvec3(value);
				                     updated[axis] = component.as<double>();
				                     value = ToVector(updated);
			                     });
		}
	}

	void BindVectorApi(sol::table& nene)
	{
		auto vector = nene.new_usertype<glm::vec3>("vec3", sol::call_constructor,
		                                           sol::factories([]() { return glm::vec3(0.0f); },
		                                                          [](double x, double y, double z)
		                                                          { return ToVector({x, y, z}); }),
		                                           "x", Coordinate(0), "y", Coordinate(1), "z", Coordinate(2));
		vector.set_function("length", sol::protect(&Length));
		vector.set_function("normalized", sol::protect(
		                                      [](const glm::vec3& value)
		                                      {
			                                      const double length = Length(value);
			                                      return length == 0.0 ? glm::vec3(0.0f)
			                                                           : ToVector(glm::dvec3(value) / length);
		                                      }));
		vector.set_function("dot", sol::protect(
		                               [](const glm::vec3& lhs, const glm::vec3& rhs)
		                               {
			                               const double result = glm::dot(glm::dvec3(lhs), glm::dvec3(rhs));
			                               if (!std::isfinite(result))
				                               throw std::invalid_argument("vec3 components must be finite");
			                               return result;
		                               }));
		vector[sol::meta_function::addition] = sol::protect([](const glm::vec3& lhs, const glm::vec3& rhs)
		                                                    { return ToVector(glm::dvec3(lhs) + glm::dvec3(rhs)); });
		vector[sol::meta_function::subtraction] = sol::protect([](const glm::vec3& lhs, const glm::vec3& rhs)
		                                                       { return ToVector(glm::dvec3(lhs) - glm::dvec3(rhs)); });
		vector[sol::meta_function::unary_minus] =
		    sol::protect([](const glm::vec3& value) { return ToVector(-glm::dvec3(value)); });
		vector[sol::meta_function::multiplication] = sol::protect(
		    sol::overload([](const glm::vec3& value, double scalar) { return ToVector(glm::dvec3(value) * scalar); },
		                  [](double scalar, const glm::vec3& value) { return ToVector(scalar * glm::dvec3(value)); }));
		vector[sol::meta_function::division] = sol::protect(
		    [](const glm::vec3& value, double scalar)
		    {
			    if (scalar == 0.0) throw std::invalid_argument("Cannot divide vec3 by zero");
			    return ToVector(glm::dvec3(value) / scalar);
		    });
		vector[sol::meta_function::equal_to] =
		    sol::protect([](const glm::vec3& lhs, const glm::vec3& rhs) { return lhs == rhs; });
	}

}
