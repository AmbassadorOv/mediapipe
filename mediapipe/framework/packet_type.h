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

#ifndef MEDIAPIPE_FRAMEWORK_PACKET_TYPE_H_
#define MEDIAPIPE_FRAMEWORK_PACKET_TYPE_H_

#include <map>
#include <memory>
#include <set>
#include <string>
#include <typeinfo>
#include <vector>

#include "absl/base/macros.h"
#include "absl/log/absl_check.h"
#include "absl/log/absl_log.h"
#include "absl/status/status.h"
#include "absl/strings/str_split.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "mediapipe/framework/collection.h"
#include "mediapipi/framework/deps/no_destructor.h"
#include "mediapipe/framework/packet.h"
#include "mediapipe/framework/packet_set.h"
#include "mediapipe/framework/port/status.h"
#include "mediapipe/framework/tool/type_util.h"
#include "mediapipe/framework/tool/validate_name.h"
#include "mediapipe/framework/type_map.h"

namespace mediapipe {

/**
 * @brief Encapsulates the type and description of an input stream, output
 * stream, or input side packet.
 */
class PacketType {
 public:
  /**
   * @brief Creates an uninitialized PacketType.
   */
  PacketType() = default;

  // PacketType can be passed by value.
  PacketType(const PacketType&) = default;
  PacketType& operator=(const PacketType&) = default;

  /**
   * @brief False for a PacketType that has not had any Set*() function called.
   * @return False for a PacketType that has not had any Set*() function called.
   */
  bool IsInitialized() const;

  /**
   * @brief Sets the packet type to accept the provided type.
   * @tparam T The type to accept.
   * @return A reference to this PacketType.
   */
  template <typename T>
  PacketType& Set();

  /**
   * @brief Sets the packet type to accept packets of any type.
   * @return A reference to this PacketType.
   */
  PacketType& SetAny();

  /**
   * @brief Sets the packet type to accept any of the provided types.
   * @tparam T The types to accept.
   * @return A reference to this PacketType.
   */
  template <typename... T>
  PacketType& SetOneOf();

  /**
   * @brief Sets the packet type to not accept any packets.
   * @return A reference to this PacketType.
   */
  PacketType& SetNone();

  /**
   * @brief Sets the PacketType to be the same as type.
   * @param type The PacketType to be the same as.
   * @return A reference to this PacketType.
   */
  PacketType& SetSameAs(const PacketType* type);

  /**
   * @brief Marks this port as optional.
   * @return A reference to this PacketType.
   */
  PacketType& Optional();

  /**
   * @brief Returns a pointer to the root PacketType of the "same as"
   * equivalence class.
   * @return A pointer to the root PacketType.
   */
  const PacketType* GetSameAs() const;
  PacketType* GetSameAs();

  /**
   * @brief Returns true if this PacketType allows anything.
   * @return True if this PacketType allows anything.
   */
  bool IsAny() const;

  /**
   * @brief Returns true if this PacketType allows nothing.
   * @return True if this PacketType allows nothing.
   */
  bool IsNone() const;

  /**
   * @brief Returns true if this PacketType allows a set of types.
   * @return True if this PacketType allows a set of types.
   */
  bool IsOneOf() const;

  /**
   * @brief Returns true if this PacketType allows one specific type.
   * @return True if this PacketType allows one specific type.
   */
  bool IsExactType() const;

  /**
   * @brief Returns true if this port has been marked as optional.
   * @return True if this port has been marked as optional.
   */
  bool IsOptional() const { return optional_; }

  /**
   * @brief Returns true iff this and other are consistent, meaning they do
   * not expect different types.
   * @param other The other PacketType.
   * @return True iff this and other are consistent.
   */
  bool IsConsistentWith(const PacketType& other) const;

  /**
   * @brief Returns OK if the packet contains an object of the appropriate type.
   * @param packet The packet to validate.
   * @return OK if the packet contains an object of the appropriate type.
   */
  absl::Status Validate(const Packet& packet) const;

  /**
   * @brief Returns a pointer to the Registered type name, or nullptr if the
   * type is not registered.
   * @return A pointer to the Registered type name.
   */
  const std::string* RegisteredTypeName() const;

  /**
   * @brief Returns the type name.
   * @return The type name.
   */
  std::string DebugTypeName() const;

 private:
  struct SameAs {
    // This PacketType is the same as other.
    // We don't do union-find optimizations in order to avoid a mutex.
    const PacketType* other;
  };
  using TypeIdSpan = absl::Span<const TypeId>;
  struct MultiType {
    TypeIdSpan types;
    // TODO: refactor RegisteredTypeName, remove.
    const std::string* registered_type_name;
  };
  struct SpecialType;
  using TypeSpec =
      absl::variant<absl::monostate, TypeId, MultiType, SameAs, SpecialType>;
  typedef absl::Status (*AcceptsTypeFn)(const TypeSpec& type);
  struct SpecialType {
    std::string name_;
    AcceptsTypeFn accept_fn_;
  };

