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
// Defines CalculatorRunner which can be used to run a Calculator in
// isolation. This is useful for testing.

#ifndef MEDIAPIPE_FRAMEWORK_CALCULATOR_RUNNER_H_
#define MEDIAPIPE_FRAMEWORK_CALCULATOR_RUNNER_H_

#include <memory>
#include <string>
#include <vector>

#include "absl/base/macros.h"
#include "mediapipe/framework/calculator_framework.h"
#include "mediapipe/framework/port/status.h"

namespace mediapipe {

class CalculatorGraph;

/**
 * @brief The class for running the Calculator with given inputs and examining
 * outputs.
 */
class CalculatorRunner {
 public:
  /**
   * @brief A representation of input or output stream contents.
   */
  struct StreamContents {
    // The Packets in the stream.
    std::vector<Packet> packets;
    // Stream header.
    Packet header;
  };
  // A collection of StreamContents by either index or tag.
  typedef internal::Collection<StreamContents> StreamContentsSet;

  /**
   * @brief Preferred constructor.
   * @param node_config The node config.
   */
  explicit CalculatorRunner(const CalculatorGraphConfig::Node& node_config);
#if !defined(MEDIAPIPE_PROTO_LITE)
  /**
   * @brief Convenience constructor which takes a node_config string directly.
   * @param node_config_string The node config string.
   */
  explicit CalculatorRunner(const std::string& node_config_string);
  /**
   * @brief Convenience constructor to initialize a calculator which uses
   * indexes (not tags) for all its fields.
   * @param calculator_type The type of the calculator.
   * @param options_string The options string.
   * @param num_inputs The number of inputs.
   * @param num_outputs The number of outputs.
   * @param num_side_packets The number of side packets.
   */
  CalculatorRunner(const std::string& calculator_type,
                   const std::string& options_string, int num_inputs,
                   int num_outputs, int num_side_packets);
#endif
  /**
   * @brief Minimal constructor which requires additional calls to define
   * inputs, outputs, and input side packets.
   * @param calculator_type The type of the calculator.
   * @param options The calculator options.
   */
  ABSL_DEPRECATED("Initialize CalculatorRunner with a proto instead.")
  CalculatorRunner(const std::string& calculator_type,
                   const CalculatorOptions& options);

  CalculatorRunner(const CalculatorRunner&) = delete;
  CalculatorRunner& operator=(const CalculatorRunner&) = delete;

  ~CalculatorRunner();

  /**
   * @brief Sets the number of input streams, output streams, or input side
   * packets, respectively.
   * @param n The number of streams or packets.
   */
  ABSL_DEPRECATED("Initialize CalculatorRunner with a proto instead.")
  void SetNumInputs(int n);
  ABSL_DEPRECATED("Initialize CalculatorRunner with a proto instead.")
  void SetNumOutputs(int n);
  ABSL_DEPRECATED("Initialize CalculatorRunner with a proto instead.")
  void SetNumInputSidePackets(int n);

  /**
   * @brief Initializes the inputs, outputs, or side packets using a
   * TagAndNameInfo.
   * @param info The TagAndNameInfo.
   */
  ABSL_DEPRECATED("Initialize CalculatorRunner with a proto instead.")
  void InitializeInputs(const tool::TagAndNameInfo& info);
  ABSL_DEPRECATED("Initialize CalculatorRunner with a proto instead.")
  void InitializeOutputs(const tool::TagAndNameInfo& info);
  ABSL_DEPRECATED("Initialize CalculatorRunner with a proto instead.")
  void InitializeInputSidePackets(const tool::TagAndNameInfo& info);

  /**
   * @brief Returns mutable access to the input stream contents.
   * @return A pointer to the input stream contents.
   */
  StreamContentsSet* MutableInputs() { return inputs_.get(); }

  /**
   * @brief Returns mutable access to the input side packets.
   * @return A pointer to the input side packets.
   */
  PacketSet* MutableSidePackets() { return input_side_packets_.get(); }

  /**
   * @brief Runs the calculator.
   * @return absl::Status OkStatus() on success, any other status on failure.
   */
  absl::Status Run();

  /**
   * @brief Returns the vector of contents of the output streams.
   * @return The vector of contents of the output streams.
   */
  const StreamContentsSet& Outputs() const { return *outputs_; }

  /**
   * @brief Returns the access to the output side packets.
   * @return The access to the output side packets.
   */
  const PacketSet& OutputSidePackets() { return *output_side_packets_; }

  /**
   * @brief Returns a graph counter.
   * @param name The name of the counter.
   * @return A pointer to the counter.
   */
  mediapipe::Counter* GetCounter(const std::string& name);

  /**
   * @brief Returns all graph counters values.
   * @return A map of counter names to values.
   */
  std::map<std::string, int64_t> GetCountersValues();

 private:
  static const char kSourcePrefix[];
  static const char kSinkPrefix[];

  // Initialize using a node config (does the constructor's work).
  absl::Status InitializeFromNodeConfig(
      const CalculatorGraphConfig::Node& node_config);

  // Builds the graph if one does not already exist.
  absl::Status BuildGraph();

  CalculatorGraphConfig::Node node_config_;

  // Log the calculator proto after it is created from the provided
  // parameters.  This aids users in migrating to the recommended
  // constructor.
  bool log_calculator_proto_ = false;

  std::unique_ptr<StreamContentsSet> inputs_;
  std::unique_ptr<StreamContentsSet> outputs_;
  std::unique_ptr<PacketSet> input_side_packets_;
  std::unique_ptr<PacketSet> output_side_packets_;
  std::unique_ptr<CalculatorGraph> graph_;
};

}  // namespace mediapipe

#endif  // MEDIAPIPE_FRAMEWORK_CALCULATOR_RUNNER_H_
