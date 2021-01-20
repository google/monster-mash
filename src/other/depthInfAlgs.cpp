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

#include "depthInfAlgs.h"

#include <iostream>
#include <set>

using namespace std;
using namespace Eigen;

void eliminateCycles(
    const std::map<int, std::list<std::pair<int, float>>> &Eout,
    const std::vector<int> &regionsIdsUniq,
    std::map<int, std::list<std::pair<int, float>>> &Eout2) {
  const int N = regionsIdsUniq.size();
  map<int, int> regionIdArrayId;
  forlist(i, regionsIdsUniq) regionIdArrayId[regionsIdsUniq[i]] = i;

  map<int, list<pair<int, float>>> EoutMerged;
  computeJointProbabilities(Eout, EoutMerged);

  // Initialize probability adjacency matrix from initial probabilities in
  // EoutMerged.
  MatrixXf P(N, N);
  P.fill(0);
  for (const auto &el : EoutMerged) {
    const int n = el.first;
    for (const pair<int, float> &el2 : el.second) {
      const int m = el2.first;
      float prob = el2.second;
      prob = prob > 0.01f ? prob : 0.f;  // thresholding
      P(regionIdArrayId[n], regionIdArrayId[m]) = prob;
    }
  }
  cout << P << endl << endl;

  // eliminate cycles by removing unreliable edges (iterative approach)
  int it = 0;
  while (1) {
    cout << endl << "iteration " << it << endl;
    MatrixXf Ptc = P;

    // Compute transitive closure of graph G+:
    // Find vertex-to-vertex probabilities of precedence (i.e. weighted
    // inequalities) using modified Floyd-Warshall algorithm for working with
    // probabilities (Palou '13).
    fora(j, 0, N) {
      fora(i, 0, N) {
        fora(k, 0, N) {
          if (i == k) continue;
          Ptc(i, k) +=
              Ptc(i, j) * Ptc(j, k) - Ptc(i, k) * Ptc(i, j) * Ptc(j, k);
        }
      }
    }

    cout << Ptc << endl << endl;

    // find two nodes R1, R2 that have a cycle and a minimal edge between them
    int R1 = -1, R2 = -1;
    float min = numeric_limits<float>::infinity();
    fora(i, 0, Ptc.rows()) fora(j, 0, Ptc.cols()) {
      if (Ptc(i, j) > 0 && Ptc(j, i) > 0 && Ptc(i, j) < min) {
        min = Ptc(i, j);
        R1 = i;
        R2 = j;
      }
    }

    // no cycles found, stop the procedure
    if (R1 == -1) break;

    // nodes having a cycle with minimal total weight found
    cout << "found a cycle between " << R1 << " and " << R2
         << ", minProb: " << min << endl;

    // compute a max-flow/min-cut between R1 and R2 using Ford-Fulkerson
    vector<pair<int, int>> edgesToCut;
    minCut(P, R1, R2, edgesToCut);
    for (const auto &el : edgesToCut) {
      int i = el.first;
      int j = el.second;
      cout << "cutting " << i << "->" << j << endl;
      P(i, j) = 0;
    }

    cout << P << endl << endl;
    it++;
  }

  // convert P to Eout2
  fora(i, 0, P.rows()) fora(j, 0, P.cols()) {
    const float prob = P(i, j);
    if (prob > 0)
      Eout2[regionsIdsUniq[i]].push_back(make_pair(regionsIdsUniq[j], prob));
  }
}

void computeJointProbabilities(
    const std::map<int, std::list<std::pair<int, float>>> &Eout,
    std::map<int, std::list<std::pair<int, float>>> &EoutMerged) {
  // If there are multiple arrows but with different probabilities from a region
  // A to region B, merge them.
  map<pair<int, int>, float> Emerged;
  for (const auto &el : Eout) {
    const int n = el.first;
    for (const pair<int, float> &el2 : el.second) {
      const int m = el2.first;
      const float prob = el2.second;
      const auto key = make_pair(n, m);
      if (Emerged.find(key) == Emerged.end()) Emerged[key] = 1;
      Emerged[key] *= 1.f - prob;  // negation rule for probability
    }
  }

  // convert Emerged to the structure of Eout
  EoutMerged.clear();
  for (auto &el : Emerged) {
    const int n = el.first.first;
    const int m = el.first.second;
    const float probNeg = el.second;
    EoutMerged[n].push_back(
        make_pair(m, 1.f - probNeg));  // negation rule for probability: back to
                                       // the previous value
  }
}

