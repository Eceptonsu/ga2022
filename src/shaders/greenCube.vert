#version 450

layout (location = 0) in vec3 inPos;
layout (location = 1) in vec3 inColor;
layout (location = 2) in vec3 inNormal;

layout (binding = 0) uniform UBO 
{
	mat4 projectionMatrix;
	mat4 modelMatrix;
	mat4 viewMatrix;
	vec4 color;
} ubo;

layout (location = 0) out vec4 outColor;
layout (location = 1) out vec3 Fragpos;
layout (location = 2) out vec3 Normal;

out gl_PerVertex
{
    vec4 gl_Position;
};

void main()
{
	Fragpos = vec3(ubo.modelMatrix * vec4(inPos, 1.0));
	Normal = mat3(transpose(inverse(ubo.modelMatrix))) * inNormal;  

	outColor = ubo.color;
	gl_Position = ubo.projectionMatrix * ubo.viewMatrix * ubo.modelMatrix * vec4(inPos.xyz, 1.0);
}
