#version 330 core

layout (location = 0) out vec4 frag_color;

void main(){
  // frag_color = vec4(1.0, 0.8, 0.4, 1.0);
  frag_color = vec4(vec3(0.2), 1.0);
}
