#ifndef SHADERMATCAP_H
#define SHADERMATCAP_H

#ifndef SHADERMATCAP_VERT
#define SHADERMATCAP_VERT \
"#version 100\n \
uniform mat4 view;\n \
uniform mat4 proj;\n \
uniform mat4 normalMatrix;\n \
attribute vec3 position;\n \
attribute vec3 normal;\n \
varying vec3 e;\n \
varying vec3 n;\n \
void main() {\n \
  vec4 p = vec4(position, 1);\n \
  e = normalize( vec3( view * p ) );\n \
  n = normalize( mat3(normalMatrix) * normal );\n \
  gl_Position = proj * view * p;\n \
}"
#endif

#ifndef SHADERMATCAP_FRAG
#define SHADERMATCAP_FRAG \
"#version 100\n \
precision mediump float;\n \
uniform sampler2D tMatCap;\n \
varying vec3 e;\n \
varying vec3 n;\n \
void main() {\n \
  vec3 r = reflect( e, n );\n \
  float m = 2. * sqrt( pow( r.x, 2. ) + pow( r.y, 2. ) + pow( r.z + 1., 2. ) );\n \
  vec2 vN = r.xy / m + .5;\n \
  vec3 base = texture2D( tMatCap, vN ).rgb;\n \
  gl_FragColor = vec4(base, 1.0);\n \
}"
#endif

#endif
