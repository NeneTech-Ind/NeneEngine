// InspectorPanel.cpp

#include "Editor/Panels/InspectorPanel.h"

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
#include "ECS/Components/TagComponent.h"
#include "ECS/Components/TransformComponent.h"
#include "ECS/World.h"
#include "Editor/EditorContext.h"
#include "Editor/EditorHistory.h"
#include "Editor/Theme/EditorIcons.h"
#include "Editor/Theme/EditorThemeConfig.h"
#include "Editor/Theme/EditorWidgets.h"

#include <algorithm>
#include <glm/gtc/type_ptr.hpp>
#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>
#include <string>

namespace NeneEngine
{
	namespace
	{
		using FieldEdits = InspectorPanel::FieldEdits;

		constexpr const char* kReadOnlyLabel = NENE_ICON_LOCK " Read-only";
		constexpr float kPositionDragSpeed = 0.05f;
		constexpr float kRotationDragSpeed = 0.5f;
		constexpr float kScaleDragSpeed = 0.01f;
		constexpr float kMinScale = 0.001f;
		constexpr float kMaxScale = 1000.0f;
		constexpr float kMinMass = 0.001f;
		constexpr float kMinColliderSize = 0.001f;

		constexpr const char* kPrimitiveTypeNames[] = {"Line", "Triangle", "Quad", "Cube"};
		constexpr const char* kColliderTypeNames[] = {"Box", "Sphere"};

		void TrackItem(FieldEdits& edits)
		{
			edits.activated |= ImGui::IsItemActivated();
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
			const std::string widgetId = std::string("##") + label;
			if (edits.valueFont != nullptr) ImGui::PushFont(edits.valueFont, 0.0f);
			const AxisDragResult result = DragFloat3Axes(widgetId.c_str(), values, speed, min, max, flags, axisColors);
			if (edits.valueFont != nullptr) ImGui::PopFont();
			edits.activated |= result.activated;
			edits.deactivatedAfterEdit |= result.deactivatedAfterEdit;

			ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
			ImGui::TextUnformatted(label);
			return result.changed;
		}

		bool DragVec3(const char* label, glm::vec3& value, float speed, FieldEdits& edits, float min = 0.0f,
		              float max = 0.0f)
		{
			const ImGuiSliderFlags flags = min < max ? ImGuiSliderFlags_AlwaysClamp : ImGuiSliderFlags_None;
			if (edits.colors != nullptr)
				return DrawAxisField(label, glm::value_ptr(value), speed, min, max, flags, edits);

			return DrawValueField(label, edits,
			                      [&](const char* widgetId) {
				                      return ImGui::DragFloat3(widgetId, glm::value_ptr(value), speed, min, max, "%.3f",
				                                               flags);
			                      });
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

		std::string GetEntityLabel(const ECS::World& world, ECS::Entity entity)
		{
			const auto* tag = world.GetRegistry().try_get<ECS::TagComponent>(entity);
			const std::string name = tag != nullptr && !tag->name.empty() ? tag->name : "Entity";
			return name + " (" + std::to_string(entt::to_integral(entity)) + ")";
		}
	} // namespace

	template <typename Component, typename DrawFields>
	void InspectorPanel::DrawComponent(EditorContext& context, const char* icon, const char* label,
	                                   DrawFields&& drawFields)
	{
		const ECS::Entity entity = context.selectedEntity;
		Component* component = context.world.GetComponent<Component>(entity);
		if (component == nullptr || !ComponentHeader(icon, label)) return;

		const bool isEditMode = context.mode == EditorMode::Edit;
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

	void InspectorPanel::Draw(EditorContext& context)
	{
		if (context.selectedEntity != ECS::NullEntity && !context.world.GetRegistry().valid(context.selectedEntity))
			context.selectedEntity = ECS::NullEntity;

		if (context.selectedEntity == ECS::NullEntity)
		{
			m_commitPendingEdit = nullptr;
			m_pendingEditLabel = nullptr;
			DrawEmptyState(context.GetMascot(EditorMascotPose::Search), "Select an entity to inspect it");
			return;
		}

		DrawEntityHeader(context);
		ImGui::Separator();
		DrawComponents(context);
	}

	void InspectorPanel::DrawEntityHeader(EditorContext& context)
	{
		const ECS::World& world = context.world;
		ImGui::AlignTextToFramePadding();
		ImGui::Text("Entity %u", static_cast<uint32_t>(entt::to_integral(context.selectedEntity)));

		if (context.mode == EditorMode::Play && context.themeColors != nullptr)
		{
			ImGui::SameLine();
			const float pillWidth = CalcStatusPillSize(kReadOnlyLabel).x;
			const float offset = (std::max)(0.0f, ImGui::GetContentRegionAvail().x - pillWidth);
			ImGui::SetCursorPosX(ImGui::GetCursorPosX() + offset);
			StatusPill(kReadOnlyLabel, context.themeColors->accentPlay, context.themeColors->background);
		}

		if (const auto* hierarchy = world.GetRegistry().try_get<ECS::HierarchyComponent>(context.selectedEntity))
		{
			if (hierarchy->parent != ECS::NullEntity && world.GetRegistry().valid(hierarchy->parent))
				ImGui::TextDisabled("Parent: %s", GetEntityLabel(world, hierarchy->parent).c_str());
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

		DrawComponent<ECS::MeshRendererComponent>(context, NENE_ICON_PRIMITIVE, "Mesh Renderer",
		                                          [](ECS::MeshRendererComponent& meshRenderer, FieldEdits& edits)
		                                          {
			                                          EnumCombo("Primitive", meshRenderer.primitiveType,
			                                                    kPrimitiveTypeNames, edits);
			                                          Checkbox("Visible", meshRenderer.visible, edits);
			                                          ImGui::ColorEdit3("Tint", glm::value_ptr(meshRenderer.tint));
			                                          TrackItem(edits);
		                                          });

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
			ImGui::Text("Contacts: %u", contactState->contactCount);
			ImGui::Text("Grounded: %s", contactState->isGrounded ? "yes" : "no");
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
			    ImGui::TextDisabled("Primary: %s", camera.isPrimary ? "yes" : "no");
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
