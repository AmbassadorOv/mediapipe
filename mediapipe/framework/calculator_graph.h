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
// Declares CalculatorGraph, which links Calculators into a directed acyclic
// graph, and allows its evaluation.

#ifndef MEDIAPIPE_FRAMEWORK_CALCULATOR_GRAPH_H_
#define MEDIAPIPE_FRAMEWORK_CALCULATOR_GRAPH_H_

#include <atomic>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/base/attributes.h"
#include "absl/base/thread_annotations.h"
#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "absl/synchronization/mutex.h"
#include "mediapipe/framework/calculator.pb.h"
#include "mediapipe/framework/calculator_base.h"
#include "mediapipe/framework/calculator_node.h"
#include "mediapipe/framework/counter_factory.h"
#include "mediapipe/framework/executor.h"
#include "mediapipe/framework/graph_output_stream.h"
#include "mediapipe/framework/graph_runtime_info.pb.h"
#include "mediapipe/framework/graph_service.h"
#include "mediapipe/framework/graph_service_manager.h"
#include "mediapipe/framework/mediapipe_profiling.h"
#include "mediapipe/framework/output_side_packet_impl.h"
#include "mediapipe/framework/output_stream_manager.h"
#include "mediapipe/framework/output_stream_poller.h"
#include "mediapipe/framework/output_stream_shard.h"
#include "mediapipe/framework/packet.h"
#include "mediapipe/framework/packet_generator_graph.h"
#include "mediapipe/framework/resources_service.h"
#include "mediapipe/framework/scheduler.h"
#include "mediapipe/framework/scheduler_shared.h"
#include "mediapipe/framework/subgraph.h"
#include "mediapipe/framework/thread_pool_executor.pb.h"
#include "mediapipe/framework/timestamp.h"
#include "mediapipe/framework/validated_graph_config.h"

#if !defined(__EMSCRIPTEN__)
#include "mediapipe/framework/tool/graph_runtime_info_logger.h"
#endif  // !defined(__EMSCRIPTEN__)

namespace mediapipe {

#if !MEDIAPIPE_DISABLE_GPU
class GpuResources;
struct GpuSharedData;
#endif  // !MEDIAPIPE_DISABLE_GPU

typedef absl::StatusOr<OutputStreamPoller> StatusOrPoller;

/**
 * @brief The class representing a DAG of calculator nodes.
 *
 * CalculatorGraph is the primary API for the MediaPipe Framework.
 * In general, CalculatorGraph should be used if the only thing you need
 * to do is run the graph (without pushing data in or extracting it as
 * the graph runs).
 */
class CalculatorGraph {
 public:
  /**
   * @brief Defines possible modes for adding a packet to a graph input stream.
   */
  enum class GraphInputStreamAddMode {
    /**
     * @brief Blocks and waits until none of the affected streams are full.
     *
     * Note that if max_queue_size is set to -1, the packet will be added
     * regardless of queue size.
     */
    WAIT_TILL_NOT_FULL,
    /**
     * @brief Returns and does not add packet if any affected input stream is
     * full.
     */
    ADD_IF_NOT_FULL
  };

  /**
   * @brief Creates an uninitialized graph.
   */
  CalculatorGraph();
  CalculatorGraph(const CalculatorGraph&) = delete;
  CalculatorGraph& operator=(const CalculatorGraph&) = delete;

  /**
   * @brief Initializes the graph from its proto description.
   * @param config The graph config.
   */
  explicit CalculatorGraph(CalculatorGraphConfig config);

  /**
   * @brief Initializes the graph with shared GraphServices from an existing MP
   * graph environment.
   *
   * It enables to run a CalculatorGraph within a Calculator.
   * @param cc The calculator context.
   */
  explicit CalculatorGraph(CalculatorContext* cc);

  virtual ~CalculatorGraph();

  /**
   * @brief Initializes the graph from a its proto description.
   * @param config The graph config.
   * @param side_packets The side packets.
   * @return absl::Status OkStatus() on success, any other status on failure.
   */
  absl::Status Initialize(CalculatorGraphConfig config,
                          const std::map<std::string, Packet>& side_packets);

  /**
   * @brief Convenience version which does not take side packets.
   * @param config The graph config.
   * @return absl::Status OkStatus() on success, any other status on failure.
   */
  absl::Status Initialize(CalculatorGraphConfig config);

