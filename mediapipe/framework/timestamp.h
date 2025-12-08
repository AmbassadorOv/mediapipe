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

// Two classes: Timestamp and TimestampDiff for specifying timestamps
// within the CalculatorFramework (and mediapipe in general).  MediaPipe
// timestamps are in units of _microseconds_.
//
// There are several special values (All these values must be constructed
// using the static methods provided):
//   Unset:       The default initialization value, not generally
//                valid when a timestamp is required.
//   Unstarted:   The timestamp before any valid timestamps.  This is
//                the input timestamp during Open().
//   PreStream:   A value for specifying that a packet contains "header"
//                data that should be processed before any other
//                timestamp.  Like PostStream, if this value is sent then
//                it must be the only value that is sent on the stream.
//   Min:         The minimum range timestamp to see in Process().
//                Any number of "range" timestamp can be sent over a
//                stream, provided that they are sent in monotonically
//                increasing order.
//   Max:         The maximum range timestamp to see in Process().
//   PostStream:  A value for specifying that a packet pertains to
//                the entire stream.  This "summary" timestamp occurs
//                after all the "range" timestamps.  If this timestamp
//                is sent on a stream, it must be the only packet sent.
//   OneOverPostStream:
//                The value immediately following PostStream.
//                This should only be used internally.
//   Done:        The timestamp after all valid timestamps.
//                This is the input timestamp during Close().

#ifndef MEDIAPIPE_FRAMEWORK_TIMESTAMP_H_
#define MEDIAPIPE_FRAMEWORK_TIMESTAMP_H_

#include <cmath>
#include <cstdint>
#include <limits>
#include <ostream>
#include <string>

#include "absl/log/absl_check.h"
#include "mediapipe/framework/deps/safe_int.h"
#include "mediapipe/framework/port/logging.h"

namespace mediapipe {

// A safe int checks each arithmetic operation to make sure it will not
// have underflow/overflow etc.  This type is used internally by Timestamp
// and TimestampDiff.
MEDIAPIPE_DEFINE_SAFE_INT_TYPE(TimestampBaseType, int64_t,
                               mediapipe::intops::LogFatalOnError)

class TimestampDiff;

/**
 * @brief A class which represents a timestamp in the calculator framework.
 */
class Timestamp {
 public:
  Timestamp();
  /**
   * @brief Constructor.
   * @param timestamp The timestamp value.
   */
  explicit Timestamp(int64_t timestamp);
  explicit Timestamp(TimestampBaseType timestamp);

  // Timestamps are in microseconds.
  static constexpr double kTimestampUnitsPerSecond = 1000000.0;
  static constexpr double kTimestampUnitsPerMillisecond = 1000.0;

  // Use the default copy constructor, assignment operator, and destructor.

  /**
   * @brief Get the underlying int64_t value being used.
   * @return The underlying int64_t value.
   */
  int64_t Value() const { return timestamp_.value(); }

  /**
   * @brief Return the value in units of seconds.
   * @return The value in units of seconds.
   */
  double Seconds() const { return Value() / kTimestampUnitsPerSecond; }

  /**
   * @brief Return the value in units of milliseconds.
   * @return The value in units of milliseconds.
   */
  double Milliseconds() const {
    return Value() / kTimestampUnitsPerMillisecond;
  }

  /**
   * @brief Return the value in units of microseconds.
   * @return The value in units of microseconds.
   */
  int64_t Microseconds() const { return Value(); }

  /**
   * @brief This provides a human readable string for the special values.
   * @return A human readable string.
   */
  std::string DebugString() const;

  /**
   * @brief For use by framework. Clients or Calculator implementations should
   * not call this.
   * @param timestamp The timestamp value.
   * @return A Timestamp object.
   */
  static Timestamp CreateNoErrorChecking(int64_t timestamp);

  /**
   * @brief Create a timestamp from a seconds value.
   * @param seconds The seconds value.
   * @return A Timestamp object.
   */
  static Timestamp FromSeconds(double seconds) {
    return Timestamp(
        TimestampBaseType{std::round(seconds * kTimestampUnitsPerSecond)});
  }

  /**
   * @brief Create a timestamp from a milliseconds value.
   * @param milliseconds The milliseconds value.
   * @return A Timestamp object.
   */
  static Timestamp FromMilliseconds(double milliseconds) {
    return Timestamp(TimestampBaseType{
        std::round(milliseconds * kTimestampUnitsPerMillisecond)});
  }

