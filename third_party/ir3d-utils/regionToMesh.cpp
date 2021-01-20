// Copyright (c) 2014-2020 Czech Technical University in Prague
// Licensed under the MIT License.

#include "regionToMesh.h"
#include <image/imageUtils.h>
#include <igl/triangle/triangulate.h>

using namespace std;
using namespace Eigen;
using namespace igl;

#ifndef DEBUG_CMD_IR
  #ifdef ENABLE_DEBUG_CMD_IR
    #define DEBUG_CMD_IR(x) {x}
  #else
    #define DEBUG_CMD_IR(x) ;
  #endif
#endif

void regionToOutline(Imguc &Ir, Imguc &Io, bool keepSingleRegion, bool addBoundaryToRegion)
{
  // create outline image by tracing a white region
  if (!Io.equalDimension(Ir)) Io.initImage(Ir);
  Io.fill(255);
  Imguc M; M.initImage(Io); M.clear();
  // find the first background pixel (black) and use flood fill to add background to the mask image
  int startX=-1, startY=-1;
  fora(y,0,M.h) fora(x,0,M.w) if (Ir(x,y,0) == 0) { startX=x; startY=y; goto EXIT; } EXIT:;
  floodFill(Ir, M, startX, startY, 0, 255);
  vector<vector<Vector2i>> bnds;
  vector<Vector2i> bnd;
  auto getBoundary = [&](int x,int y) { bnd.push_back(Vector2i(x,y)); };
  while (traceRegion(M, getBoundary, startX, startY)) {
    // if the region is composed of several separated components, find one, remove it, repeat
    const int &color = Ir(startX,startY,0);
    floodFill(Ir, M, startX, startY, color, 255);
    bnds.push_back(bnd);
    bnd.clear();
  }

  if (keepSingleRegion) {
    // only keep a single region with the largest amount of boundary points
    int bestId = 0;
    int max = 0;
    forlist(j, bnds) {
      const int size = bnds[j].size();
      if (max < size) {
        max = size;
        bestId = j;
      }
    }
    DEBUG_CMD_IR(cout << "regionToOutline: Selecting region num " << bestId << " with " << max << " boundary points" << endl;);

    // draw outline
    for(auto &p : bnds[bestId]) {
      Io(p(0),p(1),0) = 0;
      if (addBoundaryToRegion) Ir(p(0),p(1),0) = 255;
    }
  } else {
    // draw outlines
    for(auto &el : bnds)
    for(auto &p : el) {
      Io(p(0),p(1),0) = 0;
      if (addBoundaryToRegion) Ir(p(0),p(1),0) = 255;
    }
  }
}

void outlineToRegion(const Imguc &Io, Imguc &Ir)
{
  Ir = Io;
  // convert the shape to a region by filling the outside and recoloring the remaining shape to have the same color (black)
  floodFill(Ir, 0, 0, 255, 128);
  fora(y, 0, Ir.h) fora(x, 0, Ir.w) Ir(x,y,0) = Ir(x,y,0) == 128 ? 0 : 255;
}

void findRegionBoundary(const Imguc &S, std::vector<std::vector<Eigen::Vector2f>> &bnds, std::vector<Vector2f> &holePts)
{
  // create a mask from the input segment
  Imguc M; M.initImage(S); M.clear();
  Imguc O; O.initImage(S); O.clear(); // outline image - used for finding hole points
  // find the first background pixel and use flood fill to add background to the mask image
  int startX=-1, startY=-1;
  fora(y,0,M.h) fora(x,0,M.w) if (S(x,y,0) == 0) { startX=x; startY=y; goto EXIT1; } EXIT1:;
  floodFill(S, M, startX, startY, 0, 255);

  bnds.clear();
  vector<Vector2f> bnd;
  auto getBoundary = [&](int x,int y) { bnd.push_back(Vector2f(x,y)); };
  startX=-1; startY=-1;
  int j = 1;
  while (traceRegion(M, getBoundary, startX, startY)) {
    // if the region is composed of several separated components, save the current component as a separate boundary
    const int nBnd = bnd.size();
    const int &color = S(startX,startY,0);
    DEBUG_CMD_IR(cout << "new component of region " << color << ", size: " << nBnd;);
    if (color == 0) DEBUG_CMD_IR(cout << ", it is a hole";);
    if (nBnd > 10) {
      bnds.push_back(bnd);
    } else {
      DEBUG_CMD_IR(cout << " - skipping";);
    }
    DEBUG_CMD_IR(cout << endl;);
    floodFill(S, M, startX, startY, color, 255); // if the region is composed of several separated components, find one, remove it, repeat

    if (color == 0) {
      // the component is a hole, find a single point that is inside it (assuming that the shape may be concave)
      O.clear(); fora(k, 0, nBnd) O(bnd[k](0), bnd[k](1), 0) = 255; // fill outline image
      fora(k, 0, nBnd) fora(l, nBnd/2, 3*nBnd/2) {
        if (k==l) continue;
        const Vector2i centroid = (0.5*(bnd[k]+bnd[l])).cast<int>();
        if (S(centroid(0), centroid(1), 0) == 0 && M(centroid(0), centroid(1), 0) == 255 && O(centroid(0), centroid(1), 0) == 0) {
          holePts.push_back(centroid.cast<float>());
          goto EXIT2;
        }
      }
      EXIT2:;
    }

    bnd.clear();
    j++;
  }
  DEBUG_CMD_IR(cout << "Tracing boundary done; total components: " << bnds.size() << endl;);
}

