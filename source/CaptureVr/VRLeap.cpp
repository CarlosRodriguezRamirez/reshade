#include <Windows.h>
#include <Leap.h>
#include <memory>
#include "VRLeap.h"
#include "glad\glad.h" //Generated from http://glad.dav1d.de/
#include <glm\glm.hpp>
#include <iostream>
#include <fstream>
#include "log.hpp"

#include <fx/gltf.h>

namespace VRAdapter
{	
	GLuint CompileGLShader (const char *pchShaderName, const char *pchVertexShader, const char *pchFragmentShader);
	std::string LoadFile (const char* fileName);
	uint64_t GetFileLength (std::ifstream& file);
	GLuint LoadShader (const char *pchShaderName, const char *pchVertexShader, const char *pchFragmentShader);
}

// Check ASSIMP for a FBX loader.
// https://github.com/assimp/assimp

namespace
{
	// Simple Quad Shader
	GLuint m_SkinnedID = 0;
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
		if (m_SkinnedID)
			return;

		m_SkinnedID = VRAdapter::LoadShader ("Skinned", "/Res/Skinned.vs", "./Res/Simple.fs");


		// Retrieve uniforms
		if (m_SkinnedID)
		{
//			glUseProgram (m_SimpleLineShaderID);
// 			m_OffsetLoc = glGetUniformLocation (m_SimpleLineShaderID, "Offset");
// 			m_ScaleLoc = glGetUniformLocation (m_SimpleLineShaderID, "ScaleUV");;
// 			m_nPosition = glGetAttribLocation (m_SimpleLineShaderID, "position");
// 			m_nTexCoord = glGetAttribLocation (m_SimpleLineShaderID, "v2UVIn");
		}
	}

}


#define HALF_FLOAT_ENUM(isES2) ((isES2) ? 0x8D61 : 0x140B) // GL_HALF_FLOAT_OES : GL_HALF_FLOAT


class SkinnedVertex;

class SkinnedMesh
{
public:
	int32_t m_vertexCount;      // Number of vertices in mesh
	int32_t m_indexCount;       // Number of indices in mesh
	uint32_t m_vertexBuffer;     // vertex buffer object for vertices
	uint32_t m_indexBuffer;      // vertex buffer object for indices
	bool m_initialized;      // Does the mesh have data?
	bool m_useES2;


	SkinnedMesh (void);
	~SkinnedMesh (void);

	void render (uint32_t iPositionLocation, uint32_t iNormalLocation, uint32_t iWeightsLocation);
	void reset (void);
	void update (const SkinnedVertex* vertexData, int32_t vertexCount, const uint16_t* indices, int32_t indexCount);
};

class SkinnedVertex
{
public:
	SkinnedVertex (void);

	void setPosition (float x, float y, float z) { m_position[0] = x; m_position[1] = y; m_position[2] = z; }
	void setNormal (float x, float y, float z) { m_normal[0] = x; m_normal[1] = y; m_normal[2] = z; }
	void setWeights (float weight0, float boneIndex0, float weight1, float boneIndex1)
	{
		m_weights[0] = weight0;
		m_weights[1] = boneIndex0;
		m_weights[2] = weight1;
		m_weights[3] = boneIndex1;

		// TODO: pack this into two uint32
	}

	float m_position[3];
	float m_normal[3];
	float m_weights[4];

	static const intptr_t PositionOffset = 0;   // [ 0, 5]
	static const intptr_t NormalOffset = 6;   // [6, 11]
	static const intptr_t WeightsOffset = 12;  // [12, 19]
};


