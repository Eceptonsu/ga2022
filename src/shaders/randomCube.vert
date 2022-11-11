#version 450

layout (location = 0) in vec3 inPos;
layout (location = 1) in vec3 inColor;

layout (binding = 0) uniform UBO 
{
	mat4 projectionMatrix;
	mat4 modelMatrix;
	mat4 viewMatrix;
} ubo;

layout (location = 0) out vec3 outColor;

out gl_PerVertex
{
    vec4 gl_Position;
};

float random(float i) {
	return mod(4000.*sin(23464.345*i+45.345),1.);
}

void main()
{
	gl_Position = ubo.projectionMatrix * ubo.viewMatrix * ubo.modelMatrix * vec4(inPos.xyz, 1.0);
    vec3 randColor = 0.5 - 0.5*cos(gl_Position.xyz+vec3(0,1,2))/random(gl_Position.y + gl_Position.z * 7.)+0.05;
	outColor = vec3(randColor);
}
