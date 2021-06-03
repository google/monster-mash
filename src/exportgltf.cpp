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

#include "exportgltf.h"

using namespace std;

namespace exportgltf {

void ExportGltf::exportStart(const MatrixXfR &V, const MatrixXfR &N,
                             const MatrixXusR &F, const MatrixXfR &TC,
                             const int nFrames, bool mtHasNormals,
                             const int FPS, const Imguc &textureImg) {
  this->mtHasNormals = mtHasNormals;
  this->nFrames = nFrames;
  this->FPS = FPS;
  this->hasTexture = !textureImg.isNull();
  nBytesMTCum = 0;

  // allocate memory for a single one buffer containing all data
  // buffer data for base model
  nBytesF = F.size() * sizeof(unsigned short);
  nBytesV = V.size() * sizeof(float);
  nBytesN = N.size() * sizeof(float);
  nBytesTC = TC.size() * sizeof(float);
  nBytesBase = nBytesF + nBytesV + nBytesN + nBytesTC;

  // buffer data for animation
  nWeights = nFrames * (nFrames - 1);
  nBytesWeights = nWeights * sizeof(float);
  nBytesTime = nFrames * sizeof(float);
  nBytesAnim = nBytesTime + nBytesWeights;

  // buffer data for morph targets
  nBytesMTTotal = 0;
  for (int i = 0; i < nFrames - 1; ++i) {
    nBytesMTTotal += nBytesV;
    if (mtHasNormals) nBytesMTTotal += nBytesN;
  }

  // buffer data for image
  nBytesImage = 0;
  unsigned char *pngData;
  int pngDataLength;
  if (hasTexture) {
    tinygltf::Sampler sampler;
    sampler.magFilter = TINYGLTF_TEXTURE_FILTER_LINEAR;
    sampler.minFilter = TINYGLTF_TEXTURE_FILTER_LINEAR;
    sampler.wrapS = TINYGLTF_TEXTURE_WRAP_CLAMP_TO_EDGE;
    sampler.wrapT = TINYGLTF_TEXTURE_WRAP_CLAMP_TO_EDGE;
    m.samplers.push_back(sampler);

    tinygltf::Image image;
    image.width = textureImg.w;
    image.height = textureImg.h;
    image.component = textureImg.ch;
    image.bits = 8;
    image.pixel_type = TINYGLTF_COMPONENT_TYPE_BYTE;
    image.mimeType = "image/png";
    textureImg.savePNG(pngData, pngDataLength);
    nBytesImage = pngDataLength;
    m.images.push_back(image);

    tinygltf::Texture texture;
    texture.sampler = 0;
    texture.source = 0;
    m.textures.push_back(texture);
  }

  tinygltf::Buffer buffer;
  buffer.data.resize(nBytesBase + nBytesAnim + nBytesMTTotal + nBytesImage);
  if (hasTexture) {
    memcpy(buffer.data.data() + nBytesBase + nBytesAnim + nBytesMTTotal,
           pngData, pngDataLength);
  }
  m.buffers.push_back(buffer);

  tinygltf::Material material;
  if (hasTexture) {
    material.pbrMetallicRoughness.baseColorTexture.index = 0;
  }
  material.pbrMetallicRoughness.metallicFactor = 0;
  material.pbrMetallicRoughness.roughnessFactor = 1;
  m.materials.push_back(material);
}

void ExportGltf::exportStop(const std::string &outFn, bool writeBinary) {
  if (hasTexture) {
    tinygltf::BufferView bufView;
    bufView.name = "texture";
    bufView.buffer = 0;
    bufView.byteOffset = nBytesBase + nBytesAnim + nBytesMTTotal;
    bufView.byteLength = nBytesImage;
    //    bufView.target = TINYGLTF_TARGET_ARRAY_BUFFER;
    m.bufferViews.push_back(bufView);
    m.images[0].bufferView = m.bufferViews.size() - 1;
  }

  tinygltf::TinyGLTF gltf;
  gltf.WriteGltfSceneToFile(&m, outFn,
                            true,          // embedImages
                            true,          // embedBuffers
                            !writeBinary,  // pretty print
                            writeBinary);  // write binary
}

void ExportGltf::exportFullModel(const MatrixXfR &V, const MatrixXfR &N,
                                 const MatrixXusR &F, const MatrixXfR &TC) {
  // buffer data for base model
  tinygltf::Buffer &buffer = m.buffers[0];
  memcpy(buffer.data.data(), F.data(), nBytesF);
  memcpy(buffer.data.data() + nBytesF, V.data(), nBytesV);
  memcpy(buffer.data.data() + nBytesF + nBytesV, N.data(), nBytesN);
  if (nBytesTC > 0) {
    memcpy(buffer.data.data() + nBytesF + nBytesV + nBytesN, TC.data(),
           nBytesTC);
  }

  // buffer data for animation
  if (nFrames > 1) {
    // time
    vector<float> time(nFrames);
    for (int i = 0; i < nFrames; i++) time[i] = i / static_cast<float>(FPS);
    memcpy(buffer.data.data() + nBytesBase, time.data(), nBytesTime);

    // weights
    vector<float> weights(nWeights, 0);
    for (int i = 0; i < nFrames - 1; i++) {
      weights[(i + 1) * (nFrames - 1) + i] = 1;
    }
    memcpy(buffer.data.data() + nBytesBase + nBytesTime, weights.data(),
           nBytesWeights);
  }

  // buffer views for base model
  {
    tinygltf::BufferView bufView;
    bufView.name = "F";
    bufView.buffer = 0;
    bufView.byteOffset = 0;
    bufView.byteLength = nBytesF;
    bufView.target = TINYGLTF_TARGET_ELEMENT_ARRAY_BUFFER;
    m.bufferViews.push_back(bufView);
  }
  {
    tinygltf::BufferView bufView;
    bufView.name = "V";
    bufView.buffer = 0;
    bufView.byteOffset = nBytesF;
    bufView.byteLength = nBytesV;
    bufView.target = TINYGLTF_TARGET_ARRAY_BUFFER;
    m.bufferViews.push_back(bufView);
  }
  {
    tinygltf::BufferView bufView;
    bufView.name = "N";
    bufView.buffer = 0;
    bufView.byteOffset = nBytesF + nBytesV;
    bufView.byteLength = nBytesN;
    bufView.target = TINYGLTF_TARGET_ARRAY_BUFFER;
    m.bufferViews.push_back(bufView);
  }
  if (nBytesTC > 0) {
    tinygltf::BufferView bufView;
    bufView.name = "TC";
    bufView.buffer = 0;
    bufView.byteOffset = nBytesF + nBytesV + nBytesN;
    bufView.byteLength = nBytesTC;
    bufView.target = TINYGLTF_TARGET_ARRAY_BUFFER;
    m.bufferViews.push_back(bufView);
  }

  // buffer views for animation
  if (nFrames > 1) {
    {
      // time
      tinygltf::BufferView bufView;
      bufView.buffer = 0;
      bufView.byteOffset = nBytesBase;
      bufView.byteLength = nBytesTime;
      m.bufferViews.push_back(bufView);
    }
    {
      // weights
      tinygltf::BufferView bufView;
      bufView.buffer = 0;
      bufView.byteOffset = nBytesBase + nBytesTime;
      bufView.byteLength = nBytesWeights;
      m.bufferViews.push_back(bufView);
    }
  }

  // accessors for base model
  {
    tinygltf::Accessor accessor;
    accessor.name = "F";
    accessor.bufferView = 0;
    accessor.byteOffset = 0;
    accessor.componentType = TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT;
    accessor.count = F.size();
    accessor.type = TINYGLTF_TYPE_SCALAR;
    m.accessors.push_back(accessor);
  }
  {
    tinygltf::Accessor accessor;
    accessor.name = "V";
    accessor.bufferView = 1;
    accessor.byteOffset = 0;
    accessor.componentType = TINYGLTF_COMPONENT_TYPE_FLOAT;
    accessor.count = V.rows();
    accessor.type = TINYGLTF_TYPE_VEC3;
    auto VMin = V.colwise().minCoeff();
    auto VMax = V.colwise().maxCoeff();
    accessor.minValues = {VMin(0), VMin(1), VMin(2)};
    accessor.maxValues = {VMax(0), VMax(1), VMax(2)};
    m.accessors.push_back(accessor);
  }
  {
    tinygltf::Accessor accessor;
    accessor.name = "N";
    accessor.bufferView = 2;
    accessor.byteOffset = 0;
    accessor.componentType = TINYGLTF_COMPONENT_TYPE_FLOAT;
    accessor.count = N.rows();
    accessor.type = TINYGLTF_TYPE_VEC3;
    m.accessors.push_back(accessor);
  }
  if (TC.size() > 0) {
    tinygltf::Accessor accessor;
    accessor.name = "TC";
    accessor.bufferView = 3;
    accessor.byteOffset = 0;
    accessor.componentType = TINYGLTF_COMPONENT_TYPE_FLOAT;
    accessor.count = TC.rows();
    accessor.type = TINYGLTF_TYPE_VEC2;
    m.accessors.push_back(accessor);
  }

  // accessors for animation
  if (nFrames > 1) {
    {
      // time
      tinygltf::Accessor accessor;
      accessor.name = "time";
      accessor.bufferView = 3 + (TC.size() > 0 ? 1 : 0);
      accessor.byteOffset = 0;
      accessor.componentType = TINYGLTF_COMPONENT_TYPE_FLOAT;
      accessor.count = nFrames;
      accessor.type = TINYGLTF_TYPE_SCALAR;
      accessor.minValues.push_back(0);
      accessor.maxValues.push_back((nFrames - 1) / static_cast<float>(FPS));
      m.accessors.push_back(accessor);
    }
    {
      // weights
      tinygltf::Accessor accessor;
      accessor.name = "weights";
      accessor.bufferView = 4 + (TC.size() > 0 ? 1 : 0);
      accessor.byteOffset = 0;
      accessor.componentType = TINYGLTF_COMPONENT_TYPE_FLOAT;
      accessor.count = nWeights;
      accessor.type = TINYGLTF_TYPE_SCALAR;
      m.accessors.push_back(accessor);
    }
  }

  // primitive
  tinygltf::Primitive primitive;
  primitive.indices = 0;                 // accessor index for F
  primitive.attributes["POSITION"] = 1;  // accessor index for V
  primitive.attributes["NORMAL"] = 2;    // accessor index for N
  if (TC.size() > 0) {
    primitive.attributes["TEXCOORD_0"] = 3;  // accessor index for TC
  }
  primitive.mode = TINYGLTF_MODE_TRIANGLES;

  // material
  primitive.material = 0;

  // mesh
  tinygltf::Mesh mesh;
  mesh.primitives.push_back(primitive);
  if (nFrames > 1) {
    mesh.weights.resize(nFrames - 1, 0);
    vector<tinygltf::Value> morphTargetNames;
    for (int i = 0; i < nFrames - 1; i++) {
      morphTargetNames.push_back(
          tinygltf::Value("Morph Target " + to_string(i)));
    }
    mesh.extras =
        tinygltf::Value({{"targetNames", tinygltf::Value(morphTargetNames)}});
  }
  m.meshes.push_back(mesh);

  // node
  tinygltf::Node node;
  node.mesh = 0;
  m.nodes.push_back(node);

  // scene
  tinygltf::Scene scene;
  scene.nodes.push_back(0);
  m.scenes.push_back(scene);

  // other
  m.asset.version = "2.0";
  m.asset.generator = "MonsterMash.zone (using tinygltf)";

  if (nFrames > 1) {
    // sampler for animation
    tinygltf::AnimationSampler sampler;
    sampler.input = 3 + (TC.size() > 0 ? 1 : 0);  // accessor index for time
    sampler.interpolation = "LINEAR";
    sampler.output = 4 + (TC.size() > 0 ? 1 : 0);  // accessor index for weights
    tinygltf::AnimationChannel channel;
    channel.sampler = 0;
    channel.target_node = 0;
    channel.target_path = "weights";

    // animation
    tinygltf::Animation anim;
    anim.channels.push_back(channel);
    anim.samplers.push_back(sampler);
    m.animations.push_back(anim);
  }
}

void ExportGltf::exportMorphTarget(const MatrixXfR &V, const MatrixXfR &N,
                                   const int frame) {
  // buffer data for morph target
  tinygltf::Buffer &buffer = m.buffers[0];
  size_t nBytesPrev = nBytesBase + nBytesAnim + nBytesMTCum;
  memcpy(buffer.data.data() + nBytesPrev, V.data(), nBytesV);
  if (mtHasNormals > 0) {
    memcpy(buffer.data.data() + nBytesPrev + nBytesV, N.data(), nBytesN);
  }
  size_t nBytesMTCurr = nBytesV + (mtHasNormals ? nBytesN : 0);

  // buffer views for morph target
  {
    tinygltf::BufferView bufferView;
    bufferView.name = "MT" + to_string(frame) + "_V";
    bufferView.buffer = 0;
    bufferView.byteOffset =
        nBytesBase + nBytesAnim + (frame - 1) * nBytesMTCurr;
    bufferView.byteLength = nBytesV;
    //    bufferView.target = TINYGLTF_TARGET_ARRAY_BUFFER;
    m.bufferViews.push_back(bufferView);
  }
  if (mtHasNormals > 0) {
    tinygltf::BufferView bufferView;
    bufferView.name = "MT" + to_string(frame) + "_N";
    bufferView.buffer = 0;
    bufferView.byteOffset =
        nBytesBase + nBytesAnim + (frame - 1) * nBytesMTCurr + nBytesV;
    bufferView.byteLength = nBytesN;
    //    bufferView.target = TINYGLTF_TARGET_ARRAY_BUFFER;
    m.bufferViews.push_back(bufferView);
  }

  // accessors for morph target
  const int bufViewIdAccIdV =
      5 + (hasTexture ? 1 : 0) + (mtHasNormals ? 2 : 1) * (frame - 1);
  const int bufViewIdAccIdN = bufViewIdAccIdV + 1;
  {
    tinygltf::Accessor accessor;
    accessor.name = "MT" + to_string(frame) + "_V";
    accessor.bufferView = bufViewIdAccIdV;
    accessor.byteOffset = 0;
    accessor.componentType = TINYGLTF_COMPONENT_TYPE_FLOAT;
    accessor.count = V.rows();
    accessor.type = TINYGLTF_TYPE_VEC3;
    auto VMin = V.colwise().minCoeff();
    auto VMax = V.colwise().maxCoeff();
    accessor.minValues = {VMin(0), VMin(1), VMin(2)};
    accessor.maxValues = {VMax(0), VMax(1), VMax(2)};
    m.accessors.push_back(accessor);
  }
  if (mtHasNormals) {
    tinygltf::Accessor accessor;
    accessor.name = "MT" + to_string(frame) + "_N";
    accessor.bufferView = bufViewIdAccIdN;
    accessor.byteOffset = 0;
    accessor.componentType = TINYGLTF_COMPONENT_TYPE_FLOAT;
    accessor.count = N.rows();
    accessor.type = TINYGLTF_TYPE_VEC3;
    m.accessors.push_back(accessor);
  }

  // alter primitive with accessor to morph target
  tinygltf::Primitive &primitive = m.meshes[0].primitives[0];
  pair<string, int> targetV = {"POSITION", bufViewIdAccIdV};
  pair<string, int> targetN = {"NORMAL", bufViewIdAccIdN};
  map<string, int> targets = {targetV};
  if (mtHasNormals) targets.insert(targetN);
  primitive.targets.push_back(targets);

  nBytesMTCum += nBytesMTCurr;
}

}  // namespace exportgltf
