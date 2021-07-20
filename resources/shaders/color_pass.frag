#version 330 core
out vec4 FragColor;
in vec4 GeomColor;

void main()
{
    FragColor = GeomColor;
}