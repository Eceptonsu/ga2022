#version 450

layout (location = 0) in vec3 inColor;

layout (location = 0) out vec4 outFragColor;

void main()
{
  outFragColor = vec4(0, 0.39, 0, 1.0);
}