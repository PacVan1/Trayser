#include <pch.h>
#include "editor.h"
#include "engine.h"
#include "imgui.h"

void TestWindow::Update() 
{
	ImGui::Begin(m_title.c_str(), &m_opened);
	ImGui::Text("Test");
	ImGui::End();
}

Editor::Editor()
{
	TestWindow* window = nullptr;
	window = new TestWindow();
	window->m_title = "Test Window0";
	window->m_opened = false;
	m_windows.push_back(window);
	CameraWindow* window2 = nullptr;
	window2 = new CameraWindow();
	window2->m_title = "Camera Window";
	window2->m_opened = false;
	m_windows.push_back(window2);
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
