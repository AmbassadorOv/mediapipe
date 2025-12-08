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
// Defines Packet, a container capable of holding an object of any type.

#ifndef MEDIAPIPE_FRAMEWORK_PACKET_H_
#define MEDIAPIPE_FRAMEWORK_PACKET_H_

#include <cstddef>
#include <cstdint>
#include <memory>
#include <ostream>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "absl/base/attributes.h"
#include "absl/base/macros.h"
#include "absl/base/optimization.h"
#include "absl/functional/any_invocable.h"
#include "absl/log/absl_check.h"
#include "absl/log/absl_log.h"
#include "absl/memory/memory.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/synchronization/mutex.h"
#include "google/protobuf/message.h"
#include "google/protobuf/message_lite.h"
#include "mediapipe/framework/deps/no_destructor.h"
#include "mediapipe/framework/deps/registration.h"
#include "mediapipe/framework/port.h"
#include "mediapipe/framework/port/canonical_errors.h"
#include "mediapipe/framework/port/logging.h"
#include "mediapipe/framework/port/proto_ns.h"
#include "mediapipe/framework/port/ret_check.h"
#include "mediapipe/framework/port/status.h"
#include "mediapipe/framework/port/status_builder.h"
#include "mediapipe/framework/port/status_macros.h"
#include "mediapipe/framework/port/statusor.h"
#include "mediapipe/framework/timestamp.h"
#include "mediapipe/framework/tool/type_util.h"
#include "mediapipe/framework/type_map.h"

namespace mediapipe {

class Packet;

namespace packet_internal {
class HolderBase;

Packet Create(HolderBase* holder);
Packet Create(HolderBase* holder, Timestamp timestamp);
Packet Create(std::shared_ptr<const HolderBase> holder, Timestamp timestamp);
const HolderBase* GetHolder(const Packet& packet);
const std::shared_ptr<const HolderBase>& GetHolderShared(const Packet& packet);
std::shared_ptr<const HolderBase> GetHolderShared(Packet&& packet);
absl::StatusOr<Packet> PacketFromDynamicProto(const std::string& type_name,
                                              const std::string& serialized);
}  // namespace packet_internal

/**
 * @brief A generic container class which can hold data of any type.
 *
 * The type of the data is specified when accessing the data (using
 * Packet::Get<T>()).
 *
 * The Packet is implemented as a reference-counted pointer. This means
 * that copying Packets creates a fast, shallow copy. Packets are
 * copyable, movable, and assignable. Packets can be stored in STL
 * containers. A Packet may optionally contain a timestamp.
 *
 * This class is thread compatible.
 */
class Packet {
 public:
  /**
   * @brief Creates an empty Packet.
   */
  Packet() = default;

  // Copy constructor and assignment operator.
  Packet(const Packet&);
  Packet& operator=(const Packet&);

  // Move constructor and assignment operator that take non-const rvalue
  // reference.
  Packet(Packet&&);
  Packet& operator=(Packet&&);

  /**
   * @brief Returns a Packet that contains the same data as *this, and has the
   * given timestamp.
   * @param timestamp The timestamp to set.
   * @return A Packet with the given timestamp.
   */
  Packet At(class Timestamp timestamp) const&;

  /**
   * @brief The rvalue reference overload of Packet's member function
   * Packet::At(class Timestamp).
   * @param timestamp The timestamp to set.
   * @return A Packet with the given timestamp.
   */
  Packet At(class Timestamp timestamp) &&;

  /**
   * @brief Returns true iff the Packet has been created using the default
   * constructor Packet(), or is a copy of such a Packet.
   * @return True iff the Packet is empty.
   */
  bool IsEmpty() const;

  /**
   * @brief Returns the reference to the object of typename T if it contains
   * one, crashes otherwise.
   * @tparam T The type of the object.
   * @return The reference to the object.
   */
  template <typename T>
  const T& Get() const;

