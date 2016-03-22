/*-
 * Copyright (c) 2016 Zhihao Yuan.  All rights reserved.
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

#include <experimental/type_traits>
#include <functional>
#include <stdint.h>

namespace stdex
{

using namespace std::experimental;
using std::enable_if_t;

namespace
{

template <typename F, typename = void>
struct _invoke
{
	template <typename... Args>
	static decltype(auto) fn(F&& f, Args&&... args)
	{
		return std::forward<F>(f)(std::forward<Args>(args)...);
	}
};

template <typename F>
struct _invoke<F, enable_if_t<is_member_pointer_v<std::decay_t<F>>>>
{
	template <typename... Args>
	static decltype(auto) fn(F&& f, Args&&... args)
	{
		return std::mem_fn(f)(std::forward<Args>(args)...);
	}
};

template <typename F, typename... Args>
decltype(auto) invoke(F&& f, Args&&... args)
{
	return _invoke<F>::fn(std::forward<F>(f), std::forward<Args>(args)...);
}

}

// from LLVM function_ref
template<typename F> class signature;

template<typename R, typename... Args>
struct signature<R(Args...)>
{
	template
	<
	    typename F,
	    typename Ft = std::remove_reference_t<F>,
	    typename = enable_if_t<not is_same_v<Ft, signature>>,
	    typename Rt = std::result_of_t<F(Args...)>,
	    typename = enable_if_t<is_convertible_v<Rt, R> or is_void_v<R>>
	>
	signature(F&& f) noexcept :
		cb_(callback<F, R>::fn),
		f_(reinterpret_cast<intptr_t>(std::addressof(f)))
	{}

	template <typename F>
	signature(std::reference_wrapper<F> f) noexcept :
		signature(f.get())
	{}

	// not propagating const by design
	R operator()(Args... args)
	{
		return cb_(f_, std::forward<Args>(args)...);
	}

private:
	template <typename F, typename Rt, typename = void>
	struct callback
	{
		static Rt fn(intptr_t f, Args... args)
		{
			return invoke(recover<F>(f),
			    std::forward<Args>(args)...);
		}
	};

	template <typename F, typename Rt>
	struct callback<F, Rt, enable_if_t<is_void_v<Rt>>>
	{
		static Rt fn(intptr_t f, Args... args)
		{
			(void)invoke(recover<F>(f),
			    std::forward<Args>(args)...);
		}
	};

	template <typename F>
	static F&& recover(intptr_t f) noexcept
	{
		using Ft = std::remove_reference_t<F>;
		return std::forward<F>(*reinterpret_cast<Ft*>(f));
	}

	std::add_pointer_t<R(intptr_t, Args...)> cb_;
	intptr_t f_;
};

}

#endif