  /**
   * @brief Initializes the CalculatorGraph from the specified graph and
   * subgraph configs.
   * @param configs The graph configs.
   * @param templates The subgraph templates.
   * @param side_packets The side packets.
   * @param graph_type The graph type.
   * @param options The subgraph options.
   * @return absl::Status OkStatus() on success, any other status on failure.
   */
  absl::Status Initialize(
      const std::vector<CalculatorGraphConfig>& configs,
      const std::vector<CalculatorGraphTemplate>& templates,
      const std::map<std::string, Packet>& side_packets = {},
      const std::string& graph_type = "",
      const Subgraph::SubgraphOptions* options = nullptr);

  /**
   * @brief Returns the canonicalized CalculatorGraphConfig for this graph.
   * @return The canonicalized CalculatorGraphConfig for this graph.
   */
  const CalculatorGraphConfig& Config() const {
    return validated_graph_->Config();
  }

  /**
   * @brief Observes the named output stream.
   *
   * packet_callback will be invoked on every packet emitted by the output
   * stream. Can only be called before Run() or StartRun().
   * @param stream_name The name of the output stream.
   * @param packet_callback The callback function.
   * @param observe_timestamp_bounds True to observe timestamp bounds.
   * @return absl::Status OkStatus() on success, any other status on failure.
   */
  absl::Status ObserveOutputStream(
      const std::string& stream_name,
      std::function<absl::Status(const Packet&)> packet_callback,
      bool observe_timestamp_bounds = false);

  /**
   * @brief Adds an OutputStreamPoller for a stream.
   *
   * This provides a synchronous, polling API for accessing a stream's output.
   * Should only be called before Run() or StartRun().
   * @param stream_name The name of the output stream.
   * @param observe_timestamp_bounds True to observe timestamp bounds.
   * @return A StatusOrPoller object.
   */
  StatusOrPoller AddOutputStreamPoller(const std::string& stream_name,
                                       bool observe_timestamp_bounds = false);

  /**
   * @brief Gets output side packet by name.
   *
   * The output side packet can be successfully retrevied in one of the
   * following situations:
   *   - The graph is done.
   *   - The output side packet has been generated by a calculator and the graph
   *     is currently idle.
   *   - The side packet is a base packet generated by a PacketGenerator.
   * @param packet_name The name of the output side packet.
   * @return A StatusOr<Packet> object.
   */
  absl::StatusOr<Packet> GetOutputSidePacket(const std::string& packet_name);

  /**
   * @brief Runs the graph after adding the given extra input side packets.
   *
   * All arguments are forgotten after Run() returns. Run() is a blocking call
   * and will return when all calculators are done.
   * @param extra_side_packets The extra side packets.
   * @return absl::Status OkStatus() on success, any other status on failure.
   */
  virtual absl::Status Run(
      const std::map<std::string, Packet>& extra_side_packets);

  /**
   * @brief Run the graph without adding any input side packets.
   * @return absl::Status OkStatus() on success, any other status on failure.
   */
  absl::Status Run() { return Run({}); }

  /**
   * @brief Start a run of the graph.
   * @param extra_side_packets The extra side packets.
   * @return absl::Status OkStatus() on success, any other status on failure.
   */
  absl::Status StartRun(
      const std::map<std::string, Packet>& extra_side_packets) {
    return StartRun(extra_side_packets, {});
  }

  /**
   * @brief Start a run of the graph with stream headers.
   * @param extra_side_packets The extra side packets.
   * @param stream_headers The stream headers.
   * @return absl::Status OkStatus() on success, any other status on failure.
   */
  absl::Status StartRun(const std::map<std::string, Packet>& extra_side_packets,
                        const std::map<std::string, Packet>& stream_headers);

  /**
   * @brief Wait for the current run to finish.
   * @return absl::Status OkStatus() on success, any other status on failure.
   */
  absl::Status WaitUntilDone();

  /**
   * @brief Wait until the running graph is in the idle mode.
   * @return absl::Status OkStatus() on success, any other status on failure.
   */
  absl::Status WaitUntilIdle();

  /**
   * @brief Wait until a packet is emitted on one of the observed output
   * streams.
   * @return absl::Status OkStatus() on success, any other status on failure.
   */
  absl::Status WaitForObservedOutput();

