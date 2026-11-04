// ViewportPanel.cpp

#include "Editor/Panels/ViewportPanel.h"

#include "ECS/Components/CameraComponent.h"
#include "ECS/Components/HierarchyComponent.h"
#include "ECS/World.h"
#include "Editor/EditorContext.h"
#include "Editor/EditorHistory.h"
#include "Editor/Theme/EditorWidgets.h"

#include <ImGuizmo.h>
#include <algorithm>
#include <cmath>
#include <glm/gtc/type_ptr.hpp>
#include <imgui.h>

namespace NeneEngine
{
	namespace
	{
		constexpr int kMaxHierarchyDepth = 64;
		constexpr float kMinAxisScale = 1e-6f;

		constexpr float kTranslateSnap = 0.5f;
		constexpr float kRotateSnapDegrees = 15.0f;
		constexpr float kScaleSnap = 0.1f;

		glm::mat4 ComputeWorldMatrix(const ECS::World& world, ECS::Entity entity)
		{
			const entt::registry& registry = world.GetRegistry();
			glm::mat4 worldMatrix(1.0f);

			for (int depth = 0; depth < kMaxHierarchyDepth && registry.valid(entity); ++depth)
			{
				if (const auto* transform = registry.try_get<ECS::TransformComponent>(entity))
					worldMatrix = transform->GetModelMatrix() * worldMatrix;

				const auto* hierarchy = registry.try_get<ECS::HierarchyComponent>(entity);
				if (hierarchy == nullptr) break;
				entity = hierarchy->parent;
			}

			return worldMatrix;
		}

		glm::mat4 ComputeParentWorldMatrix(const ECS::World& world, ECS::Entity entity)
		{
			const auto* hierarchy = world.GetRegistry().try_get<ECS::HierarchyComponent>(entity);
			if (hierarchy == nullptr || hierarchy->parent == ECS::NullEntity) return glm::mat4(1.0f);

			return ComputeWorldMatrix(world, hierarchy->parent);
		}

		void DecomposeTransform(const glm::mat4& matrix, glm::vec3& position, glm::quat& rotation, glm::vec3& scale)
		{
			position = glm::vec3(matrix[3]);

			glm::vec3 axes[3] = {glm::vec3(matrix[0]), glm::vec3(matrix[1]), glm::vec3(matrix[2])};
			scale = {glm::length(axes[0]), glm::length(axes[1]), glm::length(axes[2])};
			if (glm::determinant(glm::mat3(matrix)) < 0.0f) scale.x = -scale.x;

			for (int axis = 0; axis < 3; ++axis)
			{
				if (std::abs(scale[axis]) < kMinAxisScale) scale[axis] = kMinAxisScale;
				axes[axis] /= scale[axis];
			}

			rotation = glm::normalize(glm::quat_cast(glm::mat3(axes[0], axes[1], axes[2])));
		}

		void ClipDrawCommands(ImDrawList* drawList, int firstCommand, const ImVec2& clipMin, const ImVec2& clipMax)
		{
			for (int index = (std::max)(0, firstCommand); index < drawList->CmdBuffer.Size; ++index)
			{
				ImVec4& clipRect = drawList->CmdBuffer[index].ClipRect;
				clipRect.x = (std::max)(clipRect.x, clipMin.x);
				clipRect.y = (std::max)(clipRect.y, clipMin.y);
				clipRect.z = (std::min)(clipRect.z, clipMax.x);
				clipRect.w = (std::min)(clipRect.w, clipMax.y);
			}
		}

		ImGuizmo::OPERATION ToImGuizmo(GizmoOperation operation)
		{
			switch (operation)
			{
			case GizmoOperation::Rotate:
				return ImGuizmo::ROTATE;
			case GizmoOperation::Scale:
				return ImGuizmo::SCALE;
			case GizmoOperation::Translate:
				break;
			}
			return ImGuizmo::TRANSLATE;
		}

		constexpr float kToolbarBackdropPadding = 4.0f;
		constexpr float kToolbarBackdropAlpha = 0.85f;

		bool ToolbarToggle(const char* label, bool isActive)
		{
			if (isActive) ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
			const bool pressed = ImGui::Button(label);
			if (isActive) ImGui::PopStyleColor();
			return pressed;
		}
	} // namespace

