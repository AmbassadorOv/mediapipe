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

// Declares CalculatorNode which is internally used by the Calculator framework
// (in particular, CalculatorGraph and Calculator) to perform the computations.

#ifndef MEDIAPIPE_FRAMEWORK_CALCULATOR_NODE_H_
#define MEDIAPIPE_FRAMEWORK_CALCULATOR_NODE_H_

#include <stddef.h>

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>

#include "absl/base/macros.h"
#include "absl/base/thread_annotations.h"
#include "absl/status/status.h"
#include "absl/synchronization/mutex.h"
#include "absl/time/time.h"
#include "mediapipe/framework/calculator.pb.h"
#include "mediapipe/framework/calculator_base.h"
#include "mediapipe/framework/calculator_context.h"
#include "mediapipe/framework/calculator_context_manager.h"
#include "mediapipe/framework/calculator_state.h"
#include "mediapipe/framework/graph_runtime_info.pb.h"
#include "mediapipe/framework/graph_service_manager.h"
#include "mediapipe/framework/input_side_packet_handler.h"
#include "mediapipe/framework/input_stream_handler.h"
#include "mediapipe/framework/legacy_calculator_support.h"
#include "mediapipe/framework/output_side_packet_impl.h"
#include "mediapipe/framework/output_stream_handler.h"
#include "mediapipe/framework/packet.h"
#include "mediapipe/framework/packet_set.h"
#include "mediapipe/framework/packet_type.h"
#include "mediapipe/framework/port.h"
#include "mediapipe/framework/port/status.h"
#include "mediapipe/framework/stream_handler.pb.h"
#include "mediapipe/framework/timestamp.h"
#include "mediapipe/framework/tool/validate_name.h"
#include "mediapipe/framework/validated_graph_config.h"

namespace mediapipe {

class CounterFactory;
class InputStreamManager;
class OutputStreamManager;

namespace internal {
class SchedulerQueue;
}  // namespace internal

/**
 * @brief Represents a node in the calculator graph.
 *
 * This class is internally used by the Calculator framework to perform the
 * computations.
 */
class CalculatorNode {
 public:
  // Handy typedef for a map from the name of an output stream to the set of ids
  // of upstream sources that affect it.
  typedef std::unordered_map<std::string, std::unordered_set<int>>
      OutputStreamToSourcesMap;

  CalculatorNode();
  CalculatorNode(const CalculatorNode&) = delete;
  CalculatorNode& operator=(const CalculatorNode&) = delete;

  /**
   * @brief Returns the ID of the node.
   * @return The ID of the node.
   */
  int Id() const {
    return node_type_info_ ? node_type_info_->Node().index : -1;
  }

  /**
   * @brief Returns a value according to which the scheduler queue determines
   * the relative priority between runnable source nodes.
   * @param cc The calculator context.
   * @return The timestamp to process next.
   */
  Timestamp SourceProcessOrder(const CalculatorContext* cc) const;

  /**
   * @brief Retrieves a string name for the node.
   * @return A string name for the node.
   */
  std::string DebugName() const;

  /**
   * @brief Name of the executor which the node will execute on.
   * @return The name of the executor.
   */
  const std::string& Executor() const { return executor_; }

  /**
   * @brief Changes the executor a node is assigned to.
   * @param executor The name of the executor.
   */
  void SetExecutor(const std::string& executor);

  /**
   * @brief Calls Process() on the Calculator corresponding to this node.
   * @param calculator_context The calculator context.
   * @return absl::Status OkStatus() on success, any other status on failure.
   */
  absl::Status ProcessNode(CalculatorContext* calculator_context);

  /**
   * @brief Initializes the node.
   * @param validated_graph The validated graph config.
   * @param node_ref A reference to the node in the validated graph.
   * @param input_stream_managers A pointer to the input stream managers.
   * @param output_stream_managers A pointer to the output stream managers.
   * @param output_side_packets A pointer to the output side packets.
   * @param buffer_size_hint A hint for the buffer size.
   * @param profiling_context The profiling context.
   * @param graph_service_manager The graph service manager.
   * @return absl::Status OkStatus() on success, any other status on failure.
   */
  absl::Status Initialize(const ValidatedGraphConfig* validated_graph,
                          NodeTypeInfo::NodeRef node_ref,
                          InputStreamManager* input_stream_managers,
                          OutputStreamManager* output_stream_managers,
                          OutputSidePacketImpl* output_side_packets,
                          int* buffer_size_hint,
                          std::shared_ptr<ProfilingContext> profiling_context,
                          const GraphServiceManager* graph_service_manager);

