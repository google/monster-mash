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

#include <base64.h>

using namespace std;

namespace exportgltf {

void exportStart(tinygltf::Model &m, const Imguc &textureImg) {
  if (!textureImg.isNull()) {
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
    image.uri = "data:image/png;base64,";
    unsigned char *pngData;
    int length;
    textureImg.savePNG(pngData, length);
    image.uri += base64_encode(pngData, static_cast<unsigned int>(length));
    m.images.push_back(image);

    tinygltf::Texture texture;
    texture.sampler = 0;
    texture.source = 0;
    m.textures.push_back(texture);
  }

  tinygltf::Material material;
  if (!textureImg.isNull())
    material.pbrMetallicRoughness.baseColorTexture.index = 0;
  material.pbrMetallicRoughness.metallicFactor = 0;
  material.pbrMetallicRoughness.roughnessFactor = 1;
  m.materials.push_back(material);
}

void exportStop(tinygltf::Model &m, const std::string &outFn,
                bool writeBinary) {
  tinygltf::TinyGLTF gltf;
  gltf.WriteGltfSceneToFile(&m, outFn,
                            true,          // embedImages
                            true,          // embedBuffers
                            !writeBinary,  // pretty print
                            writeBinary);  // write binary
}

void exportFullModel(const MatrixXfR &V, const MatrixXfR &N,
                     const MatrixXusR &F, const MatrixXfR &TC,
                     const int nFrames, const int FPS, tinygltf::Model &m) {
  // buffer for base model
  tinygltf::Buffer buffer;
  const auto nBytesV = V.size() * sizeof(float);
  const auto nBytesF = F.size() * sizeof(unsigned short);
  const auto nBytesN = N.size() * sizeof(float);
  const auto nBytesTC = TC.size() * sizeof(float);
  buffer.name = "base";
  buffer.data.resize(nBytesF + nBytesV + nBytesN + nBytesTC);
  memcpy(buffer.data.data(), F.data(), nBytesF);
  memcpy(buffer.data.data() + nBytesF, V.data(), nBytesV);
  memcpy(buffer.data.data() + nBytesF + nBytesV, N.data(), nBytesN);
  if (nBytesTC > 0) {
    memcpy(buffer.data.data() + nBytesF + nBytesV + nBytesN, TC.data(),
           nBytesTC);
  }
  m.buffers.push_back(buffer);

  // buffer for animation
  const int nWeights = nFrames * (nFrames - 1);
  const auto nBytesWeights = nWeights * sizeof(float);
  const auto nBytesTime = nFrames * sizeof(float);
  const auto nBytesAnim = nBytesTime + nBytesWeights;
  if (nFrames > 1) {
    tinygltf::Buffer buffer;
    buffer.name = "anim";
    buffer.data.resize(nBytesAnim);
    // time
    vector<float> time(nFrames);
    for (int i = 0; i < nFrames; i++) time[i] = i / static_cast<float>(FPS);
    memcpy(buffer.data.data(), time.data(), nBytesTime);

    // weights
    vector<float> weights(nWeights, 0);
    for (int i = 0; i < nFrames - 1; i++) {
      weights[(i + 1) * (nFrames - 1) + i] = 1;
    }
    memcpy(buffer.data.data() + nBytesTime, weights.data(), nBytesWeights);
    m.buffers.push_back(buffer);
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
      bufView.buffer = 1;  // anim buffer index
      bufView.byteOffset = 0;
      bufView.byteLength = nBytesTime;
      m.bufferViews.push_back(bufView);
    }
    {
      // weights
      tinygltf::BufferView bufView;
      bufView.buffer = 1;  // anim buffer index
      bufView.byteOffset = nBytesTime;
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
    vector<tinygltf::Value> morphTargetNames;
    for (int i = 0; i < nFrames; i++) {
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

void exportMorphTarget(const MatrixXfR &V, const int frame, const int nFrames,
                       const bool hasTexture, tinygltf::Model &m) {
  // buffer for morph target
  tinygltf::Buffer buffer;
  const auto nBytesV = V.size() * sizeof(float);
  buffer.data.resize(nBytesV);
  memcpy(buffer.data.data(), V.data(), nBytesV);
  m.buffers.push_back(buffer);

  // buffer view for morph target
  tinygltf::BufferView bufferView;
  bufferView.buffer = 2 + frame - 1;
  bufferView.byteOffset = 0;
  bufferView.byteLength = nBytesV;
  bufferView.target = TINYGLTF_TARGET_ARRAY_BUFFER;
  m.bufferViews.push_back(bufferView);

  // accessor for morph target
  const int bufViewIdAccId = 5 + (hasTexture ? 1 : 0) + frame - 1;
  tinygltf::Accessor accessor;
  accessor.bufferView = bufViewIdAccId;
  accessor.byteOffset = 0;
  accessor.componentType = TINYGLTF_COMPONENT_TYPE_FLOAT;
  accessor.count = V.rows();
  accessor.type = TINYGLTF_TYPE_VEC3;
  auto VMin = V.colwise().minCoeff();
  auto VMax = V.colwise().maxCoeff();
  accessor.minValues = {VMin(0), VMin(1), VMin(2)};
  accessor.maxValues = {VMax(0), VMax(1), VMax(2)};
  m.accessors.push_back(accessor);

  // alter primitive with accessor to morph target
  tinygltf::Primitive &primitive = m.meshes[0].primitives[0];
  primitive.targets.push_back(
      {{"POSITION", bufViewIdAccId}});  // accessor index for morph target
}

}  // namespace exportgltf
