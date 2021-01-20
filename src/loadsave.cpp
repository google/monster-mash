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

#include "loadsave.h"

#include <igl/writeOBJ.h>
#include <miscutils/fsutils.h>
#include <miscutils/macros.h>
#include <zip.h>

#include <Eigen/Dense>
#include <filesystem>
#include <fstream>
#include <unordered_map>
#include <vector>

#include "macros.h"

using namespace std;
using namespace Eigen;
using namespace igl;

bool saveControlPointsToFile(const std::string &fn,
                             const ManipulationMode &manipulationMode,
                             const std::string &savedCPs, CPData &cpData,
                             DefData &defData, ImgData &imgData) {
  ofstream stream(fn);
  if (!stream.is_open()) return false;
  if (manipulationMode.isGeometryModeActive()) {
    // in deform mode, get control points directly from Def3D
    saveControlPointsToStream(stream, cpData, defData, imgData);
  } else {
    // in drawing mode, control points are stored in savedCPs
    stream << savedCPs;
  }
  stream.close();
  return true;
}

bool saveControlPointsToZip(zip_t *zip, const std::string &fn,
                            const ManipulationMode &manipulationMode,
                            const std::string &savedCPs, CPData &cpData,
                            DefData &defData, ImgData &imgData) {
  stringstream stream;
  if (manipulationMode.isGeometryModeActive()) {
    // in deform mode, get control points directly from Def3D
    saveControlPointsToStream(stream, cpData, defData, imgData);
  } else {
    // in drawing mode, control points are stored in savedCPs
    stream << savedCPs;
  }
  const string &str = stream.str();
  const char *data = str.c_str();
  zip_entry_open(zip, fn.c_str());
  zip_entry_write(zip, data, str.length());
  zip_entry_close(zip);
  return true;
}

bool saveControlPointsToStream(std::ostream &stream, CPData &cpData,
                               DefData &defData, ImgData &imgData) {
  auto &selectedPoints = cpData.selectedPoints;
  auto &selectedPoint = cpData.selectedPoint;
  auto &cpsAnim = cpData.cpsAnim;
  auto &cpsAnimSyncId = cpData.cpsAnimSyncId;
  auto &def = defData.def;
  auto &verticesOfParts = defData.verticesOfParts;
  auto &layers = imgData.layers;
  auto &mesh = defData.mesh;

  stream << "v. " << APP_VERSION << endl;
  if (def.getCPs().empty()) return true;

  vector<int> vertex2part(mesh.getNumPoints());
  forlist(i, verticesOfParts) forlist(j, verticesOfParts[i])
      vertex2part[verticesOfParts[i][j]] = i;
  stream << static_cast<int>(def.getCPs().size()) << endl;
  unordered_map<int, int> cpId2IncId;  // cpId to incremental id

  int i = 0;
  for (const auto &it : def.getCPs()) {
    const int cpId = it.first;
    auto &cp = *it.second;
    const int ptId = cp.ptId;
    const auto &pr = mesh.VRest.row(ptId);
    const int partId = vertex2part[ptId];
    const int isBackPart = partId % 2;
    const int layerId = partId / 2;
    const int regId = layers[layerId];
    stream << ptId << " " << partId << " " << regId << " " << isBackPart << " "
           << pr << " " << cp.pos.transpose() << endl;
    cpId2IncId[cpId] = i;
    i++;
  }

  // save cps anim
  stream << cpsAnim.size() << endl;
  stream << cpId2IncId[cpsAnimSyncId] << endl;
  for (const auto &it : cpsAnim) {
    int cpInd = it.first;
    auto &cpAnim = it.second;
    stream << cpId2IncId[cpInd] << endl;
    stream << cpAnim;
  }

  return true;
}

bool loadControlPointsFromFile(const std::string &fn, std::string &savedCPs) {
  ifstream stream(fn);
  if (!stream.is_open()) return false;
  savedCPs.assign((istreambuf_iterator<char>(stream)),
                  (istreambuf_iterator<char>()));
  stream.close();
  return true;
}