  /**
   * @brief Sets up the node at the beginning of CalculatorGraph::Run().
   * @param all_side_packets The side packets.
   * @param service_packets The service packets.
   * @param ready_for_open_callback The callback to call when the node is ready
   * to be opened.
   * @param source_node_opened_callback The callback to call when a source node
   * is opened.
   * @param schedule_callback The callback to call to schedule the node.
   * @param error_callback The callback to call on error.
   * @param counter_factory The counter factory.
   * @return absl::Status OkStatus() on success, any other status on failure.
   */
  absl::Status PrepareForRun(
      const std::map<std::string, Packet>& all_side_packets,
      const std::map<std::string, Packet>& service_packets,
      std::function<void()> ready_for_open_callback,
      std::function<void()> source_node_opened_callback,
      std::function<void(CalculatorContext*)> schedule_callback,
      std::function<void(absl::Status)> error_callback,
      CounterFactory* counter_factory) ABSL_LOCKS_EXCLUDED(status_mutex_);

  /**
   * @brief Opens the node.
   * @return absl::Status OkStatus() on success, any other status on failure.
   */
  absl::Status OpenNode() ABSL_LOCKS_EXCLUDED(status_mutex_);

  /**
   * @brief Called when a source node's layer becomes active.
   */
  void ActivateNode() ABSL_LOCKS_EXCLUDED(status_mutex_);

  /**
   * @brief Cleans up the node after the CalculatorGraph has been run.
   * @param graph_status The status of the graph run.
   */
  void CleanupAfterRun(const absl::Status& graph_status)
      ABSL_LOCKS_EXCLUDED(status_mutex_);

  /**
   * @brief Returns true iff PrepareForRun() has been called.
   * @return True iff PrepareForRun() has been called.
   */
  bool Prepared() const ABSL_LOCKS_EXCLUDED(status_mutex_);

  /**
   * @brief Returns true iff Open() has been called on the calculator.
   * @return True iff Open() has been called on the calculator.
   */
  bool Opened() const ABSL_LOCKS_EXCLUDED(status_mutex_);

  /**
   * @brief Returns true iff a source calculator's layer is active.
   * @return True iff a source calculator's layer is active.
   */
  bool Active() const ABSL_LOCKS_EXCLUDED(status_mutex_);

  /**
   * @brief Returns true iff Close() has been called on the calculator.
   * @return True iff Close() has been called on the calculator.
   */
  bool Closed() const ABSL_LOCKS_EXCLUDED(status_mutex_);

  /**
   * @brief Returns true iff this is a source node.
   * @return True iff this is a source node.
   */
  bool IsSource() const {
    return input_stream_handler_->NumInputStreams() == 0 &&
           output_stream_handler_->NumOutputStreams() != 0;
  }

  int source_layer() const { return source_layer_; }

  /**
   * @brief Checks if the node can be scheduled.
   * @return True if the node can be scheduled.
   */
  bool TryToBeginScheduling() ABSL_LOCKS_EXCLUDED(status_mutex_);

  /**
   * @brief Ends scheduling for the current invocation.
   */
  void EndScheduling() ABSL_LOCKS_EXCLUDED(status_mutex_);

  /**
   * @brief Returns true if OpenNode() can be scheduled.
   * @return True if OpenNode() can be scheduled.
   */
  bool ReadyForOpen() const ABSL_LOCKS_EXCLUDED(status_mutex_);

  /**
   * @brief Called by the InputStreamHandler when all the input stream headers
   * become available.
   */
  void InputStreamHeadersReady() ABSL_LOCKS_EXCLUDED(status_mutex_);

  /**
   * @brief Called by the InputSidePacketHandler when all the input side
   * packets become available.
   */
  void InputSidePacketsReady() ABSL_LOCKS_EXCLUDED(status_mutex_);

  /**
   * @brief Checks scheduling_state_, and then invokes SchedulingLoop() if
   * necessary.
   */
  void CheckIfBecameReady() ABSL_LOCKS_EXCLUDED(status_mutex_);

  /**
   * @brief Called by SchedulerQueue when a node is opened.
   */
  void NodeOpened() ABSL_LOCKS_EXCLUDED(status_mutex_);

  /**
   * @brief Returns the scheduler queue the node is assigned to.
   * @return The scheduler queue the node is assigned to.
   */
  internal::SchedulerQueue* GetSchedulerQueue() const {
    return scheduler_queue_;
  }

