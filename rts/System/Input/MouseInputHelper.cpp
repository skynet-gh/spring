#include "MouseInputHelper.h"

#include "System/SpringMath.h"
#include "System/Config/ConfigHandler.h"

CONFIG(bool, SwapMouseButtons).defaultValue(false).description("Swaps Left and Right mouse buttons for in-game control");

MouseInputHelper::MouseInputHelperProxy::MouseInputHelperProxy()
{
	configHandler->NotifyOnChange(this, { "SwapMouseButtons" });
	ConfigNotify("SwapMouseButtons", "");
}

MouseInputHelper::MouseInputHelperProxy::~MouseInputHelperProxy()
{
	configHandler->RemoveObserver(this);
}

void MouseInputHelper::MouseInputHelperProxy::ConfigNotify(const std::string& key, const std::string& value)
{
	if (key != "SwapMouseButtons")
		return;

	MouseInputHelper::swapMouseButtons = configHandler->GetBool("SwapMouseButtons");
}

void MouseInputHelper::Init()
{
	proxy = std::make_unique<MouseInputHelper::MouseInputHelperProxy>();
}

void MouseInputHelper::Kill()
{
	proxy = nullptr;
}

void MouseInputHelper::Toggle()
{
	Set(!MouseInputHelper::swapMouseButtons);
}

void MouseInputHelper::Set(bool b)
{
	configHandler->Set("SwapMouseButtons", b);
}

bool MouseInputHelper::GetEmuLMBClick(const SDL_Event& event)
{
	assert(event.type == SDL_MOUSEBUTTONDOWN || event.type == SDL_MOUSEBUTTONUP);
	return GetEmuLMBClick(event.button.button);
}

bool MouseInputHelper::GetEmuRMBClick(const SDL_Event& event)
{
	assert(event.type == SDL_MOUSEBUTTONDOWN || event.type == SDL_MOUSEBUTTONUP);
	return GetEmuRMBClick(event.button.button);
}

bool MouseInputHelper::GetEmuLMBClick(uint8_t button)
{
	return mix(button == SDL_BUTTON_LEFT, button == SDL_BUTTON_RIGHT, swapMouseButtons);
}

bool MouseInputHelper::GetEmuRMBClick(uint8_t button)
{
	return mix(button == SDL_BUTTON_RIGHT, button == SDL_BUTTON_LEFT, swapMouseButtons);
}

uint8_t MouseInputHelper::GetEmuSwapButtons(uint8_t button)
{
	if (!swapMouseButtons)
		return button;

	switch (button)
	{
	case SDL_BUTTON_LEFT : return SDL_BUTTON_RIGHT;
	case SDL_BUTTON_RIGHT: return SDL_BUTTON_LEFT;
	default:
		return button;
	}
}
