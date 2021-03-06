/* ----------------------------------------------------------------------------
 * Copyright 2017, Massachusetts Institute of Technology,
 * Cambridge, MA 02139
 * All Rights Reserved
 * Authors: Luca Carlone, et al. (see THANKS for the full author list)
 * See LICENSE for the license information
 * -------------------------------------------------------------------------- */

/**
 * @file   Tracker.h
 * @brief  Class describing temporal tracking
 * @author Antoni Rosinol
 * @author Luca Carlone
 */

// TODO(Toni): put tracker in another folder.
#pragma once

#include <opencv2/opencv.hpp>

#include <gtsam/base/Matrix.h>
#include <gtsam/geometry/Pose3.h>
#include <gtsam/geometry/Rot3.h>
#include <gtsam/geometry/StereoCamera.h>

#include "kimera-vio/frontend/CameraParams.h"
#include "kimera-vio/frontend/Frame.h"
#include "kimera-vio/frontend/OpticalFlowPredictor.h"
#include "kimera-vio/frontend/StereoFrame.h"
#include "kimera-vio/frontend/Tracker-definitions.h"
#include "kimera-vio/frontend/VioFrontEndParams.h"
#include "kimera-vio/utils/ThreadsafeQueue.h"
#include "kimera-vio/utils/Macros.h"

namespace VIO {

// TODO(Toni): Fast-forwarding bcs of an issue wiht includes:
// if you include here the display-definitions.h, a million errors appear, this
// should go away after properly cleaning what each file includes.
class VisualizerOutput;
typedef ThreadsafeQueue<std::unique_ptr<VisualizerOutput>> DisplayQueue;

class Tracker {
 public:
  KIMERA_POINTER_TYPEDEFS(Tracker);
  KIMERA_DELETE_COPY_CONSTRUCTORS(Tracker);
  EIGEN_MAKE_ALIGNED_OPERATOR_NEW

  /**
   * @brief Tracker tracks features from frame to frame.
   * @param tracker_params Parameters for feature tracking
   * @param camera_params Parameters for the camera used for tracking.
   */
  Tracker(const FrontendParams& tracker_params,
          const CameraParams& camera_params,
          DisplayQueue* display_queue = nullptr);

  // Tracker parameters.
  const FrontendParams tracker_params_;

  // Mask for features.
  cv::Mat cam_mask_;

 public:
  void featureTracking(Frame* ref_frame,
                       Frame* cur_frame,
                       const gtsam::Rot3& inter_frame_rotation);

  // Get good features to track from image (wrapper for opencv
  // goodFeaturesToTrack)
  static void MyGoodFeaturesToTrackSubPix(
      const cv::Mat& image,
      const int& max_corners,
      const double& quality_level,
      const double& min_distance,  // Not const because modified dkwhy inside...
      const cv::Mat& mask,
      const int& blockSize,
      const bool& useHarrisDetector,
      const double& harrisK,
      KeypointsWithScores* corners_with_scores);

  void featureDetection(Frame* cur_frame);

  std::pair<TrackingStatus, gtsam::Pose3> geometricOutlierRejectionMono(
      Frame* ref_frame,
      Frame* cur_frame);

  std::pair<TrackingStatus, gtsam::Pose3> geometricOutlierRejectionStereo(
      StereoFrame& ref_frame,
      StereoFrame& cur_frame);

  // Contrarily to the previous 2 this also returns a 3x3 covariance for the
  // translation estimate.
  std::pair<TrackingStatus, gtsam::Pose3>
  geometricOutlierRejectionMonoGivenRotation(Frame* ref_frame,
                                             Frame* cur_frame,
                                             const gtsam::Rot3& R);

  std::pair<std::pair<TrackingStatus, gtsam::Pose3>, gtsam::Matrix3>
  geometricOutlierRejectionStereoGivenRotation(StereoFrame& ref_stereoFrame,
                                               StereoFrame& cur_stereoFrame,
                                               const gtsam::Rot3& R);

  void removeOutliersMono(const std::vector<int>& inliers,
                          Frame* ref_frame,
                          Frame* cur_frame,
                          KeypointMatches* matches_ref_cur);

  void removeOutliersStereo(const std::vector<int>& inliers,
                            StereoFrame* ref_stereoFrame,
                            StereoFrame* cur_stereoFrame,
                            KeypointMatches* matches_ref_cur);

  void checkStatusRightKeypoints(
      const std::vector<KeypointStatus>& right_keypoints_status);

  /* ---------------------------- CONST FUNCTIONS --------------------------- */
  // returns frame with markers
  cv::Mat getTrackerImage(
      const Frame& ref_frame,
      const Frame& cur_frame,
      const KeypointsCV& extra_corners_gray = KeypointsCV(),
      const KeypointsCV& extra_corners_blue = KeypointsCV()) const;

  /* ---------------------------- STATIC FUNCTIONS -------------------------- */
  static void findOutliers(const KeypointMatches& matches_ref_cur,
                           std::vector<int> inliers,
                           std::vector<int>* outliers);

  static void findMatchingKeypoints(const Frame& ref_frame,
                                    const Frame& cur_frame,
                                    KeypointMatches* matches_ref_cur);

  static void findMatchingStereoKeypoints(
      const StereoFrame& ref_stereoFrame,
      const StereoFrame& cur_stereoFrame,
      KeypointMatches* matches_ref_cur_stereo);

  static void findMatchingStereoKeypoints(
      const StereoFrame& ref_stereoFrame,
      const StereoFrame& cur_stereoFrame,
      const KeypointMatches& matches_ref_cur_mono,
      KeypointMatches* matches_ref_cur_stereo);

  static bool computeMedianDisparity(const KeypointsCV& ref_frame_kpts,
                                     const KeypointsCV& cur_frame_kpts,
                                     const KeypointMatches& matches_ref_cur,
                                     double* median_disparity);

  // Returns landmark_count (updated from the new keypoints),
  // and nr or extracted corners.
  KeypointsWithScores featureDetection(const Frame& cur_frame,
                                       const cv::Mat& cam_mask,
                                       const int need_n_corners);

  static std::pair<Vector3, Matrix3> getPoint3AndCovariance(
      const StereoFrame& stereoFrame,
      const gtsam::StereoCamera& stereoCam,
      const size_t pointId,
      const gtsam::Matrix3& stereoPtCov,
      boost::optional<gtsam::Matrix3> Rmat = boost::none);

  // Get tracker info
  inline DebugTrackerInfo getTrackerDebugInfo() { return debug_info_; }

 private:
  // Incremental id assigned to new landmarks.
  LandmarkId landmark_count_;

  // Camera params for the camera used to track: currently we only use K if the
  // rotational optical flow predictor is used
  const CameraParams camera_params_;

  // Feature tracking uses the optical flow predictor to have a better guess of
  // where the features moved from frame to frame.
  OpticalFlowPredictor::UniquePtr optical_flow_predictor_;

  // Debug info.
  DebugTrackerInfo debug_info_;

  // Display queue: push to this queue if you want to display an image.
  DisplayQueue* display_queue_;

  // This is not const as for debugging we want to redirect the image save path
  // where we like.
  std::string output_images_path_;
};

}  // namespace VIO
