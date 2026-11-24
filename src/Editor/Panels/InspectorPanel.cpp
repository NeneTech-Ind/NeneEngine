// InspectorPanel.cpp

#include "Editor/Panels/InspectorPanel.h"

#include "Core/NeneLogger.h"
#include "Core/PathResolver.h"
#include "ECS/Components/CameraComponent.h"
#include "ECS/Components/CameraControllerComponent.h"
#include "ECS/Components/ColliderComponent.h"
#include "ECS/Components/ContactStateComponent.h"
#include "ECS/Components/HierarchyComponent.h"
#include "ECS/Components/MeshRendererComponent.h"
#include "ECS/Components/MovementComponent.h"
#include "ECS/Components/PlayerControllerComponent.h"
#include "ECS/Components/PrimitiveControlComponent.h"
#include "ECS/Components/RigidbodyComponent.h"
#include "ECS/Components/ScriptComponent.h"
#include "ECS/Components/TagComponent.h"
#include "ECS/Components/TransformComponent.h"
#include "ECS/World.h"
#include "Editor/Commands/ComponentPresenceCommand.h"
#include "Editor/EditorContext.h"
#include "Editor/EditorHistory.h"
#include "Editor/Theme/EditorIcons.h"
#include "Editor/Theme/EditorThemeConfig.h"
#include "Editor/Theme/EditorWidgets.h"
#include "Graphics/Backend/IRenderAdapter.h"
#include "Scene/Prefab/PrefabLibrary.h"
#include "Scripting/ScriptEngine.h"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cwctype>
#include <filesystem>
#include <glm/gtc/type_ptr.hpp>
#include <imgui.h>
#include <limits>
#include <misc/cpp/imgui_stdlib.h>
#include <stdexcept>
#include <string>
#include <type_traits>

namespace NeneEngine
{
	namespace
	{
		constexpr const char* kTexturedMeshShaderPath = "assets/engine/shaders/textured_mesh.shader";
		using FieldEdits = InspectorPanel::FieldEdits;

		constexpr float kPositionDragSpeed = 0.05f;
		constexpr float kRotationDragSpeed = 0.5f;
		constexpr float kScaleDragSpeed = 0.01f;
		constexpr float kMinScale = 0.001f;
		constexpr float kMaxScale = 1000.0f;
		constexpr float kMinMass = 0.001f;
		constexpr float kMinColliderSize = 0.001f;

		constexpr const char* kPrimitiveTypeNames[] = {"Line", "Triangle", "Quad", "Cube"};
		constexpr const char* kColliderTypeNames[] = {"Box", "Sphere"};
		constexpr float kMaterialPreviewMaxWidth = 128.0f;
		constexpr const char* kAddComponentPopup = "##AddComponent";

		template <typename Component> constexpr bool kRemovable = false;
		template <> constexpr bool kRemovable<ECS::MeshRendererComponent> = true;
		template <> constexpr bool kRemovable<ECS::ColliderComponent> = true;
		template <> constexpr bool kRemovable<ECS::RigidbodyComponent> = true;
		template <> constexpr bool kRemovable<ECS::MovementComponent> = true;
		template <> constexpr bool kRemovable<ECS::PlayerControllerComponent> = true;
		template <> constexpr bool kRemovable<ECS::PrimitiveControlComponent> = true;

		void TrackItem(FieldEdits& edits)
		{
			edits.activated |= ImGui::IsItemActivated();
			edits.active |= ImGui::IsItemActive();
			edits.deactivatedAfterEdit |= ImGui::IsItemDeactivatedAfterEdit();
		}

		template <typename DrawWidget>
		bool DrawValueField(const char* label, FieldEdits& edits, DrawWidget&& drawWidget)
		{
			if (edits.valueFont == nullptr)
			{
				const bool changed = drawWidget(label);
				TrackItem(edits);
				return changed;
			}

			const std::string widgetId = std::string("##") + label;
			ImGui::PushFont(edits.valueFont, 0.0f);
			const bool changed = drawWidget(widgetId.c_str());
			ImGui::PopFont();
			TrackItem(edits);

			ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
			ImGui::TextUnformatted(label);
			return changed;
		}

		bool DrawAxisField(const char* label, float* values, float speed, float min, float max, ImGuiSliderFlags flags,
		                   FieldEdits& edits)
		{
			const ImVec4 axisColors[3] = {edits.colors->axisX, edits.colors->axisY, edits.colors->axisZ};
			const AxisDragResult result = DragFloat3Axes(label, values, speed, min, max, flags, axisColors, edits.valueFont);
			edits.activated |= result.activated;
			edits.active |= result.active;
			edits.deactivatedAfterEdit |= result.deactivatedAfterEdit;
			return result.changed;
		}

		bool DragVec3(const char* label, glm::vec3& value, float speed, FieldEdits& edits, float min = 0.0f,
		              float max = 0.0f)
		{
			const ImGuiSliderFlags flags = min < max ? ImGuiSliderFlags_AlwaysClamp : ImGuiSliderFlags_None;
			if (edits.colors != nullptr)
				return DrawAxisField(label, glm::value_ptr(value), speed, min, max, flags, edits);

			return DrawValueField(
			    label, edits, [&](const char* widgetId)
			    { return ImGui::DragFloat3(widgetId, glm::value_ptr(value), speed, min, max, "%.3f", flags); });
		}

