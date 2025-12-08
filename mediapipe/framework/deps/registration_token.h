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

#ifndef MEDIAPIPE_DEPS_REGISTRATION_TOKEN_H_
#define MEDIAPIPE_DEPS_REGISTRATION_TOKEN_H_

#include <functional>
#include <vector>

namespace mediapipe {
/**
 * @brief A generic class that represents a registration that can be later
 * undone, via a call to Unregister().
 *
 * It is generally a good idea for registration methods, such as
 * RegisterListener(X) to return ways to undo the registration (for instance if
 * X goes out of scope).
 * RegistrationToken is a good candidate as a return value for those methods.
 *
 * This class is thread compatible.
 */
class RegistrationToken {
 public:
  /**
   * @brief Constructor.
   * @param unregisterer A function that unregisters the registration.
   */
  explicit RegistrationToken(std::function<void()> unregisterer);

  /**
   * @brief It is useful to have an empty constructor for when we want to
   * declare a token, and assign it later.
   */
  RegistrationToken() {}

  RegistrationToken(const RegistrationToken&) = delete;
  RegistrationToken& operator=(const RegistrationToken&) = delete;

  RegistrationToken(RegistrationToken&& rhs);
  RegistrationToken& operator=(RegistrationToken&& rhs);

  /**
   * @brief Unregisters the registration for which this token is in charge, and
   * voids the token. It is safe to call this more than once, but further calls
   * are guaranteed to be noop.
   */
  void Unregister();

  /**
   * @brief Returns a token whose Unregister() will Unregister() all <tokens>.
   * @param tokens The tokens to combine.
   * @return A token that unregisters all the given tokens.
   */
  static RegistrationToken Combine(std::vector<RegistrationToken> tokens);

 private:
  std::function<void()> unregister_function_ = nullptr;
};

/**
 * @brief RAII class for registration tokens: it calls Unregister() when it
 * goes out of scope.
 */
class Unregister {
 public:
  /**
   * @brief Useful to have an empty constructor for when we want to assign it
   * later. The default is an empty token that does nothing.
   */
  Unregister() : token_() {}
  explicit Unregister(RegistrationToken token);
  ~Unregister();

  Unregister(const Unregister&) = delete;
  Unregister& operator=(const Unregister&) = delete;

  Unregister(Unregister&& rhs);
  Unregister& operator=(Unregister&& rhs);

  /**
   * @brief Similar to unique_ptr.reset() and the likes: this will unregister
   * the current token if any, and then assume registration ownership of this
   * new <token>.
   * @param token The new token to own.
   */
  void Reset(RegistrationToken token = RegistrationToken());

 private:
  RegistrationToken token_;
};
}  // namespace mediapipe

#endif  // MEDIAPIPE_DEPS_REGISTRATION_TOKEN_H_
