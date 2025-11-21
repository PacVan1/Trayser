#include <pch.h>
#include <input.h>

Input::Input()
{
	memset(m_keyStates, false, sizeof(bool) * SDL_NUM_SCANCODES);
	memset(m_prevKeyStates, false, sizeof(bool) * SDL_NUM_SCANCODES);
	InitKeyMapping();
}

void Input::InitKeyMapping()
{
	m_keyMapping[KeyboardKey_A]				= SDLK_a;
	m_keyMapping[KeyboardKey_B]				= SDLK_b;
	m_keyMapping[KeyboardKey_C]				= SDLK_c;
	m_keyMapping[KeyboardKey_D]				= SDLK_d;
	m_keyMapping[KeyboardKey_E]				= SDLK_e;
	m_keyMapping[KeyboardKey_F]				= SDLK_f;
	m_keyMapping[KeyboardKey_G]				= SDLK_g;
	m_keyMapping[KeyboardKey_H]				= SDLK_h;
	m_keyMapping[KeyboardKey_I]				= SDLK_i;
	m_keyMapping[KeyboardKey_J]				= SDLK_j;
	m_keyMapping[KeyboardKey_K]				= SDLK_k;
	m_keyMapping[KeyboardKey_L]				= SDLK_l;
	m_keyMapping[KeyboardKey_M]				= SDLK_m;
	m_keyMapping[KeyboardKey_N]				= SDLK_n;
	m_keyMapping[KeyboardKey_O]				= SDLK_o;
	m_keyMapping[KeyboardKey_P]				= SDLK_p;
	m_keyMapping[KeyboardKey_Q]				= SDLK_q;
	m_keyMapping[KeyboardKey_R]				= SDLK_r;
	m_keyMapping[KeyboardKey_S]				= SDLK_s;
	m_keyMapping[KeyboardKey_T]				= SDLK_t;
	m_keyMapping[KeyboardKey_U]				= SDLK_u;
	m_keyMapping[KeyboardKey_V]				= SDLK_v;
	m_keyMapping[KeyboardKey_W]				= SDLK_w;
	m_keyMapping[KeyboardKey_X]				= SDLK_x;
	m_keyMapping[KeyboardKey_Y]				= SDLK_y;
	m_keyMapping[KeyboardKey_Z]				= SDLK_z;
	m_keyMapping[KeyboardKey_0]				= SDLK_0;
	m_keyMapping[KeyboardKey_1]				= SDLK_1;
	m_keyMapping[KeyboardKey_2]				= SDLK_2;
	m_keyMapping[KeyboardKey_3]				= SDLK_3;
	m_keyMapping[KeyboardKey_4]				= SDLK_4;
	m_keyMapping[KeyboardKey_5]				= SDLK_5;
	m_keyMapping[KeyboardKey_6]				= SDLK_6;
	m_keyMapping[KeyboardKey_7]				= SDLK_7;
	m_keyMapping[KeyboardKey_8]				= SDLK_8;
	m_keyMapping[KeyboardKey_9]				= SDLK_9;
	m_keyMapping[KeyboardKey_Tab]			= SDLK_TAB;
	m_keyMapping[KeyboardKey_CapsLock]		= SDLK_CAPSLOCK;
	m_keyMapping[KeyboardKey_LeftShift]		= SDLK_LSHIFT;
	m_keyMapping[KeyboardKey_LeftCtrl]		= SDLK_LCTRL;
	m_keyMapping[KeyboardKey_LeftAlt]		= SDLK_LALT;
	m_keyMapping[KeyboardKey_Escape]		= SDLK_ESCAPE;
	m_keyMapping[KeyboardKey_RightShift]	= SDLK_RSHIFT;
	m_keyMapping[KeyboardKey_Enter]			= SDLK_END;
	m_keyMapping[KeyboardKey_Up]			= SDLK_UP;
	m_keyMapping[KeyboardKey_Right]			= SDLK_RIGHT;
	m_keyMapping[KeyboardKey_Down]			= SDLK_DOWN;
	m_keyMapping[KeyboardKey_Left]			= SDLK_LEFT;
	m_keyMapping[KeyboardKey_Space]			= SDLK_SPACE;
}

void Input::ProcessEvent(SDL_Event& e)
{
    m_shouldUpdate = true;
    SDL_Keycode key = e.key.keysym.sym;
    if (e.type == SDL_KEYUP)   { if (key >= 0) m_keyStates[key & (SDL_NUM_SCANCODES - 1)] = false; return; }
    if (e.type == SDL_KEYDOWN) { if (key >= 0) m_keyStates[key & (SDL_NUM_SCANCODES - 1)] = true; }
}

void Input::UpdatePrevious()
{
    if (m_shouldUpdate)
    {
        memcpy(m_prevKeyStates, m_keyStates, SDL_NUM_SCANCODES * sizeof(bool));
        m_shouldUpdate = false;
    }
}

bool Input::IsKeyUp(KeyboardKey key) const
{
    return !m_keyStates[m_keyMapping[key] & (SDL_NUM_SCANCODES - 1)];
}

bool Input::IsKeyDown(KeyboardKey key) const
{
    return m_keyStates[m_keyMapping[key] & (SDL_NUM_SCANCODES - 1)];
}

bool Input::IsKeyReleased(KeyboardKey key) const
{
    return IsKeyUp(key) && m_prevKeyStates[m_keyMapping[key] & (SDL_NUM_SCANCODES - 1)];
}