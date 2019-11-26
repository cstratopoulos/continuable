
/*

                        /~` _  _ _|_. _     _ |_ | _
                        \_,(_)| | | || ||_|(_||_)|(/_

                    https://github.com/Naios/continuable
                                   v4.0.0

  Copyright(c) 2015 - 2019 Denis Blank <denis.blank at outlook dot com>

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files(the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and / or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions :

  The above copyright notice and this permission notice shall be included in
  all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
  SOFTWARE.
**/

#ifndef CONTINUABLE_ASIO_ASYNC_RESULT_HPP_INCLUDED
#define CONTINUABLE_ASIO_ASYNC_RESULT_HPP_INCLUDED

#include <continuable/continuable-promisify.hpp>

#include <asio/async_result.hpp>
#include <asio/error_code.hpp>

namespace asio {

// Class used to specify an async op should return a continuable
struct use_cti_t {};

// Special value instance of `use_cti_t`
constexpr use_cti_t use_cti;

namespace detail {

// Type deduction helper for `Result` in `cti::make_continuable<Result>`
template <typename Signature>
struct continuable_result;

template <>
struct continuable_result<void(error_code)> {
  using type = void;
};

template <>
struct continuable_result<void(error_code const&)>
    : continuable_result<void(error_code)> {};

template <typename T>
struct continuable_result<void(error_code, T)> {
  using type = T;
};

template <typename T>
struct continuable_result<void(error_code const&, T)>
    : continuable_result<void(error_code, T)> {};

template <typename Signature>
using continuable_result_t = typename continuable_result<Signature>::type;

// Binds `promise` to the first argument of a continuable resolver, turning it into an
// asio handler.
template <typename Promise>
auto resolver_to_handler(Promise&& promise) {
  return [promise = std::forward<Promise>(promise)](asio::error_code e,
                                                    auto&& arg) mutable {
    if (e) {
#if defined(CONTINUABLE_HAS_EXCEPTIONS)
      promise.set_exception(std::make_exception_ptr(e));
#else
      promise.set_exception(cti::exception_t(e.value(), e.category()));
#endif
    } else {
      promise.set_value(std::forward<decltype(arg)>(arg));
    }
  };
}

} // namespace detail

template <typename Signature>
class async_result<use_cti_t, Signature> {
public:
  template <typename Initiation, typename... Args>
  static auto initiate(Initiation initiation, use_cti_t, Args... args) {
    return cti::make_continuable<detail::continuable_result_t<Signature>>(
        [initiation = std::move(initiation),
         init_args =
             std::make_tuple(std::move(args)...)](auto&& promise) mutable {
          cti::detail::traits::unpack(
              [initiation = std::move(initiation),
               handler = detail::resolver_to_handler(
                   std::forward<decltype(promise)>(promise))](
                  auto&&... args) mutable {
                std::move(initiation)(std::move(handler),
                                      std::forward<decltype(args)>(args)...);
              },
              std::move(init_args));
        });
  }
};

} // namespace asio

#endif // !CONTINUABLE_ASIO_ASYNC_RESULT_HPP_INCLUDED