  /**
   * @brief Quick non-locking means of checking if the graph has encountered an
   * error.
   * @return True if the graph has encountered an error.
   */
  bool HasError() const { return has_error_; }

  /**
   * @brief Returns debugging information about the graph transient state.
   * @return A StatusOr<GraphRuntimeInfo> object.
   */
  absl::StatusOr<GraphRuntimeInfo> GetGraphRuntimeInfo();

  /**
   * @brief Add a Packet to a graph input stream.
   * @param stream_name The name of the input stream.
   * @param packet The packet to add.
   * @return absl::Status OkStatus() on success, any other status on failure.
   */
  absl::Status AddPacketToInputStream(absl::string_view stream_name,
                                      const Packet& packet);

  /**
   * @brief Add a Packet to a graph input stream.
   * @param stream_name The name of the input stream.
   * @param packet The packet to add.
   * @return absl::Status OkStatus() on success, any other status on failure.
   */
  absl::Status AddPacketToInputStream(absl::string_view stream_name,
                                      Packet&& packet);

  /**
   * @brief Indicates that input will arrive no earlier than a certain
   * timestamp.
   * @param stream_name The name of the input stream.
   * @param timestamp The timestamp.
   * @return absl::Status OkStatus() on success, any other status on failure.
   */
  absl::Status SetInputStreamTimestampBound(const std::string& stream_name,
                                            Timestamp timestamp);

  /**
   * @brief Sets the queue size of a graph input stream.
   * @param stream_name The name of the input stream.
   * @param max_queue_size The maximum queue size.
   * @return absl::Status OkStatus() on success, any other status on failure.
   */
  absl::Status SetInputStreamMaxQueueSize(const std::string& stream_name,
                                          int max_queue_size);

  /**
   * @brief Check if an input stream exists in the graph.
   * @param name The name of the input stream.
   * @return True if the input stream exists.
   */
  bool HasInputStream(const std::string& name);

  /**
   * @brief Close a graph input stream.
   * @param stream_name The name of the input stream.
   * @return absl::Status OkStatus() on success, any other status on failure.
   */
  absl::Status CloseInputStream(const std::string& stream_name);

  /**
   * @brief Closes all the graph input streams.
   * @return absl::Status OkStatus() on success, any other status on failure.
   */
  absl::Status CloseAllInputStreams();

  /**
   * @brief Closes all the graph input streams and source calculator nodes.
   * @return absl::Status OkStatus() on success, any other status on failure.
   */
  absl::Status CloseAllPacketSources();

  /**
   * @brief Returns the pointer to the stream with the given name.
   * @param name The name of the output stream.
   * @return The pointer to the stream with the given name, or dies if none
   * exists.
   */
  ABSL_DEPRECATED(
      "Prefer using a Calculator to get information of all sorts out of the "
      "graph.")
  const OutputStreamManager* FindOutputStreamManager(const std::string& name);

  /**
   * @brief Returns the ProfilingContext assocoaited with the CalculatorGraph.
   * @return The ProfilingContext assocoaited with the CalculatorGraph.
   */
  ProfilingContext* profiler() { return profiler_.get(); }
  /**
   * @brief Collects the runtime profile for Open(), Process(), and Close() of
   * each calculator in the graph.
   * @param profiles A vector to store the profiles.
   * @return absl::Status OkStatus() on success, any other status on failure.
   */
  ABSL_DEPRECATED("Use profiler()->GetCalculatorProfiles() instead")
  absl::Status GetCalculatorProfiles(std::vector<CalculatorProfile>*) const;

  /**
   * @brief Set the type of counter used in this graph.
   * @param factory The counter factory.
   */
  void SetCounterFactory(CounterFactory* factory) {
    counter_factory_.reset(factory);
  }

  /**
   * @brief Get the type of counter used in this graph.
   * @return The counter factory.
   */
  CounterFactory* GetCounterFactory() { return counter_factory_.get(); }

  /**
   * @brief Sets the error callback to receive graph execution errors.
   * @param error_callback The error callback.
   * @return absl::Status OkStatus() on success, any other status on failure.
   */
  absl::Status SetErrorCallback(
      std::function<void(const absl::Status&)> error_callback);

  /**
   * @brief Callback when an error is encountered.
   * @param error The error.
   */
  void RecordError(const absl::Status& error) ABSL_LOCKS_EXCLUDED(error_mutex_);