  /**
   * @brief Returns a shared pointer to the object of typename T if it contains
   * one, an error otherwise.
   * @tparam T The type of the object.
   * @return A shared pointer to the object.
   */
  template <typename T>
  absl::StatusOr<std::shared_ptr<const T>> Share() const;

  /**
   * @brief Transfers the ownership of holder's data to a unique pointer
   * of the object if the packet is the sole owner of a non-foreign
   * holder.
   * @tparam T The type of the object.
   * @return A unique pointer to the object.
   */
  template <typename T>
  ABSL_DEPRECATED(
      "Avoid Consume* functions usage as in most cases it's hard to ensure "
      "the proper usage (taken the nature of calculators not knowing where "
      "packets are received from and sent to) and leads to races. Consider "
      "SharedPtrWithPacket instead to get a shared_ptr<T> if applicable.")
  absl::StatusOr<std::unique_ptr<T>> Consume();

  /**
   * @brief Consumes the packet and transfers the ownership of the data to a
   * unique pointer if the packet is the sole owner of a non-foreign
   * holder.
   * @tparam T The type of the object.
   * @param was_copied A pointer to a boolean that will be set to true if the
   * data was copied.
   * @return A unique pointer to the object.
   */
  template <typename T>
  ABSL_DEPRECATED(
      "Avoid Consume* functions usage as in most cases it's hard to ensure "
      "the proper usage (taken the nature of calculators not knowing where "
      "packets are received from and sent to) and leads to races. Consider "
      "SharedPtrWithPacket instead to get a shared_ptr<T> if applicable.")
  absl::StatusOr<std::unique_ptr<T>> ConsumeOrCopy(
      bool* was_copied = nullptr,
      typename std::enable_if<!std::is_array<T>::value>::type* = nullptr);

  /**
   * @brief Version for bounded array.
   * @tparam T The type of the object.
   * @param was_copied A pointer to a boolean that will be set to true if the
   * data was copied.
   * @return A unique pointer to the object.
   */
  template <typename T>
  ABSL_DEPRECATED(
      "Avoid Consume* functions usage as in most cases it's hard to ensure "
      "the proper usage (taken the nature of calculators not knowing where "
      "packets are received from and sent to) and leads to races. Consider "
      "SharedPtrWithPacket instead to get a shared_ptr<T> if applicable.")
  absl::StatusOr<std::unique_ptr<T>> ConsumeOrCopy(
      bool* was_copied = nullptr,
      typename std::enable_if<std::is_array<T>::value &&
                              std::extent<T>::value != 0>::type* = nullptr);

  /**
   * @brief Version for unbounded array.
   * @tparam T The type of the object.
   * @param was_copied A pointer to a boolean that will be set to true if the
   * data was copied.
   * @return A unique pointer to the object.
   */
  template <typename T>
  ABSL_DEPRECATED(
      "Avoid Consume* functions usage as in most cases it's hard to ensure "
      "the proper usage (taken the nature of calculators not knowing where "
      "packets are received from and sent to) and leads to races. Consider "
      "SharedPtrWithPacket instead to get a shared_ptr<T> if applicable.")
  absl::StatusOr<std::unique_ptr<T>> ConsumeOrCopy(
      bool* was_copied = nullptr,
      typename std::enable_if<std::is_array<T>::value &&
                              std::extent<T>::value == 0>::type* = nullptr);

  /**
   * @brief Returns the reference to type MessageLite data, if the underlying
   * object type is protocol buffer, crashes otherwise.
   * @return The reference to type MessageLite data.
   */
  const proto_ns::MessageLite& GetProtoMessageLite() const;

  /**
   * @brief Returns a vector of pointers to MessageLite data, if the underlying
   * object type is a vector of MessageLite data, returns an error otherwise.
   * @return A vector of pointers to MessageLite data.
   */
  absl::StatusOr<std::vector<const proto_ns::MessageLite*>>
  GetVectorOfProtoMessageLitePtrs() const;