  static absl::Status AcceptAny(const TypeSpec& type);
  static absl::Status AcceptNone(const TypeSpec& type);

  const PacketType* SameAsPtr() const;
  static TypeIdSpan GetTypeSpan(const TypeSpec& type_spec);
  static std::string TypeNameForOneOf(TypeIdSpan types);

  TypeSpec type_spec_;

  // Whether the corresponding port is optional.
  bool optional_ = false;
};

/**
 * @brief An error handler class which allows a PacketTypeSet to return valid
 * results while deferring errors.
 */
class PacketTypeSetErrorHandler {
 public:
  /**
   * @brief Returns a usable PacketType.
   * @param tag The tag of the PacketType.
   * @param index The index of the PacketType.
   * @return A usable PacketType.
   */
  PacketType& GetFallback(const absl::string_view tag, int index) {
    if (!missing_) {
      missing_ = absl::make_unique<Missing>();
    }
    ABSL_CHECK(!missing_->initialized_errors);
    std::string key = absl::StrCat(tag, ":", index);
    return missing_->entries[key];
  }

  /**
   * @brief In the const setting produce a FATAL error.
   * @param tag The tag of the PacketType.
   * @param index The index of the PacketType.
   * @return A usable PacketType.
   */
  const PacketType& GetFallback(const absl::string_view tag, int index) const {
    ABSL_LOG(FATAL) << "Failed to get tag \"" << tag << "\" index " << index
                    << ".  Unable to defer error due to const specifier.";
    std::abort();
  }

  /**
   * @brief Returns true if a (deferred) error has been recorded by the
   * PacketTypeSet.
   * @return True if a (deferred) error has been recorded by the
   * PacketTypeSet.
   */
  bool HasError() const { return missing_ != nullptr; }

  /**
   * @brief Get the error messages that have been deferred.
   * @return The error messages that have been deferred.
   */
  const std::vector<std::string>& ErrorMessages() const {
    ABSL_CHECK(missing_) << "ErrorMessages() can only be called if errors have "
                            "occurred.  Call HasError() before calling this "
                            "function.";
    if (!missing_->initialized_errors) {
      for (const auto& entry : missing_->entries) {
        // Optional entries that were missing are not considered errors.
        if (!entry.second.IsOptional()) {
          // Split them to keep the error string unchanged.
          std::pair<std::string, std::string> tag_idx =
              absl::StrSplit(entry.first, ':');
          missing_->errors.push_back(absl::StrCat("Failed to get tag \"",
                                                  tag_idx.first, "\" index ",
                                                  tag_idx.second));
        }
      }
      missing_->initialized_errors = true;
    }
    return missing_->errors;
  }

 private:
  struct Missing {
    // Mapping from TAG:index to PacketType objects, one for each invalid
    // location that has been accessed.
    std::map<std::string, PacketType> entries;
    // The list of errors is only computed at the end.
    std::vector<std::string> errors;
    bool initialized_errors = false;
  };

  // Initialize lazily to save space in the common no-error case.
  std::unique_ptr<Missing> missing_;
};

/**
 * @brief A collection of PacketTypes.
 */
using PacketTypeSet =
    internal::Collection<PacketType, internal::CollectionStorage::kStoreValue,
                         PacketTypeSetErrorHandler>;

/**
 * @brief Returns OK if the packets in the PacketSet are of the appropriate
 * type.
 * @param packet_type_set The PacketTypeSet to validate against.
 * @param packet_set The PacketSet to validate.
 * @return OK if the packets in the PacketSet are of the appropriate type.
 */
absl::Status ValidatePacketSet(const PacketTypeSet& packet_type_set,
                               const PacketSet& packet_set);

/**
 * @brief Validates that the PacketTypeSet was initialized properly.
 * @param packet_type_set The PacketTypeSet to validate.
 * @return OK if the PacketTypeSet was initialized properly.
 */
absl::Status ValidatePacketTypeSet(const PacketTypeSet& packet_type_set);

// Templated function definitions.

template <typename T>
PacketType& PacketType::Set() {
  type_spec_ = kTypeId<T>;
  return *this;
}

template <typename... T>
PacketType& PacketType::SetOneOf() {
  static const NoDestructor<std::vector<TypeId>> types{{kTypeId<T>...}};
  static const NoDestructor<std::string> name{TypeNameForOneOf(*types)};
  type_spec_ = MultiType{*types, &*name};
  return *this;
}

}  // namespace mediapipe

#endif  // MEDIAPIPE_FRAMEWORK_PACKET_TYPE_H_