  /**
   * @brief Combines errors into a status.
   * @param error_prefix The error prefix.
   * @param error_status The status to store the combined errors.
   * @return True if the vector of errors is non-empty.
   */
  bool GetCombinedErrors(const std::string& error_prefix,
                         absl::Status* error_status);
  /**
   * @brief Convenience overload which specifies a default error prefix.
   * @param error_status The status to store the combined errors.
   * @return True if the vector of errors is non-empty.
   */
  bool GetCombinedErrors(absl::Status* error_status);

  /**
   * @brief Returns the maximum input stream queue size.
   * @return The maximum input stream queue size.
   */
  int GetMaxInputStreamQueueSize();

  /**
   * @brief Get the mode for adding packets to an input stream.
   * @return The mode for adding packets to an input stream.
   */
  GraphInputStreamAddMode GetGraphInputStreamAddMode() const;

  /**
   * @brief Set the mode for adding packets to an input stream.
   * @param mode The mode for adding packets to an input stream.
   */
  void SetGraphInputStreamAddMode(GraphInputStreamAddMode mode);

  /**
   * @brief Aborts the scheduler if the graph is not terminated; no-op
   * otherwise.
   */
  void Cancel();

  /**
   * @brief Pauses the scheduler.
   */
  ABSL_DEPRECATED(
      "CalculatorGraph will not allow external callers to explictly pause and "
      "resume a graph.")
  void Pause();

  /**
   * @brief Resumes the scheduler.
   */
  ABSL_DEPRECATED(
      "CalculatorGraph will not allow external callers to explictly pause and "
      "resume a graph.")
  void Resume();

  /**
   * @brief Sets the executor that will run the nodes assigned to the executor
   * named |name|.
   * @param name The name of the executor.
   * @param executor The executor.
   * @return absl::Status OkStatus() on success, any other status on failure.
   */
  absl::Status SetExecutor(const std::string& name,
                           std::shared_ptr<Executor> executor);

  // WARNING: the following public methods are exposed to Scheduler only.

  /**
   * @brief Return true if all the graph input streams have been closed.
   * @return True if all the graph input streams have been closed.
   */
  bool GraphInputStreamsClosed() {
    return num_closed_graph_input_streams_ == graph_input_streams_.size();
  }

  /**
   * @brief Returns true if this node or graph input stream is connected to
   * any input stream whose queue has hit maximum capacity.
   * @param node_id The ID of the node.
   * @return True if this node or graph input stream is connected to
   * any input stream whose queue has hit maximum capacity.
   */
  bool IsNodeThrottled(int node_id)
      ABSL_LOCKS_EXCLUDED(full_input_streams_mutex_);

  /**
   * @brief If any active source node or graph input stream is throttled and not
   * yet closed, increases the max_queue_size for each full input stream in the
   * graph.
   * @return True if at least one max_queue_size has been grown.
   */
  bool UnthrottleSources() ABSL_LOCKS_EXCLUDED(full_input_streams_mutex_);

  /**
   * @brief Returns the scheduler's runtime measures for overhead measurement.
   * @return The scheduler's runtime measures for overhead measurement.
   */
  internal::SchedulerTimes GetSchedulerTimes() {
    return scheduler_.GetSchedulerTimes();
  }

#if !MEDIAPIPE_DISABLE_GPU
  /**
   * @brief Returns a pointer to the GpuResources in use, if any.
   * @return A pointer to the GpuResources in use, if any.
   */
  std::shared_ptr<GpuResources> GetGpuResources() const;

  /**
   * @brief Sets the GpuResources.
   * @param resources The GpuResources.
   * @return absl::Status OkStatus() on success, any other status on failure.
   */
  absl::Status SetGpuResources(std::shared_ptr<GpuResources> resources);
#endif  // !MEDIAPIPE_DISABLE_GPU

  /**
   * @brief Sets a service object.
   * @tparam T The type of the service object.
   * @param service The service.
   * @param object The service object.
   * @return absl::Status OkStatus() on success, any other status on failure.
   */
  template <typename T>
  absl::Status SetServiceObject(const GraphService<T>& service,
                                std::shared_ptr<T> object) {
    if (initialized_) {
      // TODO: check that the graph has not been initialized for
      // all services!
      if (service.key == kResourcesService.key) {
        return absl::InternalError(
            "Service objects must be set before graph is initialized.");
      }
    }

    return service_manager_.SetServiceObject(service, object);
  }