void outlineToMesh(const std::vector<Imguc> &outlines, const std::vector<Imguc> &segs,
              const bool aboveOnly, const bool belowOnly, const bool interiorMergingPtsOnly,
              const std::string &triangleOpts,
              std::set<int> &mergeBothSides,
              std::vector<Eigen::MatrixXd> &Vs, std::vector<Eigen::MatrixXi> &Fs,
              const int smoothFactor)
{
  std::vector<std::vector<std::vector<Eigen::Vector2f>>> regionsBnds;
  vector<TriData> triData;
  outlineToMesh(outlines, segs, aboveOnly, belowOnly, interiorMergingPtsOnly, triangleOpts, mergeBothSides, Vs, Fs, smoothFactor, regionsBnds, triData);
}

void outlineToMesh(const std::vector<Imguc> &outlines, const std::vector<Imguc> &segs,
              const bool aboveOnly, const bool belowOnly, const bool interiorMergingPtsOnly,
              const std::string &triangleOpts,
              std::set<int> &mergeBothSides,
              std::vector<Eigen::MatrixXd> &Vs, std::vector<Eigen::MatrixXi> &Fs,
              const int smoothFactor,
              std::vector<std::vector<std::vector<Eigen::Vector2f>>> &regionsBnds,
              std::vector<TriData> &triData)
{
  assert(outlines.size() == segs.size());

  triData.resize(segs.size());

  vector<vector<Vector2f>> regionsHolePts;

  // find region boundaries
  forlist(i, segs) {
    const Imguc &S = segs[i].resize(segs[i].w+2, segs[i].h+2, 1, 1, Cu{0}); // handle image boundary

    vector<vector<Vector2f>> bnds;
    vector<Vector2f> holePts;
    findRegionBoundary(S, bnds, holePts);

    // back coordinates of original image
    for(auto &bnd : bnds) {
      for(auto &p : bnd) p -= Vector2f(1,1);
    }
    for(auto &p : holePts) p -= Vector2f(1,1);

    regionsBnds.push_back(bnds);
    regionsHolePts.push_back(holePts);
  }

  struct BndHints {
    bool missingBoundary = false;
    bool surrounded = false;
  };
  // find a part of region boundary which does not have an outline (i.e. interconnection boundary)
  vector<vector<vector<BndHints>>> regionsBndsHints(regionsBnds.size());
  int n = 0;
  forlist(i, regionsBnds) {
    const Imguc &Oorig = outlines[i];
    forlist(j, regionsBnds[i]) {
      if (j == 0) regionsBndsHints[i].resize(regionsBnds[i].size());
      bool surrounded = true;
      forlist(k, regionsBnds[i][j]) {
        if (k == 0) regionsBndsHints[i][j].resize(regionsBnds[i][j].size());
        const auto &p = regionsBnds[i][j][k];
        // if there is no outline along a part of the region, mark it as interconnection boundary
        if (Oorig(p(0), p(1), 0) != 0) {
          regionsBndsHints[i][j][k].missingBoundary = true;
          surrounded = false;
          n++;
        }
        if (surrounded) {
          // Check if the region boundary is surrounded by other regions:
          // If there is at least one boundary point of other regions with foreground pixel, then
          // we treat the region as surrounded.
          bool foregroundPresent = false;
          forlist(l, segs) if (l != i) if (segs[l](p(0),p(1),0) != 0) { foregroundPresent = true; break; }
          surrounded = foregroundPresent;
        }
      }

      if (surrounded) {
        forlist(k, regionsBnds[i][j]) {
          regionsBndsHints[i][j][k].surrounded = true;
        }
      }
    }
  }
  DEBUG_CMD_IR(cout << "interconnectionBnd: " << n << endl;);

  // boundary smoothing
  forlist(i, regionsBnds) {
    forlist(j, regionsBnds[i]) {
      fora(s,0,smoothFactor) {
        vector<Vector2f> &bnd = regionsBnds[i][j];
        forlist(k, bnd) {
          // do not smooth the interconnection boundary
          if (!regionsBndsHints[i][j][k].missingBoundary) {
            bnd[k] = (bnd[(bnd.size()+k-1)%bnd.size()] + bnd[k] + bnd[(k+1)%bnd.size()])/3.f;
          }
        }
      }
    }
  }

  // it is necessary to regenerate the segmentation and outline images according to the smoothed boundary
  vector<Imguc> segsSmooth(segs.size()), outlinesSmooth(segs.size());
  forlist(i, segs) {
    Imguc &S = segsSmooth[i]; S.initImage(segs[i]); S.clear();
    const vector<vector<Vector2f>> &bnds = regionsBnds[i];
    for(const auto &bnd : bnds) for(const auto &p : bnd) {
      S(p(0),p(1),0) = 255;
    }

    // fill the interior of the outline to obtain region
    scanlineFillOutline(S.deepCopy(), S, 255);
  }

  // process region boundaries
  forlist(i, segs) {
    const Imguc &S = segs[i];
    Imguc O; O.initImage(S); O.clear(); // outline image
    for(const auto &bnd : regionsBnds[i]) for(const auto &p : bnd) O(p(0),p(1),0) = 255;

    // find interior points from other region boundaries
    vector<Vector2f> interiorPts;
    forlist(j, regionsBnds) {
      if (i == j) continue;
      bool bothSides = mergeBothSides.find(j) != mergeBothSides.end();
      if (!bothSides) {
        if (aboveOnly && j < i) continue;
        if (belowOnly && j > i) continue;
      }
      const vector<vector<Vector2f>> &bnds = regionsBnds[j];
      forlist(k, bnds) {
        const auto &bnd = bnds[k];
        forlist(l, bnd) {
          const Vector2f &p = bnd[l];
          // If a boundary point of some other region lays inside the current region
          // and it is not on the current region's outline then it is an interior point.
          if (S(p(0),p(1),0) != 0 && O(p(0),p(1),0) == 0) {
            if (!interiorMergingPtsOnly || regionsBndsHints[j][k][l].missingBoundary) interiorPts.push_back(p);
          }
        }
      }
    }
    DEBUG_CMD_IR(cout << "interiorPts: " << interiorPts.size() << endl;);

    // prepare data for triangulation
    const vector<vector<Vector2f>> &bnds = regionsBnds[i];
    vector<int> interconnectionBnd, eqBnd;
    int n = 0;
    forlist(k, bnds) {
      const auto &bnd = bnds[k];
      forlist(l, bnd) {
        if (regionsBndsHints[i][k][l].missingBoundary) interconnectionBnd.push_back(n+l);
        if (regionsBndsHints[i][k][l].surrounded) eqBnd.push_back(n+l);
      }
      n += bnd.size();
    }

    MatrixXd Vout;
    MatrixXi Fout;
    triData[i] = TriData(regionsBnds[i], regionsHolePts[i], interiorPts, triangleOpts);
    triangulate(triData[i], Vout, Fout);

    // add annotations for the interconnection boundary points to the final triangulation
    for(const auto &el : interconnectionBnd) Vout(el,2) = 1100;

    Vs.push_back(Vout);
    Fs.push_back(Fout);
  }
}