  /**
   * @brief Returns an error if the packet does not contain data of type T.
   * @tparam T The type of the data.
   * @return An error if the packet does not contain data of type T.
   */
  template <typename T>
  absl::Status ValidateAsType() const {
    return ValidateAsType(kTypeId<T>);
  }

  /**
   * @brief Returns an error if the packet is not an instance of
   * a protocol buffer message.
   * @return An error if the packet is not an instance of
   * a protocol buffer message.
   */
  absl::Status ValidateAsProtoMessageLite() const;

  /**
   * @brief Get the type id for the underlying type stored in the Packet.
   * @return The type id for the underlying type stored in the Packet.
   */
  TypeId GetTypeId() const;

  /**
   * @brief Returns the timestamp.
   * @return The timestamp.
   */
  class Timestamp Timestamp() const;

  /**
   * @brief Returns a debug string for the packet.
   * @return A debug string for the packet.
   */
  std::string DebugString() const;
  friend std::ostream& operator<<(std::ostream& stream, const Packet& p) {
    return stream << p.DebugString();
  }

  /**
   * @brief Returns the type name.
   * @return The type name.
   */
  std::string RegisteredTypeName() const;

  /**
   * @brief Returns a string with the best guess at the type name.
   * @return A string with the best guess at the type name.
   */
  std::string DebugTypeName() const;

 private:
  friend Packet packet_internal::Create(packet_internal::HolderBase* holder);
  friend Packet packet_internal::Create(packet_internal::HolderBase* holder,
                                        class Timestamp timestamp);
  friend Packet packet_internal::Create(
      std::shared_ptr<const packet_internal::HolderBase> holder,
      class Timestamp timestamp);
  friend const packet_internal::HolderBase* packet_internal::GetHolder(
      const Packet& packet);
  friend const std::shared_ptr<const packet_internal::HolderBase>&
  packet_internal::GetHolderShared(const Packet& packet);
  friend std::shared_ptr<const packet_internal::HolderBase>
  packet_internal::GetHolderShared(Packet&& packet);

  friend class PacketType;
  absl::Status ValidateAsType(TypeId type_id) const;

  std::shared_ptr<const packet_internal::HolderBase> holder_;
  class Timestamp timestamp_;
};

// Factory functions for creating Packets. Non-members as opposed to static
// methods, to prevent users from mistakenly calling them on Packet instances.

/**
 * @brief Returns a Packet that adopts the object; the Packet assumes the
 * ownership.
 * @tparam T The type of the object.
 * @param ptr A pointer to the object.
 * @return A Packet that adopts the object.
 */
template <typename T>
Packet Adopt(const T* ptr);

/**
 * @brief Returns a Packet that does not own its data.
 * @tparam T The type of the object.
 * @param ptr A pointer to the object.
 * @param cleanup A cleanup function.
 * @return A Packet that does not own its data.
 */
template <typename T>
Packet PointToForeign(const T* ptr,
                      absl::AnyInvocable<void()> cleanup = nullptr);

/**
 * @brief Adopts the data but places it in a std::unique_ptr inside the
 * resulting Packet, leaving the timestamp unset.
 * @tparam T The type of the object.
 * @param ptr A pointer to the object.
 * @return A Packet that adopts the data.
 */
template <typename T>
Packet AdoptAsUniquePtr(T* ptr) {
  static_assert(
      !std::is_const<T>::value,
      "AdoptAsUniquePtr should not be called with a pointer-to-const.");
  return Adopt(new std::unique_ptr<T>(ptr));
}

/**
 * @brief A SyncedPacket is a packet containing a reference to another packet,
 * and the reference can be updated.
 */
class SyncedPacket {
 public:
  explicit SyncedPacket(const Packet& packet) : packet_(packet) {}
  void UpdatePacket(const Packet& packet) {
    absl::WriterMutexLock writer_lock(&mutex_);
    packet_ = packet;
  }
  Packet Get() {
    absl::ReaderMutexLock reader_lock(&mutex_);
    return packet_;
  }