  /**
   * @brief Sets the scheduler queue the node is assigned to.
   * @param queue The scheduler queue.
   */
  void SetSchedulerQueue(internal::SchedulerQueue* queue) {
    scheduler_queue_ = queue;
  }

  /**
   * @brief Sets callbacks in the scheduler that should be invoked when an input
   * queue becomes full/non-full.
   * @param becomes_full_callback The callback to call when the queue becomes
   * full.
   * @param becomes_not_full_callback The callback to call when the queue
   * becomes not full.
   */
  void SetQueueSizeCallbacks(
      InputStreamManager::QueueSizeCallback becomes_full_callback,
      InputStreamManager::QueueSizeCallback becomes_not_full_callback);

  /**
   * @brief Sets each of this node's input streams to use the specified
   * max_queue_size to trigger callbacks.
   * @param max_queue_size The maximum queue size.
   */
  void SetMaxInputStreamQueueSize(int max_queue_size);

  /**
   * @brief Closes the node's calculator and input and output streams.
   * @param graph_status The status of the graph run.
   * @param graph_run_ended True if the graph run has ended.
   * @return absl::Status OkStatus() on success, any other status on failure.
   */
  absl::Status CloseNode(const absl::Status& graph_status, bool graph_run_ended)
      ABSL_LOCKS_EXCLUDED(status_mutex_);

  /**
   * @brief Returns a pointer to the default calculator context.
   * @return A pointer to the default calculator context.
   */
  CalculatorContext* GetDefaultCalculatorContext() const {
    return calculator_context_manager_.GetDefaultCalculatorContext();
  }

  const CalculatorState& GetCalculatorState() const {
    return *calculator_state_;
  }

  /**
   * @brief Returns the node's contract.
   * @return The node's contract.
   */
  const CalculatorContract& Contract() const {
    return node_type_info_->Contract();
  }

  /**
   * @brief Returns the last timestamp bound used to schedule this node.
   * @return The last timestamp bound used to schedule this node.
   */
  Timestamp GetLastTimestampBound() const { return last_timestamp_bound_; }

  /**
   * @brief Returns the stream monitoring info for this node.
   * @return The stream monitoring info for this node.
   */
  CalculatorRuntimeInfo GetStreamMonitoringInfo() const;

 private:
  // Sets up the output side packets from the main flat array.
  absl::Status InitializeOutputSidePackets(
      const PacketTypeSet& output_side_packet_types,
      OutputSidePacketImpl* output_side_packets);
  // Connects the input side packets as mirrors on the output side packets.
  // Output side packets are looked up in the main flat array which is
  // provided.
  absl::Status InitializeInputSidePackets(
      OutputSidePacketImpl* output_side_packets);
  // Sets up the output streams from the main flat array.
  absl::Status InitializeOutputStreams(
      OutputStreamManager* output_stream_managers);
  // Sets up the input streams and connects them as mirrors on the
  // output streams.  Both input streams and output streams are looked
  // up in the main flat arrays which are provided.
  absl::Status InitializeInputStreams(
      InputStreamManager* input_stream_managers,
      OutputStreamManager* output_stream_managers);

  absl::Status InitializeInputStreamHandler(
      const InputStreamHandlerConfig& handler_config,
      const PacketTypeSet& input_stream_types);
  absl::Status InitializeOutputStreamHandler(
      const OutputStreamHandlerConfig& handler_config,
      const PacketTypeSet& output_stream_types);

  // Connects the input/output stream shards in the given calculator context to
  // the input/output streams of the node.
  absl::Status ConnectShardsToStreams(CalculatorContext* calculator_context);

  // The general scheduling logic shared by EndScheduling() and
  // CheckIfBecameReady().
  // Inside the function, a while loop keeps preparing CalculatorContexts and
  // scheduling the node until 1) the node becomes not ready or 2) the max
  // number of in flight invocations is reached. It also attempts to propagate
  // the latest input timestamp bound if no invocations can be scheduled.
  void SchedulingLoop();

  // Closes the input and output streams.
  void CloseInputStreams() ABSL_LOCKS_EXCLUDED(status_mutex_);
  void CloseOutputStreams(OutputStreamShardSet* outputs)
      ABSL_LOCKS_EXCLUDED(status_mutex_);
  // Get a string describing the input streams.
  std::string DebugInputStreamNames() const;