  /**
   * @brief Gets a service object.
   * @tparam T The type of the service object.
   * @param service The service.
   * @return The service object.
   */
  template <typename T>
  std::shared_ptr<T> GetServiceObject(const GraphService<T>& service) {
    return service_manager_.GetServiceObject(service);
  }

  /**
   * @brief Disallows/disables default initialization of MediaPipe graph
   * services.
   * @return absl::Status OkStatus() on success, any other status on failure.
   */
  absl::Status DisallowServiceDefaultInitialization() {
    allow_service_default_initialization_ = false;
    return absl::OkStatus();
  }

  /**
   * @brief Sets a service object.
   * @param service The service.
   * @param p The service packet.
   * @return absl::Status OkStatus() on success, any other status on failure.
   */
  absl::Status SetServicePacket(const GraphServiceBase& service, Packet p) {
    // TODO: check that the graph has not been started!
    return service_manager_.SetServicePacket(service, p);
  }

 private:
  // GraphRunState is used as a parameter in the function CallStatusHandlers.
  enum class GraphRunState {
    // State of the graph before the run; see status_handler.h for details.
    PRE_RUN,
    // State of the graph after after the run; set by CleanUpAfterRun.
    POST_RUN,
  };

  // The graph input streams (which have packets added to them from
  // outside the graph).  Since these will be connected directly to a
  // node's input streams they are implemented as "output" streams.
  // Based on the assumption that all the graph input packets must be added to a
  // graph input stream sequentially, a GraphInputStream object only contains
  // one reusable output stream shard.
  class GraphInputStream {
   public:
    explicit GraphInputStream(OutputStreamManager* manager)
        : manager_(manager) {
      shard_.SetSpec(manager_->Spec());
    }

    void PrepareForRun(std::function<void(absl::Status)> error_callback) {
      manager_->PrepareForRun(std::move(error_callback));
    }

    void SetMaxQueueSize(int max_queue_size) {
      manager_->SetMaxQueueSize(max_queue_size);
    }

    void SetHeader(const Packet& header);

    void AddPacket(const Packet& packet) { shard_.AddPacket(packet); }

    void AddPacket(Packet&& packet) { shard_.AddPacket(std::move(packet)); }

    void SetNextTimestampBound(Timestamp timestamp);

    void PropagateUpdatesToMirrors();

    void Close();

    bool IsClosed() const { return manager_->IsClosed(); }

    OutputStreamManager* GetManager() { return manager_; }

   private:
    OutputStreamManager* manager_ = nullptr;
    OutputStreamShard shard_;
  };

  // Initializes the graph from a ValidatedGraphConfig object.
  absl::Status Initialize(std::unique_ptr<ValidatedGraphConfig> validated_graph,
                          const std::map<std::string, Packet>& side_packets);

  // AddPacketToInputStreamInternal template is called by either
  // AddPacketToInputStream(Packet&& packet) or
  // AddPacketToInputStream(const Packet& packet).
  template <typename T>
  absl::Status AddPacketToInputStreamInternal(absl::string_view stream_name,
                                              T&& packet);

  // Sets the executor that will run the nodes assigned to the executor
  // named |name|.  If |name| is empty, this sets the default executor.
  // Does not check that the graph is uninitialized and |name| is not a
  // reserved executor name.
  absl::Status SetExecutorInternal(const std::string& name,
                                   std::shared_ptr<Executor> executor);

  // If the num_threads field in default_executor_options is not specified,
  // assigns a reasonable value based on system configuration and the graph.
  // Then, creates the default thread pool if appropriate.
  //
  // Only called by InitializeExecutors().
  absl::Status InitializeDefaultExecutor(
      const ThreadPoolExecutorOptions* default_executor_options,
      bool use_application_thread);

  // Creates a thread pool as the default executor. The num_threads argument
  // overrides the num_threads field in default_executor_options.
  absl::Status CreateDefaultThreadPool(
      const ThreadPoolExecutorOptions* default_executor_options,
      int num_threads);

  // Returns true if |name| is a reserved executor name.
  static bool IsReservedExecutorName(const std::string& name);