		bool DragFloat(const char* label, float& value, float speed, FieldEdits& edits, float min = 0.0f,
		               float max = 0.0f)
		{
			return DrawValueField(label, edits,
			                      [&](const char* widgetId)
			                      {
				                      return ImGui::DragFloat(widgetId, &value, speed, min, max, "%.3f",
				                                              min < max ? ImGuiSliderFlags_AlwaysClamp
				                                                        : ImGuiSliderFlags_None);
			                      });
		}

		bool Checkbox(const char* label, bool& value, FieldEdits& edits)
		{
			const bool changed = ImGui::Checkbox(label, &value);
			TrackItem(edits);
			return changed;
		}

		template <typename Number>
		bool InputScriptNumber(const char* id, Number& value)
		{
			char buffer[64];
			const auto formatted = std::to_chars(buffer, buffer + sizeof(buffer), value);
			std::string text(buffer, formatted.ptr);
			const bool submitted = ImGui::InputText(id, &text, ImGuiInputTextFlags_EnterReturnsTrue);
			const bool finished = submitted || ImGui::IsItemDeactivatedAfterEdit();
			if (!finished && !ImGui::IsItemActive()) return false;

			const char* first = text.data();
			const char* end = first + text.size();
			if (first != end && *first == '+' && first + 1 != end && first[1] != '-') ++first;
			Number parsed{};
			const auto result = std::from_chars(first, end, parsed);
			if (result.ec != std::errc{} || result.ptr != end || !std::isfinite(parsed))
			{
				ImGui::SetItemTooltip("Enter a valid number");
				return false;
			}
			if (!finished || parsed == value) return false;
			value = parsed;
			return true;
		}

		template <typename Number>
		bool DrawRangedScriptField(const char* label, Number& value, Number min, Number max, FieldEdits& edits)
		{
			// ImGui sliders require bounds within half the numeric type's range.
			const bool useSlider = min >= std::numeric_limits<Number>::lowest() / 2 &&
			                       max <= (std::numeric_limits<Number>::max)() / 2;
			return DrawValueField(label, edits, [&](const char* id)
			{
				if (useSlider)
				{
					const float width = ImGui::CalcItemWidth();
					const float spacing = ImGui::GetStyle().ItemInnerSpacing.x;
					const float inputWidth = (std::max)(1.0f, (width - spacing) * 0.35f);
					ImGui::PushID(id);
					ImGui::SetNextItemWidth((std::max)(1.0f, width - spacing - inputWidth));
					bool changed;
					if constexpr (std::is_same_v<Number, int>)
						changed = ImGui::SliderInt("##slider", &value, min, max);
					else
						changed = ImGui::SliderFloat("##slider", &value, min, max, "%.3f");
					TrackItem(edits);
					ImGui::SameLine(0.0f, spacing);
					ImGui::SetNextItemWidth(inputWidth);
					auto typedValue = value;
					const bool typed = InputScriptNumber("##value", typedValue);
					if (typed && !changed) value = typedValue;
					TrackItem(edits);
					ImGui::PopID();
					if (edits.valueFont == nullptr)
					{
						ImGui::SameLine(0.0f, spacing);
						ImGui::TextUnformatted(label);
					}
					return changed || typed;
				}
				return InputScriptNumber(id, value);
			});
		}

		bool DrawScriptField(const ScriptFieldDefinition& field, ScriptFieldValue& value, FieldEdits& edits)
		{
			const char* label = field.name.c_str();
			if (!field.options.empty())
			{
				auto& current = std::get<std::string>(value);
				bool changed = false;
				if (ImGui::BeginCombo(label, current.c_str()))
				{
					for (const auto& option : field.options)
						if (ImGui::Selectable(option.c_str(), option == current))
						{
							current = option;
							changed = true;
						}
					ImGui::EndCombo();
				}
				return changed;
			}
			return std::visit(
			    [&](auto& current)
			    {
				    using Value = std::decay_t<decltype(current)>;
				    if constexpr (std::is_same_v<Value, bool>)
					    return Checkbox(label, current, edits);
				    else if constexpr (std::is_same_v<Value, int>)
					    return field.minimum ? DrawRangedScriptField(label, current, std::get<int>(*field.minimum),
					                                                std::get<int>(*field.maximum), edits)
					                         : DrawValueField(label, edits, [&](const char* id)
					                           { return ImGui::DragInt(id, &current); });
				    else if constexpr (std::is_same_v<Value, float>)
					    return field.minimum ? DrawRangedScriptField(label, current, std::get<float>(*field.minimum),
					                                                std::get<float>(*field.maximum), edits)
					                         : DragFloat(label, current, 0.05f, edits);
				    else if constexpr (std::is_same_v<Value, std::string>)
					    return DrawValueField(label, edits,
					                          [&](const char* id) { return ImGui::InputText(id, &current); });
				    else if constexpr (std::is_same_v<Value, glm::vec3>)
					    return DragVec3(label, current, kPositionDragSpeed, edits);
			    },
			    value);
		}