  /**
   * @brief Create a timestamp from an integer microseconds value.
   * @param microseconds The microseconds value.
   * @return A Timestamp object.
   */
  static Timestamp FromMicroseconds(int64_t microseconds) {
    return Timestamp(TimestampBaseType{microseconds});
  }

  // Special values.
  static Timestamp Unset();
  static Timestamp Unstarted();
  static Timestamp PreStream();
  static Timestamp Min();
  static Timestamp Max();
  static Timestamp PostStream();
  static Timestamp OneOverPostStream();
  static Timestamp Done();

  /**
   * @brief A special value is any of the values which cannot be constructed
   * directly but must be constructed using the helper functions given above.
   * @return True if the timestamp is a special value.
   */
  bool IsSpecialValue() const { return *this <= Min() || *this >= Max(); }

  /**
   * @brief A range value is anything between Min() and Max() (inclusive).
   * @return True if the timestamp is a range value.
   */
  bool IsRangeValue() const { return *this >= Min() && *this <= Max(); }

  /**
   * @brief Returns true iff this can be the timestamp of a Packet in a
   * stream.
   * @return True iff this can be the timestamp of a Packet in a
   * stream.
   */
  bool IsAllowedInStream() const {
    // This is a simplified expression for
    // IsRangeValue() or PreStream() or PostStream().
    return *this >= PreStream() && *this <= PostStream();
  }

  // Common operators.
  bool operator==(Timestamp other) const {
    return timestamp_ == other.timestamp_;
  }
  bool operator!=(Timestamp other) const {
    return !(timestamp_ == other.timestamp_);
  }
  bool operator<(Timestamp other) const {
    return timestamp_ < other.timestamp_;
  }
  bool operator<=(Timestamp other) const {
    return timestamp_ <= other.timestamp_;
  }
  bool operator>(Timestamp other) const {
    return timestamp_ > other.timestamp_;
  }
  bool operator>=(Timestamp other) const {
    return timestamp_ >= other.timestamp_;
  }
  TimestampDiff operator-(Timestamp other) const;
  Timestamp operator+(TimestampDiff offset) const;
  Timestamp operator-(TimestampDiff offset) const;

  // Provided for convenience.
  Timestamp operator+=(TimestampDiff other);
  Timestamp operator-=(TimestampDiff other);
  Timestamp operator++();
  Timestamp operator--();
  Timestamp operator++(int);
  Timestamp operator--(int);

  /**
   * @brief Returns the next timestamp in the range [Min .. Max].
   * @return The next timestamp in the range [Min .. Max].
   */
  Timestamp NextAllowedInStream() const;

  /**
   * @brief Returns true if there's a next timestamp in the range [Min .. Max]
   * after this one.
   * @return True if there's a next timestamp in the range [Min .. Max]
   * after this one.
   */
  bool HasNextAllowedInStream() const;

  /**
   * @brief Returns the previous timestamp in the range [Min .. Max].
   * @return The previous timestamp in the range [Min .. Max].
   */
  Timestamp PreviousAllowedInStream() const;

 private:
  TimestampBaseType timestamp_;
};

/**
 * @brief A class which represents the difference between two timestamps in the
 * calculator framework.
 */
class TimestampDiff {
 public:
  TimestampDiff() : timestamp_(0) {}
  /**
   * @brief Constructor.
   * @param timestamp The timestamp value.
   */
  TimestampDiff(int64_t timestamp) : timestamp_(timestamp) {  // NOLINT
  }
  TimestampDiff(TimestampBaseType timestamp)  // NOLINT
      : timestamp_(timestamp) {}

  // Use the default copy constructor, assignment operator, and destructor.

  /**
   * @brief Get the underlying int64_t value being used.
   * @return The underlying int64_t value.
   */
  int64_t Value() const { return timestamp_.value(); }

  /**
   * @brief Return the value in units of seconds.
   * @return The value in units of seconds.
   */
  double Seconds() const {
    return Value() / Timestamp::kTimestampUnitsPerSecond;
  }

  /**
   * @brief Return the value in units of milliseconds.
   * @return The value in units of milliseconds.
   */
  double Milliseconds() const {
    return Value() / Timestamp::kTimestampUnitsPerMillisecond;
  }

  /**
   * @brief Return the value in units of microseconds.
   * @return The value in units of microseconds.
   */
  int64_t Microseconds() const { return Value(); }
  std::string DebugString() const;