 private:
  absl::Mutex mutex_;
  Packet packet_;
};

/**
 * @brief Adopt the data as SyncedPacket, so that the content of the packet can
 * be updated in a thread-safe way.
 * @tparam T The type of the object.
 * @param ptr A pointer to the object.
 * @return A Packet that adopts the data as SyncedPacket.
 */
template <typename T>
Packet AdoptAsSyncedPacket(const T* ptr) {
  Packet packet = Adopt(ptr);
  return AdoptAsUniquePtr(new SyncedPacket(packet));
}

/**
 * @brief Create a packet containing an object of type T initialized with the
 * provided arguments.
 * @tparam T The type of the object.
 * @tparam Args The types of the arguments.
 * @param args The arguments.
 * @return A Packet containing the object.
 */
template <typename T,
          typename std::enable_if<!std::is_array<T>::value>::type* = nullptr,
          typename... Args>
Packet MakePacket(Args&&... args) {  // NOLINT(build/c++11)
  return Adopt(new T(std::forward<Args>(args)...));
}

/**
 * @brief Version for arrays.
 * @tparam T The type of the object.
 * @tparam Args The types of the arguments.
 * @param args The arguments.
 * @return A Packet containing the object.
 */
template <typename T,
          typename std::enable_if<std::is_array<T>::value>::type* = nullptr,
          typename... Args>
Packet MakePacket(Args&&... args) {  // NOLINT(build/c++11)
  return Adopt(reinterpret_cast<T*>(
      new T{std::forward<typename std::remove_extent<T>::type>(args)...}));
}

/**
 * @brief Returns a mutable pointer to the data in a unique_ptr in a packet.
 * @tparam T The type of the object.
 * @param packet The packet.
 * @return A mutable pointer to the data.
 */
template <typename T>
T* GetFromUniquePtr(const Packet& packet) {
  return packet.Get<std::unique_ptr<T>>().get();
}

/**
 * @brief Returns a shared_ptr to the payload of the packet which retains its
 * object through a copy of the packet.
 * @tparam T The type of the object.
 * @param packet The packet.
 * @return A shared_ptr to the payload of the packet.
 */
template <typename T>
ABSL_DEPRECATED("Use Packet::Share<T>() instead.")
std::shared_ptr<const T> SharedPtrWithPacket(const Packet& packet) {
  absl::StatusOr<std::shared_ptr<const T>> shared_ptr = packet.Share<T>();
  ABSL_CHECK_OK(shared_ptr.status());
  return *std::move(shared_ptr);
}

//// Implementation details.
namespace packet_internal {

template <typename T>
class Holder;
template <typename T>
class ForeignHolder;

class HolderBase {
 public:
  HolderBase() {}
  HolderBase(const HolderBase&) = delete;
  HolderBase& operator=(const HolderBase&) = delete;
  virtual ~HolderBase();
  template <typename T>
  bool PayloadIsOfType() const {
    return GetTypeId() == kTypeId<T>;
  }
  // Returns a printable string identifying the type stored in the holder.
  virtual const std::string DebugTypeName() const = 0;
  // Returns debug data id.
  virtual int64_t DebugDataId() const = 0;
  // Returns the registered type name if it's available, otherwise the
  // empty string.
  virtual const std::string RegisteredTypeName() const = 0;
  // Get the type id of the underlying data type.
  virtual TypeId GetTypeId() const = 0;

  // Downcasts this to Holder<T>.  Returns nullptr if deserialization
  // failed or if the requested type is not what is stored.
  template <typename T>
  const Holder<T>* As() const;

  // Same as As() but returns this as a mutable Holder<T>.
  // Dangerous! Only use in contexts when we are guaranteed the holder is not
  // shared.
  template <typename T>
  Holder<T>* AsMutable() const;

  // Returns the pointer to MessageLite type for the data in holder, if
  // underlying object is protocol buffer type, otherwise, nullptr is returned.
  virtual const proto_ns::MessageLite* GetProtoMessageLite() const = 0;

