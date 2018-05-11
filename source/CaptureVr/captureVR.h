#pragma once
#include <memory>
// using VRWindowsPtr = std::shared_ptr <class VRWindows>;
// class VRWindows
// {
// public:
// 	static VRWindowsPtr Create ();
// 	HWND m_hWnd = 0;
// 	HDC m_hDC = 0;
// };

namespace VRAdapter
{
	void Init (void);
	void Shutdown (void);

	void AcquireContext (uint8_t*);
	void SetIPD (float);
	void SetScale (float u, float v);
	
	// Set the size of the capture surface. 
	void SetCaptureSize (uint32_t width, uint32_t height);

	// Set the size of the textures passed to steam, should be 
	// called before AcquireContext.
	void SetTargetSize (uint32_t width, uint32_t height);

}