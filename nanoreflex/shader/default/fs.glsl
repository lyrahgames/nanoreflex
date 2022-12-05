#version 330 core

in vec3 normal;

layout (location = 0) out vec4 frag_color;

void main(){
  vec3 n = normalize(normal);
  vec3 view_dir = vec3(0.0, 0.0, 1.0);
  vec3 light_dir = view_dir;
  float d = max(dot(light_dir, n), 0.0);
  vec3 color = vec3(0.1 + 1.0 * pow(d,1000) + 0.85 * pow(d,0.1));
  frag_color = vec4(color, 1.0);
}
