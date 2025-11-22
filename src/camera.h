#pragma once

#include <types.h>

class Camera 
{
public:
	void Input();

public:
	glm::vec3 m_position;
	glm::mat4 m_view;
};