  // Helper functions for Initialize().
  absl::Status InitializeExecutors();
  absl::Status InitializePacketGeneratorGraph(
      const std::map<std::string, Packet>& side_packets);
  absl::Status InitializeStreams();
  absl::Status InitializeProfiler();
  absl::Status InitializeCalculatorNodes();
  absl::Status InitializePacketGeneratorNodes(
      const std::vector<int>& non_scheduled_generators);

  // Iterates through all nodes and schedules any that can be opened.
  void ScheduleAllOpenableNodes();

  // Does the bulk of the work for StartRun but does not start the scheduler.
  absl::Status PrepareForRun(
      const std::map<std::string, Packet>& extra_side_packets,
      const std::map<std::string, Packet>& stream_headers);

  absl::Status PrepareServices();

#if !MEDIAPIPE_DISABLE_GPU
  absl::Status MaybeSetUpGpuServiceFromLegacySidePacket(Packet legacy_sp);
  // Helper for PrepareForRun. If it returns a non-empty map, those packets
  // must be added to the existing side packets, replacing existing values
  // that have the same key.
  std::map<std::string, Packet> MaybeCreateLegacyGpuSidePacket(
      Packet legacy_sp);
  absl::Status PrepareGpu();
#endif  // !MEDIAPIPE_DISABLE_GPU

  // Cleans up any remaining state after the run and returns any errors that may
  // have occurred during the run. Called after the scheduler has terminated.
  absl::Status FinishRun();

  // Cleans up any remaining state after the run. All status handlers run here
  // if their requested input side packets exist.
  // The original |*status| is passed to all the status handlers. If any status
  // handler fails, it appends its error to errors_, and CleanupAfterRun sets
  // |*status| to the new combined errors on return.
  void CleanupAfterRun(absl::Status* status) ABSL_LOCKS_EXCLUDED(error_mutex_);

  // Calls HandlePreRunStatus or HandleStatus on the StatusHandlers. Which one
  // is called depends on the GraphRunState parameter (PRE_RUN or POST_RUN).
  // current_run_side_packets_ must be set before this function is called.
  // On error, has_error_ will be set.
  void CallStatusHandlers(GraphRunState graph_run_state,
                          const absl::Status& status);

  // Callback function to throttle or unthrottle source nodes when a stream
  // becomes full or non-full. A node is throttled (i.e. prevented being
  // scheduled) if it has caused a downstream input queue to become full. Note
  // that all sources (including graph input streams) that affect this stream
  // will be throttled. A node is unthrottled (i.e. added to the scheduler
  // queue) if all downstream input queues have become non-full.
  //
  // This method is invoked from an input stream when its queue becomes full or
  // non-full. However, since streams are not allowed to hold any locks while
  // invoking a callback, this method must re-lock the stream and query its
  // status before taking any action.
  void UpdateThrottledNodes(InputStreamManager* stream, bool* stream_was_full);

  // Returns a comma-separated list of source nodes.
  std::string ListSourceNodes() const;

  // Returns a parent node name for the given input stream.
  std::string GetParentNodeDebugName(InputStreamManager* stream) const;

#if !MEDIAPIPE_DISABLE_GPU
  // Owns the legacy GpuSharedData if we need to create one for backwards
  // compatibility.
  std::unique_ptr<GpuSharedData> legacy_gpu_shared_;
#endif  // !MEDIAPIPE_DISABLE_GPU

  // True if the graph was initialized.
  bool initialized_ = false;

  // A packet type that has SetAny() called on it.
  PacketType any_packet_type_;

  // The ValidatedGraphConfig object defining this CalculatorGraph.
  std::unique_ptr<ValidatedGraphConfig> validated_graph_;

  // The PacketGeneratorGraph to use to generate all the input side packets.
  PacketGeneratorGraph packet_generator_graph_;

  // True if the graph has source nodes.
  bool has_sources_ = false;

  // A flat array of InputStreamManager/OutputStreamManager/
  // OutputSidePacketImpl/CalculatorNode corresponding to the input/output
  // stream indexes, output side packet indexes, and calculator indexes
  // respectively in validated_graph_.
  // Once allocated these structures must not be reallocated since
  // internal structures may point to individual entries in the array.
  std::unique_ptr<InputStreamManager[]> input_stream_managers_;
  std::unique_ptr<OutputStreamManager[]> output_stream_managers_;
  std::unique_ptr<OutputSidePacketImpl[]> output_side_packets_;
  std::vector<std::unique_ptr<CalculatorNode>> nodes_;
  bool packet_generator_nodes_added_ = false;