		template <typename Enum, size_t Count>
		bool EnumCombo(const char* label, Enum& value, const char* const (&names)[Count], FieldEdits& edits)
		{
			int index = static_cast<int>(value);
			if (!ImGui::Combo(label, &index, names, static_cast<int>(Count))) return false;

			value = static_cast<Enum>(index);
			edits.changedInstantly = true;
			return true;
		}

		void ReadOnlyVec3(const char* label, const glm::vec3& value, FieldEdits& edits)
		{
			glm::vec3 copy = value;
			ImGui::BeginDisabled();
			if (edits.colors != nullptr)
				DrawAxisField(label, glm::value_ptr(copy), 1.0f, 0.0f, 0.0f, ImGuiSliderFlags_None, edits);
			else
				DrawValueField(label, edits,
				               [&](const char* widgetId) { return ImGui::DragFloat3(widgetId, glm::value_ptr(copy)); });
			ImGui::EndDisabled();
		}

		void DrawReadOnlyValue(ImFont* font, const char* label, const char* value)
		{
			if (font != nullptr) ImGui::PushFont(font, 0.0f);
			ImGui::TextUnformatted(value);
			if (font != nullptr) ImGui::PopFont();
			ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
			ImGui::TextUnformatted(label);
		}

		bool HasMeshChild(const entt::registry& registry, ECS::Entity entity)
		{
			const auto* hierarchy = registry.try_get<ECS::HierarchyComponent>(entity);
			return hierarchy != nullptr &&
			       std::any_of(hierarchy->children.begin(), hierarchy->children.end(), [&registry](ECS::Entity child)
			                   { return registry.valid(child) && registry.all_of<ECS::MeshRendererComponent>(child); });
		}

		const char* GetEntityName(const entt::registry& registry, ECS::Entity entity)
		{
			const auto* tag = registry.try_get<ECS::TagComponent>(entity);
			return tag != nullptr && !tag->name.empty() ? tag->name.c_str() : "Entity";
		}

		std::string GetEntityLabel(const entt::registry& registry, ECS::Entity entity)
		{
			const std::string name = GetEntityName(registry, entity);
			return name + " (" + std::to_string(entt::to_integral(entity)) + ")";
		}
	} // namespace

	template <typename Component, typename DrawFields>
	void InspectorPanel::DrawComponent(EditorContext& context, const char* icon, const char* label,
	                                   DrawFields&& drawFields)
	{
		const ECS::Entity entity = context.selectedEntity;
		Component* component = context.world.GetComponent<Component>(entity);
		if (component == nullptr) return;

		const bool isOpen = ComponentHeader(icon, label);
		if constexpr (kRemovable<Component>) DrawRemoveMenu<Component>(context, label);
		if (!isOpen) return;

		const bool isEditMode = context.CanEdit();
		const Component before = *component;
		FieldEdits edits;
		edits.valueFont = context.monospaceFont;
		edits.colors = context.themeColors;
		{
			ComponentCardScope card;
			ImGui::PushID(label);
			ImGui::BeginDisabled(!isEditMode);
			drawFields(*component, edits);
			ImGui::EndDisabled();
			ImGui::PopID();
		}

		if (!isEditMode) return;

		const std::string commandName = std::string("Edit ") + label;
		if (edits.changedInstantly) context.history.RecordComponentChange(commandName, entity, before, *component);

		if (edits.activated)
		{
			m_pendingEditLabel = label;
			m_commitPendingEdit = [entity, before, commandName](EditorContext& editorContext)
			{
				if (const Component* current = editorContext.world.GetComponent<Component>(entity))
					editorContext.history.RecordComponentChange(commandName, entity, before, *current);
			};
		}

		if (edits.deactivatedAfterEdit && m_pendingEditLabel == label && m_commitPendingEdit)
		{
			m_commitPendingEdit(context);
			m_commitPendingEdit = nullptr;
			m_pendingEditLabel = nullptr;
		}
	}

	template <typename Component> void InspectorPanel::DrawRemoveMenu(EditorContext& context, const char* label)
	{
		if (!ImGui::BeginPopupContextItem()) return;

		using Command = ComponentPresenceCommand<Component>;
		const std::string itemLabel = std::string(NENE_ICON_CLOSE " Remove ") + label;
		if (ImGui::MenuItem(itemLabel.c_str(), nullptr, false, context.CanEdit()))
			m_pendingCommand = eastl::make_unique<Command>(std::string("Remove ") + label, context.selectedEntity,
			                                               Command::Action::Remove, Component{});
		ImGui::EndPopup();
	}

	template <typename Component>
	bool InspectorPanel::DrawAddItem(EditorContext& context, const char* icon, const char* label)
	{
		if (context.world.HasComponent<Component>(context.selectedEntity)) return false;

		using Command = ComponentPresenceCommand<Component>;
		const std::string itemLabel = std::string(icon) + " " + label;
		if (ImGui::Selectable(itemLabel.c_str()))
			m_pendingCommand = eastl::make_unique<Command>(std::string("Add ") + label, context.selectedEntity,
			                                               Command::Action::Add, Component{});
		return true;
	}

