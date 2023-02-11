#version 420 core

uniform mat4 projection;
uniform mat4 view;

layout (location = 0) in vec3 p;
layout (location = 1) in vec3 n;

flat out float light;

void main(){
  gl_Position = projection * view * vec4(p, 1.0);
  vec3 normal = vec3(view * vec4(n, 0.0));
  vec3 view_dir = vec3(0.0, 0.0, 1.0);
  vec3 light_dir = view_dir;
  float d = max(dot(light_dir, normal), 0.0);
  // light = 0.2 + 1.0 * pow(d,1000) + 0.75 * pow(d,0.2);
  light = 0.2 + 0.5 * d + 0.3 * pow(d, 10);
}
