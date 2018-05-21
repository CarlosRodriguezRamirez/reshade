#version 100
// This is a simple 2 bone skinning shader

// Attributes
attribute vec3 iPosition;
attribute vec3 iNormal;
attribute vec4 iWeights;          // Contains [boneWeight0, boneIndex0, boneWeight1, boneIndex1]

// Uniforms
uniform mat4 ModelViewProjection;
uniform mat4 Bones[14];            // Our simple skeleton only has 9 bones
uniform vec3 LightDir0;
uniform vec3 LightDir1;

varying lowp vec4 color;

void main()
{
    highp vec3 skinnedPosition;   // Location of vertex after skinning in model space
    vec3       skinnedNormal;     // Normal for vertex after skinning in model space

    // The position of the skinned vertex is a weighted sum of the individual bone matrices applied to the position
    float boneWeight0 = iWeights[0];
    int   boneIndex0  = int(iWeights[1]);
    float boneWeight1 = iWeights[2];
    int   boneIndex1  = int(iWeights[3]);

    // Compute the skinned position and normal
    skinnedPosition = (boneWeight0 * (Bones[boneIndex0] * vec4(iPosition, 1.0)) +
                    boneWeight1 * (Bones[boneIndex1] * vec4(iPosition, 1.0))).xyz;

    // The normal of the skinned vertex is a weighted sum of the individual bone matrices applied to the normal.
    // Only the 3x3 rotation portion of the bone matrix is applied to the normal
    skinnedNormal = boneWeight0 * (mat3(Bones[boneIndex0]) * iNormal) +
                boneWeight1 * (mat3(Bones[boneIndex1]) * iNormal);
    skinnedNormal = normalize(skinnedNormal);


    // Compute the final position
    gl_Position = ModelViewProjection * vec4(skinnedPosition, 1.0);

    // Compute the vertex color.
    color.w = 1.0;
    // Compute a simple diffuse lit vertex color with two lights
    color.xyz = vec3(0.4, 0.0, 0.0) * dot(skinnedNormal, LightDir0) + vec3(0.4, 0.2, 0.2);
    color.xyz += vec3(0.4, 0.0, 0.0) * dot(skinnedNormal, LightDir1);
}