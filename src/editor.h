#pragma once

#include <types.h>
#include <string>
#include <vector>
#include <imgui.h>

namespace trayser
{

class IWindow
{
public:
	virtual void Update() = 0;
};

class CameraWindow final : public IWindow
{
public:
	void Update() override;
};

class InspectorWindow final : public IWindow
{
public:
	Entity selected				= entt::null;
	ModelResource selectedModel = ModelResource_DamagedHelmet;

public:
	void Update() override;
	void EditNode(Entity ent);
};

class RenderSettingsWindow final : public IWindow
{
public:
	void Update() override;
};

class Editor
{
public:
	static constexpr size_t kWindowCount = 3;

public:
	IWindow*			m_windows[kWindowCount];
	std::string			m_titles[kWindowCount];
	ImGuiWindowFlags	m_flags[kWindowCount];
	bool				m_opened[kWindowCount];
	bool				m_update = true;

public:
	Editor();
	~Editor();
	void Update();

private:
	void AddWindow(int windowIdx, IWindow* window, const char* title = "", ImGuiWindowFlags addons = ImGuiWindowFlags_None, bool opened = false);
};

} // namespace trayser