  // Returns a vector<MessageLite*> for the data in the holder, if the
  // underlying object is a vector of protocol buffer objects, otherwise,
  // returns an error.
  virtual absl::StatusOr<std::vector<const proto_ns::MessageLite*>>
  GetVectorOfProtoMessageLite() const = 0;

  virtual bool HasForeignOwner() const { return false; }
};

// Two helper functions to get the proto base pointers.
template <typename T>
const proto_ns::MessageLite* ConvertToProtoMessageLite(const T* data,
                                                       std::false_type) {
  return nullptr;
}

template <typename T>
const proto_ns::MessageLite* ConvertToProtoMessageLite(const T* data,
                                                       std::true_type) {
  return data;
}

// Helper structs for determining if a type is an std::vector<Proto>.
template <typename Type>
struct is_proto_vector : public std::false_type {};

template <typename ItemT, typename Allocator>
struct is_proto_vector<std::vector<ItemT, Allocator>>
    : public std::is_base_of<proto_ns::MessageLite, ItemT>::type {};

// Helper function to create and return a vector of pointers to proto message
// elements of the vector passed into the function.
template <typename T>
absl::StatusOr<std::vector<const proto_ns::MessageLite*>>
ConvertToVectorOfProtoMessageLitePtrs(const T* data,
                                      /*is_proto_vector=*/std::false_type) {
  return absl::InvalidArgumentError(absl::StrCat(
      "The Packet stores \"", kTypeId<T>.name(), "\"",
      "which is not convertible to vector<proto_ns::MessageLite*>."));
}

template <typename T>
absl::StatusOr<std::vector<const proto_ns::MessageLite*>>
ConvertToVectorOfProtoMessageLitePtrs(const T* data,
                                      /*is_proto_vector=*/std::true_type) {
  std::vector<const proto_ns::MessageLite*> result;
  for (auto it = data->begin(); it != data->end(); ++it) {
    const proto_ns::MessageLite* element = &(*it);
    result.push_back(element);
  }
  return result;
}

// This registry is used to create Holders of the right concrete C++ type given
// a proto type string (which is used as the registration key).
class MessageHolderRegistry
    : public GlobalFactoryRegistry<std::unique_ptr<HolderBase>> {};

template <typename T>
struct is_concrete_proto_t
    : public std::integral_constant<
          bool, std::is_base_of<proto_ns::MessageLite, T>{} &&
                    !std::is_same<proto_ns::MessageLite, T>{} &&
                    !std::is_same<proto_ns::Message, T>{}> {};

template <typename T>
std::unique_ptr<HolderBase> CreateMessageHolder() {
  return absl::make_unique<Holder<T>>(new T);
}

// Registers a message type. T must be a non-cv-qualified concrete proto type.
MEDIAPIPE_STATIC_REGISTRATOR_TEMPLATE(MessageRegistrator, MessageHolderRegistry,
                                      T{}.GetTypeName(), CreateMessageHolder<T>)

// For non-Message payloads, this does nothing.
template <typename T, typename Enable = void>
struct HolderPayloadRegistrator {};

// This template ensures that, for each concrete MessageLite subclass that is
// stored in a Packet, we register a function that allows us to create a
// Holder with the correct payload type from the proto's type name.
//
// We must use std::remove_cv to ensure we don't try to register Foo twice if
// there are Holder<Foo> and Holder<const Foo>. TODO: lift this
// up to Holder?
template <typename T>
struct HolderPayloadRegistrator<
    T, typename std::enable_if<is_concrete_proto_t<T>{}>::type>
    : private MessageRegistrator<typename std::remove_cv<T>::type> {};

template <typename T>
class Holder : public HolderBase, private HolderPayloadRegistrator<T> {
 public:
  explicit Holder(const T* ptr) : ptr_(ptr) {}
  ~Holder() override { delete_helper(); }
  const T& data() const { return *ptr_; }
  TypeId GetTypeId() const final { return kTypeId<T>; }
  // Releases the underlying data pointer and transfers the ownership to a
  // unique pointer.
  // This method is dangerous and is only used by Packet::Consume() if the
  // packet is the only owner of the holder.
  template <typename U = T>
  absl::StatusOr<std::unique_ptr<T>> Release(
      typename std::enable_if<!std::is_array<U>::value ||
                              std::extent<U>::value != 0>::type* = 0) {
    if (HasForeignOwner()) {
      return InternalError(
          "Foreign holder can't release data ptr without ownership.");
    }
    // Casts away constness to make the data mutable after the release.
    std::unique_ptr<T> data_ptr(const_cast<T*>(ptr_));
    ptr_ = nullptr;
    return std::move(data_ptr);
  }
  // TODO: support unbounded array after fixing the bug in holder's
  // delete helper.
  template <typename U = T>
  absl::StatusOr<std::unique_ptr<T>> Release(
      typename std::enable_if<std::is_array<U>::value &&
                              std::extent<U>::value == 0>::type* = 0) {
    return absl::InternalError("Release T[] isn't supported.");
  }
  const std::string DebugTypeName() const final {
    return MediaPipeTypeStringOrDemangled<T>();
  }
  int64_t DebugDataId() const final { return reinterpret_cast<int64_t>(ptr_); }
  const std::string RegisteredTypeName() const final {
    const std::string* type_string = MediaPipeTypeString<T>();
    if (type_string) {
      return *type_string;
    }
    return "";
  }

