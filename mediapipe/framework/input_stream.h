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
// Defines InputStream, from which Calculator implementations get input
// packets.

#ifndef MEDIAPIPE_FRAMEWORK_INPUT_STREAM_H_
#define MEDIAPIPE_FRAMEWORK_INPUT_STREAM_H_

#include <string>

#include "absl/base/macros.h"
#include "mediapipe/framework/packet.h"
#include "mediapipe/framework/port.h"

namespace mediapipe {

/**
 * @brief An interface for getting input packets in a calculator.
 */
class InputStream {
 public:
  InputStream(const InputStream&) = delete;
  InputStream& operator=(const InputStream&) = delete;

  /**
   * @brief Returns the const reference of the input, if the input is present,
   * or an empty Packet otherwise.
   * @return The const reference of the input.
   */
  virtual const Packet& Value() const = 0;

  /**
   * @brief Returns the non-const reference of the input, if the input is
   * present, or an empty Packet otherwise.
   * @return The non-const reference of the input.
   */
  virtual Packet& Value() = 0;

  /**
   * @brief Syntactic sugar for a common way to get the typed value from the
   * stream.
   * @tparam T The type of the value.
   * @return The typed value.
   */
  template <typename T>
  const T& Get() const {
    return Value().Get<T>();
  }

  /**
   * @brief Syntactic sugar for checking if the input is empty.
   * @return True if the input is empty.
   */
  bool IsEmpty() const { return Value().IsEmpty(); }

  /**
   * @brief Returns true iff the Inputstream has been closed and there are no
   * remaining Packets queued for processing.
   * @return True iff the Inputstream has been closed and there are no
   * remaining Packets queued for processing.
   */
  virtual bool IsDone() const = 0;

  /**
   * @brief Returns the header Packet, if one was set for the
   * corresponding OutputStream in the upstream Calculator's Open method.
   * @return The header Packet.
   */
  Packet Header() const { return header_; }

 protected:
  InputStream() = default;
  virtual ~InputStream() = default;

  Packet header_;
};

}  // namespace mediapipe

#endif  // MEDIAPIPE_FRAMEWORK_INPUT_STREAM_H_