  // Returns true if all outputs will be identical to the previous graph run.
  bool OutputsAreConstant(CalculatorContext* cc);

  // The calculator.
  std::unique_ptr<CalculatorBase> calculator_;
  // Keeps data which a Calculator subclass needs access to.
  std::unique_ptr<CalculatorState> calculator_state_;

  std::string name_;  // Optional user-defined name
  // Name of the executor which the node will execute on. If empty, the node
  // will execute on the default executor.
  std::string executor_;
  // The layer a source calculator operates on.
  int source_layer_ = 0;
  // The status of the current Calculator that this CalculatorNode
  // is wrapping.  kStateActive is currently used only for source nodes.
  enum NodeStatus {
    kStateUninitialized = 0,
    kStatePrepared = 1,
    kStateOpened = 2,
    kStateActive = 3,
    kStateClosed = 4
  };
  NodeStatus status_ ABSL_GUARDED_BY(status_mutex_){kStateUninitialized};

  // The max number of invocations that can be scheduled in parallel.
  int max_in_flight_ = 1;
  // The following two variables are used for the concurrency control of node
  // scheduling.
  //
  // The number of invocations that are scheduled but not finished.
  int current_in_flight_ ABSL_GUARDED_BY(status_mutex_) = 0;
  // SchedulingState incidates the current state of the node scheduling process.
  // There are four possible transitions:
  // (a) From kIdle to kScheduling.
  // Any thread that makes this transition becomes the scheduling thread and
  // will be responsible for preparing and scheduling all possible invocations.
  // (b) From kScheduling to kSchedulingPending.
  // Any thread, except the scheduling thread, can make this transition.
  // kSchedulingPending indicates that some recent changes require the
  // scheduling thread to recheck the node readiness after current scheduling
  // iteration.
  // (c) From kSchedulingPending to kScheduling.
  // Made by the scheduling thread to indicate that it has already caught up
  // with all the recent changes that can affect node readiness.
  // (d) From kScheduling to kIdle. Made by the scheduling thread when there is
  // no more scheduling work to be done.
  enum SchedulingState {
    kIdle = 0,        //
    kScheduling = 1,  //
    kSchedulingPending = 2
  };
  SchedulingState scheduling_state_ ABSL_GUARDED_BY(status_mutex_) = kIdle;

  std::function<void()> ready_for_open_callback_;
  std::function<void()> source_node_opened_callback_;
  bool input_stream_headers_ready_called_ ABSL_GUARDED_BY(status_mutex_) =
      false;
  bool input_side_packets_ready_called_ ABSL_GUARDED_BY(status_mutex_) = false;
  bool input_stream_headers_ready_ ABSL_GUARDED_BY(status_mutex_) = false;
  bool input_side_packets_ready_ ABSL_GUARDED_BY(status_mutex_) = false;

  // Owns and manages all CalculatorContext objects.
  CalculatorContextManager calculator_context_manager_;

  std::shared_ptr<ProfilingContext> profiling_context_;

  // Mutex for node status.
  mutable absl::Mutex status_mutex_;

  // Describes the input side packets required to run this node.
  std::unique_ptr<PacketTypeSet> input_side_packet_types_;

  // Manages the set of input side packets.
  InputSidePacketHandler input_side_packet_handler_;

  // Collection of all OutputSidePacket objects.
  std::unique_ptr<OutputSidePacketSet> output_side_packets_;

  std::unique_ptr<InputStreamHandler> input_stream_handler_;

  std::unique_ptr<OutputStreamHandler> output_stream_handler_;

  // True if CleanupAfterRun() needs to call CloseNode().
  bool needs_to_close_ = false;

  internal::SchedulerQueue* scheduler_queue_ = nullptr;

  const ValidatedGraphConfig* validated_graph_ = nullptr;

  const NodeTypeInfo* node_type_info_ = nullptr;

  // Keeps track of the latest timestamp bound used to schedule this node.
  Timestamp last_timestamp_bound_ = Timestamp::Unset();

  // Keeps track of the runtime info for this node.
  mutable absl::Mutex runtime_info_mutex_;
  absl::Time last_process_start_ts_ ABSL_GUARDED_BY(runtime_info_mutex_) =
      absl::InfinitePast();
  absl::Time last_process_finish_ts_ ABSL_GUARDED_BY(runtime_info_mutex_) =
      absl::InfinitePast();
};

}  // namespace mediapipe

#endif  // MEDIAPIPE_FRAMEWORK_CALCULATOR_NODE_H_