bool loadControlPointsFromZip(zip_t *zip, const std::string &fn,
                              std::string &savedCPs) {
  int ret = zip_entry_open(zip, fn.c_str());
  if (ret != 0) return false;
  size_t size = zip_entry_size(zip);
  void *data = calloc(sizeof(unsigned char), size);
  ret = zip_entry_noallocread(zip, data, size);
  if (ret == -1) return false;
  savedCPs = string(reinterpret_cast<const char *>(data), size);
  free(data);
  zip_entry_close(zip);
  return true;
}

bool loadControlPointsFromStream(std::istream &stream, CPData &cpData,
                                 DefData &defData, ImgData &imgData) {
  auto &selectedPoints = cpData.selectedPoints;
  auto &selectedPoint = cpData.selectedPoint;
  auto &cpsAnim = cpData.cpsAnim;
  auto &cpsAnimSyncId = cpData.cpsAnimSyncId;
  auto &def = defData.def;
  auto &verticesOfParts = defData.verticesOfParts;
  auto &regionImgs = imgData.regionImgs;
  auto &layers = imgData.layers;
  auto &mesh = defData.mesh;

  string versionStr;
  int version;
  stream >> versionStr >> version;
  if (versionStr != "v.") {
    DEBUG_CMD_MM(
        cout << "loadControlPoints: skipping - probably an old saved file"
             << endl;)
    return false;
  }
  DEBUG_CMD_MM(cout << "loadControlPoints: loading CP file version " << version
                    << endl;)

  int num = -1;
  stream >> num;
  if (num != -1) {
    vector<int> cpId2NewCPId(num);
    forlist(i, cpId2NewCPId) cpId2NewCPId[i] = i;
    fora(i, 0, num) {
      double x1, y1, z1, x2, y2, z2;
      int ptId, partIdOrig, regId, isBackPart;
      if (version < 200610) {
        stream >> ptId >> partIdOrig >> x1 >> y1 >> z1 >> x2 >> y2 >> z2;
        regId = partIdOrig / 2;
        isBackPart = partIdOrig % 2;
      } else {
        stream >> ptId >> partIdOrig >> regId >> isBackPart >> x1 >> y1 >> z1 >>
            x2 >> y2 >> z2;
      }
      Vector3d r(x1, y1, z1), pos(x2, y2, z2);
      DEBUG_CMD_MM(cout << "loaded CP: " << ptId << " " << partIdOrig << " "
                        << regId << " " << isBackPart << " " << r.transpose()
                        << " " << pos.transpose();)
      bool res = true;
      if (regId == -1 || regId >= regionImgs.size() ||
          regionImgs[regId].isNull()) {
        res = false;
        DEBUG_CMD_MM(cout << " region does not exist";)
      } else {
        int layerId = -1;
        forlist(i, layers) if (layers[i] == regId) {
          layerId = i;
          break;
        }
        const int partId = 2 * layerId + isBackPart;
        if (partId < verticesOfParts.size()) {
          MatrixXd VPart(verticesOfParts[partId].size(), 3);
          fora(j, 0, VPart.rows()) VPart.row(j) =
              mesh.VRest.row(verticesOfParts[partId][j]);
          int cpInd;
          const double maxRadius = 25;
          if (!def.addControlPointExhaustive(VPart, x1, y1, maxRadius, z1,
                                             false, cpInd)) {
            res = false;
          } else {
            try {
              auto &cp = def.getCP(cpInd);
              cp.ptId += verticesOfParts[partId][0];
              cp.pos = cp.prevPos = pos;
            } catch (out_of_range &e) {
              cerr << e.what() << endl;
            }
          }
        } else {
          res = false;
        }
        if (!res) DEBUG_CMD_MM(cout << " failed";)
      }
      if (!res) {
        cpId2NewCPId[i] = -1;
        fora(j, i + 1, num) cpId2NewCPId[j]--;
        DEBUG_CMD_MM(cout << " (skipping)";)
      }
      DEBUG_CMD_MM(cout << endl;)
    }

    // load cps anim
    num = -1;
    stream >> num;
    if (num != -1) {
      stream >> cpsAnimSyncId;
      cpsAnimSyncId = cpId2NewCPId[cpsAnimSyncId];
      fora(i, 0, num) {
        int cpInd;
        stream >> cpInd;
        int cpIndOrig = cpInd;
        cpInd = cpId2NewCPId[cpInd];
        CPAnim cpAnim;
        stream >> cpAnim;
        if (cpInd == -1) {
          DEBUG_CMD_MM(cout << "skipping cpAnim " << cpIndOrig << endl;)
          continue;
        }
        DEBUG_CMD_MM(cout << "cpAnim for cp " << cpIndOrig << "->" << cpInd
                          << " exists";)
        const auto &cps = def.getCPs();
        if (cps.find(cpInd) != cps.end()) {
          cpsAnim[cpInd] = cpAnim;
        } else {
          DEBUG_CMD_MM(cout << " - failed: cp does not exist";);
        }
        DEBUG_CMD_MM(cout << endl;)
      }
      if (cpsAnim.find(cpsAnimSyncId) == cpsAnim.end()) cpsAnimSyncId = -1;
      DEBUG_CMD_MM(cout << "cpsAnimSyncId: " << cpsAnimSyncId << endl;)
    }

    // update selected points
    const auto &cps = def.getCPs();
    set<int> selectedPointsNew;
    for (int cpId : selectedPoints) {
      const int newId = cpId2NewCPId[cpId];
      if (newId != -1 && cps.find(newId) != cps.end()) {
        selectedPointsNew.insert(newId);
      }
    }
    selectedPoints = selectedPointsNew;
    if (selectedPoint != -1) {
      const int newId = cpId2NewCPId[selectedPoint];
      if (newId != -1 && cps.find(newId) != cps.end()) selectedPoint = newId;
    }
  }

  return true;
}