 protected:
  // The pointer that uniquely owns the data. However, the ownership of the
  // Holder itself may be shared by several Packets.
  const T* ptr_;

  // Returns the MessageLite pointer to the data, if the underlying object type
  // is protocol buffer, otherwise, nullptr is returned.
  const proto_ns::MessageLite* GetProtoMessageLite() const override {
    return ConvertToProtoMessageLite(
        ptr_, std::is_base_of<proto_ns::MessageLite, T>());
  }

  // Returns a vector<MessageLite*> for the data in the holder, if the
  // underlying object is a vector of protocol buffer objects, otherwise,
  // returns an error.
  absl::StatusOr<std::vector<const proto_ns::MessageLite*>>
  GetVectorOfProtoMessageLite() const override {
    return ConvertToVectorOfProtoMessageLitePtrs(ptr_, is_proto_vector<T>());
  }

 private:
  // Call delete[] if T is an array, delete otherwise.
  template <typename U = T>
  inline void delete_helper(
      typename std::enable_if<!std::is_array<U>::value>::type* = 0) {
    delete ptr_;
  }
  template <typename U = T>
  inline void delete_helper(
      typename std::enable_if<std::is_array<U>::value>::type* = 0) {
    // Casts ptr_ from const Type(*)[] or const Type(*)[N] to const Type*.
    // Deleting a pointer to incomplete type (Type(*)[]) causes compile error.
    delete[] reinterpret_cast<const typename std::remove_extent<U>::type*>(
        ptr_);
  }
};

// Like Holder, but does not own its data.
template <typename T>
class ForeignHolder : public Holder<T> {
 public:
  explicit ForeignHolder(const T* ptr,
                         absl::AnyInvocable<void()> cleanup = nullptr)
      : Holder<T>::Holder(ptr), cleanup_(std::move(cleanup)) {}

  ~ForeignHolder() override {
    // Null out ptr_ so it doesn't get deleted by ~Holder.
    // Note that ~Holder cannot call HasForeignOwner because the subclass's
    // destructor runs first.
    this->ptr_ = nullptr;
    if (cleanup_) {
      cleanup_();
    }
  }

  bool HasForeignOwner() const final { return true; }

