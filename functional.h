/*-
 * Copyright (c) 2016, 2017 Zhihao Yuan.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef _STDEX_FUNCTIONAL_H
#define _STDEX_FUNCTIONAL_H

#if defined(_MSC_VER)
#include <type_traits>
#else
#include <experimental/type_traits>
#endif
#include <functional>
#include <stdint.h>

#define STDEX_BODY(...) \
	noexcept(noexcept(__VA_ARGS__)) { return __VA_ARGS__; }

// simulate noexcept-in-type-system
#define FAKE_NOEXCEPT volatile

namespace stdex
{

#if defined(_MSC_VER)
using std::is_member_pointer_v;
using std::is_void_v;
using std::is_convertible_v;
using std::is_base_of_v;
#else
using std::experimental::is_member_pointer_v;
using std::experimental::is_void_v;
using std::experimental::is_convertible_v;
using std::experimental::is_base_of_v;
#endif

using std::enable_if_t;

template <typename F, typename = void>
struct _invoke
{
	template <typename... Args>
	static decltype(auto) fn(F&& f, Args&&... args)
	STDEX_BODY(std::forward<F>(f)(std::forward<Args>(args)...));
};

template <typename F>
struct _invoke<F, enable_if_t<is_member_pointer_v<std::decay_t<F>>>>
{
	template <typename... Args>
	static decltype(auto) fn(F&& f, Args&&... args)
	STDEX_BODY(std::mem_fn(f)(std::forward<Args>(args)...));
};

template <typename F, typename... Args>
decltype(auto) invoke(F&& f, Args&&... args)
STDEX_BODY(_invoke<F>::fn(std::forward<F>(f), std::forward<Args>(args)...));

template <typename R, typename = void>
struct _invoke_r
{
	template <typename F, typename... Args>
	static decltype(auto) fn(F&& f, Args&&... args)
	STDEX_BODY(stdex::invoke(std::forward<F>(f),
	                         std::forward<Args>(args)...));
};

template <typename R>
struct _invoke_r<R, enable_if_t<is_void_v<R>>>
{
	template <typename F, typename... Args>
	static decltype(auto) fn(F&& f, Args&&... args)
	STDEX_BODY((void)stdex::invoke(std::forward<F>(f),
	                               std::forward<Args>(args)...));
};

template
<
    typename R, typename F, typename... Args,
    typename Rt = std::result_of_t<F&&(Args&&...)>,
    typename = enable_if_t<is_convertible_v<Rt, R> || is_void_v<R>>
>
R invoke(F&& f, Args&&... args)
STDEX_BODY(_invoke_r<R>::fn(std::forward<F>(f), std::forward<Args>(args)...));

template <typename, typename R = void, typename = void>
struct is_callable : std::false_type
{
	static constexpr bool nothrow = false;
};

template <typename F, typename... Args, typename R>
struct is_callable
<
    F(Args...), R,
    decltype(void(stdex::invoke<R>(std::declval<F>(),
                                   std::declval<Args>()...)))
>
: std::true_type
{
	static constexpr bool nothrow = noexcept(
	    stdex::invoke<R>(std::declval<F>(), std::declval<Args>()...));
};

template <typename T, typename R = void>
constexpr bool is_callable_v = is_callable<T, R>::value;

template <typename T, typename R = void>
constexpr bool is_nothrow_callable_v = is_callable<T, R>::nothrow;

template <typename T, typename R = void>
struct is_nothrow_callable :
	std::integral_constant<bool, is_nothrow_callable_v<T, R>>
{};

// from LLVM function_ref
template <typename F> struct signature;

template <typename R, typename... Args>
struct signature<R(Args...)>
{
	template
	<
	    typename F,
	    typename Ft = std::remove_reference_t<F>,
	    typename = enable_if_t<!is_base_of_v<signature, Ft>>,
	    typename = enable_if_t<is_callable_v<F(Args...), R>>
	>
	signature(F&& f) noexcept :
		cb_(callback_fn<F>),
		f_(reinterpret_cast<intptr_t>(std::addressof(f)))
	{}

	template
	<
	    typename F,
	    typename = enable_if_t<is_callable_v<F(Args...), R>>
	>
	signature(std::reference_wrapper<F> f) noexcept :
		signature(f.get())
	{}

	// not propagating const by design
	R operator()(Args... args)
	{
		return cb_(f_, std::forward<Args>(args)...);
	}

private:
	template <typename F>
	static F&& recover(intptr_t f) noexcept
	{
		using Ft = std::remove_reference_t<F>;
		return std::forward<F>(*reinterpret_cast<Ft*>(f));
	}

	template <typename F>
	static R callback_fn(intptr_t f, Args... args)
	STDEX_BODY(stdex::invoke<R>(recover<F>(f),
	                            std::forward<Args>(args)...));

	friend signature<R(Args...) FAKE_NOEXCEPT>;

	std::add_pointer_t<R(intptr_t, Args...)> cb_;
	intptr_t f_;
};

template <typename R, typename... Args>
struct signature<R(Args...) FAKE_NOEXCEPT> : signature<R(Args...)>
{
	template
	<
	    typename F,
	    typename Ft = std::remove_reference_t<F>,
	    typename = enable_if_t<!is_base_of_v<signature, Ft>>,
	    typename = enable_if_t<is_nothrow_callable_v<F(Args...), R>>
	>
	signature(F&& f) noexcept :
		signature<R(Args...)>(std::forward<F>(f))
	{}

	R operator()(Args... args) noexcept
	{
		static_assert(
		    noexcept(cb_(this->f_, std::forward<Args>(args)...)),
		    "epic fail");
		return cb_(this->f_, std::forward<Args>(args)...);
	}
};

}

#undef STDEX_BODY

#endif