bool loadSettingsFromStream(std::istream &stream, CPData &cpData,
                            RecData &recData, ShadingOptions &shadingOpts,
                            ManipulationMode &manipulationMode,
                            bool &middleMouseSimulation) {
  string versionStr;
  int version;
  stream >> versionStr >> version;
  if (versionStr != "v.") {
    DEBUG_CMD_MM(
        cout << "loadSettingsFromStream: skipping - probably an old saved file"
             << endl;);
    return false;
  }
  DEBUG_CMD_MM(cout << "loadSettingsFromStream: loading settings (version "
                    << version << ")" << endl;);

  string str, s;
  int i, c = 0;
  while (!stream.eof()) {
    long pos = stream.tellg();
    stream >> str;
    if (str == "manipulationMode") {
      stream >> s;
      if (s == "animate")
        manipulationMode.setMode(ANIMATE_MODE);
      else if (s == "deform")
        manipulationMode.setMode(DEFORM_MODE);
      else if (s == "redraw")
        manipulationMode.setMode(REGION_REDRAW_MODE);
      else
        manipulationMode.setMode(DRAW_OUTLINE);
    } else if (str == "animRecMode") {
      stream >> s;
      if (s == "timescaled")
        cpData.animMode = ANIM_MODE_TIME_SCALED;
      else
        cpData.animMode = ANIM_MODE_OVERWRITE;
    } else if (str == "playAnimation") {
      stream >> i;
      cpData.playAnimation = i;
    } else if (str == "playAnimWhenSelected") {
      stream >> i;
      cpData.playAnimWhenSelected = i;
    } else if (str == "showControlPoints") {
      stream >> i;
      cpData.showControlPoints = i;
    } else if (str == "showTemplateImg") {
      stream >> i;
      shadingOpts.showTemplateImg = i;
    } else if (str == "showBackgroundImg") {
      stream >> i;
      shadingOpts.showBackgroundImg = i;
    } else if (str == "showTextureUseMatcapShading") {
      stream >> i;
      shadingOpts.showTextureUseMatcapShading = i;
    } else if (str == "enableArmpitsStitching") {
      stream >> i;
      recData.armpitsStitching = i;
    } else if (str == "enableNormalSmoothing") {
      stream >> i;
      shadingOpts.useNormalSmoothing = i;
    } else if (str == "middleMouseSimulation") {
      stream >> i;
      middleMouseSimulation = i;
    } else {
      stream >> s;
      DEBUG_CMD_MM(cout << "loadSettingsFromStream: skipping " << str << " "
                        << s << endl;);
    }
    c++;
  }

  DEBUG_CMD_MM(cout << "loadSettingsFromStream: read " << c << " settings"
                    << endl;);
  return true;
}

