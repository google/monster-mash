// Copyright 2020-2021 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef SHADERTEXVERTCOORDS_H
#define SHADERTEXVERTCOORDS_H

#ifndef SHADERTEXVERTCOORDS_VERT
#define SHADERTEXVERTCOORDS_VERT \
  "#version 100\n \
uniform mat4 view;\n \
uniform mat4 proj;\n \
uniform mat4 normalMatrix;\n \
attribute vec3 position;\n \
attribute vec3 normal;\n \
attribute vec3 texCoord;\n \
varying vec3 frag_texCoord;\n \
varying vec3 frag_normal;\n \
varying vec3 frag_lightDirection;\n \
mat3 transpose(mat3 m) {\n \
  return mat3(m[0][0], m[1][0], m[2][0],\n \
              m[0][1], m[1][1], m[2][1],\n \
              m[0][2], m[1][2], m[2][2]);\n \
}\n \
void main() {\n \
  vec4 p = vec4(position, 1);\n \
  frag_normal = normalize(normal);\n \
  frag_lightDirection = transpose(mat3(normalMatrix)) * vec3(0,0,1);\n \
  frag_texCoord = texCoord;\n \
  gl_Position = proj * view * p;\n \
}"
#endif

#ifndef SHADERTEXVERTCOORDS_FRAG
#define SHADERTEXVERTCOORDS_FRAG \
  "#version 100\n \
precision mediump float;\n \
uniform sampler2D tex;\n \
uniform float useShading;\n \
varying vec3 frag_normal;\n \
varying vec3 frag_lightDirection;\n \
varying vec3 frag_texCoord;\n \
void main() {\n \
  vec4 ctex = texture2D(tex, frag_texCoord.xy).rgba;\n \
  if (useShading > 0.) {\n \
    vec3 diffuse = max(dot(frag_normal, frag_lightDirection), 0.) * vec3(1.,1.,1.);\n \
    vec4 diffuseColor = vec4(diffuse * vec3(1.,1.,1.), 1.);\n \
    gl_FragColor = ctex * (0.25+0.75*diffuseColor);\n \
  } else {\n \
    gl_FragColor = ctex;\n \
  }\n \
}"
#endif

#endif
