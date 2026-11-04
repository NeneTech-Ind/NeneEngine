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
#include "Graphics/Backend/IRenderAdapter.h"

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
			m_colorEntity = ECS::NullEntity;
			m_materialError.clear();
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

		if (context.mode == EditorMode::Play) DrawReadOnlyPill(*context.themeColors);

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
			const bool valid = texture ? context.assetResolver.Texture(path, *context.renderer).IsValid()
			                           : context.assetResolver.Shader(path, *context.renderer).IsValid();
			if (!valid)
			{
				m_materialError = texture ? "Could not load texture" : "Could not compile shader";
				return;
			}
		}
		const auto before = material;
		(texture ? material.texturePath : material.shaderPath) = path;
		if (texture && !path.empty() && material.shaderPath.empty())
			material.shaderPath = "assets/shaders/textured_mesh.shader";
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
		if (material == nullptr || context.renderer == nullptr || !ComponentHeader(NENE_ICON_PALETTE, "Material"))
			return;

		const EditorThemeColorsConfig& colors = *context.themeColors;
		ComponentCardScope card;
		ImGui::PushID("Material");
		ImGui::BeginDisabled(context.mode != EditorMode::Edit);
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