void loadSettingsFromZip(zip_t *zip, const std::string &fn, CPData &cpData,
                         RecData &recData, ShadingOptions &shadingOpts,
                         ManipulationMode &manipulationMode,
                         bool &middleMouseSimulation) {
  int ret = zip_entry_open(zip, fn.c_str());
  bool settingsExist = true;

  if (ret != 0) settingsExist = false;
  size_t size = zip_entry_size(zip);
  void *data = calloc(sizeof(unsigned char), size);
  ret = zip_entry_noallocread(zip, data, size);
  if (ret == -1) settingsExist = false;

  stringstream stream;
  if (settingsExist) {
    stream.str(string(reinterpret_cast<const char *>(data), size));
  }
  loadSettingsFromStream(stream, cpData, recData, shadingOpts, manipulationMode,
                         middleMouseSimulation);
  free(data);
  zip_entry_close(zip);
}

bool saveSettingsToStream(std::ostream &stream, CPData &cpData,
                          RecData &recData, ShadingOptions &shadingOpts,
                          ManipulationMode &manipulationMode,
                          bool middleMouseSimulation) {
  string manipulationModeStr;
  switch (manipulationMode.mode) {
    case REGION_REDRAW_MODE:
      manipulationModeStr = "redraw";
      break;
    case ANIMATE_MODE:
      manipulationModeStr = "animate";
      break;
    case DEFORM_MODE:
      manipulationModeStr = "deform";
      break;
    default:
    case DRAW_OUTLINE:
      manipulationModeStr = "draw";
      break;
  }
  string animModeStr;
  switch (cpData.animMode) {
    case ANIM_MODE_TIME_SCALED:
      animModeStr = "timescaled";
      break;
    default:
    case ANIM_MODE_OVERWRITE:
      animModeStr = "overwrite";
      break;
  }

  stream << "v. " << APP_VERSION << endl;
  stream << "manipulationMode " << manipulationModeStr << endl;
  stream << "animRecMode " << animModeStr << endl;
  stream << "playAnimation " << (cpData.playAnimation ? 1 : 0) << endl;
  stream << "playAnimWhenSelected " << (cpData.playAnimWhenSelected ? 1 : 0)
         << endl;
  stream << "showControlPoints " << (cpData.showControlPoints ? 1 : 0) << endl;
  stream << "showTemplateImg " << (shadingOpts.showTemplateImg ? 1 : 0) << endl;
  stream << "showBackgroundImg " << (shadingOpts.showBackgroundImg ? 1 : 0)
         << endl;
  stream << "showTextureUseMatcapShading "
         << (shadingOpts.showTextureUseMatcapShading ? 1 : 0) << endl;
  stream << "enableArmpitsStitching " << (recData.armpitsStitching ? 1 : 0)
         << endl;
  stream << "enableNormalSmoothing " << (shadingOpts.useNormalSmoothing ? 1 : 0)
         << endl;
  stream << "middleMouseSimulation " << (middleMouseSimulation ? 1 : 0) << endl;

  return true;
}

