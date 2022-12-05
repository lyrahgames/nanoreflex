#version 420 core

uniform mat4 projection;
uniform mat4 view;

layout (location = 0) in vec3 p;
layout (location = 1) in vec3 n;

out vec3 normal;

void main(){
  gl_Position = projection * view * vec4(p, 1.0);
  normal = vec3(view * vec4(n, 0.0));
}
