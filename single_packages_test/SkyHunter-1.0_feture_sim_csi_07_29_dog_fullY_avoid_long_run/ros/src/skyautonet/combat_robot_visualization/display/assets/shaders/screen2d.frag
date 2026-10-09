#version 300 es
precision highp float;
precision highp int;
precision mediump sampler2D;

out vec4 FragColor;
in vec2 TexCoord;
uniform sampler2D tex;
uniform float opacity;

void main()
{
    FragColor = texture(tex, TexCoord);
    FragColor.a = FragColor.a * opacity;
}