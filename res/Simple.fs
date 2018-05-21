#version 100
// Trivial fragment shader that just returns the interpolated vertex color

precision mediump float;
varying lowp vec4 color;

void main()
{
    gl_FragColor = color;
}