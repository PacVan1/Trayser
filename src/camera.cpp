#include <pch.h>

#include "camera.h"
#include "engine.h"

void Camera::Input()
{
	auto& input = VulkanEngine::Get().m_input;

	float const vel = 0.02f;
	if (input.IsKeyDown(KeyboardKey_D))	m_position += m_right * vel;
	if (input.IsKeyDown(KeyboardKey_W))	m_position += m_ahead * vel;
	if (input.IsKeyDown(KeyboardKey_A))	m_position -= m_right * vel;
	if (input.IsKeyDown(KeyboardKey_S))	m_position -= m_ahead * vel;

	int deltaX = input.GetMouseDeltaPos().x;
	int deltaY = input.GetMouseDeltaPos().y;

	m_yaw	+= deltaX * 0.1f;
	m_pitch -= deltaY * 0.1f;

	m_ahead.x	= cos(glm::radians(m_yaw)) * cos(glm::radians(m_pitch));
	m_ahead.y	= sin(glm::radians(m_pitch));
	m_ahead.z	= sin(glm::radians(m_yaw)) * cos(glm::radians(m_pitch));
	m_ahead		= glm::normalize(m_ahead);
	m_right		= glm::normalize(glm::cross(m_ahead, glm::vec3(0.0f, 1.0f, 0.0f)));
	m_up		= glm::normalize(glm::cross(m_right, m_ahead));

	m_view = glm::lookAt(m_position, m_position + m_ahead, glm::vec3(0.0f, 1.0f, 0.0f));
	//m_view = glm::lookAt(m_position, glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
}
