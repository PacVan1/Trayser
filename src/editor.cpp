#include <pch.h>
#include "editor.h"

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
	window = new TestWindow();
	window->m_title = "Test Window1";
	window->m_opened = false;
	m_windows.push_back(window);
	window = new TestWindow();
	window->m_title = "Test Window2";
	window->m_opened = false;
	m_windows.push_back(window);
	window = new TestWindow();
	window->m_title = "Test Window3";
	window->m_opened = false;
	m_windows.push_back(window);
	window = new TestWindow();
	window->m_title = "Test Window4";
	window->m_opened = false;
	m_windows.push_back(window);
	window = new TestWindow();
	window->m_title = "Test Window0";
	window->m_opened = false;
	m_windows.push_back(window);
	window = new TestWindow();
	window->m_title = "Test Window1";
	window->m_opened = false;
	m_windows.push_back(window);
	window = new TestWindow();
	window->m_title = "Test Window2";
	window->m_opened = false;
	m_windows.push_back(window);
	window = new TestWindow();
	window->m_title = "Test Window3";
	window->m_opened = false;
	m_windows.push_back(window);
	window = new TestWindow();
	window->m_title = "Test Window4";
	window->m_opened = false;
	m_windows.push_back(window);
	window = new TestWindow();
	window->m_title = "Test Window0";
	window->m_opened = false;
	m_windows.push_back(window);
	window = new TestWindow();
	window->m_title = "Test Window1";
	window->m_opened = false;
	m_windows.push_back(window);
	window = new TestWindow();
	window->m_title = "Test Window2";
	window->m_opened = false;
	m_windows.push_back(window);
	window = new TestWindow();
	window->m_title = "Test Window3";
	window->m_opened = false;
	m_windows.push_back(window);
	window = new TestWindow();
	window->m_title = "Test Window4";
	window->m_opened = false;
	m_windows.push_back(window);
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
