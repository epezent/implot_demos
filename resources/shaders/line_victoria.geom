#version 330

uniform float Weight  = 1;
uniform float Miter   = 0.1;
uniform vec2 Viewport;

layout(lines_adjacency) in;
layout(triangle_strip, max_vertices = 7) out;

vec2 toScreenSpace(vec4 vertex)
{
    return vec2( vertex.xy / vertex.w ) * Viewport;
}

void drawSegment(vec2 points[4])
{
    float half_weight = Weight * 0.5;

    vec2 p0 = points[0];
    vec2 p1 = points[1];
    vec2 p2 = points[2];
    vec2 p3 = points[3];

    /* perform naive culling */
    vec2 area = Viewport * 4;
    if( p1.x < -area.x || p1.x > area.x ) return;
    if( p1.y < -area.y || p1.y > area.y ) return;
    if( p2.x < -area.x || p2.x > area.x ) return;
    if( p2.y < -area.y || p2.y > area.y ) return;

    /* determine the direction of each of the 3 segments (previous, current, next) */
    vec2 v0 = normalize( p1 - p0 );
    vec2 v1 = normalize( p2 - p1 );
    vec2 v2 = normalize( p3 - p2 );

    /* determine the normal of each of the 3 segments (previous, current, next) */
    vec2 n0 = vec2( -v0.y, v0.x );
    vec2 n1 = vec2( -v1.y, v1.x );
    vec2 n2 = vec2( -v2.y, v2.x );

    /* determine miter lines by averaging the normals of the 2 segments */
    vec2 miter_a = normalize( n0 + n1 );	// miter at start of current segment
    vec2 miter_b = normalize( n1 + n2 ); // miter at end of current segment

    /* determine the length of the miter by projecting it onto normal and then inverse it */
    float an1 = dot(miter_a, n1);
    float bn1 = dot(miter_b, n2);
    if (an1==0) an1 = 1;
    if (bn1==0) bn1 = 1;
    float length_a = half_weight / an1;
    float length_b = half_weight / bn1;

    /* prevent excessively long miters at sharp corners */
    if( dot( v0, v1 ) < -Miter ) {
        miter_a = n1;
        length_a = half_weight;

        /* close the gap */
        if( dot( v0, n1 ) > 0 ) {
            gl_Position = vec4( ( p1 + half_weight * n0 ) / Viewport, 0.0, 1.0 );
            EmitVertex();

            gl_Position = vec4( ( p1 + half_weight * n1 ) / Viewport, 0.0, 1.0 );
            EmitVertex();

            gl_Position = vec4( p1 / Viewport, 0.0, 1.0 );
            EmitVertex();

            EndPrimitive();
        }
        else {
            gl_Position = vec4( ( p1 - half_weight * n1 ) / Viewport, 0.0, 1.0 );
            EmitVertex();

            gl_Position = vec4( ( p1 - half_weight * n0 ) / Viewport, 0.0, 1.0 );
            EmitVertex();

            gl_Position = vec4( p1 / Viewport, 0.0, 1.0 );
            EmitVertex();

            EndPrimitive();
        }
    }
    if( dot( v1, v2 ) < -Miter ) {
        miter_b = n1;
        length_b = half_weight;
    }
    // generate the triangle strip
    gl_Position = vec4( ( p1 + length_a * miter_a ) / Viewport, 0.0, 1.0 );
    EmitVertex();

    gl_Position = vec4( ( p1 - length_a * miter_a ) / Viewport, 0.0, 1.0 );
    EmitVertex();

    gl_Position = vec4( ( p2 + length_b * miter_b ) / Viewport, 0.0, 1.0 );
    EmitVertex();

    gl_Position = vec4( ( p2 - length_b * miter_b ) / Viewport, 0.0, 1.0 );
    EmitVertex();

    EndPrimitive();
}

void main(void)
{
    vec2 points[4];
    points[0] = toScreenSpace(gl_in[0].gl_Position);
    points[1] = toScreenSpace(gl_in[1].gl_Position);
    points[2] = toScreenSpace(gl_in[2].gl_Position);
    points[3] = toScreenSpace(gl_in[3].gl_Position);
    drawSegment(points);
}