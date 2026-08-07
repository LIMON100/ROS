#version 300 es
precision highp float;
precision highp int;

layout (location = 0) in vec2 aPos;
layout (location = 1) in vec2 aTexCoord;

uniform vec2 screen_size;
uniform vec4 pos_size;
uniform vec2 additional_move;

out vec2 TexCoord;

void main()
{
    TexCoord = vec2(aTexCoord.x, aTexCoord.y);
    vec2 pos = (-1.0 + 2.0 * (pos_size.xy / screen_size)) + aPos * (pos_size.zw / screen_size) + (2.0 * additional_move / screen_size);
    gl_Position = vec4(pos.xy, 0.0, 1.0);
}