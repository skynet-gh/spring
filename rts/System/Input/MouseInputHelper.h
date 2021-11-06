/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */

#pragma once

#include <SDL_events.h>
#include <string>
#include <memory>

class MouseInputHelper
{
public:
	static void Init();
	static void Kill();
	static void Toggle();
	static void Set(bool b);
	static bool GetEmuLMBClick(const SDL_Event& event);
	static bool GetEmuRMBClick(const SDL_Event& event);
	static bool GetEmuLMBClick(uint8_t button);
	static bool GetEmuRMBClick(uint8_t button);
	static uint8_t GetEmuSwapButtons(uint8_t button);
private:
	bool buttonsSwapped = false;
	class MouseInputHelperProxy {
	public:
		MouseInputHelperProxy();
		~MouseInputHelperProxy();
		void ConfigNotify(const std::string& key, const std::string& value);
	};
	inline static std::unique_ptr<MouseInputHelperProxy> proxy = nullptr;

	inline static bool swapMouseButtons = false;
};
