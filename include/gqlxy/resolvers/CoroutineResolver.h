#pragma once
#include <gqlxy/task.h>
#include <utility>

namespace gqlxy {
class ResolverArgs;

// Custom type-erased callable for coroutine resolvers. std::function cannot be
// used here because GCC 14 evaluates sizeof(F) and is_trivially_copyable<F> in
// constexpr contexts during construction, which fails for coroutine lambda
// closure types (GCC bug — the closure is incomplete at that point).
class CoroutineResolver {
    using CallFn = Task<ValueResolver> (*)(void*, const ResolverArgs&);
    using FreeFn = void (*)(void*);
    using CopyFn = void* (*) (const void*);

    template<typename F>
    static Task<ValueResolver> CallImpl(void* p, const ResolverArgs& args) {
        return (*static_cast<F*>(p))(args);
    }

    template<typename F>
    static void FreeImpl(void* p) {
        delete static_cast<F*>(p);
    }

    template<typename F>
    static void* CopyImpl(const void* p) {
        return new F(*static_cast<const F*>(p));
    }

    void* _data = nullptr;
    CallFn _call = nullptr;
    FreeFn _free = nullptr;
    CopyFn _copy = nullptr;

  public:
    CoroutineResolver() = default;

    template<typename F>
        requires(
            !std::is_same_v<std::remove_cvref_t<F>, CoroutineResolver> &&
            requires(F f, const ResolverArgs& args) {
                requires is_task<std::invoke_result_t<F, const ResolverArgs&>>::value;
            })
    CoroutineResolver(F&& f) {
        using Decay = std::decay_t<F>;
        _data = new Decay(std::forward<F>(f));
        _call = &CallImpl<Decay>;
        _free = &FreeImpl<Decay>;
        _copy = &CopyImpl<Decay>;
    }

    CoroutineResolver(const CoroutineResolver& o) : _call(o._call), _free(o._free), _copy(o._copy) {
        if (o._data) _data = _copy(o._data);
    }

    CoroutineResolver(CoroutineResolver&& o) noexcept : _data(o._data), _call(o._call), _free(o._free), _copy(o._copy) {
        o._data = nullptr;
    }

    CoroutineResolver& operator=(CoroutineResolver o) noexcept {
        std::swap(_data, o._data);
        std::swap(_call, o._call);
        std::swap(_free, o._free);
        std::swap(_copy, o._copy);
        return *this;
    }

    ~CoroutineResolver() {
        if (_data) _free(_data);
    }

    Task<ValueResolver> operator()(const ResolverArgs& args) const {
        if (!_call) throw std::bad_function_call {};
        return _call(_data, args);
    }

    explicit operator bool() const noexcept {
        return _data != nullptr;
    }
};

}