#include <openvr.h>
#include <string>
#include <vector>
#include "runtime.hpp"
#include "captureVR.h"
#include "log.hpp"
#include <thread>         // std::thread
#include <iostream>       // std::cout
#include<mutex>
#include "glad\glad.h" //Generated from http://glad.dav1d.de/

#ifdef _DEBUG
	#include <gl\GLU.h>
#endif

#define SCREEN_WIDTH 300
#define SCREEN_HEIGHT 300

extern HMODULE g_module_handle;

void LOGGLError ()
{
#ifdef _DEBUG
	GLenum glErr;
	int retCode = 0;
	int nCount = 0;

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

		nCount++;
		retCode = 1;
		glErr = glGetError ();
	}
#endif
}

namespace VRAdapter
{
	void CreateCompanion ();
	uint8_t* m_Buffer = nullptr;
	volatile bool m_Capturing = false;
	volatile bool m_Dirty = false;
	std::thread m_thread;
	std::mutex mutex;
	bool bInitialized = false;
	uint32_t m_VRWidth = 0;		// The Texture size passed to OpenVr (per eye)
	uint32_t m_VRHeight = 0;	// The Texture size passed to OpenVr (per eye)

	GLuint m_nCaptureTextureId = 0; // the texture capture

	// Size of the texture capture, when the values are different to m_Width or m_Height
	// a new opengl texture will be generated
	uint32_t m_CaptureWidth = 0; 
	uint32_t m_CaptureHeight = 0;

	uint32_t m_Width = 0;
	uint32_t m_Height = 0;

	extern HWND m_hWnd; // The companion windows
	extern HDC m_hDC;
	HGLRC m_hRC = 0; // Gl context

	// Simple Quad Shader
	GLuint m_SimpleQuadShaderID = 0;
	GLint  m_OffsetLoc = -1;
	GLint  m_nPosition = -1;
	GLint  m_nTexCoord = -1;
	GLint  m_ScaleLoc = -1;
	float m_fIPD = 0.06f;
	float m_fScale[2] = { 1.0f,1.0f };


	struct FramebufferDesc
	{
		GLuint m_LeftTextureId;
		GLuint m_RightTextureId;
		GLuint m_nRenderFramebufferId;
	};
	FramebufferDesc m_frameBuffer;


	GLuint CompileGLShader (const char *pchShaderName, const char *pchVertexShader, const char *pchFragmentShader)
	{
		GLuint unProgramID = glCreateProgram ();

		GLuint nSceneVertexShader = glCreateShader (GL_VERTEX_SHADER);
		glShaderSource (nSceneVertexShader, 1, &pchVertexShader, NULL);
		glCompileShader (nSceneVertexShader);

		GLint vShaderCompiled = GL_FALSE;
		glGetShaderiv (nSceneVertexShader, GL_COMPILE_STATUS, &vShaderCompiled);
		if (vShaderCompiled != GL_TRUE)
		{
			printf ("%s - Unable to compile vertex shader %d!\n", pchShaderName, nSceneVertexShader);
			glDeleteProgram (unProgramID);
			glDeleteShader (nSceneVertexShader);
			return 0;
		}
		glAttachShader (unProgramID, nSceneVertexShader);
		glDeleteShader (nSceneVertexShader); // the program hangs onto this once it's attached

		GLuint  nSceneFragmentShader = glCreateShader (GL_FRAGMENT_SHADER);
		glShaderSource (nSceneFragmentShader, 1, &pchFragmentShader, NULL);
		glCompileShader (nSceneFragmentShader);

		GLint fShaderCompiled = GL_FALSE;
		glGetShaderiv (nSceneFragmentShader, GL_COMPILE_STATUS, &fShaderCompiled);
		if (fShaderCompiled != GL_TRUE)
		{
			printf ("%s - Unable to compile fragment shader %d!\n", pchShaderName, nSceneFragmentShader);
			glDeleteProgram (unProgramID);
			glDeleteShader (nSceneFragmentShader);
			return 0;
		}

		glAttachShader (unProgramID, nSceneFragmentShader);
		glDeleteShader (nSceneFragmentShader); // the program hangs onto this once it's attached

		glLinkProgram (unProgramID);

		GLint programSuccess = GL_TRUE;
		glGetProgramiv (unProgramID, GL_LINK_STATUS, &programSuccess);
		if (programSuccess != GL_TRUE)
		{
			printf ("%s - Error linking program %d!\n", pchShaderName, unProgramID);
			glDeleteProgram (unProgramID);
			return 0;
		}

		glUseProgram (unProgramID);
		glUseProgram (0);

		return unProgramID;
	}

