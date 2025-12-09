#include <pch.h>
#include "editor.h"
#include "engine.h"
#include "imgui.h"

namespace trayser
{

static void EditLocalTransform(LocalTransform& transform)
{
	ImGui::PushStyleColor(ImGuiCol_FrameBg, IM_COL32(150, 50, 50, 255));
	ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, IM_COL32(180, 70, 70, 255));
	ImGui::PushStyleColor(ImGuiCol_FrameBgActive, IM_COL32(200, 90, 90, 255));
	if (ImGui::DragFloat3("##Translation", glm::value_ptr(transform.translation), 0.001f)) transform.dirty = true;
	ImGui::PopStyleColor(3);
	ImGui::SameLine();
	if (ImGui::Button("-##Button0", ImVec2(ImGui::GetFrameHeight(), ImGui::GetFrameHeight())))
	{
		transform.translation = glm::vec3(0.0f);
		transform.dirty = true;
	}
	ImGui::SameLine();
	ImGui::Text("Translation");

	ImGui::PushStyleColor(ImGuiCol_FrameBg, IM_COL32(50, 150, 50, 255));
	ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, IM_COL32(70, 180, 70, 255));
	ImGui::PushStyleColor(ImGuiCol_FrameBgActive, IM_COL32(90, 200, 90, 255));
	if (ImGui::DragFloat4("##Orientation", &transform.orientation.x, 0.01f)) transform.dirty = true;
	ImGui::PopStyleColor(3);
	ImGui::SameLine();
	if (ImGui::Button("-##Button1", ImVec2(ImGui::GetFrameHeight(), ImGui::GetFrameHeight())))
	{
		transform.orientation = glm::quat();
		transform.dirty = true;
	}
	ImGui::SameLine();
	ImGui::Text("Orientation");

	ImGui::PushStyleColor(ImGuiCol_FrameBg, IM_COL32(50, 50, 150, 255));
	ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, IM_COL32(70, 70, 180, 255));
	ImGui::PushStyleColor(ImGuiCol_FrameBgActive, IM_COL32(90, 90, 200, 255));
	if (ImGui::DragFloat3("##Scale", glm::value_ptr(transform.scale), 0.01f)) transform.dirty = true;
	ImGui::PopStyleColor(3);
	ImGui::SameLine();
	if (ImGui::Button("-##Button2", ImVec2(ImGui::GetFrameHeight(), ImGui::GetFrameHeight())))
	{
		transform.scale = glm::vec3(1.0f);
		transform.dirty = true;
	}
	ImGui::SameLine();
	ImGui::Text("Scale");
}

} // namespace trayser

trayser::Editor::Editor()
{
	AddWindow(0, new CameraWindow(), "Camera", ImGuiWindowFlags_AlwaysAutoResize);
	AddWindow(1, new InspectorWindow(), "Inspector");
	AddWindow(2, new RenderSettingsWindow(), "Render Settings", ImGuiWindowFlags_AlwaysAutoResize);
}

trayser::Editor::~Editor()
{
	for (IWindow* window : m_windows)
		delete window;
}

void trayser::Editor::Update()
{
	if (g_engine.GetInput().IsKeyReleased(KeyboardKey_Tab))
	{
		m_update = !m_update;
		SDL_SetRelativeMouseMode(SDL_bool(!m_update));
	}

	if (!m_update)
		return;

	ImGui::SetNextWindowPos(ImVec2(10, 10));
	ImGui::Begin("Windows", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize);
	ImVec4 yellow = ImVec4(1.0f, 1.0f, 0.0f, 1.0f); // RGBA
	ImGui::TextColored(yellow, "Press 'Tab' to toggle editor");
	for (int i = 0; i < kWindowCount; i++)
	{
		ImGui::Checkbox(m_titles[i].c_str(), &m_opened[i]);
	}
	ImGui::End();

	for (int i = 0; i < kWindowCount; i++)
	{
		if (m_opened[i])
		{
			ImGui::Begin(m_titles[i].c_str(), &m_opened[i], m_flags[i]);
			m_windows[i]->Update();
			ImGui::End();
		}
	}
}

void trayser::Editor::AddWindow(int windowIdx, IWindow* window, const char* title, ImGuiWindowFlags addons, bool opened)
{
	m_windows[windowIdx] = window;
	m_titles[windowIdx]  = title;
	m_opened[windowIdx]  = opened;
	m_flags[windowIdx]   = ImGuiWindowFlags_NoCollapse | addons;
}

void trayser::CameraWindow::Update()
{
	Camera& cam = g_engine.m_camera;

	ImGui::DragFloat("FOV", &cam.m_fov, 0.05f);
	ImGui::DragFloat("Speed", &cam.m_speed, 0.0001f);
	ImGui::DragFloat("Sensitivity", &cam.m_sensitivity, 0.002f);

	ImGui::SeparatorText("Transform");

	EditLocalTransform(cam.m_transform);
}

void trayser::InspectorWindow::EditNode(Entity ent)
{
	auto& scene = g_engine.m_scene;

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

void trayser::InspectorWindow::Update()
{
	auto& scene = g_engine.m_scene;

	if (ImGui::Button("Create"))
	{
		ModelHandle handle = g_engine.m_modelPool.Create(kModelPaths[selectedModel], kModelPaths[selectedModel], &g_engine);
		scene.CreateModel(g_engine.m_modelPool.Get(handle));
	}
	ImGui::SameLine();
	ImGui::Combo("Model", &selectedModel, kModelResourceStr.c_str());

	float totalHeight = ImGui::GetContentRegionAvail().y - 3 * ImGui::GetFrameHeightWithSpacing() - 2 * ImGui::GetFrameHeight() - 2;

	ImGui::BeginChild("Hierarchy", ImVec2(ImGui::GetContentRegionAvail().x, totalHeight));
	auto& node = scene.m_registry.get<SGNode>(scene.GetRootEntity());
	for (int i = 0; i < node.children.size(); i++)
	{
		EditNode(node.children[i]);
	}
	ImGui::EndChild();

	if (selected == entt::null)
		return;

	ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(120, 120, 120, 255));
	ImGui::Text("Entity ID: %u", selected);
	ImGui::PopStyleColor();

	ImGui::SeparatorText("Transform");

	auto& tf = scene.m_registry.get<LocalTransform>(selected);
	EditLocalTransform(tf);
};

void trayser::RenderSettingsWindow::Update()
{
	ImGui::Combo("Render Mode", &g_engine.m_renderMode, kRenderModeStr.c_str());
	ImGui::Combo("Tonemap Mode", &g_engine.m_tonemapMode, kTonemapModeStr.c_str());
	ImGui::Checkbox("Ray traced", &g_engine.m_rayTraced);
}