bool saveSettingsToZip(zip_t *zip, const std::string &fn, CPData &cpData,
                       RecData &recData, ShadingOptions &shadingOpts,
                       ManipulationMode &manipulationMode,
                       bool middleMouseSimulation) {
  stringstream stream;
  saveSettingsToStream(stream, cpData, recData, shadingOpts, manipulationMode,
                       middleMouseSimulation);
  const string &str = stream.str();
  const char *data = str.c_str();
  zip_entry_open(zip, fn.c_str());
  zip_entry_write(zip, data, str.length());
  zip_entry_close(zip);
  return true;
}

bool loadImageFromZip(zip_t *zip, const string &fn, Imguc &I, int alphaChannel,
                      int desiredNumChannels) {
  bool success = true;
  int ret = zip_entry_open(zip, fn.c_str());
  if (ret != 0) {
    success = false;
  } else {
    size_t size = zip_entry_size(zip);
    void *data = calloc(sizeof(unsigned char), size);
    ret = zip_entry_noallocread(zip, data, size);
    if (ret == -1) {
      success = false;
    } else {
      I = Imguc::loadImage(static_cast<unsigned char *>(data),
                           static_cast<int>(size), alphaChannel,
                           desiredNumChannels);
    }
    free(data);
  }
  zip_entry_close(zip);
  return success;
}

void loadImagesFromZip(zip_t *zip, const string &dir, const int viewportW,
                       const int viewportH, vector<Imguc> &outlineImgsTmp,
                       vector<Imguc> &regionImgsTmp,
                       unordered_map<int, Img<float>> &depthImgsTmp,
                       unordered_map<int, deque<Imguc>> &regionWeightImgsTmp) {
  DEBUG_CMD_MM(cout << "Loading project from a zip file" << endl;);

  string fn;
  fora(i, 0, 255 + 1) {
    ostringstream oss;
    oss << setw(3) << setfill('0') << i;
    fn = dir + "_org_" + oss.str() + ".png";
    {
      Imguc I;
      if (loadImageFromZip(zip, fn, I, -1, 1))
        outlineImgsTmp.push_back(
            I.resize(viewportW, viewportH, 0, 0, Cu({255})));
    }
    fn = dir + "_seg_" + oss.str() + ".png";
    {
      Imguc I;
      if (loadImageFromZip(zip, fn, I, -1, 1))
        regionImgsTmp.push_back(I.resize(viewportW, viewportH, 0, 0, Cu({0})));
    }
  }
}

void loadImagesFromDir(const std::string &dir, const int viewportW,
                       const int viewportH, vector<Imguc> &outlineImgsTmp,
                       vector<Imguc> &regionImgsTmp) {
  DEBUG_CMD_MM(cout << "Loading project from " << dir << endl;)
  string fn;

  fora(i, 0, 255 + 1) {
    ostringstream oss;
    oss << setw(3) << setfill('0') << i;
    fn = dir + "/_org_" + oss.str() + ".png";
    if (file_exists(fn))
      outlineImgsTmp.push_back(Imguc::loadPNG(fn, -1, 1).resize(
          viewportW, viewportH, 0, 0, Cu({255})));
    fn = dir + "/_seg_" + oss.str() + ".png";
    if (file_exists(fn))
      regionImgsTmp.push_back(Imguc::loadPNG(fn, -1, 1).resize(
          viewportW, viewportH, 0, 0, Cu({0})));
  }
}