	void InspectorPanel::DrawAddComponent(EditorContext& context)
	{
		ImGui::Spacing();
		ImGui::BeginDisabled(!context.CanEdit());
		if (PillButton("Add Component", ImVec2(-FLT_MIN, 0.0f))) ImGui::OpenPopup(kAddComponentPopup);
		ImGui::EndDisabled();
		ImGui::SetItemTooltip("Right-click a component header to remove it");

		if (!ImGui::BeginPopup(kAddComponentPopup)) return;
		if (!context.CanEdit())
		{
			ImGui::CloseCurrentPopup();
			ImGui::EndPopup();
			return;
		}

		bool hasChoices = false;
		hasChoices |= DrawAddItem<ECS::TagComponent>(context, NENE_ICON_TAG, "Tag");
		hasChoices |= DrawAddItem<ECS::TransformComponent>(context, NENE_ICON_TRANSFORM, "Transform");
		hasChoices |= DrawAddItem<ECS::MeshRendererComponent>(context, NENE_ICON_MESH, "Mesh Renderer");
		hasChoices |= DrawAddItem<ECS::ColliderComponent>(context, NENE_ICON_COLLIDER, "Collider");
		hasChoices |= DrawAddItem<ECS::RigidbodyComponent>(context, NENE_ICON_RIGIDBODY, "Rigidbody");
		hasChoices |= DrawAddItem<ECS::MovementComponent>(context, NENE_ICON_MOVEMENT, "Movement");
		hasChoices |= DrawAddItem<ECS::PlayerControllerComponent>(context, NENE_ICON_CONTROLLER, "Player Controller");
		hasChoices |= DrawAddItem<ECS::PrimitiveControlComponent>(context, NENE_ICON_CONTROLLER, "Primitive Control");
		hasChoices |= DrawAddScript(context);
		if (!hasChoices) ImGui::TextDisabled("Every component is already added");
		ImGui::EndPopup();
	}

	bool InspectorPanel::DrawAddScript(EditorContext& context)
	{
		if (context.world.HasComponent<ECS::ScriptComponent>(context.selectedEntity)) return false;
		if (!ImGui::BeginMenu(NENE_ICON_FILE_CODE " Script")) return true;

		if (ImGui::IsWindowAppearing())
		{
			m_scriptChoices.clear();
			const auto root = ResolveFromExecutionRoots("assets/scripts");
			if (!root.empty())
			{
				std::error_code error;
				for (std::filesystem::recursive_directory_iterator
				         it(root, std::filesystem::directory_options::skip_permission_denied, error),
				     end;
				     !error && it != end; it.increment(error))
				{
					const auto& path = it->path();
					if (it.depth() == 0 && (path.filename() == "modules" || path.filename() == "meta"))
					{
						it.disable_recursion_pending();
						continue;
					}
					std::error_code entryError;
					if (!it->is_regular_file(entryError)) continue;
					auto extension = path.extension().wstring();
					std::ranges::transform(extension, extension.begin(), std::towlower);
					if (extension == L".lua") m_scriptChoices.push_back(path.lexically_relative(root).generic_string());
				}
			}
			std::sort(m_scriptChoices.begin(), m_scriptChoices.end());
		}

		if (m_scriptChoices.empty()) ImGui::TextDisabled("No attachable Lua scripts found in assets/scripts");
		for (const auto& path : m_scriptChoices)
		{
			if (!ImGui::MenuItem(path.c_str())) continue;
			using Command = ComponentPresenceCommand<ECS::ScriptComponent>;
			m_pendingCommand = eastl::make_unique<Command>("Add Script", context.selectedEntity, Command::Action::Add,
			                                               ECS::ScriptComponent{path, {}});
			break;
		}
		ImGui::EndMenu();
		return true;
	}

	void InspectorPanel::Draw(EditorContext& context)
	{
		if (context.selectedEntity != ECS::NullEntity && !context.world.GetRegistry().valid(context.selectedEntity))
			context.selectedEntity = ECS::NullEntity;

		if (context.selectedEntity == ECS::NullEntity)
		{
			FinishScriptFieldEdit(context);
			m_commitPendingEdit = nullptr;
			m_pendingEditLabel = nullptr;
			m_colorEntity = ECS::NullEntity;
			m_materialError.clear();
			DrawEmptyState(context.GetMascot(EditorMascotPose::Search), "Select an entity to inspect it");
			return;
		}

		DrawEntityHeader(context);
		ImGui::Separator();
		DrawComponents(context);
		if (!DrawScript(context)) FinishScriptFieldEdit(context);
		DrawAddComponent(context);

		if (m_pendingCommand && context.CanEdit()) context.history.Execute(std::move(m_pendingCommand), context.world);
		m_pendingCommand.reset();
	}

