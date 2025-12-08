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
// Defines OutputStream, which allows Calculator implementations to produce
// outputs, and allows clients to access the results produced by
// CalculatorGraph.

#ifndef MEDIAPIPE_FRAMEWORK_OUTPUT_STREAM_H_
#define MEDIAPIPE_FRAMEWORK_OUTPUT_STREAM_H_

#include <string>

#include "absl/base/macros.h"
#include "mediapipe/framework/packet.h"
#include "mediapipe/framework/port.h"
#include "mediapipe/framework/timestamp.h"

namespace mediapipe {

/**
 * @brief An interface for producing output packets in a calculator.
 */
class OutputStream {
 public:
  OutputStream(const OutputStream&) = delete;
  OutputStream& operator=(const OutputStream&) = delete;

  /**
   * @brief Returns the name of the stream.
   * @return The name of the stream.
   */
  virtual const std::string& Name() const = 0;

  /**
   * @brief Adds a Packet to the output stream.
   * @param packet The packet to add.
   */
  virtual void AddPacket(const Packet& packet) = 0;

  /**
   * @brief Adds a Packet to the output stream.
   * @param packet The packet to add.
   */
  virtual void AddPacket(Packet&& packet) = 0;

  /**
   * @brief Syntactic sugar for a common way to add Packets.
   * @tparam T The type of the data.
   * @param ptr A pointer to the data.
   * @param timestamp The timestamp of the packet.
   */
  template <typename T>
  void Add(T* ptr, Timestamp timestamp) {
    AddPacket(Adopt(ptr).At(timestamp));
  }

  /**
   * @brief Indicates that the next output will arrive no earlier than a
   * certain timestamp.
   * @param timestamp The next timestamp bound.
   */
  virtual void SetNextTimestampBound(Timestamp timestamp) = 0;

  /**
   * @brief Returns the next timestamp bound.
   * @return The next timestamp bound.
   */
  virtual Timestamp NextTimestampBound() const = 0;

  /**
   * @brief If a Calculator is guaranteed not to add any more Packets to an
   * OutputStream but may continue processing, it SHOULD call Close()
   * on a stream.
   */
  virtual void Close() = 0;

  /**
   * @brief true iff the OutputStream is closed.
   * @return true iff the OutputStream is closed.
   */
  virtual bool IsClosed() const = 0;

  /**
   * @brief Set the offset.
   * @param offset The offset.
   */
  virtual void SetOffset(TimestampDiff offset) = 0;

  /**
   * @brief Returns true iff SetOffset() has been called.
   * @return true iff SetOffset() has been called.
   */
  virtual bool OffsetEnabled() const = 0;

  /**
   * @brief Returns the offset set by the Calculator in the Open() method.
   * @return The offset.
   */
  virtual TimestampDiff Offset() const = 0;

  /**
   * @brief Sets the header for this OutputStream.
   * @param packet The header packet.
   */
  virtual void SetHeader(const Packet& packet) = 0;

  /**
   * @brief Returns the header set by the Calculator in the Open() method, or
   * an empty Packet if none was set.
   * @return The header packet.
   */
  virtual const Packet& Header() const = 0;

 protected:
  OutputStream() {}
  virtual ~OutputStream() {}
};

}  // namespace mediapipe

#endif  // MEDIAPIPE_FRAMEWORK_OUTPUT_STREAM_H_