void loadLayersFromStream(istream &stream, bool layersExist, ImgData &imgData,
                          RecData &recData, vector<Imguc> &outlineImgsTmp,
                          vector<Imguc> &regionImgsTmp) {
  auto &regionImgs = imgData.regionImgs;
  auto &outlineImgs = imgData.outlineImgs;
  auto &layers = imgData.layers;

  outlineImgs.clear();
  regionImgs.clear();
  layers.clear();

  if (outlineImgsTmp.empty()) {
    DEBUG_CMD_MM(cout << "No outline images" << endl;)
  }

  // load image order (layers)
  if (!layersExist) {
    DEBUG_CMD_MM(cout << "loadImages: failed to load layers.txt, assuming "
                         "incremental region order"
                      << endl;)
    // create layers from regions
    const int nRegions = regionImgsTmp.size();
    regionImgs.resize(nRegions);
    outlineImgs.resize(nRegions);
    forlist(i, regionImgsTmp) {
      regionImgs[i] = regionImgsTmp[i];
      outlineImgs[i] = outlineImgsTmp[i];
      layers.push_back(i);
    }
  } else {
    string versionStr;
    int version;
    stream >> versionStr >> version;
    if (versionStr != "v.") {
      DEBUG_CMD_MM(cout << "loadImages: skipping - probably an old saved file"
                        << endl;)
    } else {
      int nRegions;
      stream >> nRegions;
      regionImgs.resize(nRegions);
      outlineImgs.resize(nRegions);
      const int nNonEmptyRegions = regionImgsTmp.size();
      fora(i, 0, nNonEmptyRegions) {
        int regId;
        stream >> regId;
        if (regId >= 0 && regId < nRegions) {
          regionImgs[regId] = regionImgsTmp[i];
          outlineImgs[regId] = outlineImgsTmp[i];
        } else {
          DEBUG_CMD_MM(cout << "loadImages: incorrect regId " << regId << endl;)
        }
      }
      int nLayers;
      stream >> nLayers;
      fora(i, 0, nLayers) {
        int regId;
        stream >> regId;
        if (regId >= 0 && regId < regionImgs.size() &&
            !regionImgs[regId].isNull()) {
          layers.push_back(regId);
        } else {
          DEBUG_CMD_MM(cout << "loadImages: skipping layer " << i << "; region "
                            << regId << " does not exist" << endl;)
        }
      }
    }
  }
}

void loadLayersFromFile(const std::string &fn, ImgData &imgData,
                        RecData &recData, vector<Imguc> &outlineImgsTmp,
                        vector<Imguc> &regionImgsTmp) {
  ifstream stream(fn);
  bool layersExist = true;
  if (!stream.is_open()) layersExist = false;
  loadLayersFromStream(stream, layersExist, imgData, recData, outlineImgsTmp,
                       regionImgsTmp);
  stream.close();
}

void loadLayersFromZip(zip_t *zip, const std::string &fn, ImgData &imgData,
                       RecData &recData, vector<Imguc> &outlineImgsTmp,
                       vector<Imguc> &regionImgsTmp) {
  int ret = zip_entry_open(zip, fn.c_str());
  bool layersExist = true;

  if (ret != 0) layersExist = false;
  size_t size = zip_entry_size(zip);
  void *data = calloc(sizeof(unsigned char), size);
  ret = zip_entry_noallocread(zip, data, size);
  if (ret == -1) layersExist = false;

  stringstream stream;
  if (layersExist) {
    stream.str(string(reinterpret_cast<const char *>(data), size));
  }
  loadLayersFromStream(stream, layersExist, imgData, recData, outlineImgsTmp,
                       regionImgsTmp);
  free(data);
  zip_entry_close(zip);
}

void loadAllFromDir(const std::string &dir, const int viewportW,
                    const int viewportH, ImgData &imgData, RecData &recData,
                    std::string &savedCPs) {
  vector<Imguc> outlineImgsTmp;
  vector<Imguc> regionImgsTmp;
  loadImagesFromDir(dir, viewportW, viewportH, outlineImgsTmp, regionImgsTmp);
  loadLayersFromFile(dir + "/layers.txt", imgData, recData, outlineImgsTmp,
                     regionImgsTmp);
  loadControlPointsFromFile(dir + "/cps.txt", savedCPs);
}

