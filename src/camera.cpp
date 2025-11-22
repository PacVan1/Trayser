#include <pch.h>

#include "camera.h"
#include "engine.h"

void Camera::Input()
{
	auto& input = VulkanEngine::Get().m_input;

	float const vel = 0.02f;
	if (input.IsKeyDown(KeyboardKey_D))	m_position -= glm::vec3(1.0, 0.0, 0.0) * vel;
	if (input.IsKeyDown(KeyboardKey_W))	m_position += glm::vec3(0.0, 0.0, 1.0) * vel;
	if (input.IsKeyDown(KeyboardKey_A))	m_position += glm::vec3(1.0, 0.0, 0.0) * vel;
	if (input.IsKeyDown(KeyboardKey_S))	m_position -= glm::vec3(0.0, 0.0, 1.0) * vel;

	m_view = glm::translate(glm::mat4(1.0f), m_position);
}
