#version 330 core
layout (lines) in;
layout (triangle_strip, max_vertices = 6) out;

uniform vec2 Scale;

in vec4 VertColor[];
out vec4 GeomColor;  

void main() {   

    vec2 p1 = gl_in[0].gl_Position.xy / Scale;
    vec2 p2 = gl_in[1].gl_Position.xy / Scale;
    vec2 v = p2 - p1;
    vec2 n = normalize(v);

    GeomColor = vec4(VertColor[0].rgb, 0);
    gl_Position = vec4(vec2(p1.x + n.y, p1.y - n.x) * Scale, 0, 1);
    EmitVertex();
    GeomColor = vec4(VertColor[1].rgb, 0);
    gl_Position = vec4(vec2(p2.x + n.y, p2.y - n.x) * Scale, 0, 1);
    EmitVertex();

    GeomColor = VertColor[0];
    gl_Position = vec4(p1 * Scale, 0, 1);
    EmitVertex();
    GeomColor = VertColor[1];
    gl_Position = vec4(p2 * Scale, 0, 1);
    EmitVertex();

    GeomColor = vec4(VertColor[0].rgb, 0);;
    gl_Position = vec4(vec2(p1.x - n.y, p1.y + n.x) * Scale, 0, 1);
    EmitVertex();
    GeomColor = vec4(VertColor[1].rgb, 0);;
    gl_Position = vec4(vec2(p2.x - n.y, p2.y + n.x) * Scale, 0, 1);
    EmitVertex();

    EndPrimitive();
}  