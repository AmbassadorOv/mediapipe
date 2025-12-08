// Copyright 2019 The MediaPipe Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef MEDIAPIPE_FRAMEWORK_CAMERA_INTRINSICS_H_
#define MEDIAPIPE_FRAMEWORK_CAMERA_INTRINSICS_H_

/**
 * @brief Holds the camera intrinsics parameters.
 */
class CameraIntrinsics {
 public:
  /**
   * @brief Constructor.
   * @param fx The focal length in x.
   * @param fy The focal length in y.
   * @param cx The principal point in x.
   * @param cy The principal point in y.
   * @param width The image width.
   * @param height The image height.
   */
  CameraIntrinsics(float fx, float fy, float cx, float cy, float width,
                   float height)
      : fx_(fx), fy_(fy), cx_(cx), cy_(cy), width_(width), height_(height) {}

  /**
   * @brief Constructor.
   * @param fx The focal length in x.
   * @param fy The focal length in y.
   * @param cx The principal point in x.
   * @param cy The principal point in y.
   */
  CameraIntrinsics(float fx, float fy, float cx, float cy)
      : CameraIntrinsics(fx, fy, cx, cy, -1, -1) {}

  /**
   * @brief Returns the focal length in x.
   * @return The focal length in x.
   */
  float fx() const { return fx_; }

  /**
   * @brief Returns the focal length in y.
   * @return The focal length in y.
   */
  float fy() const { return fy_; }

  /**
   * @brief Returns the principal point in x.
   * @return The principal point in x.
   */
  float cx() const { return cx_; }

  /**
   * @brief Returns the principal point in y.
   * @return The principal point in y.
   */
  float cy() const { return cy_; }

  /**
   * @brief Returns the image width.
   * @return The image width.
   */
  float width() const { return width_; }

  /**
   * @brief Returns the image height.
   * @return The image height.
   */
  float height() const { return height_; }

 private:
  // Lens focal length along the x-axis, in pixels.
  const float fx_;

  // Lens focal length along the y-axis, in pixels.
  const float fy_;

  // Principal point, x-coordinate on the image, in pixels.
  const float cx_;

  // Principal point, y-coordinate on the image, in pixels.
  const float cy_;

  // Image width, in pixels.
  const float width_;

  // Image height, in pixels.
  const float height_;
};

#endif  // MEDIAPIPE_FRAMEWORK_CAMERA_INTRINSICS_H_