	bool InspectorPanel::DrawScript(EditorContext& context)
	{
		const auto* script = context.world.GetComponent<ECS::ScriptComponent>(context.selectedEntity);
		if (script == nullptr || !ComponentHeader(NENE_ICON_FILE_CODE, "Script")) return false;

		ComponentCardScope card;
		if (script->scriptPath.empty())
		{
			ImGui::TextDisabled("No script assigned");
			return false;
		}

		ImGui::TextWrapped("Path: %s", script->scriptPath.c_str());
		const std::filesystem::path path(script->scriptPath);
		const auto* schema = m_scriptEngine.GetFieldSchema(path);
		if (schema == nullptr && m_scriptEngine.LoadScript(path) != nullptr)
			schema = m_scriptEngine.GetFieldSchema(path);

		if (schema == nullptr)
		{
			ImGui::TextWrapped("Could not load script. See the Console for details.");
			if (context.CanEdit() && ImGui::Button("Retry##ScriptLoad")) m_scriptEngine.ReloadScript(path);
			return false;
		}

		const auto revision = m_scriptEngine.GetScriptRevision(path);
		if (m_scriptFieldEdit && m_scriptFieldEdit->path == script->scriptPath &&
		    m_scriptFieldEdit->scriptRevision != revision)
			FinishScriptFieldEdit(context);

		if (schema->empty())
		{
			ImGui::TextDisabled("This script exposes no fields");
			return false;
		}

		const bool canEdit = context.CanEdit();
		bool editActive = false;
		ImGui::PushID("Script");
		ImGui::PushID(static_cast<int>(entt::to_integral(context.selectedEntity)));
		ImGui::PushID(script->scriptPath.c_str());
		ImGui::PushID(std::to_string(revision).c_str());
		ImGui::BeginDisabled(!canEdit);
		for (const auto& field : *schema)
		{
			const auto override = script->fields.find(field.name);
			const auto& saved = override != script->fields.end() ? override->second : field.defaultValue;
			const bool editing = canEdit && m_scriptFieldEdit && m_scriptFieldEdit->entity == context.selectedEntity &&
			                     m_scriptFieldEdit->path == script->scriptPath && m_scriptFieldEdit->field == field.name;
			auto value = editing ? m_scriptFieldEdit->value : saved;
			if (value.index() != field.defaultValue.index()) value = field.defaultValue;
			const bool invalid = !IsValidScriptFieldValue(field, editing ? m_scriptFieldEdit->value : saved);
			FieldEdits edits{};
			edits.valueFont = context.monospaceFont;
			edits.colors = context.themeColors;
			ImGui::PushID(field.name.c_str());
			if (invalid) ImGui::PushStyleColor(ImGuiCol_FrameBg, context.themeColors ? context.themeColors->danger : ImVec4(0.8f, 0.1f, 0.1f, 1.0f));
			const bool changed = DrawScriptField(field, value, edits) && canEdit;
			if (invalid) ImGui::PopStyleColor();
			if (invalid) ImGui::TextColored(context.themeColors ? context.themeColors->danger : ImVec4(0.8f, 0.1f, 0.1f, 1.0f), "Invalid value");
			ImGui::PopID();

			if (changed)
			{
				if (editing)
					m_scriptFieldEdit->value = std::move(value);
				else
				{
					ScriptFieldEdit edit{context.selectedEntity, script->scriptPath, revision, field.name, saved,
					                     std::move(value)};
					FinishScriptFieldEdit(context);
					m_scriptFieldEdit = std::move(edit);
				}
			}
			if (editing || changed)
			{
				editActive = edits.active;
				if (!editActive) FinishScriptFieldEdit(context);
			}
		}
		ImGui::EndDisabled();
		ImGui::PopID();
		ImGui::PopID();
		DrawSaveToPrefab(context, *schema);
		ImGui::PopID();
		ImGui::PopID();
		return editActive;
	}

	void InspectorPanel::DrawSaveToPrefab(EditorContext& context, const ScriptFieldSchema& schema)
	{
		if (!context.prefabs) return;
		constexpr const char* popup = "Choose prefab";
		ImGui::BeginDisabled(!context.CanEdit());
		if (ImGui::Button("Save to prefab")) ImGui::OpenPopup(popup);
		ImGui::EndDisabled();
		if (!ImGui::BeginPopup(popup)) return;

		bool hasPrefabs = false;
		if (context.CanEdit())
		{
			for (const auto& asset : context.assets.Entries())
			{
				if (asset.kind != AssetKind::Prefab) continue;
				hasPrefabs = true;
				const auto name = GenericPathToUtf8(PathFromUtf8(asset.path).lexically_relative("assets/prefabs"));
				if (!ImGui::MenuItem(name.c_str())) continue;

				try
				{
					FinishScriptFieldEdit(context);
					if (m_scriptFieldEdit && m_scriptFieldEdit->entity == context.selectedEntity)
						throw std::runtime_error("Correct the invalid script field before saving");
					const auto& script = *context.world.GetComponent<ECS::ScriptComponent>(context.selectedEntity);
					ECS::ScriptComponent saved{script.scriptPath, {}};
					for (const auto& field : schema)
					{
						const auto override = script.fields.find(field.name);
						const auto& value = override == script.fields.end() ? field.defaultValue : override->second;
						if (!IsValidScriptFieldValue(field, value))
							throw std::runtime_error("Invalid script field: " + field.name);
						saved.fields.emplace(field.name, value);
					}
					context.prefabs->SaveScriptFields(name, saved);
					context.SetStatus(EditorStatusKind::Success, "Script fields saved to " + name);
				}
				catch (const std::exception& exception)
				{
					NENE_LOG_ERROR("Could not save script fields to prefab '{}': {}", name, exception.what());
					context.SetStatus(EditorStatusKind::Danger, exception.what());
				}
				break;
			}
		}
		else
		{
			ImGui::CloseCurrentPopup();
		}
		if (!hasPrefabs) ImGui::TextDisabled("No prefab assets found");
		ImGui::EndPopup();
	}

