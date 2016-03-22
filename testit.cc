#include <functional.h>

using stdex::signature;

void do_f1(signature<void(char const*)> f)
{
	auto f2 = f;
	f = std::move(f2);
	f("");
}

void do_f1(signature<void(int*)> f)
{
	f(nullptr);
}

enum be_called
{
	no_qs, const_qs, rvref_qs,
};

struct F2
{
	int operator()() &
	{
		return no_qs;
	}

	int operator()() const &
	{
		return const_qs;
	}

	int operator()() &&
	{
		return rvref_qs;
	}

	int operator()() const &&
	{
		return const_qs | rvref_qs;
	}
};

auto do_f2(signature<int()> f)
{
	return f();
}

struct F3
{
	F3 next()
	{
		return { char(c + 1) };
	}

	char c;
};

auto do_f3(signature<F3(F3)> f, F3 obj)
{
	return f(obj);
};

auto do_f3(signature<char(F3)> f, F3 obj)
{
	return f(obj);
};

#include <cassert>

using namespace std::experimental;

int main()
{
	// basics
	static_assert(is_nothrow_constructible_v<signature<int()>, F2>, "");
	static_assert(is_trivially_copyable_v<signature<int()>>, "");

	static_assert(is_nothrow_constructible_v<signature<int()>,
	    std::reference_wrapper<F2>>, "");

	// overloading and void-discarding
	do_f1([](char const*) { return 3; });

	// forwarding
	F2 f2;
	F2 const f2c;

	assert(do_f2(f2) == no_qs);
	assert(do_f2(f2c) == const_qs);
	assert(do_f2(F2()) == rvref_qs);
	assert(do_f2((F2 const)f2) == (const_qs | rvref_qs));

	// more overloading plus INVOKE support
	F3 f3 = { 'a' };

	assert(do_f3(&F3::c, f3) == 'a');
	assert(do_f3(&F3::next, f3).c == 'b');

	// ref-unwrapping
	signature<int()> xf = std::ref(f2);
	assert(xf() == no_qs);
}
