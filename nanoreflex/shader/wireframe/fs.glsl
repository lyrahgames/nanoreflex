#version 330 core

in vec3 pos;
in vec3 nor;
noperspective in vec3 edge_distance;

layout (location = 0) out vec4 frag_color;

void main(){
  // Compute distance from edges.
  float d = min(edge_distance.x, edge_distance.y);
  d = min(d, edge_distance.z);
  float line_width = 0.3;
  float line_delta = 1.0;
  vec4 line_color = vec4(vec3(0.2), 0.8);
  float mix_value = smoothstep(line_width - line_delta, line_width + line_delta, d);
  // Compute viewer shading.
  float s = abs(normalize(nor).z);
  float light = 0.2 + 1.0 * pow(s, 1000) + 0.75 * pow(s, 0.2);
  // float light = 0.2 + 0.75 * pow(s, 0.2);
  vec4 light_color = vec4(vec3(light), 0.8);
  // Mix both color values.
  frag_color = mix(line_color, light_color, mix_value);
    // if (mix_value > 0.9) discard;
  //   frag_color = (1 - mix_value) * line_color;
}
