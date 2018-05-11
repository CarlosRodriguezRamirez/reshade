#if 0
bool gMultithread = false;

#include <openvr.h>
#include <glew.h>
#include <wglew.h>
#include <string>
#include <vector>
#include "runtime.hpp"
#include "captureVR.h"
#include "log.hpp"
extern HMODULE g_module_handle;
LRESULT CALLBACK WindowProc (HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	if (message == WM_DESTROY)
	{
		PostQuitMessage (0);
		return 0;
	}

	return DefWindowProc (hWnd, message, wParam, lParam);
}

void LOGGLError ()
{	
	GLenum glErr;
	int    retCode = 0;
	int     nCount = 0;

	glErr = glGetError ();
	while (glErr != GL_NO_ERROR)
	{
		const GLubyte* sError = gluErrorString (glErr);

		if (nCount > 10)
		{
			// For some reason the error buffer is full, so do nothing until if stop
			/* Empty */
			break;
		}
		else if (sError)
		{
			printf ("GL Error #%d (%s)\n", glErr, gluErrorString (glErr));
			std::string sz = (char*)gluErrorString (glErr);
			LOG (ERROR) << "GL Error! '" << sz;
			//cout << "GL Error #" << glErr << "(" << gluErrorString(glErr) << ") " << " in File " << file << " at line: " << line << endl;
		}

		if (GLEW_GREMEDY_string_marker)
			glStringMarkerGREMEDY (12, "CheckGLError");

		nCount++;
		retCode = 1;
		glErr = glGetError ();
	}
}

namespace VRAdapter
{
	GLuint m_nRenderTextureId = 0; // the texture capture
	uint32_t m_VRWidth = 0;
	uint32_t m_VRHeight = 0;
	uint32_t m_CaptureWidth = 0;
	uint32_t m_CaptureHeight = 0;

	HWND m_hWnd = 0; // The companion windows
	HDC m_hDCGL = 0;
	HGLRC m_hRC = 0; // Gl context


#define SCREEN_WIDTH 300
#define SCREEN_HEIGHT 300