////////////////////////////////////////////////////////////////////////////////
//
//  Method: SkinnedMesh::render()
//
//     This does the actual skinned mesh rendering
//
////////////////////////////////////////////////////////////////////////////////
void SkinnedMesh::render (uint32_t iPositionLocation, uint32_t iNormalLocation, uint32_t iWeightsLocation)
{
	// Bind the VBO for the vertex data
	glBindBuffer (GL_ARRAY_BUFFER, m_vertexBuffer);

	// Set up attribute for the position (3 floats)
	glVertexAttribPointer (iPositionLocation, 3, HALF_FLOAT_ENUM (m_useES2), GL_FALSE, sizeof (SkinnedVertex), (GLvoid*)SkinnedVertex::PositionOffset);
	glEnableVertexAttribArray (iPositionLocation);

	// Set up attribute for the normal (3 floats)
	glVertexAttribPointer (iNormalLocation, 3, HALF_FLOAT_ENUM (m_useES2), GL_FALSE, sizeof (SkinnedVertex), (GLvoid*)SkinnedVertex::NormalOffset);
	glEnableVertexAttribArray (iNormalLocation);

	// Set up attribute for the bone weights (4 floats)
	glVertexAttribPointer (iWeightsLocation, 4, HALF_FLOAT_ENUM (m_useES2), GL_FALSE, sizeof (SkinnedVertex), (GLvoid*)SkinnedVertex::WeightsOffset);
	glEnableVertexAttribArray (iWeightsLocation);

	// Set up the indices
	glBindBuffer (GL_ELEMENT_ARRAY_BUFFER, m_indexBuffer);

	// Do the actual drawing
	glDrawElements (GL_TRIANGLES, m_indexCount, GL_UNSIGNED_SHORT, 0);

	// Clear state
	glBindBuffer (GL_ARRAY_BUFFER, 0);
	glDisableVertexAttribArray (iPositionLocation);
	glDisableVertexAttribArray (iNormalLocation);
	glDisableVertexAttribArray (iWeightsLocation);
	glBindBuffer (GL_ELEMENT_ARRAY_BUFFER, 0);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Method: SkinnedMesh::update()
//
//       This method copies the vertex and index buffers into their respective VBO's
//
////////////////////////////////////////////////////////////////////////////////
void SkinnedMesh::update (const SkinnedVertex* vertices, int32_t vertexCount, const uint16_t* indices, int32_t indexCount)
{
	// Check to see if we have a buffer for the vertices, if not create one
	if (m_vertexBuffer == 0)
	{
		glGenBuffers (1, &m_vertexBuffer);
	}

	// Check to see if we have a buffer for the indices, if not create one
	if (m_indexCount == 0)
	{
		glGenBuffers (1, &m_indexBuffer);
	}

	// Stick the data for the vertices into its VBO
	glBindBuffer (GL_ARRAY_BUFFER, m_vertexBuffer);
	glBufferData (GL_ARRAY_BUFFER, sizeof (vertices[0]) * vertexCount, vertices, GL_STATIC_DRAW);

	// Stick the data for the indices into its VBO
	glBindBuffer (GL_ELEMENT_ARRAY_BUFFER, m_indexBuffer);
	glBufferData (GL_ELEMENT_ARRAY_BUFFER, sizeof (indices[0]) * indexCount, indices, GL_STATIC_DRAW);

	// Clear the VBO state
	glBindBuffer (GL_ARRAY_BUFFER, 0);
	glBindBuffer (GL_ELEMENT_ARRAY_BUFFER, 0);

	// Record mesh information
	m_vertexCount = vertexCount;
	m_indexCount = indexCount;
	m_initialized = true;
}

void SkinnedMesh::reset (void)
{
	if (m_vertexBuffer != 0)
	{
		glDeleteBuffers (1, &m_vertexBuffer);
	}
	m_vertexBuffer = 0;
	m_vertexCount = 0;

	if (m_indexBuffer != 0)
	{
		glDeleteBuffers (1, &m_indexBuffer);
	}
	m_indexBuffer = 0;
	m_indexCount = 0;

	m_initialized = false;
}

SkinnedMesh::SkinnedMesh (void)
{
	m_vertexBuffer = 0;
	m_indexBuffer = 0;

	m_vertexCount = 0;
	m_indexCount = 0;

	m_initialized = false;
	m_useES2 = false;
}
SkinnedMesh::~SkinnedMesh (void)
{
	reset ();
}
SkinnedVertex::SkinnedVertex (void)
{
	m_position[0] = 0.0f;
	m_position[1] = 0.0f;
	m_position[2] = 0.0f;
	m_normal[0] = 0.0f;
	m_normal[1] = 0.0f;
	m_normal[2] = 0.0f;
	m_weights[0] = 0.0f;
	m_weights[1] = 0.0f;
	m_weights[2] = 0.0f;
	m_weights[3] = 0.0f;
}


std::unique_ptr<SkinnedMesh> gHandMesh;
namespace VRLeap
{

	void drawHand ()
	{

	}
	void updateHand ()
	{

	}

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

		if (!m_SkinnedID)
			createShaders ();

		if (!gHandMesh)
			gHandMesh = std::make_unique<SkinnedMesh> ();

		
		glUseProgram (m_SkinnedID);
	}

	void Initialize ()
	{
		gController = std::make_unique<Leap::Controller> ();		
	}

	void UpdateFrame ()
	{
// 		if (!gController->isConnected ())
// 			return;
// 		gController->setPolicyFlags (Leap::Controller::POLICY_OPTIMIZE_HMD);

//		Leap::Frame frame = gController->frame ();
// 		std::cout << "Frame id: " << frame.id ()
// 			<< ", timestamp: " << frame.timestamp ()
// 			<< ", hands: " << frame.hands ().count ()
// 			<< ", fingers: " << frame.fingers ().count ();

		Leap::Frame frame;
		display (frame);
	}

}