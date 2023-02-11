#version 330 core

layout (triangles) in;
layout (line_strip, max_vertices = 2) out;

in vec3 position[];
in vec3 normal[];

void main(){
  vec4 a = gl_in[0].gl_Position;
  vec4 b = gl_in[1].gl_Position;
  vec4 c = gl_in[2].gl_Position;

  float sa = dot(normal[0], position[0]);
  float sb = dot(normal[1], position[1]);
  float sc = dot(normal[2], position[2]);

  if (sa * sb < 0) {
    gl_Position = ((abs(sb) * a + abs(sa) * b) / (abs(sa) + abs(sb)));
    EmitVertex();
  }
  if (sa * sc < 0) {
    gl_Position = ((abs(sc) * a + abs(sa) * c) / (abs(sa) + abs(sc)));
    EmitVertex();
  }
  if (sb * sc < 0) {
    gl_Position = ((abs(sc) * b + abs(sb) * c) / (abs(sb) + abs(sc)));
    EmitVertex();
  }
  EndPrimitive();
}
