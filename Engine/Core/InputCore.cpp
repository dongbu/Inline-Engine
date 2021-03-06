#include "InputCore.hpp"

namespace inl::core {

InputCore::InputCore()
{
	for (int i = 0; i < (int)eMouseButton::COUNT; ++i)
	{
		bMouseDownCurFrame[i] = false;
		bMouseDownPrevFrame[i] = false;
		cursorLastClickStartPos[i] = Vec2i(-1, -1);
	}

	mouseDelta.x = 0;
	mouseDelta.y = 0;
	clientCursorPos.x = 0;
	clientCursorPos.y = 0;
}

void InputCore::SimulateKeyPress(eKey key)
{
	std::queue<bool>& ref = keyDown[(int)key].keyDownQueue;

	// There are already some queued key presses for that key
	if (ref.size() > 0)
	{
		// Don't push that key press to the queue again
		if (ref.front())
		{
			return;
		}
	}

	keyDown[(int)key].keyDownQueue.push(true);
	
	// Dispatch registered callbacks binded to that key
	OnKeyPressed[(int)key]();
}

void InputCore::SimulateKeyRelease(eKey key)
{
	std::queue<bool>& ref = keyDown[(int)key].keyDownQueue;

	// There are already some queued key presses for that key
	if (ref.size() > 0)
	{
		// Don't push that key release to the queue again
		if (!ref.front())
		{
			return;
		}
	}

	keyDown[(size_t)key].keyDownQueue.push(false);

	// Dispatch registered callbacks binded to that key
	OnKeyReleased[(int)key]();
}

void InputCore::SimulateMouseBtnPress(eMouseButton eButton)
{
	bMouseDownCurFrame[(int)eButton] = true;
	bMouseDownPrevFrame[(int)eButton] = false;

	cursorLastClickStartPos[(int)eButton] = clientCursorPos;

	// Dispatch callbacks binded to right mouse press
	OnMousePressed[(int)eButton](clientCursorPos);
}

void InputCore::SimulateMouseBtnRelease(eMouseButton eButton)
{
	bMouseDownCurFrame[(int)eButton] = false;
	bMouseDownPrevFrame[(int)eButton] = true;

	
	// Dispatch callbacks binded to right mouse release
	OnMouseReleased[(int)eButton](clientCursorPos);
}

void InputCore::SimulateMouseMove(const Vec2i& mouseDelta, const Vec2i& clientCursorPos)
{
	this->mouseDelta += mouseDelta;
	this->clientCursorPos = clientCursorPos;
	
	// Dispatch registered mouseMove callbacks
	OnMouseMove(mouseDelta, clientCursorPos);
}

void InputCore::BindKeyDown(eKey key, const std::function<void()>& function)
{
	OnKeyDown[(int)key] += function;
}

void InputCore::BindKeyPress(eKey key, const std::function<void()>& function)
{
	OnKeyPressed[(int)key] += function;
}

void InputCore::BindKeyRelease(eKey key, const std::function<void()>& function)
{
	OnKeyReleased[(int)key] += function;
}

void InputCore::BindMousePress(eMouseButton button, const std::function<void(const Vec2i& mousePos)>& function)
{
	OnMousePressed[(int)button] += function;
}

void InputCore::BindMouseDown(eMouseButton button, const std::function<void(const Vec2i& mousePos)>& function)
{
	OnMouseDown[(int)button] += function;
}

void InputCore::BindMouseRelease(eMouseButton button, const std::function<void(const Vec2i& mousePos)>& function)
{
	OnMouseReleased[(int)button] += function;
}

void InputCore::BindMouseMove(const std::function<void(const Vec2i& mouseDelta, const Vec2i& clientCursorPos)>& function)
{
	OnMouseMove += function;
}

void InputCore::ClearFrameData()
{
	// General equation downPrevFrame = downCurFrame | (downPrevFrame & downCurFrame)
	//// Simply if button is down, then prevFrame must become true cuz 1 frame passed...
	// and if button is not down, but prevFrame true, this means we release button previous frame, so 1 frame passed, and prevFrame must become false
	// eq. pseudo
	////if(bMouseLeftDownPrevFrame && !bMouseLeftDownCurFrame)
	////	bMouseLeftDownPrevFrame = false;
	////0 0 -> false
	////0 1 -> false
	////1 0 -> false
	////1 1 -> true
	
	// Updating key downs with general equation
	//for (auto& keyDownInfo : keyDownArray)
	//	keyDownInfo.bDownPrevFrame = keyDownInfo.bDownCurFrame | (keyDownInfo.bDownPrevFrame & keyDownInfo.bDownCurFrame);
	//
	//// Updating mouse downs with general equation
	for(int i = 0; i < (int)eMouseButton::COUNT; ++i)
		bMouseDownPrevFrame[i] = bMouseDownCurFrame[i] | (bMouseDownPrevFrame[i] & bMouseDownCurFrame[i]);
	
	// Consume keyboard key queue
	for (auto& keyDownInfo : keyDown)
	{
		// Consume
		if(keyDownInfo.keyDownQueue.size() > 0)
			keyDownInfo.keyDownQueue.pop();
	}

	mouseDelta.x = 0;
	mouseDelta.y = 0;
}

void InputCore::Update()
{
	for(int i = 0; i < (int)eKey::COUNT; i++)
	{
		const std::queue<bool>& keyDownQueue = keyDown[i].keyDownQueue;

		if (keyDownQueue.size() > 0)
			keyDown[i].bStateDown = keyDownQueue.front();

		if (keyDown[i].bStateDown)
			OnKeyDown[i]();
	}
	
	for (int i = 0; i < (int)eMouseButton::COUNT; ++i)
	{
		if (bMouseDownCurFrame[i])
			OnMouseDown[i](clientCursorPos);
	}
}

bool InputCore::IsKeyDown(eKey key)
{
	return keyDown[(int)key].bStateDown;
}

bool InputCore::IsKeyPressed(eKey key)
{	
	const std::queue<bool>& keyQueue = keyDown[(int)key].keyDownQueue;

	if (keyQueue.size() == 0)
		return false;
	
	return keyQueue.front();
}

bool InputCore::IsKeyReleased(eKey key)
{
	const std::queue<bool>& keyQueue = keyDown[(int)key].keyDownQueue;

	if (keyQueue.size() == 0)
		return false;

	return ! keyQueue.front();
}

bool InputCore::IsMousePressed(eMouseButton eButton)
{
	return bMouseDownCurFrame[(int)eButton] && !bMouseDownPrevFrame[(int)eButton];
}

bool InputCore::IsMouseReleased(eMouseButton eButton)
{
	return bMouseDownPrevFrame[(int)eButton] && !bMouseDownCurFrame[(int)eButton];
}

bool InputCore::IsMouseDown(eMouseButton eButton)
{
	return bMouseDownCurFrame[(int)eButton];
}

bool InputCore::IsMouseClicked(eMouseButton eButton)
{
	return IsMouseReleased(eButton) && cursorLastClickStartPos[(int)eButton] == clientCursorPos;
}

} // namespace inl::core