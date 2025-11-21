#pragma once

#include <types.h>
#include <string>
#include <vector>

class IWindow
{
public:
	std::string m_title;
	bool		m_opened = false;

public:
	virtual void Update() = 0;
};

class TestWindow final : public IWindow
{
public:
	void Update() override;
};

class Editor
{
public:
	std::vector<IWindow*> m_windows;
	bool m_update = true;

public:
	Editor();
	~Editor();
	void Update();
};