	void ViewportPanel::Draw(EditorContext& context)
	{
		context.isViewportHovered = ImGui::IsWindowHovered();
		context.isViewportFocused = ImGui::IsWindowFocused();

		HandleShortcuts(context);
		DrawToolbar();
		DrawGizmo(context);
	}

	void ViewportPanel::HandleShortcuts(const EditorContext& context)
	{
		if (context.mode != EditorMode::Edit || !context.isViewportHovered || ImGui::GetIO().WantTextInput ||
		    ImGui::IsMouseDown(ImGuiMouseButton_Right))
			return;

		if (ImGui::IsKeyPressed(ImGuiKey_W, false)) m_operation = GizmoOperation::Translate;
		if (ImGui::IsKeyPressed(ImGuiKey_E, false)) m_operation = GizmoOperation::Rotate;
		if (ImGui::IsKeyPressed(ImGuiKey_R, false)) m_operation = GizmoOperation::Scale;
		if (ImGui::IsKeyPressed(ImGuiKey_X, false))
			m_space = m_space == GizmoSpace::World ? GizmoSpace::Local : GizmoSpace::World;
	}

	void ViewportPanel::DrawToolbar()
	{
		const ImGuiStyle& style = ImGui::GetStyle();
		ImDrawList* drawList = ImGui::GetWindowDrawList();
		drawList->ChannelsSplit(2);
		drawList->ChannelsSetCurrent(1);

		ImGui::BeginGroup();
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, style.ItemSpacing.y));

		if (ToolbarToggle(NENE_ICON_MOVE "##Move", m_operation == GizmoOperation::Translate))
			m_operation = GizmoOperation::Translate;
		ImGui::SetItemTooltip("Translate gizmo (W)");
		ImGui::SameLine();
		if (ToolbarToggle(NENE_ICON_ROTATE "##Rotate", m_operation == GizmoOperation::Rotate))
			m_operation = GizmoOperation::Rotate;
		ImGui::SetItemTooltip("Rotate gizmo (E)");
		ImGui::SameLine();
		if (ToolbarToggle(NENE_ICON_SCALE "##Scale", m_operation == GizmoOperation::Scale))
			m_operation = GizmoOperation::Scale;
		ImGui::SetItemTooltip("Scale gizmo (R)");

		ImGui::PopStyleVar();
		ImGui::SameLine(0.0f, style.ItemSpacing.x * 3.0f);
		const bool isScale = m_operation == GizmoOperation::Scale;
		const bool isLocal = isScale || m_space == GizmoSpace::Local;
		ImGui::BeginDisabled(isScale);
		if (ImGui::Button(isLocal ? NENE_ICON_LOCAL "##Space" : NENE_ICON_WORLD "##Space"))
			m_space = m_space == GizmoSpace::World ? GizmoSpace::Local : GizmoSpace::World;
		ImGui::EndDisabled();
		if (isScale)
			ImGui::SetItemTooltip("Scale always uses local axes");
		else
			ImGui::SetItemTooltip(isLocal ? "Local axes, click for world (X)" : "World axes, click for local (X)");

		ImGui::SameLine();
		if (ToolbarToggle(NENE_ICON_SNAP "##Snap", m_useSnap)) m_useSnap = !m_useSnap;
		ImGui::SetItemTooltip(m_useSnap ? "Snapping on" : "Snapping off");

		ImGui::PopStyleColor();
		ImGui::EndGroup();

		const ImVec2 groupMin = ImGui::GetItemRectMin();
		const ImVec2 groupMax = ImGui::GetItemRectMax();
		drawList->ChannelsSetCurrent(0);
		const float padding = ScaledSize(kToolbarBackdropPadding);
		drawList->AddRectFilled(ImVec2(groupMin.x - padding, groupMin.y - padding),
		                        ImVec2(groupMax.x + padding, groupMax.y + padding),
		                        ImGui::GetColorU32(ImGuiCol_PopupBg, kToolbarBackdropAlpha),
		                        style.FrameRounding + padding);
		drawList->ChannelsMerge();
	}

	void ViewportPanel::DrawGizmo(EditorContext& context)
	{
		const ECS::World& world = context.world;
		const entt::registry& registry = world.GetRegistry();
		const ECS::Entity entity = context.selectedEntity;

		const bool canManipulate = context.mode == EditorMode::Edit && entity != ECS::NullEntity &&
		                           registry.valid(entity) && entity != context.cameraEntity &&
		                           registry.all_of<ECS::TransformComponent>(entity) &&
		                           registry.valid(context.cameraEntity) &&
		                           registry.all_of<ECS::TransformComponent, ECS::CameraComponent>(context.cameraEntity);
		if (!canManipulate)
		{
			TrackGizmoUndo(context, entity, false);
			return;
		}

		const auto& camera = registry.get<ECS::CameraComponent>(context.cameraEntity);
		const glm::vec3 cameraPosition = glm::vec3(ComputeWorldMatrix(world, context.cameraEntity)[3]);
		const glm::mat4 view =
		    glm::lookAt(cameraPosition, cameraPosition + glm::normalize(camera.forward), glm::normalize(camera.up));
		const glm::mat4 projection = camera.GetProjectionMatrix();

		const ImGuiViewport* mainViewport = ImGui::GetMainViewport();
		ImDrawList* drawList = ImGui::GetWindowDrawList();
		const ImVec2 clipMin = drawList->GetClipRectMin();
		const ImVec2 clipMax = drawList->GetClipRectMax();
		const int firstGizmoCommand = drawList->CmdBuffer.Size - 1;
		ImGuizmo::SetOrthographic(false);
		ImGuizmo::SetDrawlist(drawList);
		ImGuizmo::SetRect(mainViewport->Pos.x, mainViewport->Pos.y, mainViewport->Size.x, mainViewport->Size.y);

		const glm::mat4 parentWorld = ComputeParentWorldMatrix(world, entity);
		auto* transform = context.world.GetComponent<ECS::TransformComponent>(entity);
		glm::mat4 worldMatrix = parentWorld * transform->GetModelMatrix();

		const ImGuizmo::OPERATION operation = ToImGuizmo(m_operation);
		const ImGuizmo::MODE space =
		    m_space == GizmoSpace::Local || m_operation == GizmoOperation::Scale ? ImGuizmo::LOCAL : ImGuizmo::WORLD;

		float snapValue = kTranslateSnap;
		if (m_operation == GizmoOperation::Rotate) snapValue = kRotateSnapDegrees;
		if (m_operation == GizmoOperation::Scale) snapValue = kScaleSnap;
		const float snap[3] = {snapValue, snapValue, snapValue};

		const bool isManipulated =
		    ImGuizmo::Manipulate(glm::value_ptr(view), glm::value_ptr(projection), operation, space,
		                         glm::value_ptr(worldMatrix), nullptr, m_useSnap ? snap : nullptr);
		ClipDrawCommands(drawList, firstGizmoCommand, clipMin, clipMax);
		if (isManipulated)
		{
			glm::vec3 position;
			glm::quat rotation;
			glm::vec3 scale;
			DecomposeTransform(glm::inverse(parentWorld) * worldMatrix, position, rotation, scale);

			switch (m_operation)
			{
			case GizmoOperation::Translate:
				transform->position = position;
				break;
			case GizmoOperation::Rotate:
				transform->rotation = rotation;
				break;
			case GizmoOperation::Scale:
				transform->scale = scale;
				break;
			}
		}

		TrackGizmoUndo(context, entity, ImGuizmo::IsUsing());
	}

	void ViewportPanel::TrackGizmoUndo(EditorContext& context, ECS::Entity entity, bool isUsing)
	{
		if (isUsing && !m_wasUsingGizmo)
		{
			if (const auto* transform = context.world.GetComponent<ECS::TransformComponent>(entity))
			{
				m_dragEntity = entity;
				m_transformBeforeDrag = *transform;
			}
		}
		else if (!isUsing && m_wasUsingGizmo && m_dragEntity != ECS::NullEntity)
		{
			const auto* transform = context.world.GetRegistry().valid(m_dragEntity)
			                            ? context.world.GetComponent<ECS::TransformComponent>(m_dragEntity)
			                            : nullptr;
			if (transform != nullptr && context.mode == EditorMode::Edit)
				context.history.RecordComponentChange<ECS::TransformComponent>("Gizmo Transform", m_dragEntity,
				                                                               m_transformBeforeDrag, *transform);
			m_dragEntity = ECS::NullEntity;
		}

		m_wasUsingGizmo = isUsing;
	}

} // namespace NeneEngine