void loadAllFromZip(const std::string &zipFn, const int viewportW,
                    const int viewportH, CPData &cpData, ImgData &imgData,
                    RecData &recData, std::string &savedCPs, Imguc &templateImg,
                    Imguc &backgroundImg, ShadingOptions &shadingOpts,
                    ManipulationMode &manipulationMode,
                    bool &middleMouseSimulation) {
  vector<Imguc> outlineImgsTmp;
  vector<Imguc> regionImgsTmp;
  unordered_map<int, Img<float>> depthImgsTmp;
  unordered_map<int, deque<Imguc>> regionWeightImgsTmp;
  zip_t *zip = zip_open(zipFn.c_str(), 0, 'r');
  if (zip == nullptr) {
    DEBUG_CMD_MM(cout << "loadAllFromZip: Could not open " << zipFn << endl;);
    return;
  }

  // Find a directory in the zip file containing _org_000.png.
  // We assume that the whole project is contained in this directory.
  const int n = zip_total_entries(zip);
  string dir = "";
  fora(i, 0, n) {
    zip_entry_openbyindex(zip, i);
    const char *name = zip_entry_name(zip);
    auto path = filesystem::path(name);
    if (path.filename() == "_org_000.png") {
      dir = path.parent_path();
      if (!dir.empty()) dir += "/";
      break;
    }
    zip_entry_close(zip);
  }

  loadImagesFromZip(zip, dir, viewportW, viewportH, outlineImgsTmp,
                    regionImgsTmp, depthImgsTmp, regionWeightImgsTmp);
  loadLayersFromZip(zip, dir + "layers.txt", imgData, recData, outlineImgsTmp,
                    regionImgsTmp);
  loadControlPointsFromZip(zip, dir + "cps.txt", savedCPs);
  loadImageFromZip(zip, dir + "template.png", templateImg, 3, 4);
  loadImageFromZip(zip, dir + "bg.png", backgroundImg, 3, 4);
  loadSettingsFromZip(zip, dir + "settings.txt", cpData, recData, shadingOpts,
                      manipulationMode, middleMouseSimulation);
  zip_close(zip);
}

void saveLayersToStream(ostream &stream, const ImgData &imgData) {
  auto &regionImgs = imgData.regionImgs;
  auto &layers = imgData.layers;

  // save image order (layers)
  stream << "v. " << APP_VERSION << endl;
  stream << regionImgs.size() << endl;
  forlist(regId, regionImgs) {
    if (!regionImgs[regId].isNull()) stream << regId << endl;
  }
  stream << endl;
  stream << layers.size() << endl;
  for (const int regId : layers) {
    stream << regId << endl;
  }
}

bool saveLayersToFile(const std::string &fn, const ImgData &imgData) {
  ofstream stream(fn);
  if (!stream.is_open()) {
    DEBUG_CMD_MM(cout << "saveImages: failed to save layers" << endl;)
    return false;
  }
  saveLayersToStream(stream, imgData);
  stream.close();
  return true;
}

bool saveLayersToZip(zip_t *zip, const std::string &fn,
                     const ImgData &imgData) {
  stringstream stream;
  saveLayersToStream(stream, imgData);
  const string &str = stream.str();
  const char *data = str.c_str();
  zip_entry_open(zip, fn.c_str());
  zip_entry_write(zip, data, str.length());
  zip_entry_close(zip);
  return true;
}

void saveImagesToDir(const string &dir, const ImgData &imgData,
                     const RecData &recData) {
  auto &regionImgs = imgData.regionImgs;
  auto &outlineImgs = imgData.outlineImgs;

  DEBUG_CMD_MM(cout << "Saving project to " << dir << endl;)
  int j = 0;
  forlist(regId, regionImgs) {
    if (regionImgs[regId].isNull()) continue;
    ostringstream oss;
    oss << setw(3) << setfill('0') << j;
    outlineImgs[regId].savePNG(dir + "/_org_" + oss.str() + ".png");
    regionImgs[regId].savePNG(dir + "/_seg_" + oss.str() + ".png");

    j++;
  }
}