void findTJunctions(const Image8 &segImg,
                    const std::vector<int> &bndToRegionId,
                    const std::vector<std::vector<Eigen::Vector2f>> &bnds,
                    const float maxDist,
                    std::map<int, std::list<std::pair<int, float>>> &E) {
  vector<Vector2f> centroids;
  vector<float> inAngles;
  vector<float> outAngles;
  vector<float> angles;
  findTJunctions(segImg, bndToRegionId, bnds, maxDist, E, centroids, inAngles,
                 outAngles, angles);
}

void findTJunctions(const Image8 &segImg,
                    const std::vector<int> &bndToRegionId,
                    const std::vector<std::vector<Eigen::Vector2f>> &bnds,
                    const float maxDist,
                    std::map<int, std::list<std::pair<int, float>>> &E,
                    std::vector<Eigen::Vector2f> &outCentroids,
                    std::vector<float> &outInAngles,
                    std::vector<float> &outOutAngles,
                    std::vector<float> &outAngles) {
  const Image8 &I = segImg;

  // find T-junctions centroids and angles
  forlist(k, bnds) {
    const int regionId = bndToRegionId[k];
    vector<Vector2f> centroids;
    vector<int> centroidsBndId;

    // find T-junction centroids
    int subseqNum = 0;
    Vector2f centroid(0, 0);
    forlist(l, bnds[k]) {
      const Vector2f &coord = bnds[k][l];
      int x = coord(0), y = coord(1);

      // Check 3x3 neighborhood for T-junctions, if there are subsequent
      // candidates, average them to find their centroid.
      const int N = 1;
      int numColors = 0;
      vector<bool> visited(256, false);
      fora(wy, -N, N + 1) fora(wx, -N, N + 1) {
        const int i = I(x + wx, y + wy, 0);
        if (!visited[i]) {
          visited[i] = true;
          numColors++;
        }
      }
      if (numColors == 3) {
        centroid += Vector2f(x, y);
        subseqNum++;
      } else {
        if (subseqNum > 0) {
          centroid /= subseqNum;
          centroids.push_back(centroid);
        }
        subseqNum = 0;
        centroid.fill(0);
      }
    }

    if (centroids.empty()) {
      cout << "centroids empty" << endl;
      continue;
    }

    // Assign each centroid (which is a 2D point with float coordinates) the
    // nearest boundary pixel.
    centroidsBndId.resize(centroids.size());
    forlist(i, centroids) {
      float minDist = numeric_limits<float>::infinity();
      int id = -1;
      forlist(j, bnds[k]) {
        const Vector2f &coord = bnds[k][j];
        const Vector2f &currCentroid = centroids[i];
        float dist = (coord - currCentroid).norm();
        if (dist < minDist) {
          minDist = dist;
          id = j;
        }
      }
      centroidsBndId[i] = id;
    }

    // Find T-junction inbound and outbound angles: Go along the boundary in
    // clockwise and counter clockwise direction, gather boundary point samples,
    // compute angle for each sample, do this until a maximal distance from
    // T-junction centroid or until crossing another T-junction centroid.
    forlist(i, centroids) {
      vector<double> angles;
      vector<double> weights;
      set<int> allAdjRegionIds;

      auto sampleAngles = [&](int j) -> bool {
        int l = j % bnds[k].size();
        const Vector2f &coord = bnds[k][l];
        const Vector2f &currCentroid = centroids[i];
        Vector2f diff = coord - currCentroid;
        float dist = diff.norm();
        int x = coord(0), y = coord(1);
        if (dist < maxDist) {
          // check 3x3 neighborhood for adjacent regions
          const int N = 1;
          vector<int> adjacentRegionIds;
          vector<bool> visited(256, false);
          fora(wy, -N, N + 1) fora(wx, -N, N + 1) {
            const int i = I(x + wx, y + wy, 0);
            if (!visited[i]) {
              visited[i] = true;
              adjacentRegionIds.push_back(i);
            }
          }

          // Only gather samples along boundary having 2 different regions on
          // both sides also neglect all samples withing radius of 3 of the
          // T-junction centroid (as in Palou '13).
          if (adjacentRegionIds.size() == 2 && dist > 3) {
            float ang = atan2(diff(1), diff(0));
            angles.push_back(ang);
            float weight = dist / maxDist;
            weight = weight * weight * weight;
            weights.push_back(weight);
            for (const int id : adjacentRegionIds)
              if (id != regionId) allAdjRegionIds.insert(id);
          }

          return true;
        } else
          return false;
      };

      // clockwise: outbound angles
      angles.clear();
      weights.clear();
      float ang = 0, outAng, inAng;
      for (int j = centroidsBndId[i]; j < bnds[k].size() + centroidsBndId[i];
           j++) {
        // stop gathering samples when crossing the next T-junction
        int nextCentroidBndId = -1;
        if (i + 1 >= centroids.size())
          nextCentroidBndId = centroidsBndId[0] + bnds[k].size();
        else
          nextCentroidBndId = centroidsBndId[i + 1];
        if (j % bnds[k].size() > nextCentroidBndId) break;

        if (!sampleAngles(j)) break;
      }
      if (angles.empty()) {
        cout << "warning: angles empty, skipping" << endl;
        continue;
      }
      outAng = avgRotation(
          angles,
          weights);  // find average rotation from all gathered rotations

      // counter clockwise: inbound angles
      angles.clear();
      weights.clear();
      for (int j = bnds[k].size() + centroidsBndId[i]; j > centroidsBndId[i];
           j--) {
        // stop gathering samples when crossing the next T-junction
        int nextCentroidBndId = -1;  // next in counter clockwise direction
        if (i - 1 < 0)
          nextCentroidBndId = centroidsBndId.back();
        else
          nextCentroidBndId = centroidsBndId[i - 1] + bnds[k].size();
        if (j < nextCentroidBndId) break;

        if (!sampleAngles(j)) break;
      }
      inAng = avgRotation(
          angles,
          weights);  // find average rotation from all gathered rotations
      ang = -outAng + inAng;         // total angle at the T-junction
      if (ang < 0) ang += 2 * M_PI;  // convert negative to positive angles

      if (angles.empty()) {
        cout << "warning: angles empty, skipping" << endl;
        continue;
      }

      for (const int id : allAdjRegionIds) {
        if (id == 0) continue;   // skip background
        float sigma = M_PI / 6;  // Palou '13
        float prob = exp(-fabs(ang - M_PI) / (sigma * sigma));  // Palou '13
        E[id].push_back(make_pair(regionId, prob));
      }

      outCentroids.push_back(centroids[i]);
      outInAngles.push_back(inAng);
      outOutAngles.push_back(outAng);
      outAngles.push_back(ang);
    }
  }
}

