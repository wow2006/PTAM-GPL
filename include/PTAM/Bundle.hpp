// -*- c++ -*-
// Copyright 2008 Isis Innovation Limited
#pragma once
// Bundle.h
//
// This file declares the Bundle class along with a few helper classes.
// Bundle is the bundle adjustment core of the mapping system; instances
// of Bundle are generated by MapMaker to adjust the positions of
// keyframes (called Cameras in this file) and map points.
//
// It's a pretty straight-forward Levenberg-Marquardt bundle adjustment
// implementation closely following Hartley and Zisserman's MVG book, with
// the addition of a robust M-Estimator.
//
// Unfortunately, having undergone a few tweaks, the code is now
// not the easiest to read!
//
// Basic operation: MapMaker creates a new Bundle object;
// then adds map points and keyframes to adjust;
// then adds measurements of map points in keyframes;
// then calls Compute() to do bundle adjustment;
// then reads results back to update the map.

#include <list>
#include <map>
#include <set>
#include <vector>

#include <TooN/TooN.h>
#include <TooN/se3.h>

#include "PTAM/ATANCamera.hpp"

using namespace TooN;

namespace PTAM {
// An index into the big measurement map which stores all the measurements.

/**
 * @brief Camera struct holds the pose of a keyframe
 * and some computation intermediates
 */
struct Camera {
  bool bFixed;
  SE3<> se3CfW;
  SE3<> se3CfWNew;
  Matrix<6> m6U;        // Accumulator
  Vector<6> v6EpsilonA; // Accumulator
  int nStartRow;
};

/**
 * @brief Camera-camera pair index
 */
struct OffDiagScriptEntry {
  int j;
  int k;
};

/**
 * @brief A map point, plus computation intermediates.
 */
struct Point {
  /**
   * @brief
   */
  Point() {
    nMeasurements = 0;
    nOutliers = 0;
  }

  Vector<3> v3Pos;
  Vector<3> v3PosNew;
  Matrix<3> m3V;        // Accumulator
  Vector<3> v3EpsilonB; // Accumulator
  Matrix<3> m3VStarInv;

  int nMeasurements;
  int nOutliers;
  std::set<int> sCameras; // Which cameras observe this point?
  std::vector<OffDiagScriptEntry>
      vOffDiagonalScript; // A record of all camera-camera pairs observing this
                          // point
};

/**
 * @brief A measurement of a point by a camera, plus
 * computation intermediates.
 */
struct Meas {
  /**
   * @brief
   */
  Meas() { bBad = false; }

  // Which camera/point did this measurement come from?
  int p; // The point  - called i in MVG
  int c; // The camera - called j in MVG

  /**
   * @brief
   *
   * @param rhs
   *
   * @return
   */
  bool operator<(const Meas &rhs) const {
    return (c < rhs.c || (c == rhs.c && p < rhs.p));
  }

  bool bBad;

  Vector<2> v2Found;
  Vector<2> v2Epsilon;
  Matrix<2, 6> m26A;
  Matrix<2, 3> m23B;
  Matrix<6, 3> m63W;
  Matrix<6, 3> m63Y;
  double dSqrtInvNoise;

  // Temporary projection quantities
  Vector<3> v3Cam;
  double dErrorSquared;
  Matrix<2> m2CamDerivs;
};

/**
 * @brief Core bundle adjustment class
 */
class Bundle {
public:
  /**
   * @brief We need the camera model because we do full distorting
   * projection in the bundle adjuster. Could probably get
   * away with a linear approximation.
   *
   * @param TCam
   */
  Bundle(const ATANCamera &TCam);

  /**
   * @brief Add a viewpoint. bFixed signifies that this one
   * is not to be adjusted.
   *
   * @param se3CamFromWorld
   * @param bFixed
   *
   * @return
   */
  int AddCamera(SE3<> se3CamFromWorld, bool bFixed);

  /**
   * @brief Add a map point.
   *
   * @param v3Pos
   *
   * @return
   */
  int AddPoint(Vector<3> v3Pos);

  /**
   * @brief Add a measurement
   *
   * @param nCam
   * @param nPoint
   * @param v2Pos
   * @param dSigmaSquared
   */
  void AddMeas(int nCam, int nPoint, Vector<2> v2Pos, double dSigmaSquared);

  /**
   * @brief Perform bundle adjustment. Aborts if *pbAbortSignal gets set to
   * true. Returns number of accepted update iterations, or negative on error.
   *
   * @param pbAbortSignal
   *
   * @return
   */
  int Compute(bool *pbAbortSignal);

  /**
   * @brief Has bundle adjustment converged?
   *
   * @return
   */
  bool Converged() const { return mbConverged; }

  /**
   * @brief Point coords after adjustment
   *
   * @param n
   *
   * @return
   */
  Vector<3> GetPoint(int n);

  /**
   * @brief Camera pose after adjustment
   *
   * @param n
   *
   * @return
   */
  SE3<> GetCamera(int n);

  /**
   * @brief Measurements flagged as outliers
   */
  std::vector<std::pair<int, int>> GetOutlierMeasurements();

  /**
   * @brief Points flagged as outliers
   *
   * @return
   */
  std::set<int> GetOutliers();

protected:
  inline void
  ProjectAndFindSquaredError(Meas &meas); // Project a single point in a single
                                          // view, compare to measurement
  template <class MEstimator> bool Do_LM_Step(bool *pbAbortSignal);
  template <class MEstimator> double FindNewError();
  void GenerateMeasLUTs();
  void GenerateOffDiagScripts();
  void
  ClearAccumulators(); // Zero temporary quantities stored in cameras and points
  void ModifyLambda_GoodStep();
  void ModifyLambda_BadStep();

  std::vector<Point> mvPoints;
  std::vector<Camera> mvCameras;
  std::list<Meas> mMeasList;
  std::vector<std::pair<int, int>> mvOutlierMeasurementIdx; // p-c pair
  std::vector<std::vector<Meas *>>
      mvMeasLUTs; // Each camera gets a per-point table of pointers to valid
                  // measurements

  ATANCamera mCamera;
  int mnCamsToUpdate;
  int mnStartRow;
  double mdSigmaSquared;
  double mdLambda;
  double mdLambdaFactor;
  bool mbConverged;
  bool mbHitMaxIterations;
  int mnCounter;
  int mnAccepted;

  GVars3::gvar3<int> mgvnMaxIterations;
  GVars3::gvar3<double> mgvdUpdateConvergenceLimit;
  GVars3::gvar3<int> mgvnBundleCout;

  bool *mpbAbortSignal;
};

} // namespace PTAM