	void initGL (HDC hDC)
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

		GLuint PixelFormat = ChoosePixelFormat (hDC, &pfd);
		SetPixelFormat (hDC, PixelFormat, &pfd);
		m_hRC = wglCreateContext (hDC);
		wglMakeCurrent (hDC, m_hRC);

		if (!m_hDC || !m_hRC)
			throw std::invalid_argument ("Invalid HDC or HRC");

		// Initialize Glad,, Should be called after the context creation!!
		if (!gladLoadGL ())
			throw std::runtime_error ("unable to initialize GLAD");

		glViewport (0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
		glDisable (GL_DEPTH_TEST);
	}

	// Paint a screen quad with inverted in Y
	void paintScreenQuad ()
	{		
		// Array for full-screen quad
		static const GLfloat verts[] =
		{
			-1.0f, -1.0f, 0.0f, 1.0f, -1.0f, 0.0f, 1.0f, 1.0f, 0.0f,
			-1.0f, -1.0f, 0.0f, 1.0f, 1.0f, 0.0f, -1.0f, 1.0f, 0.0f
		};

		// Inverted Y
		static const GLfloat tcInv[] =
		{
			0.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.0f,
			0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 0.0f
		};

		glUniform1f (m_OffsetLoc, m_fIPD);
		glUniform2fv (m_ScaleLoc, 1, m_fScale);

		glVertexAttribPointer (m_nPosition, 3, GL_FLOAT, GL_FALSE, 0, verts);
		glEnableVertexAttribArray (m_nPosition);  // Vertex position
		LOGGLError ();
		
		glVertexAttribPointer (m_nTexCoord, 2, GL_FLOAT, GL_FALSE, 0, tcInv);
		glEnableVertexAttribArray (m_nTexCoord);  // Texture coordinates
		LOGGLError ();

		glDrawArrays (GL_TRIANGLES, 0, 6);
		LOGGLError ();
		
		glDisableVertexAttribArray (m_nTexCoord);
		glDisableVertexAttribArray (m_nPosition);
		LOGGLError ();
	}


