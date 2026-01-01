#pragma once

#include <types.h>
#include <bitset>
#include <SDL2/SDL_events.h>
#include <SDL2/SDL_scancode.h>

enum MouseButton_ : u32
{
	MouseButton_Left,
	MouseButton_Middle,
	MouseButton_Right,
	kMouseButtonCount
};
using MouseButton = u32;

enum KeyboardKey_ : u32
{
	KeyboardKey_A, KeyboardKey_B, KeyboardKey_C, KeyboardKey_D,
	KeyboardKey_E, KeyboardKey_F, KeyboardKey_G, KeyboardKey_H,
	KeyboardKey_I, KeyboardKey_J, KeyboardKey_K, KeyboardKey_L,
	KeyboardKey_M, KeyboardKey_N, KeyboardKey_O, KeyboardKey_P,
	KeyboardKey_Q, KeyboardKey_R, KeyboardKey_S, KeyboardKey_T,
	KeyboardKey_U, KeyboardKey_V, KeyboardKey_W, KeyboardKey_X,
	KeyboardKey_Y, KeyboardKey_Z, KeyboardKey_0, KeyboardKey_1,
	KeyboardKey_2, KeyboardKey_3, KeyboardKey_4, KeyboardKey_5,
	KeyboardKey_6, KeyboardKey_7, KeyboardKey_8, KeyboardKey_9,
	KeyboardKey_Tab,
	KeyboardKey_CapsLock,
	KeyboardKey_LeftShift,
	KeyboardKey_LeftCtrl,
	KeyboardKey_LeftAlt,
	KeyboardKey_Escape,
	KeyboardKey_RightShift,
	KeyboardKey_Enter,
	KeyboardKey_Up,
	KeyboardKey_Right,
	KeyboardKey_Down,
	KeyboardKey_Left,
	KeyboardKey_Space,
	kKeyboardKeyCount
};
using KeyboardKey = u32;

class Input
{
public:
	Input();
	~Input() = default;
	void InitKeyMapping();
	void ProcessEvent(SDL_Event& event);
	void UpdatePrevious();
	bool IsKeyUp(KeyboardKey key) const;
	bool IsKeyDown(KeyboardKey key) const;
	bool IsKeyReleased(KeyboardKey key) const;
	bool IsMouseButtonUp(MouseButton button) const;
	bool IsMouseButtonDown(MouseButton button) const;
	bool IsMouseButtonReleased(MouseButton button) const;
	glm::ivec2 GetMousePos() const;
	glm::ivec2 GetMouseDeltaPos() const;
	int GetMouseScroll() const;

public:
	SDL_Keycode						m_keyMapping[kKeyboardKeyCount];
	std::bitset<SDL_NUM_SCANCODES>	m_keyStates;
	std::bitset<SDL_NUM_SCANCODES>	m_prevKeyStates;
	std::bitset<kMouseButtonCount>	m_buttonStates;
	std::bitset<kMouseButtonCount>	m_prevButtonStates;
	glm::ivec2 m_mousePos;
	glm::ivec2 m_mouseDeltaPos;
	int m_scroll;
};