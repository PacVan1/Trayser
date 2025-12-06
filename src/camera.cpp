#include <pch.h>

#include "camera.h"
#include "engine.h"

void trayser::Camera::Input()
{
	auto& engine = g_engine;
	auto& input = engine.GetInput();

	float mod = input.IsKeyDown(KeyboardKey_LeftShift) ? 2.0f : 1.0f;
	float vel = m_speed * mod;
	if (input.IsKeyDown(KeyboardKey_D))	m_transform.translation += m_right * vel;
	if (input.IsKeyDown(KeyboardKey_W))	m_transform.translation += m_ahead * vel;
	if (input.IsKeyDown(KeyboardKey_R))	m_transform.translation += m_up * vel;
	if (input.IsKeyDown(KeyboardKey_A))	m_transform.translation -= m_right * vel;
	if (input.IsKeyDown(KeyboardKey_S))	m_transform.translation -= m_ahead * vel;
	if (input.IsKeyDown(KeyboardKey_F))	m_transform.translation -= m_up * vel;

	m_yaw	+= input.GetMouseDeltaPos().x * m_sensitivity;
	m_pitch -= input.GetMouseDeltaPos().y * m_sensitivity;
	m_fov	-= (float)input.GetMouseScroll();

	m_ahead.x	= cos(glm::radians(m_yaw)) * cos(glm::radians(m_pitch));
	m_ahead.y	= sin(glm::radians(m_pitch));
	m_ahead.z	= sin(glm::radians(m_yaw)) * cos(glm::radians(m_pitch));
	m_ahead		= glm::normalize(m_ahead);
	m_right		= glm::normalize(glm::cross(m_ahead, glm::vec3(0.0f, 1.0f, 0.0f)));
	m_up		= glm::normalize(glm::cross(m_right, m_ahead));

	m_view = glm::lookAt(m_transform.translation, m_transform.translation + m_ahead, glm::vec3(0.0f, 1.0f, 0.0f));
	VkExtent2D extent = { g_engine.m_gBuffer.colorImage.imageExtent.width, g_engine.m_gBuffer.colorImage.imageExtent.height };
	m_proj = glm::perspective(glm::radians(m_fov), (float)extent.width / (float)extent.height, 10000.0f, 0.1f);
}