  // The graph output streams.
  std::vector<std::shared_ptr<internal::GraphOutputStream>>
      graph_output_streams_;

  // Maximum queue size for an input stream. This is used by the scheduler to
  // restrict memory usage.
  int max_queue_size_ = -1;

  // Mode for adding packets to a graph input stream. Set to block until all
  // affected input streams are not full by default.
  GraphInputStreamAddMode graph_input_stream_add_mode_
      ABSL_GUARDED_BY(full_input_streams_mutex_);

  // For a source node or graph input stream (specified using id),
  // this stores the set of dependent input streams that have hit their
  // maximum capacity. Graph input streams are also treated as nodes.
  // A node is scheduled only if this set is empty.  Similarly, a packet
  // is added to a graph input stream only if this set is empty.
  // Note that this vector contains an unused entry for each non-source node.
  std::vector<absl::flat_hash_set<InputStreamManager*>> full_input_streams_
      ABSL_GUARDED_BY(full_input_streams_mutex_);

  // Input stream to index within `input_stream_managers_` mapping.
  absl::flat_hash_map<InputStreamManager*, int> input_stream_to_index_;

  // Maps stream names to graph input stream objects.
  absl::flat_hash_map<std::string, std::unique_ptr<GraphInputStream>>
      graph_input_streams_;

  // Maps graph input streams to their virtual node ids.
  absl::flat_hash_map<std::string, int> graph_input_stream_node_ids_;

  // Maps graph input streams to their max queue size.
  absl::flat_hash_map<std::string, int> graph_input_stream_max_queue_size_;

  // The factory for making counters associated with this graph.
  std::unique_ptr<CounterFactory> counter_factory_;

  // Executors for the scheduler, keyed by the executor's name. The default
  // executor's name is the empty string.
  std::map<std::string, std::shared_ptr<Executor>> executors_;

  // The processed input side packet map for this run.
  std::map<std::string, Packet> current_run_side_packets_;

  // Object to manage graph services.
  GraphServiceManager service_manager_;

  // Indicates whether service default initialization is allowed.
  bool allow_service_default_initialization_ = true;

  // Vector of errors encountered while running graph. Always use RecordError()
  // to add an error to this vector.
  std::vector<absl::Status> errors_ ABSL_GUARDED_BY(error_mutex_);

  // Optional error callback set by client.
  std::function<void(const absl::Status&)> error_callback_;

  // True if the default executor uses the application thread.
  bool use_application_thread_ = false;

  // Condition variable that waits until all input streams that depend on a
  // graph input stream are below the maximum queue size.
  absl::CondVar wait_to_add_packet_cond_var_
      ABSL_GUARDED_BY(full_input_streams_mutex_);

  // Mutex for the vector of errors.
  absl::Mutex error_mutex_;

  // Status variable to indicate if the graph has encountered an error.
  std::atomic<bool> has_error_;

  // Mutex for full_input_streams_.
  mutable absl::Mutex full_input_streams_mutex_;

  // Number of closed graph input streams. This is a separate variable because
  // it is not safe to hold a lock on the scheduler while calling Close() on an
  // input stream. Hence, we decouple the closing of the stream and checking its
  // status.
  // TODO: update this comment.
  std::atomic<unsigned int> num_closed_graph_input_streams_;

  // The graph tracing and profiling interface.  It is owned by the
  // CalculatorGraph using a shared_ptr in order to allow threadsafe access
  // to the ProfilingContext from clients that may outlive the CalculatorGraph
  // such as GlContext.  It is declared here before the Scheduler so that it
  // remains available during the Scheduler destructor.
  std::shared_ptr<ProfilingContext> profiler_;

  internal::Scheduler scheduler_;

#if !defined(__EMSCRIPTEN__)
  // Collects runtime information about the graph in the background.
  tool::GraphRuntimeInfoLogger graph_runtime_info_logger_;
#endif  // !defined(__EMSCRIPTEN__)
};

}  // namespace mediapipe

#endif  // MEDIAPIPE_FRAMEWORK_CALCULATOR_GRAPH_H_
