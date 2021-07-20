#version 330 core
layout (lines) in; // GL_LINE_STRIP
layout (triangle_strip, max_vertices = 8) out;

uniform float Weight;
uniform float Fringe;
uniform vec2 Viewport;
uniform vec4 Color;

out vec4 VertColor;  

vec2 toScreenSpace(vec4 vertex)
{
    return vec2( vertex.xy / vertex.w ) * Viewport;
}

void main() {   

    vec2 p1 = toScreenSpace(gl_in[0].gl_Position);
    vec2 p2 = toScreenSpace(gl_in[1].gl_Position);
    vec2 v = p2 - p1;
    vec2 n = normalize(v);
    
    vec2 w = n * Weight * 0.5;
    vec2 f = n * (Weight + Fringe) * 0.5;

    vec4 alpha = vec4(Color.rgb, 0);

    VertColor = alpha;
    gl_Position = vec4(vec2(p1.x + f.y, p1.y - f.x) / Viewport, 0, 1);
    EmitVertex();
    VertColor = alpha;
    gl_Position = vec4(vec2(p2.x + f.y, p2.y - f.x) / Viewport, 0, 1);
    EmitVertex();

    VertColor = Color;
    gl_Position = vec4(vec2(p1.x + w.y, p1.y - w.x) / Viewport, 0, 1);
    EmitVertex();
    VertColor = Color;
    gl_Position = vec4(vec2(p2.x + w.y, p2.y - w.x) / Viewport, 0, 1);
    EmitVertex();

    VertColor = Color;
    gl_Position = vec4(vec2(p1.x - w.y, p1.y + w.x) / Viewport, 0, 1);
    EmitVertex();
    VertColor = Color;
    gl_Position = vec4(vec2(p2.x - w.y, p2.y + w.x) / Viewport, 0, 1);
    EmitVertex();

    VertColor = alpha;
    gl_Position = vec4(vec2(p1.x - f.y, p1.y + f.x) / Viewport, 0, 1);
    EmitVertex();
    VertColor = alpha;
    gl_Position = vec4(vec2(p2.x - f.y, p2.y + f.x) / Viewport, 0, 1);
    EmitVertex();

    EndPrimitive();
}  