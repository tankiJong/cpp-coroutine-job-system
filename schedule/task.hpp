#pragma once
#include "token.hpp"


namespace co
{
//
// `token` is designed as tasks that the user does not care about the result.
// It does not have the overhead to deal with future object
// 
template<bool Deferred, typename T>
class meta_token: public base_token<Deferred, meta_token, T>
{
public:
   using base_t = base_token<Deferred, meta_token, T>;
   using coro_handle_t = typename base_t::coro_handle_t;

   meta_token() = default;
   meta_token( const meta_token& from ) = delete;
   meta_token(coro_handle_t handle): base_t(handle, nullptr) {}
   meta_token(meta_token&& from) noexcept: base_t(std::move(from)) {}

   meta_token& operator=(meta_token&& from)
   {
      std::swap(base_t::mHandle, from.mHandle);
      return *this;
   }
};

template<typename T = void>
using token = meta_token<false, T>;
template<typename T = void>
using deferred_token = meta_token<true, T>;

//
// For `task<T>`, system expects user to call on task<T>::Result() at some moment to perform a block wait
// Notice since users need the result so it makes no sense to instantiate for `void`
//
template<bool Deferred, typename T>
class meta_task: public base_token<Deferred, meta_task, T>
{
   using base_t = base_token<Deferred, meta_task, T>;
   using coro_handle_t = typename base_t::coro_handle_t;
public:
   meta_task() = default;
   meta_task( const meta_task& from ) = delete;
   meta_task(coro_handle_t handle): base_t(handle, &mFuture) {}

   meta_task(meta_task&& from) noexcept: base_t(from)
   {
      auto& promise = base_t::mHandle.promise();
      promise.futuerPtr = &mFuture;
   }

   decltype(auto) Result()
   {
      return mFuture.Get();
   }

   ~meta_task()
   {
      auto& promise = base_t::mHandle.promise();
      promise.futuerPtr = nullptr;
   }
protected:
   future<T> mFuture;
};

template<typename T = void>
using task = meta_task<false, T>;
template<typename T = void>
using deferred_task = meta_task<true, T>;

}