	void createCaptureTexture (int nWidth, int nHeight)
	{
		glGenTextures (1, &m_nCaptureTextureId);
		glBindTexture (GL_TEXTURE_2D, m_nCaptureTextureId);
		glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
		glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_MIRRORED_REPEAT);
		glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_MIRRORED_REPEAT);
		glTexImage2D (GL_TEXTURE_2D, 0, GL_RGBA8, nWidth, nHeight, 0, GL_BGRA, GL_UNSIGNED_BYTE, nullptr);
		glBindTexture (GL_TEXTURE_2D, 0);

		m_Width = nWidth;
		m_Height = nHeight;
	}

	bool createFrameBuffer (int nWidth, int nHeight)
	{
		glGenFramebuffers (1, &m_frameBuffer.m_nRenderFramebufferId);
		glBindFramebuffer (GL_FRAMEBUFFER, m_frameBuffer.m_nRenderFramebufferId);

		glGenTextures (1, &m_frameBuffer.m_LeftTextureId);
		glBindTexture (GL_TEXTURE_2D, m_frameBuffer.m_LeftTextureId);
		glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
		glTexImage2D (GL_TEXTURE_2D, 0, GL_RGBA8, nWidth, nHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

		glGenTextures (1, &m_frameBuffer.m_RightTextureId);
		glBindTexture (GL_TEXTURE_2D, m_frameBuffer.m_RightTextureId);
		glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
		glTexImage2D (GL_TEXTURE_2D, 0, GL_RGBA8, nWidth, nHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

		// Attach textures to FBO
		glFramebufferTexture2D (GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_frameBuffer.m_LeftTextureId, 0);
		glFramebufferTexture2D (GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, m_frameBuffer.m_RightTextureId, 0);

		// check FBO status
		GLenum status = glCheckFramebufferStatus (GL_FRAMEBUFFER);
		if (status != GL_FRAMEBUFFER_COMPLETE)
			return false;

		glBindFramebuffer (GL_FRAMEBUFFER, 0);
		LOGGLError ();
		return true;
	}

	void createShaders ()
	{
		// Check for already initialized
		if (m_SimpleQuadShaderID)
			return;

		m_SimpleQuadShaderID = CompileGLShader ("CompanionWindow",

			// vertex shader
			"#version 410 core\n"
			"layout(location = 0) in vec4 position;\n"
			"layout(location = 1) in vec2 v2UVIn;\n"
			"noperspective out vec2 v2UV;\n"
			"void main()\n"
			"{\n"
			"	v2UV = v2UVIn;\n"
			"	gl_Position = position;\n"
			"}\n",

			// fragment shader
			"#version 410 core\n"
			"#define LEFT_EYE    0\n"
			"#define RIGHT_EYE    1\n"
			"uniform float Offset;\n"
			"uniform vec2 ScaleUV;\n"
			"uniform sampler2D mytexture;\n"
			"noperspective in vec2 v2UV;\n"
			"layout (location = LEFT_EYE) out vec4 outFragColorLeft;\n"
			"layout (location = RIGHT_EYE) out vec4 outFragColorRight;\n"

			"void main()\n"
			"{\n"
			"       vec2 uv = v2UV * ScaleUV;\n"
			"       vec4 color0 = texture(mytexture, uv-vec2(Offset,0));\n"
			"       vec4 color1 = texture(mytexture, uv+vec2(Offset,0));\n"			
			"       outFragColorLeft = vec4(color0.b,color0.g,color0.r,1.0);\n"
			"       outFragColorRight = vec4(color1.b,color1.g,color1.r,1.0);\n"
			"}\n"
		);

		// Retrieve uniforms
		if (m_SimpleQuadShaderID)
		{
			glUseProgram (m_SimpleQuadShaderID);
			m_OffsetLoc = glGetUniformLocation (m_SimpleQuadShaderID, "Offset");
			m_ScaleLoc = glGetUniformLocation (m_SimpleQuadShaderID, "ScaleUV");;
			m_nPosition = glGetAttribLocation (m_SimpleQuadShaderID, "position");
			m_nTexCoord = glGetAttribLocation (m_SimpleQuadShaderID, "v2UVIn");
		}
	}

	void destroyCaptureTexture ()
	{
		glDeleteTextures (1, &m_nCaptureTextureId);
		m_nCaptureTextureId = 0;
	}
	
	// PURPOSE: fill to textures of size [m_VRWidth,m_VRHeight] with the texture captured
	void renderStereoTargets ()
	{
		// for copy texture see:
		// see https://stackoverflow.com/questions/19564657/flipping-texture-when-copying-to-another-texture
		// https://www.khronos.org/opengl/wiki/Pixel_Buffer_Object

		LOGGLError ();

		glViewport (0, 0, m_VRWidth, m_VRHeight);
		LOGGLError ();

		// Left & Right Eye
		glBindFramebuffer (GL_FRAMEBUFFER, m_frameBuffer.m_nRenderFramebufferId);

		static const GLenum drawBuffers[] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1 };
		glDrawBuffers (2, drawBuffers);
		LOGGLError ();

		glBindTexture (GL_TEXTURE_2D, m_nCaptureTextureId);
		glUseProgram (m_SimpleQuadShaderID);
		
		paintScreenQuad ();

		LOGGLError ();

		glBindFramebuffer (GL_FRAMEBUFFER, 0);
		LOGGLError ();

		glFinish ();
		glFlush ();
	}

	void renderBegin ()
	{
		LOGGLError ();

		glViewport (0, 0, m_VRWidth, m_VRHeight);
		LOGGLError ();

		// Left & Right Eye
		glBindFramebuffer (GL_FRAMEBUFFER, m_frameBuffer.m_nRenderFramebufferId);

		static const GLenum drawBuffers[] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1 };
		glDrawBuffers (2, drawBuffers);
		LOGGLError ();

		//////////////////////////////////////////////////////////////////////////
		// paintScreenQuad
		glBindTexture (GL_TEXTURE_2D, m_nCaptureTextureId);
		glUseProgram (m_SimpleQuadShaderID);

		// Array for full-screen quad
		static const GLfloat verts[] =
		{
			-1.0f, -1.0f, 0.0f, 1.0f, -1.0f, 0.0f, 1.0f, 1.0f, 0.0f,
			-1.0f, -1.0f, 0.0f, 1.0f, 1.0f, 0.0f, -1.0f, 1.0f, 0.0f
		};

		// Inverted Y
		static const GLfloat tcInv[] =
		{
			0.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.0f,
			0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 0.0f
		};

		glUniform1f (m_OffsetLoc, m_fIPD);
		glUniform2fv (m_ScaleLoc, 1, m_fScale);

		glVertexAttribPointer (m_nPosition, 3, GL_FLOAT, GL_FALSE, 0, verts);
		glEnableVertexAttribArray (m_nPosition);  // Vertex position
		LOGGLError ();

		glVertexAttribPointer (m_nTexCoord, 2, GL_FLOAT, GL_FALSE, 0, tcInv);
		glEnableVertexAttribArray (m_nTexCoord);  // Texture coordinates
		LOGGLError ();
	}

	void renderDO ()
	{
		glDrawArrays (GL_TRIANGLES, 0, 6);
		glUniform1f (m_OffsetLoc, m_fIPD);
	}

	void renderEnd ()
	{
		glDisableVertexAttribArray (m_nTexCoord);
		glDisableVertexAttribArray (m_nPosition);
		glBindFramebuffer (GL_FRAMEBUFFER, 0);
		LOGGLError ();

		glFinish ();
		glFlush ();
	}

	void waitVr ()
	{
		glFinish ();
		glFlush ();
		// Get VR headset poses
		vr::TrackedDevicePose_t poses[vr::k_unMaxTrackedDeviceCount] = {};

		if (vr::VRCompositor () &&
			vr::VRCompositor ()->WaitGetPoses (
				poses, vr::k_unMaxTrackedDeviceCount, nullptr, 0) == vr::EVRCompositorError::VRCompositorError_None)
		{
		}
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

		vr::VRCompositor ()->PostPresentHandoff ();
		LOGGLError ();
	}

	// change the capture textures to the capture size
	void onResetCapture ()
	{
		destroyCaptureTexture ();
		createCaptureTexture (m_CaptureWidth, m_CaptureHeight);

		// let the texture be binded to avoid extra bind
		// when using renderDO
		glBindTexture (GL_TEXTURE_2D, m_nCaptureTextureId);
	}

	void adquire ()
	{
		if (!m_Buffer)
			return;

		// Check for valid texture
		if (m_CaptureWidth != m_Width || m_CaptureHeight != m_Height ||
			!m_nCaptureTextureId)
			onResetCapture ();


		GLint alignment = 0;
		glBindTexture (GL_TEXTURE_2D, m_nCaptureTextureId);
		glGetIntegerv (GL_UNPACK_ALIGNMENT, &alignment);
		glPixelStorei (GL_UNPACK_ALIGNMENT, 1);
		glTexSubImage2D (GL_TEXTURE_2D, 0, 0, 0, m_Width, m_Height, GL_RGBA, GL_UNSIGNED_BYTE, (void*)m_Buffer);
		glPixelStorei (GL_UNPACK_ALIGNMENT, alignment);
		LOGGLError ();
		m_Dirty = false;
		m_Buffer = nullptr;
	}

	void startVr ()
	{
		vr::EVRInitError e = vr::VRInitError_None;
		vr::VR_Init (&e, vr::EVRApplicationType::VRApplication_Scene);

		if (e != vr::VRInitError_None || !vr::VRCompositor ())
		{
			LOG (ERROR) << "Failed to initialize VR system with error code " << e << ".";
		}

		CreateCompanion ();
		initGL (m_hDC);
		
		//////////////////////////////////////////////////////////////////////////
		// Initialize constant objects
		createShaders ();
		if (!m_VRWidth || !m_VRHeight)
			vr::VRSystem ()->GetRecommendedRenderTargetSize (&m_VRWidth, &m_VRHeight);

		createFrameBuffer (m_VRWidth, m_VRHeight);

		m_Capturing = true;
	}

	void shutdownVr ()
	{
		wglMakeCurrent (0, 0);

		// Delete RC
		wglDeleteContext (m_hRC);

		// Release DC
		ReleaseDC (m_hWnd, m_hDC);

		// Destroy windows
		DestroyWindow (m_hWnd);
		
		// shutdown OpenVr
		vr::VR_Shutdown ();
	}

	void mainLoop ()
	{
		startVr ();

		adquire ();
		renderBegin ();
		// main loop
		while (m_Capturing)
		{
			if (m_Dirty)
			{
				adquire ();
				//renderStereoTargets ();
				renderDO ();
				submitFrames (m_frameBuffer.m_LeftTextureId, m_frameBuffer.m_RightTextureId);				
				waitVr ();
			}
		}

		renderEnd ();

		shutdownVr ();
	}

	//////////////////////////////////////////////////////////////////////////
	void Init ()
	{
	}

	void Shutdown ()
	{
		m_Capturing = false;

		Sleep (100);
		vr::VR_Shutdown ();
	}

	void AcquireContext (uint8_t* buffer)
	{
		//vr::VRCompositor ()->ClearLastSubmittedFrame ();
		if (!buffer)
			throw std::invalid_argument ("Null parameter");

// 		// force to syncronized previous frame
// 		vr::VRCompositor ()->PostPresentHandoff ();
		while (m_Dirty);

		// Skip this frame if it didn't finished 
		//if (m_Dirty)
		//	return;

		m_Buffer = buffer;
		m_Dirty = true;

		
		// Wait for the other thread to copy the resource
		//while (m_Dirty);


		// Pass texture to VR hmd
	}

	void SetCaptureSize (uint32_t width, uint32_t height)
	{		
		if (!m_hRC || !m_hDC)
		{
			m_CaptureWidth = width;
			m_CaptureHeight = height;
			m_thread = std::thread (mainLoop);     // spawn new thread that calls foo()
			if ( m_thread.joinable () )
				m_thread.detach ();
		}
		
		// Wait until the initialization is finished
		while (!m_Capturing)
		{
			Sleep (100);
		}

		// In case of change ion resolution, note the new resolution
		// and wait a second to allow the other thread to catch up
		if (m_CaptureWidth != width || m_CaptureHeight != height)
		{
			m_CaptureWidth = width;
			m_CaptureHeight = height;
			// wait a second 
			Sleep (1000);
		}
	}

	void SetIPD (float fIpd)
	{
		m_fIPD = fIpd;
	}

	void SetScale (float u, float v)
	{
		m_fScale[0] = u;
		m_fScale[1] = v;
	}

	void SetTargetSize (uint32_t width, uint32_t height)
	{
		// It is only possible to change the target size at initialization
		if (bInitialized)
			return;

		m_VRWidth = width;
		m_VRHeight = height;
		bInitialized = true;
	}
}
