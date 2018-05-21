#include <Windows.h>
#include <Leap.h>
#include <memory>
#include "VRLeap.h"
#include "glad\glad.h" //Generated from http://glad.dav1d.de/
#include <glm\glm.hpp>

namespace VRAdapter
{
	GLuint CompileGLShader (const char *pchShaderName, const char *pchVertexShader, const char *pchFragmentShader);
}

// Check ASSIMP for a FBX loader.
namespace
{
	// Simple Quad Shader
	GLuint m_SimpleLineShaderID = 0;

	std::unique_ptr<Leap::Controller> gController;
	void display (const Leap::Frame& frame)
	{
		Leap::HandList handlist = frame.hands ();

		for (auto hand : handlist)
		{
			//Distal ends of bones for each digit
			for (int f = 0; f < 5; f++)
			{
				auto finger = hand.finger (f);

			}
			// End of draw hand

		}
	}
	void createShaders ()
	{
		// Check for already initialized
		if (m_SimpleLineShaderID)
			return;

		m_SimpleLineShaderID = VRAdapter::CompileGLShader ("CompanionWindow",

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
		if (m_SimpleLineShaderID)
		{
//			glUseProgram (m_SimpleLineShaderID);
// 			m_OffsetLoc = glGetUniformLocation (m_SimpleLineShaderID, "Offset");
// 			m_ScaleLoc = glGetUniformLocation (m_SimpleLineShaderID, "ScaleUV");;
// 			m_nPosition = glGetAttribLocation (m_SimpleLineShaderID, "position");
// 			m_nTexCoord = glGetAttribLocation (m_SimpleLineShaderID, "v2UVIn");
		}
	}

}

namespace VRLeap
{
	void display (const Leap::Frame& frame)
	{
		Leap::HandList handlist = frame.hands ();

		for (auto hand : handlist)
		{
			//Distal ends of bones for each digit
			for (int f = 0; f < 5; f++)
			{
				auto finger = hand.finger (f);
			}
			// End of draw hand

		}

		if (!m_SimpleLineShaderID)
			createShaders ();
	}

	void Initialize ()
	{
		gController = std::make_unique<Leap::Controller> ();
	}

	void UpdateFrame ()
	{
		if (!gController->isConnected ())
			return;
		gController->setPolicyFlags (Leap::Controller::POLICY_OPTIMIZE_HMD);

		Leap::Frame frame = gController->frame ();
		std::cout << "Frame id: " << frame.id ()
			<< ", timestamp: " << frame.timestamp ()
			<< ", hands: " << frame.hands ().count ()
			<< ", fingers: " << frame.fingers ().count ();

		if (frame.hands ().count () > 0)
		{
			int a = 0;
			a++;
		}

		display (frame);
	}

}