	void InspectorPanel::FinishScriptFieldEdit(EditorContext& context)
	{
		if (!m_scriptFieldEdit) return;
		auto& pending = *m_scriptFieldEdit;
		const auto* script = context.world.GetComponent<ECS::ScriptComponent>(pending.entity);
		if (!context.CanEdit() || pending.before == pending.value || script == nullptr ||
		    script->scriptPath != pending.path)
		{
			m_scriptFieldEdit.reset();
			return;
		}

		const auto* schema = m_scriptEngine.GetFieldSchema(pending.path);
		if (schema == nullptr)
		{
			m_scriptFieldEdit.reset();
			return;
		}
		const auto field =
		    std::find_if(schema->begin(), schema->end(), [&](const auto& item) { return item.name == pending.field; });
		if (field == schema->end() || !IsValidScriptFieldValue(*field, pending.value))
		{
			if (field == schema->end() || pending.scriptRevision != m_scriptEngine.GetScriptRevision(pending.path))
				m_scriptFieldEdit.reset();
			return;
		}

		auto after = *script;
		after.fields.insert_or_assign(pending.field, std::move(pending.value));
		auto command = eastl::make_unique<ComponentChangeCommand<ECS::ScriptComponent>>(
		    "Edit Script Field", pending.entity, *script, std::move(after));
		m_scriptFieldEdit.reset();
		context.history.Execute(std::move(command), context.world);
	}

	void InspectorPanel::DrawEntityHeader(EditorContext& context)
	{
		const auto& registry = context.world.GetRegistry();
		const ECS::Entity entity = context.selectedEntity;
		ImGui::TextWrapped("%s", GetEntityName(registry, entity));
		ImGui::AlignTextToFramePadding();
		ImGui::TextDisabled("ID: %u", static_cast<uint32_t>(entt::to_integral(entity)));

		if (!context.CanEdit()) DrawReadOnlyPill(*context.themeColors);

		if (const auto* hierarchy = registry.try_get<ECS::HierarchyComponent>(entity))
		{
			if (hierarchy->parent != ECS::NullEntity && registry.valid(hierarchy->parent))
				ImGui::TextDisabled("Parent: %s", GetEntityLabel(registry, hierarchy->parent).c_str());
			if (!hierarchy->children.empty()) ImGui::TextDisabled("Children: %zu", hierarchy->children.size());
		}
	}

