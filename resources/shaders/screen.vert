#version 330 core
layout (location = 0) in vec2 Position;
layout (location = 1) in vec4 Color;
uniform mat4 ProjMtx;
out vec4 VertColor;
void main()
{
    VertColor = Color;
    gl_Position = ProjMtx * vec4(Position.xy, 0.0, 1.0);
}