  bool operator==(TimestampDiff other) const {
    return timestamp_ == other.timestamp_;
  }
  bool operator!=(TimestampDiff other) const {
    return !(timestamp_ == other.timestamp_);
  }
  bool operator<(TimestampDiff other) const {
    return timestamp_ < other.timestamp_;
  }
  bool operator<=(TimestampDiff other) const {
    return timestamp_ <= other.timestamp_;
  }
  bool operator>(TimestampDiff other) const {
    return timestamp_ > other.timestamp_;
  }
  bool operator>=(TimestampDiff other) const {
    return timestamp_ >= other.timestamp_;
  }
  TimestampDiff operator-() const { return TimestampDiff(-timestamp_); }
  TimestampDiff operator+(TimestampDiff other) const;
  TimestampDiff operator-(TimestampDiff other) const;
  Timestamp operator+(Timestamp timestamp) const;

  /**
   * @brief Create a timestamp from a seconds value.
   * @param seconds The seconds value.
   * @return A TimestampDiff object.
   */
  static TimestampDiff FromSeconds(double seconds) {
    return TimestampDiff(TimestampBaseType{
        std::round(seconds * Timestamp::kTimestampUnitsPerSecond)});
  }

  /**
   * @brief Create a timestamp from a milliseconds value.
   * @param milliseconds The milliseconds value.
   * @return A TimestampDiff object.
   */
  static TimestampDiff FromMilliseconds(double milliseconds) {
    return TimestampDiff(TimestampBaseType{
        std::round(milliseconds * Timestamp::kTimestampUnitsPerMillisecond)});
  }

  /**
   * @brief Create a timestamp from an integer microseconds value.
   * @param microseconds The microseconds value.
   * @return A TimestampDiff object.
   */
  static TimestampDiff FromMicroseconds(int64_t microseconds) {
    return TimestampDiff(TimestampBaseType{microseconds});
  }

  // Special values.

  static TimestampDiff Unset() {
    return TimestampDiff(Timestamp::Unset().Value());
  }

 private:
  TimestampBaseType timestamp_;
};

// Convenience functions which are equivalent to using arg.DebugString().
std::ostream& operator<<(std::ostream& os, Timestamp arg);
std::ostream& operator<<(std::ostream& os, TimestampDiff arg);

// Implementation details.

inline Timestamp::Timestamp()
    : timestamp_(std::numeric_limits<int64_t>::min()) {}

inline Timestamp::Timestamp(int64_t timestamp) : timestamp_(timestamp) {
  ABSL_CHECK(!IsSpecialValue())
      << "Cannot directly create a Timestamp with a special value: "
      << CreateNoErrorChecking(timestamp);
}

inline Timestamp::Timestamp(TimestampBaseType timestamp)
    : timestamp_(timestamp) {
  ABSL_CHECK(!IsSpecialValue())
      << "Cannot directly create a Timestamp with a special value: "
      << CreateNoErrorChecking(timestamp.value());
}

inline Timestamp Timestamp::CreateNoErrorChecking(int64_t timestamp) {
  Timestamp tmp;
  tmp.timestamp_ = TimestampBaseType(timestamp);
  return tmp;
}

inline Timestamp Timestamp::Unset() { return Timestamp(); }

inline Timestamp Timestamp::Unstarted() {
  return CreateNoErrorChecking(std::numeric_limits<int64_t>::min() + 1);
}

inline Timestamp Timestamp::PreStream() {
  return CreateNoErrorChecking(std::numeric_limits<int64_t>::min() + 2);
}

inline Timestamp Timestamp::Min() {
  return CreateNoErrorChecking(std::numeric_limits<int64_t>::min() + 3);
}

inline Timestamp Timestamp::Max() {
  return CreateNoErrorChecking(std::numeric_limits<int64_t>::max() - 3);
}

inline Timestamp Timestamp::PostStream() {
  return CreateNoErrorChecking(std::numeric_limits<int64_t>::max() - 2);
}

inline Timestamp Timestamp::OneOverPostStream() {
  return CreateNoErrorChecking(std::numeric_limits<int64_t>::max() - 1);
}

inline Timestamp Timestamp::Done() {
  return CreateNoErrorChecking(std::numeric_limits<int64_t>::max());
}

}  // namespace mediapipe

#endif  // MEDIAPIPE_FRAMEWORK_TIMESTAMP_H_