	void InspectorPanel::DrawComponents(EditorContext& context)
	{
		const ECS::Entity entity = context.selectedEntity;

		DrawComponent<ECS::TagComponent>(context, NENE_ICON_TAG, "Tag",
		                                 [](ECS::TagComponent& tag, FieldEdits& edits)
		                                 {
			                                 ImGui::InputText("Name", &tag.name);
			                                 TrackItem(edits);
		                                 });

		DrawComponent<ECS::TransformComponent>(context, NENE_ICON_TRANSFORM, "Transform",
		                                       [this, entity](ECS::TransformComponent& transform, FieldEdits& edits)
		                                       { DrawTransformFields(entity, transform, edits); });

		DrawComponent<ECS::MeshRendererComponent>(context, NENE_ICON_MESH, "Mesh Renderer",
		                                          [](ECS::MeshRendererComponent& meshRenderer, FieldEdits& edits)
		                                          {
			                                          EnumCombo("Primitive", meshRenderer.primitiveType,
			                                                    kPrimitiveTypeNames, edits);
			                                          Checkbox("Visible", meshRenderer.visible, edits);
		                                          });

		DrawMaterial(context);

		DrawComponent<ECS::ColliderComponent>(
		    context, NENE_ICON_COLLIDER, "Collider",
		    [](ECS::ColliderComponent& collider, FieldEdits& edits)
		    {
			    EnumCombo("Type", collider.type, kColliderTypeNames, edits);
			    if (collider.type == ECS::ColliderType::Box)
				    DragVec3("Half Extents", collider.halfExtents, 0.01f, edits, kMinColliderSize, kMaxScale);
			    else
				    DragFloat("Radius", collider.radius, 0.01f, edits, kMinColliderSize, kMaxScale);
			    DragVec3("Offset", collider.offset, 0.01f, edits);
		    });

		DrawComponent<ECS::RigidbodyComponent>(context, NENE_ICON_RIGIDBODY, "Rigidbody",
		                                       [](ECS::RigidbodyComponent& rigidbody, FieldEdits& edits)
		                                       {
			                                       DragFloat("Mass", rigidbody.mass, 0.01f, edits, kMinMass, 10000.0f);
			                                       Checkbox("Use Gravity", rigidbody.useGravity, edits);
			                                       Checkbox("Freeze Rotation", rigidbody.freezeRotation, edits);
			                                       DragVec3("Acceleration", rigidbody.acceleration, 0.05f, edits);
			                                       ReadOnlyVec3("Velocity", rigidbody.velocity, edits);
		                                       });

		if (const auto* contactState = context.world.GetComponent<ECS::ContactStateComponent>(entity);
		    contactState != nullptr && ComponentHeader(NENE_ICON_SNAP, "Contact State"))
		{
			ComponentCardScope card;
			const std::string contactCount = std::to_string(contactState->contactCount);
			DrawReadOnlyValue(context.monospaceFont, "Contacts", contactCount.c_str());
			DrawReadOnlyValue(context.monospaceFont, "Grounded", contactState->isGrounded ? "yes" : "no");
		}

		DrawComponent<ECS::MovementComponent>(context, NENE_ICON_MOVEMENT, "Movement",
		                                      [](ECS::MovementComponent& movement, FieldEdits& edits)
		                                      {
			                                      DragVec3("Velocity", movement.velocity, 0.05f, edits);
			                                      Checkbox("Oscillate", movement.useOscillation, edits);
			                                      if (!movement.useOscillation) return;

			                                      DragVec3("Origin", movement.origin, 0.05f, edits);
			                                      DragVec3("Axis", movement.oscillationAxis, 0.01f, edits);
			                                      DragFloat("Amplitude", movement.oscillationAmplitude, 0.01f, edits);
			                                      DragFloat("Speed", movement.oscillationSpeed, 0.01f, edits);
		                                      });

		DrawComponent<ECS::PlayerControllerComponent>(
		    context, NENE_ICON_CONTROLLER, "Player Controller",
		    [](ECS::PlayerControllerComponent& controller, FieldEdits& edits)
		    {
			    DragFloat("Move Speed", controller.moveSpeed, 0.05f, edits, 0.0f, 100.0f);
			    DragFloat("Jump Speed", controller.jumpSpeed, 0.05f, edits, 0.0f, 100.0f);
		    });

		DrawComponent<ECS::PrimitiveControlComponent>(
		    context, NENE_ICON_CONTROLLER, "Primitive Control",
		    [](ECS::PrimitiveControlComponent& control, FieldEdits& edits)
		    {
			    DragFloat("Move Speed", control.moveSpeed, 0.05f, edits, 0.0f, 100.0f);
			    DragFloat("Scale Smoothing", control.scaleSmoothing, 0.1f, edits, 0.0f, 100.0f);
			    DragFloat("Rotation Smoothing", control.rotationSmoothing, 0.1f, edits, 0.0f, 100.0f);
		    });

		DrawComponent<ECS::CameraComponent>(
		    context, NENE_ICON_CAMERA, "Camera",
		    [](ECS::CameraComponent& camera, FieldEdits& edits)
		    {
			    DragFloat("FOV", camera.fovDegrees, 0.1f, edits, 1.0f, 179.0f);
			    DragFloat("Near", camera.nearPlane, 0.01f, edits, 0.001f, camera.farPlane);
			    DragFloat("Far", camera.farPlane, 1.0f, edits, camera.nearPlane, 100000.0f);
			    DrawReadOnlyValue(edits.valueFont, "Primary", camera.isPrimary ? "yes" : "no");
		    });

		DrawComponent<ECS::CameraControllerComponent>(
		    context, NENE_ICON_CONTROLLER, "Camera Controller",
		    [](ECS::CameraControllerComponent& controller, FieldEdits& edits)
		    {
			    DragFloat("Move Speed", controller.moveSpeed, 0.05f, edits, 0.0f, 100.0f);
			    DragFloat("Sprint Multiplier", controller.sprintMultiplier, 0.05f, edits, 1.0f, 20.0f);
			    DragFloat("Look Sensitivity", controller.lookSensitivity, 0.0001f, edits, 0.0001f, 0.1f);
		    });
	}

	void InspectorPanel::SetMaterialAsset(EditorContext& context, ECS::MeshRendererComponent& material,
	                                      const std::string& path, bool texture)
	{
		if (!path.empty())
		{
			if (texture)
				context.assetResolver.Texture(path, *context.renderer);
			else
				context.assetResolver.Shader(path, *context.renderer);
			const auto state = texture ? context.assetResolver.GetTextureState(path, *context.renderer)
			                           : context.assetResolver.GetShaderState(path, *context.renderer);
			if (state == ResourceLoadState::Failed)
			{
				m_materialError = texture ? "Could not load texture" : "Could not compile shader";
				return;
			}
		}
		const auto before = material;
		(texture ? material.texturePath : material.shaderPath) = path;
		if (texture && !path.empty() && material.shaderPath.empty())
			material.shaderPath = kTexturedMeshShaderPath;
		context.history.RecordComponentChange("Edit Material", context.selectedEntity, before, material);
		m_materialError.clear();
	}

