#version 420 core

uniform mat4 projection;
uniform mat4 view;

layout (location = 0) in vec3 p;

void main(){
  gl_Position = projection * view * vec4(p, 1.0);
}
