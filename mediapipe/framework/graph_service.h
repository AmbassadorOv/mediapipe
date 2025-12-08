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

#ifndef MEDIAPIPE_FRAMEWORK_GRAPH_SERVICE_H_
#define MEDIAPIPE_FRAMEWORK_GRAPH_SERVICE_H_

#include <memory>
#include <type_traits>
#include <utility>

#include "absl/log/absl_check.h"
#include "absl/strings/str_cat.h"
#include "mediapipe/framework/packet.h"
#include "mediapipe/framework/port/status.h"

namespace mediapipe {

/**
 * @brief The base class for graph services.
 *
 * The GraphService API can be used to define extensions to a graph's execution
 * environment. These are, essentially, graph-level singletons, and are
 * available to all calculators in the graph (and in any subgraphs) without
 * requiring a manual connection.
 */
class GraphServiceBase {
 public:
  /**
   * @brief The support for default initialization.
   */
  enum DefaultInitSupport {
    kAllowDefaultInitialization,
    kDisallowDefaultInitialization
  };

  /**
   * @brief Constructor.
   * @param key The key of the service.
   */
  explicit constexpr GraphServiceBase(const char* key) : key(key) {}

  /**
   * @brief Creates a default object for the service.
   * @return A packet containing the default object.
   */
  inline virtual absl::StatusOr<Packet> CreateDefaultObject() const {
    return DefaultInitializationUnsupported();
  }

  const char* key;

 protected:
  ~GraphServiceBase() = default;

  /**
   * @brief Returns an error status indicating that default initialization is
   * not supported.
   * @return An error status.
   */
  absl::Status DefaultInitializationUnsupported() const {
    return absl::UnimplementedError(absl::StrCat(
        "Graph service '", key, "' does not support default initialization"));
  }
};

/**
 * @brief A global constant to refer a service.
 *
 * @tparam T The type of the service.
 */
template <typename T>
class GraphService final : public GraphServiceBase {
 public:
  using type = T;
  using packet_type = std::shared_ptr<T>;

  /**
   * @brief Constructor.
   * @param my_key The key of the service.
   * @param default_init The support for default initialization.
   */
  constexpr GraphService(const char* my_key, DefaultInitSupport default_init =
                                                 kDisallowDefaultInitialization)
      : GraphServiceBase(my_key), default_init_(default_init) {}

  /**
   * @brief Creates a default object for the service.
   * @return A packet containing the default object.
   */
  absl::StatusOr<Packet> CreateDefaultObject() const final {
    if (default_init_ != kAllowDefaultInitialization) {
      return DefaultInitializationUnsupported();
    }
    auto packet_or = CreateDefaultObjectInternal();
    if (packet_or.ok()) {
      return MakePacket<std::shared_ptr<T>>(std::move(packet_or).value());
    } else {
      return packet_or.status();
    }
  }

 private:
  absl::StatusOr<std::shared_ptr<T>> CreateDefaultObjectInternal() const {
    auto call_create = [](auto x) -> decltype(decltype(x)::type::Create()) {
      return decltype(x)::type::Create();
    };
    if constexpr (std::is_invocable_r_v<absl::StatusOr<std::shared_ptr<T>>,
                                        decltype(call_create), type_tag<T>>) {
      return T::Create();
    }
    if constexpr (std::is_default_constructible_v<T>) {
      return std::make_shared<T>();
    }
    return DefaultInitializationUnsupported();
  }

  template <class U>
  struct type_tag {
    using type = U;
  };

  DefaultInitSupport default_init_;
};

/**
 * @brief A binding to a graph service.
 *
 * @tparam T The type of the service.
 */
template <typename T>
class ServiceBinding {
 public:
  /**
   * @brief Returns true if the service is available.
   * @return True if the service is available.
   */
  bool IsAvailable() { return service_ != nullptr; }

  /**
   * @brief Gets the service object.
   * @return The service object.
   */
  T& GetObject() {
    ABSL_CHECK(service_) << "Service is unavailable.";
    return *service_;
  }

  ServiceBinding() {}
  explicit ServiceBinding(std::shared_ptr<T> service) : service_(service) {}

 private:
  std::shared_ptr<T> service_;
};

}  // namespace mediapipe

#endif  // MEDIAPIPE_FRAMEWORK_GRAPH_SERVICE_H_