	void InspectorPanel::DrawMaterialAsset(EditorContext& context, ECS::MeshRendererComponent& material, bool texture)
	{
		const auto& path = texture ? material.texturePath : material.shaderPath;
		const auto kind = texture ? AssetKind::Texture : AssetKind::Shader;
		const std::string label =
		    path.empty() ? (texture ? "None" : "Default") : std::filesystem::path(path).filename().string();

		if (context.monospaceFont != nullptr) ImGui::PushFont(context.monospaceFont, 0.0f);
		if (ImGui::BeginCombo(texture ? "##Texture" : "##Shader", label.c_str()))
		{
			if (ImGui::Selectable(texture ? "None" : "Default", path.empty()))
				SetMaterialAsset(context, material, "", texture);
			if (!texture && ImGui::Selectable("Textured Mesh", path == kTexturedMeshShaderPath))
				SetMaterialAsset(context, material, kTexturedMeshShaderPath, false);
			for (const auto& entry : context.assets.Entries())
				if (entry.kind == kind && ImGui::Selectable(entry.path.c_str(), entry.path == path))
					SetMaterialAsset(context, material, entry.path, texture);
			ImGui::EndCombo();
		}
		if (context.monospaceFont != nullptr) ImGui::PopFont();

		if (ImGui::BeginDragDropTarget())
		{
			if (const auto* payload = ImGui::AcceptDragDropPayload(texture ? TextureAssetPayload : ShaderAssetPayload))
				SetMaterialAsset(context, material, static_cast<const char*>(payload->Data), texture);
			ImGui::EndDragDropTarget();
		}
		if (!path.empty()) ImGui::SetItemTooltip("%s", path.c_str());

		ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
		ImGui::TextUnformatted(texture ? "Texture" : "Shader");
	}

	void InspectorPanel::DrawMaterialPreview(EditorContext& context, const ECS::MeshRendererComponent& material)
	{
		if (material.texturePath.empty()) return;

		const auto texture = context.assetResolver.Texture(material.texturePath, *context.renderer);
		if (context.assetResolver.GetTextureState(material.texturePath, *context.renderer) ==
		    ResourceLoadState::Loading)
			ImGui::TextDisabled("Loading texture...");
		if (!texture.IsValid()) return;

		if (context.monospaceFont != nullptr) ImGui::PushFont(context.monospaceFont, 0.0f);
		ImGui::Text("%u x %u", texture.width, texture.height);
		if (context.monospaceFont != nullptr) ImGui::PopFont();

		const float width = (std::min)(ImGui::GetContentRegionAvail().x, ScaledSize(kMaterialPreviewMaxWidth));
		ImGui::Image(static_cast<ImTextureID>(context.renderer->GetTextureHandle(texture.textureId)),
		             ImVec2(width, width * texture.height / texture.width));
	}

	void InspectorPanel::DrawMaterial(EditorContext& context)
	{
		auto* material = context.world.GetComponent<ECS::MeshRendererComponent>(context.selectedEntity);
		if (material == nullptr)
		{
			if (HasMeshChild(context.world.GetRegistry(), context.selectedEntity))
				StatusText(*context.themeColors, EditorStatusKind::Muted, "Select a mesh part to edit its material");
			return;
		}
		if (context.renderer == nullptr || !ComponentHeader(NENE_ICON_PALETTE, "Material")) return;

		const EditorThemeColorsConfig& colors = *context.themeColors;
		ComponentCardScope card;
		ImGui::PushID("Material");
		ImGui::BeginDisabled(!context.CanEdit());
		const auto before = *material;
		ImGui::ColorEdit4("Tint", glm::value_ptr(material->tint));
		if (ImGui::IsItemActivated())
		{
			m_colorBefore = before;
			m_colorEntity = context.selectedEntity;
		}
		if (ImGui::IsItemDeactivatedAfterEdit() && m_colorEntity == context.selectedEntity)
		{
			context.history.RecordComponentChange("Change Tint", context.selectedEntity, m_colorBefore, *material);
			m_colorEntity = ECS::NullEntity;
		}
		if (!material->meshPath.empty() ||
		    (material->primitiveType != PrimitiveType::Line && material->primitiveType != PrimitiveType::DebugLine))
		{
			DrawMaterialAsset(context, *material, true);
			DrawMaterialAsset(context, *material, false);
		}
		else
			StatusText(colors, EditorStatusKind::Muted, "Line material uses the built-in shader");
		ImGui::EndDisabled();
		if (!m_materialError.empty()) StatusText(colors, EditorStatusKind::Danger, m_materialError.c_str());
		DrawMaterialPreview(context, *material);
		if (!material->texturePath.empty() &&
		    context.assetResolver.GetTextureState(material->texturePath, *context.renderer) ==
		        ResourceLoadState::Failed)
			StatusText(colors, EditorStatusKind::Danger, "Texture loading failed. Refresh assets to retry.");
		if (!material->shaderPath.empty() &&
		    context.assetResolver.GetShaderState(material->shaderPath, *context.renderer) == ResourceLoadState::Failed)
			StatusText(colors, EditorStatusKind::Danger, "Shader loading failed. Refresh assets to retry.");
		ImGui::PopID();
	}

	void InspectorPanel::DrawTransformFields(ECS::Entity entity, ECS::TransformComponent& transform, FieldEdits& edits)
	{
		DragVec3("Position", transform.position, kPositionDragSpeed, edits);

		if (entity != m_eulerEntity || transform.rotation != m_eulerSource)
		{
			m_eulerEntity = entity;
			m_eulerSource = transform.rotation;
			m_eulerDegrees = glm::degrees(glm::eulerAngles(transform.rotation));
		}
		if (DragVec3("Rotation", m_eulerDegrees, kRotationDragSpeed, edits))
		{
			transform.rotation = glm::quat(glm::radians(m_eulerDegrees));
			m_eulerSource = transform.rotation;
		}

		DragVec3("Scale", transform.scale, kScaleDragSpeed, edits, kMinScale, kMaxScale);
	}

} // namespace NeneEngine