bool topologicalSort(
    const std::vector<int> &N,
    const std::map<int, std::list<std::pair<int, float>>> &Eout2,
    std::vector<int> &NSorted) {
  map<int, list<pair<int, float>>> Eout = Eout2;  // outgoing edges
  map<int, list<int>> Ein;                        // incoming edges

  // create Ein from Eout
  for (const auto &el : Eout) {
    const int n = el.first;
    for (const pair<int, float> &el2 : el.second) {
      const int m = el2.first;
      Ein[m].push_back(n);
    }
  }

  vector<int> L;  // empty set at the beginning, sorted nodes at the end
  vector<int> S;  // all nodes having no incoming edges, i.e. start nodes

  // create S
  for (const int n : N) {
    if (Ein.find(n) == Ein.end()) S.push_back(n);
  }

  while (!S.empty()) {
    const int n = S.back();
    S.pop_back();
    L.push_back(n);
    // find all outgoing edges for node n, i.e. n->m edges
    map<int, list<pair<int, float>>>::iterator itOut = Eout.find(n);
    if (itOut != Eout.end()) {
      list<pair<int, float>> &Eoutn = Eout[n];
      for (list<pair<int, float>>::iterator it1 = Eoutn.begin();
           it1 != Eoutn.end();) {
        const int m = it1->first;
        // remove the n->m edge from Eout
        Eoutn.erase(it1++);
        // find incoming edges for node m, i.e. l->m edges
        bool emptyin = false;
        map<int, list<int>>::iterator itIn = Ein.find(m);
        if (itIn != Ein.end()) {
          list<int> &Einm = itIn->second;
          for (list<int>::iterator it2 = Einm.begin(); it2 != Einm.end();) {
            const int l = *it2;
            // remove the n->m edge from Ein
            if (l == n)
              Einm.erase(it2++);
            else
              it2++;
          }
          if (Einm.empty()) {
            Ein.erase(itIn);
            emptyin = true;
          }
        } else
          emptyin = true;
        if (emptyin) S.push_back(m);  // m has no other incoming edges
      }
      if (Eoutn.empty()) {
        Eout.erase(itOut);
      }
    }
  }

  if (!Eout.empty()) {
    cout << "oriented cycles exist" << endl;
    return false;
  }
  NSorted = L;
  return true;
}