	std::string iGetLastErrorText (DWORD nErrorCode)
	{
		char* msg = NULL;
		// Ask Windows to prepare a standard message for a GetLastError() code:
		DWORD len = FormatMessageA (FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, nErrorCode, MAKELANGID (LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&msg, 0, NULL);

		// Empty message, exit with unknown.
		if (!msg)
			return("Unknown error");

		// Copy buffer into the result string.
		std::string result = msg;

		// Because we used FORMAT_MESSAGE_ALLOCATE_BUFFER for our buffer, now wee need to release it.
		LocalFree (msg);

		return result;
	}
	std::string GetLastErrorStr (void)
	{
		return iGetLastErrorText (GetLastError ());
	}

	//- Create a dummy windows that we will use for gl initialization and texture resolved
	void CreateCompanion ()
	{
		WNDCLASSEX wc;

		ZeroMemory (&wc, sizeof (WNDCLASSEX));

		wc.cbSize = sizeof (WNDCLASSEX);
		wc.style = CS_HREDRAW | CS_VREDRAW;
		wc.lpfnWndProc = WindowProc;
		wc.hInstance = g_module_handle;//hInstance;
		wc.hCursor = LoadCursor (NULL, IDC_ARROW);
		wc.lpszClassName = L"WindowClass";

		RegisterClassEx (&wc);

		m_hWnd = CreateWindowEx (NULL, L"WindowClass", L"GL - Shared Resource",
			WS_OVERLAPPEDWINDOW, SCREEN_WIDTH + 20, 0, SCREEN_WIDTH, SCREEN_HEIGHT,
			NULL, NULL, g_module_handle, NULL);

		std::string info = GetLastErrorStr ();
		ShowWindow (m_hWnd, SW_SHOW);
	}

	void Init ()
	{

	}


	void InitGL (HWND hWndGL)
	{
		static	PIXELFORMATDESCRIPTOR pfd =
		{
			sizeof (PIXELFORMATDESCRIPTOR),              // Size Of This Pixel Format Descriptor
			1,                                          // Version Number
			PFD_DRAW_TO_WINDOW |                        // Format Must Support Window
			PFD_SUPPORT_OPENGL |                        // Format Must Support OpenGL
			PFD_DOUBLEBUFFER,                           // Must Support Double Buffering
			PFD_TYPE_RGBA,                              // Request An RGBA Format
			32,                                         // Select Our Color Depth
			0, 0, 0, 0, 0, 0,                           // Color Bits Ignored
			0,                                          // No Alpha Buffer
			0,                                          // Shift Bit Ignored
			0,                                          // No Accumulation Buffer
			0, 0, 0, 0,                                 // Accumulation Bits Ignored
			16,                                         // 16Bit Z-Buffer (Depth Buffer)  
			0,                                          // No Stencil Buffer
			0,                                          // No Auxiliary Buffer
			PFD_MAIN_PLANE,                             // Main Drawing Layer
			0,                                          // Reserved
			0, 0, 0                                     // Layer Masks Ignored
		};

		m_hDCGL = GetDC (hWndGL);
		GLuint PixelFormat = ChoosePixelFormat (m_hDCGL, &pfd);
		SetPixelFormat (m_hDCGL, PixelFormat, &pfd);
		m_hRC = wglCreateContext (m_hDCGL);
		wglMakeCurrent (m_hDCGL, m_hRC);

		GLenum x = glewInit ();

		glViewport (0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
		glMatrixMode (GL_PROJECTION);
		glLoadIdentity ();
		glOrtho (0, SCREEN_WIDTH, SCREEN_HEIGHT, 0, -1, 1);
		glDisable (GL_DEPTH_TEST);
		glMatrixMode (GL_MODELVIEW);
		glLoadIdentity ();
	}

	void CreateFrameBuffer (int nWidth, int nHeight)
	{						
		glGenTextures (1, &m_nRenderTextureId);
		glBindTexture (GL_TEXTURE_2D, m_nRenderTextureId);
		glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
		glTexImage2D (GL_TEXTURE_2D, 0, GL_RGBA8, nWidth, nHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
		//glTexStorage2D (GL_TEXTURE_2D, 0, GL_RGBA8, nWidth, nHeight);
		glBindTexture (GL_TEXTURE_2D, 0);

		//glFramebufferTexture2D (GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_nRenderTextureId, 0);

		m_CaptureWidth = nWidth;
		m_CaptureHeight = nHeight;
	}

	void DestroyCaptureTexture ()
	{
		wglMakeCurrent (m_hDCGL, m_hRC);
		glDeleteTextures (1, &m_nRenderTextureId);
		m_nRenderTextureId = 0;
	}



	void submitFrames (GLint leftEyeTex, GLint rightEyeTex, bool linear = false)
	{
		// Present image to VR headset
		vr::VRTextureBounds_t bounds;
		bounds.uMin = 0.0f;
		bounds.uMax = 1.0f;
		bounds.vMin = 0.0f;
		bounds.vMax = 1.0f;

		vr::Texture_t leftEyeTexture = { (void*)leftEyeTex, vr::TextureType_OpenGL, vr::ColorSpace_Gamma };
		vr::Texture_t rightEyeTexture = { (void*)rightEyeTex, vr::TextureType_OpenGL, vr::ColorSpace_Gamma };

		vr::EVRCompositorError error; 
		error = vr::VRCompositor ()->Submit (vr::Eye_Left, &leftEyeTexture, &bounds);
		error = vr::VRCompositor ()->Submit (vr::Eye_Right, &rightEyeTexture, &bounds);
		glFlush ();
		glFinish ();

		vr::VRCompositor ()->PostPresentHandoff ();
		LOGGLError ();
	}
	
	void AcquireContext (uint8_t* buffer)
	{
		//vr::VRCompositor ()->ClearLastSubmittedFrame ();

		if (!buffer)
			throw std::invalid_argument ("Null parameter");

		// Create texture if required
		if (!m_nRenderTextureId)
		{
			vr::VRSystem ()->GetRecommendedRenderTargetSize (&m_VRWidth, &m_VRHeight);
			CreateFrameBuffer (m_CaptureWidth, m_CaptureHeight);
			LOGGLError ();
		}

		wglMakeCurrent (m_hDCGL, m_hRC);
		LOGGLError ();
		glBindTexture (GL_TEXTURE_2D, m_nRenderTextureId);
		LOGGLError ();

		GLint Alignment = 0;
		glGetIntegerv (GL_UNPACK_ALIGNMENT, &Alignment);
		glPixelStorei (GL_UNPACK_ALIGNMENT, 1);
		glTexSubImage2D (GL_TEXTURE_2D, 0, 0, 0, m_CaptureWidth, m_CaptureHeight, GL_RGBA, GL_UNSIGNED_BYTE, (void*)buffer);
		glPixelStorei (GL_UNPACK_ALIGNMENT, Alignment);
		LOGGLError ();
		submitFrames (m_nRenderTextureId, m_nRenderTextureId);

		//glBindTexture (GL_TEXTURE_2D, 0);
		glFinish ();

		LOGGLError ();
		wglMakeCurrent (0, 0);
		// Pass texture to VR hmd
	}

	void SetFrame (uint32_t width, uint32_t height)
	{
		if (!m_hRC || !m_hDCGL)
		{
			CreateCompanion ();
			InitGL (m_hWnd);
		}

		if (m_CaptureWidth != width || m_CaptureHeight != height)
		{
			wglMakeCurrent (m_hDCGL, m_hRC);
			DestroyCaptureTexture ();
			CreateFrameBuffer (width, height);
		}
	}
}
#endif