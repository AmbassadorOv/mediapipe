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

#ifndef MEDIAPIPE_FRAMEWORK_PACKET_GENERATOR_GRAPH_H_
#define MEDIAPIPE_FRAMEWORK_PACKET_GENERATOR_GRAPH_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "absl/base/attributes.h"
#include "absl/base/macros.h"
#include "mediapipe/framework/executor.h"
#include "mediapipe/framework/packet.h"
#include "mediapipe/framework/packet_generator.pb.h"
#include "mediapipe/framework/port.h"
#include "mediapipe/framework/port/proto_ns.h"
#include "mediapipe/framework/port/status.h"
#include "mediapipe/framework/validated_graph_config.h"

namespace mediapipe {

/**
 * @brief A graph of packet generators.
 *
 * Initialize runs all the generators which it can (i.e. whose input
 * side packets are available), and stores the produced packets and
 * the generators that are not yet executed.
 *
 * Each call to RunGraphSetup uses the provided extra side packets, runs
 * all remaining components of the graph, and produces a complete set of
 * output side packets.  Initialize should only be called once.
 * RunGraphSetup may be called any number of times.
 *
 * This class is thread compatible.
 */
class PacketGeneratorGraph {
 public:
  PacketGeneratorGraph() = default;
  PacketGeneratorGraph(const PacketGeneratorGraph&) = delete;
  PacketGeneratorGraph& operator=(const PacketGeneratorGraph&) = delete;

  /**
   * @brief The destructor has an essentially default implementation.
   */
  virtual ~PacketGeneratorGraph();

  /**
   * @brief Initialize the PacketGeneratorGraph with the validated graph config
   * and executor to use.
   * @param validated_graph The validated graph config.
   * @param executor The executor to use.
   * @param input_side_packets The input side packets.
   * @return absl::Status OkStatus() on success, any other status on failure.
   */
  virtual absl::Status Initialize(
      const ValidatedGraphConfig* validated_graph,
      mediapipe::Executor* executor,
      const std::map<std::string, Packet>& input_side_packets);

  /**
   * @brief Add the input_side_packets and run any remaining generators to
   * produce output_side_packets.
   * @param input_side_packets The input side packets.
   * @param output_side_packets The output side packets.
   * @param non_scheduled_generators The non-scheduled generators.
   * @return absl::Status OkStatus() on success, any other status on failure.
   */
  virtual absl::Status RunGraphSetup(
      const std::map<std::string, Packet>& input_side_packets,
      std::map<std::string, Packet>* output_side_packets,
      std::vector<int>* non_scheduled_generators = nullptr) const;

  /**
   * @brief Get the base packets: the packets which are produced when
   * Initialize is called.
   * @return The base packets.
   */
  virtual const std::map<std::string, Packet>& BasePackets() const {
    return base_packets_;
  }

  /**
   * @brief Get the non-base PacketGenerators (those not run at Initialize
   * time due to missing dependencies).
   * @return The non-base PacketGenerators.
   */
  virtual const std::vector<int>& NonBaseGenerators() const {
    return non_base_generators_;
  }

 private:
  // Execute the generators until no more can be run, outputting all side
  // packets and unrunnable generators.  "initial" must be set to true for
  // the first pass and false for subsequent passes.  output_side_packets
  // must be set to include the input side packets before calling.
  absl::Status ExecuteGenerators(
      std::map<std::string, Packet>* output_side_packets,
      std::vector<int>* non_scheduled_generators, bool initial) const;

  // The validated graph configuration.  We do not own this but it must
  // outlive this object.
  const ValidatedGraphConfig* validated_graph_ = nullptr;
  // An object to own the validated_graph_config if it needs to be deleted.
  // TODO Remove this after removing legacy functions.
  std::unique_ptr<ValidatedGraphConfig> validated_graph_owner_;

  // The executor to use for running the generators.  We do not own the
  // executor but it must outlive this object.
  mediapipe::Executor* executor_ = nullptr;
  // An object to own the executor if it needs to be deleted.
  std::unique_ptr<mediapipe::Executor> executor_owner_;

  // The base level packets available after initialization.
  std::map<std::string, Packet> base_packets_;
  // The non-base level generators in the graph, excluding those already
  // executed in Initialize.  We store the indexes into their positions
  // in the ValidatedGraphConfig object.
  std::vector<int> non_base_generators_;
};

}  // namespace mediapipe

#endif  // MEDIAPIPE_FRAMEWORK_PACKET_GENERATOR_GRAPH_H_
