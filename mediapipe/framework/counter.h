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
//
// The abstract class of counter.

#ifndef MEDIAPIPE_FRAMEWORK_COUNTER_H_
#define MEDIAPIPE_FRAMEWORK_COUNTER_H_

#include <cstdint>

namespace mediapipe {

/**
 * @brief The abstract class of counter.
 */
class Counter {
 public:
  Counter() {}
  virtual ~Counter() {}

  /**
   * @brief Increments the counter by 1.
   */
  virtual void Increment() = 0;

  /**
   * @brief Increments the counter by a given amount.
   * @param amount The amount to increment by.
   */
  virtual void IncrementBy(int amount) = 0;

  /**
   * @brief Gets the current value of the counter.
   * @return The current value of the counter.
   */
  virtual int64_t Get() = 0;
};

}  // namespace mediapipe

#endif  // MEDIAPIPE_FRAMEWORK_COUNTER_H_