void triangulate(TriData &triData, MatrixXd &Vout, MatrixXi &Fout)
{
  const vector<vector<Vector2f>> &bnds = triData.regionBnds;
  const vector<Vector2f> &holePts = triData.regionHolePts;
  const vector<Vector2f> &interiorPts = triData.interiorPts;
  int nV = 0, nE = 0;
  for(const auto &bnd : bnds) nV += bnd.size();
  nE = nV;
  nV += interiorPts.size();
  MatrixXd &V = triData.V;
  MatrixXi &E = triData.E;
  MatrixXd &H = triData.H;
  V.resize(nV, 2);
  E.resize(nE, 2);
  H.resize(holePts.size(), 2);

  int n = 0;
  forlist(k, bnds) {
    const auto &bnd = bnds[k];
    forlist(l, bnd) {
      const Vector2f &c = bnd[l];
      V.row(n+l) = Vector2d(c(0), c(1));
      E.row(n+l) = Vector2i(n+l, n+(l+1)%bnd.size());
    }
    n += bnd.size();
  }

  forlist(l, interiorPts) {
    V.row(n+l) = Vector2d(interiorPts[l](0), interiorPts[l](1));
  }

  forlist(l, holePts) H.row(l) = holePts[l].cast<double>();

  MatrixXd Vout2;
  triangle::triangulate(V, E, H, triData.triangleOpts, Vout2, Fout);
  Vout.resize(Vout2.rows(), 3);
  Vout.col(0) = Vout2.col(0);
  Vout.col(1) = Vout2.col(1);
  Vout.col(2).fill(0);
}