 private:
  absl::AnyInvocable<void()> cleanup_;
};

template <typename T>
Holder<T>* HolderBase::AsMutable() const {
  if (PayloadIsOfType<T>()) {
    return static_cast<Holder<T>*>(const_cast<HolderBase*>(this));
  }
  // Does not hold a T.
  return nullptr;
}

template <typename T>
const Holder<T>* HolderBase::As() const {
  if (PayloadIsOfType<T>()) {
    return static_cast<const Holder<T>*>(this);
  }
  // Does not hold a T.
  return nullptr;
}

}  // namespace packet_internal

inline Packet::Packet(const Packet& packet)
    : holder_(packet.holder_), timestamp_(packet.timestamp_) {
  VLOG(4) << "Using copy constructor of " << packet.DebugString();
}

inline Packet& Packet::operator=(const Packet& packet) {
  VLOG(4) << "Using copy assignment operator of " << packet.DebugString();
  if (this != &packet) {
    holder_ = packet.holder_;
    timestamp_ = packet.timestamp_;
  }
  return *this;
}

template <typename T>
inline absl::StatusOr<std::unique_ptr<T>> Packet::Consume() {
  // If type validation fails, returns error.
  MP_RETURN_IF_ERROR(ValidateAsType<T>());
  // Clients who use this function are responsible for ensuring that no
  // other thread is doing anything with this Packet.
  if (!holder_->HasForeignOwner() && holder_.use_count() == 1) {
    VLOG(2) << "Consuming the data of " << DebugString();
    absl::StatusOr<std::unique_ptr<T>> release_result =
        holder_->AsMutable<T>()->Release();
    if (release_result.ok()) {
      VLOG(2) << "Setting " << DebugString() << " to empty.";
      holder_.reset();
    }
    return release_result;
  }
  // If packet isn't the sole owner of the holder, returns kFailedPrecondition
  // error with message.
  return absl::Status(absl::StatusCode::kFailedPrecondition,
                      "Packet isn't the sole owner of the holder.");
}

template <typename T>
inline absl::StatusOr<std::unique_ptr<T>> Packet::ConsumeOrCopy(
    bool* was_copied,
    typename std::enable_if<!std::is_array<T>::value>::type*) {
  MP_RETURN_IF_ERROR(ValidateAsType<T>());
  // If holder is the sole owner of the underlying data, consumes this packet.
  if (!holder_->HasForeignOwner() && holder_.use_count() == 1) {
    VLOG(2) << "Consuming the data of " << DebugString();
    absl::StatusOr<std::unique_ptr<T>> release_result =
        holder_->AsMutable<T>()->Release();
    if (release_result.ok()) {
      VLOG(2) << "Setting " << DebugString() << " to empty.";
      holder_.reset();
    }
    if (was_copied) {
      *was_copied = false;
    }
    return release_result;
  }
  VLOG(2) << "Copying the data of " << DebugString();
  std::unique_ptr<T> data_ptr = absl::make_unique<T>(Get<T>());
  VLOG(2) << "Setting " << DebugString() << " to empty.";
  holder_.reset();
  if (was_copied) {
    *was_copied = true;
  }
  return std::move(data_ptr);
}

template <typename T>
inline absl::StatusOr<std::unique_ptr<T>> Packet::ConsumeOrCopy(
    bool* was_copied,
    typename std::enable_if<std::is_array<T>::value &&
                            std::extent<T>::value != 0>::type*) {
  MP_RETURN_IF_ERROR(ValidateAsType<T>());
  // If holder is the sole owner of the underlying data, consumes this packet.
  if (!holder_->HasForeignOwner() && holder_.use_count() == 1) {
    VLOG(2) << "Consuming the data of " << DebugString();
    absl::StatusOr<std::unique_ptr<T>> release_result =
        holder_->AsMutable<T>()->Release();
    if (release_result.ok()) {
      VLOG(2) << "Setting " << DebugString() << " to empty.";
      holder_.reset();
    }
    if (was_copied) {
      *was_copied = false;
    }
    return release_result;
  }
  VLOG(2) << "Copying the data of " << DebugString();
  const auto& original_array = Get<T>();
  // Type T is bounded array type, such as int[N] and float[M].
  // The new operator creates a new bounded array.
  std::unique_ptr<T> data_ptr(reinterpret_cast<T*>(new T));
  // Copies bounded array data into data_ptr.
  std::copy(std::begin(original_array), std::end(original_array),
            std::begin(*data_ptr));
  VLOG(2) << "Setting " << DebugString() << " to empty.";
  holder_.reset();
  if (was_copied) {
    *was_copied = true;
  }
  return std::move(data_ptr);
}

template <typename T>
inline absl::StatusOr<std::unique_ptr<T>> Packet::ConsumeOrCopy(
    bool* was_copied,
    typename std::enable_if<std::is_array<T>::value &&
                            std::extent<T>::value == 0>::type*) {
  return absl::InternalError("Unbounded array isn't supported.");
}

inline Packet::Packet(Packet&& packet) {
  VLOG(4) << "Using move constructor of " << packet.DebugString();
  holder_ = std::move(packet.holder_);
  timestamp_ = packet.timestamp_;
  packet.timestamp_ = Timestamp::Unset();
}

inline Packet& Packet::operator=(Packet&& packet) {
  VLOG(4) << "Using move assignment operator of " << packet.DebugString();
  if (this != &packet) {
    holder_ = std::move(packet.holder_);
    timestamp_ = packet.timestamp_;
    packet.timestamp_ = Timestamp::Unset();
  }
  return *this;
}

inline bool Packet::IsEmpty() const { return holder_ == nullptr; }

inline TypeId Packet::GetTypeId() const {
  ABSL_CHECK(holder_);
  return holder_->GetTypeId();
}

template <typename T>
inline const T& Packet::Get() const {
  const packet_internal::Holder<T>* holder =
      IsEmpty() ? nullptr : holder_->As<T>();
  if (holder == nullptr) {
    // Produce a good error message.
    absl::Status status = ValidateAsType<T>();
    ABSL_LOG(FATAL) << "Packet::Get() failed: " << status.message();
  }
  return holder->data();
}

// Returns a shared pointer to the object of typename T if it contains
// one, an error otherwise (if the packet is empty or of another type). It is
// safe to concurrently call Share() on the same packet from multiple threads.
template <typename T>
inline absl::StatusOr<std::shared_ptr<const T>> Packet::Share() const {
  MP_RETURN_IF_ERROR(ValidateAsType<T>());
  const T* ptr = &Get<T>();
  return std::shared_ptr<const T>(
      ptr, [packet = *this](const T* ptr) mutable { packet = {}; });
}

inline Timestamp Packet::Timestamp() const { return timestamp_; }

template <typename T>
Packet Adopt(const T* ptr) {
  ABSL_CHECK(ptr != nullptr);
  return packet_internal::Create(new packet_internal::Holder<T>(ptr));
}

template <typename T>
Packet PointToForeign(const T* ptr, absl::AnyInvocable<void()> cleanup) {
  ABSL_CHECK(ptr != nullptr);
  return packet_internal::Create(
      new packet_internal::ForeignHolder<T>(ptr, std::move(cleanup)));
}

// Equal Packets refer to the same memory contents, like equal pointers.
inline bool operator==(const Packet& p1, const Packet& p2) {
  return packet_internal::GetHolder(p1) == packet_internal::GetHolder(p2);
}
inline bool operator!=(const Packet& p1, const Packet& p2) {
  return !(p1 == p2);
}

namespace packet_internal {

inline const std::shared_ptr<const HolderBase>& GetHolderShared(
    const Packet& packet) {
  return packet.holder_;
}

inline std::shared_ptr<const HolderBase> GetHolderShared(Packet&& packet) {
  return std::move(packet.holder_);
}

}  // namespace packet_internal

}  // namespace mediapipe

#endif  // MEDIAPIPE_FRAMEWORK_PACKET_H_
