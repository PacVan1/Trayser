#include <pch.h>
#include "editor.h"
#include "engine.h"
#include "imgui.h"

Editor::Editor()
{
	CameraWindow* window0 = nullptr;
	window0 = new CameraWindow();
	window0->m_title = "Camera Window";
	window0->m_opened = false;
	m_windows.push_back(window0);
	InspectorWindow* window1 = nullptr;
	window1 = new InspectorWindow();
	window1->m_title = "Inspector";
	window1->m_opened = false;
	m_windows.push_back(window1);
}

Editor::~Editor()
{
	for (IWindow* window : m_windows)
	{
		delete window;
	}
}

void Editor::Update()
{
	if (VulkanEngine::Get().m_input.IsKeyReleased(KeyboardKey_Tab))
	{
		m_update = !m_update;
		SDL_SetRelativeMouseMode(SDL_bool(!m_update));
	}

	if (!m_update)
		return;

	ImGui::SetNextWindowPos(ImVec2(10, 10));
	ImGui::SetNextWindowSizeConstraints(
		ImVec2(100, 300),   // min size (width, height)
		ImVec2(FLT_MAX, 300) // max size (width, height)
	);
	ImGui::Begin("Windows", nullptr, ImGuiWindowFlags_NoCollapse);
	ImVec4 yellow = ImVec4(1.0f, 1.0f, 0.0f, 1.0f); // RGBA
	ImGui::TextColored(yellow, "Press 'Tab' to toggle editor");
	for (IWindow* window : m_windows)
	{
		ImGui::Checkbox(window->m_title.c_str(), &window->m_opened);
	}
	ImGui::End();

	for (IWindow* window : m_windows)
	{
		if (window->m_opened)
		{
			window->Update();
		}
	}
}

void CameraWindow::Update()
{
	Camera& cam = VulkanEngine::Get().m_camera;

	ImGui::Begin(m_title.c_str(), &m_opened);
	ImGui::DragFloat("FOV", &cam.m_fov, 0.05f);
	ImGui::DragFloat("Speed", &cam.m_speed, 0.01f);
	ImGui::DragFloat("Sensitivity", &cam.m_sensitivity, 0.002f);
	ImGui::End();
}

void InspectorWindow::EditNode(Entity ent)
{
	auto& scene = VulkanEngine::Get().m_scene;

	const auto& [node, tf] = scene.m_registry.get<SGNode, LocalTransform>(ent);
	ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow;
	if (ent == selected)		flags |= ImGuiTreeNodeFlags_Selected;
	if (node.children.empty())	flags |= ImGuiTreeNodeFlags_Leaf;

	bool treeNodeOpened = ImGui::TreeNodeEx(reinterpret_cast<void*>(ent), flags, "%s", "Element");

	if (ImGui::IsItemClicked())
	{
		selected = ent;
	}

	if (treeNodeOpened)
	{
		for (int i = 0; i < node.children.size(); i++)
		{
			EditNode(node.children[i]);
		}
		ImGui::TreePop();
	}
}

void InspectorWindow::Update()
{
	ImGui::Begin(m_title.c_str(), &m_opened, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysUseWindowPadding);

	float totalHeight = ImGui::GetContentRegionAvail().y - 3 * ImGui::GetFrameHeightWithSpacing() - 2 * ImGui::GetFrameHeight() - 2;

	ImGui::BeginChild("Hierarchy", ImVec2(ImGui::GetContentRegionAvail().x, totalHeight));
	EditNode(VulkanEngine::Get().m_scene.GetRootEntity());
	ImGui::EndChild();

	if (selected != entt::null)
	{
		ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(120, 120, 120, 255));
		ImGui::Text("Entity ID: %u", selected);
		ImGui::PopStyleColor();

		ImGui::SeparatorText("Transform");

		auto& scene = VulkanEngine::Get().m_scene;
		auto& tf = scene.m_registry.get<LocalTransform>(selected);

		ImGui::PushStyleColor(ImGuiCol_FrameBg, IM_COL32(150, 50, 50, 255));
		ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, IM_COL32(180, 70, 70, 255));
		ImGui::PushStyleColor(ImGuiCol_FrameBgActive, IM_COL32(200, 90, 90, 255));
		if (ImGui::DragFloat3("Translation", glm::value_ptr(tf.position))) tf.dirty = true;
		ImGui::PopStyleColor(3);
		ImGui::PushStyleColor(ImGuiCol_FrameBg, IM_COL32(50, 150, 50, 255));
		ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, IM_COL32(70, 180, 70, 255));
		ImGui::PushStyleColor(ImGuiCol_FrameBgActive, IM_COL32(90, 200, 90, 255));
		if (ImGui::DragFloat3("Orientation", glm::value_ptr(tf.position))) tf.dirty = true;
		ImGui::PopStyleColor(3);
		ImGui::PushStyleColor(ImGuiCol_FrameBg, IM_COL32(50, 50, 150, 255));
		ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, IM_COL32(70, 70, 180, 255));
		ImGui::PushStyleColor(ImGuiCol_FrameBgActive, IM_COL32(90, 90, 200, 255));
		if (ImGui::DragFloat3("Scale", glm::value_ptr(tf.position))) tf.dirty = true;
		ImGui::PopStyleColor(3);
	}

	ImGui::End();
};