void saveImageToZip(zip_t *zip, const string &fn, const Imguc &I) {
  unsigned char *data = nullptr;
  int length = 0;
  I.savePNG(data, length);
  zip_entry_open(zip, fn.c_str());
  zip_entry_write(zip, data, length);
  zip_entry_close(zip);
  free(data);
}

void saveImagesToZip(zip_t *zip, const ImgData &imgData,
                     const RecData &recData) {
  auto &regionImgs = imgData.regionImgs;
  auto &outlineImgs = imgData.outlineImgs;
  auto &layers = imgData.layers;

  DEBUG_CMD_MM(cout << "Saving project to a zip file" << endl;)
  int j = 0;
  forlist(regId, regionImgs) {
    if (regionImgs[regId].isNull()) continue;
    ostringstream oss;
    oss << setw(3) << setfill('0') << j;
    saveImageToZip(zip, "_org_" + oss.str() + ".png", outlineImgs[regId]);
    saveImageToZip(zip, "_seg_" + oss.str() + ".png", regionImgs[regId]);
    j++;
  }
}

void saveAllToZip(const std::string &zipFn, CPData &cpData, DefData &defData,
                  ImgData &imgData, RecData &recData,
                  const std::string &savedCPs, const Imguc &templateImg,
                  const Imguc &backgroundImg, ShadingOptions &shadingOpts,
                  ManipulationMode &manipulationMode,
                  bool middleMouseSimulation) {
  zip_t *zip = zip_open(zipFn.c_str(), ZIP_DEFAULT_COMPRESSION_LEVEL, 'w');
  if (zip == nullptr) {
    DEBUG_CMD_MM(cout << "saveAllToZip: Could not open " << zipFn << endl;);
    return;
  }
  saveImagesToZip(zip, imgData, recData);
  saveLayersToZip(zip, "layers.txt", imgData);
  saveControlPointsToZip(zip, "cps.txt", manipulationMode, savedCPs, cpData,
                         defData, imgData);
  if (!templateImg.isNull()) saveImageToZip(zip, "template.png", templateImg);
  if (!backgroundImg.isNull()) saveImageToZip(zip, "bg.png", backgroundImg);
  saveSettingsToZip(zip, "settings.txt", cpData, recData, shadingOpts,
                    manipulationMode, middleMouseSimulation);
  zip_close(zip);
}

void saveAllToDir(const std::string &dir,
                  const ManipulationMode &manipulationMode,
                  const std::string &savedCPs, CPData &cpData, DefData &defData,
                  ImgData &imgData, RecData &recData) {
  auto &VCurr = defData.VCurr;
  auto &VRest = defData.VRest;
  auto &Faces = defData.Faces;

  auto &layers = imgData.layers;
  auto &mergedOutlinesImg = imgData.mergedOutlinesImg;
  auto &mergedRegionsImg = imgData.mergedRegionsImg;
  auto &minRegionsImg = imgData.minRegionsImg;

  bool debug = false;
  if (debug) {
    mergedOutlinesImg.savePNG(dir + "/org.png");
    mergedRegionsImg.savePNG(dir + "/seg.png");
    minRegionsImg.savePNG(dir + "/Mm.png");
    if (VCurr.size() > 0) {
      writeOBJ(dir + "/out.obj", VCurr, Faces);
      writeOBJ(dir + "/outRest.obj", VRest, Faces);
    }
  }

  saveImagesToDir(dir, imgData, recData);

  // remove images not belonging to the current project anymore
  fora(i, layers.size(), 255 + 1) {
    ostringstream oss;
    oss << setw(3) << setfill('0') << i;
    remove((dir + "/_org_" + oss.str() + ".png").c_str());
    remove((dir + "/_seg_" + oss.str() + ".png").c_str());
    remove((dir + "/_dpt_" + oss.str() + ".img").c_str());
  }

  saveLayersToFile(dir + "/layers.txt", imgData);
  saveControlPointsToFile(dir + "/cps.txt", manipulationMode, savedCPs, cpData,
                          defData, imgData);
}
