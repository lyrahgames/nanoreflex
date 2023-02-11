#version 330 core

flat in float light;

layout (location = 0) out vec4 frag_color;

void main(){
  frag_color = vec4(vec3(light), 1.0);
}
