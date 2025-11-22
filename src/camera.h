#pragma once

#include <types.h>

class Camera 
{
public:
	void Input();

public:
	glm::vec3 m_position = glm::vec3(0.0f);
	glm::vec3 m_ahead;
	glm::vec3 m_right;
	glm::vec3 m_up;

	glm::mat4 m_view = glm::mat4(1.0f);

	float m_pitch = 0.0f;
	float m_yaw = 0.0f;
};
