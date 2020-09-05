#pragma once
#include <atomic>
#include <experimental/coroutine>
#include "scheduler.hpp"
#include "event.hpp"
namespace co
{

// simple future type that is implemented based on OS wait object
template<typename T>
class future
{
public:
   future(): mSetEvent( 1 ) { }
   void Set( T&& v )
   {
      EXPECTS( !IsReady() );
      value = v;
      mSetEvent.decrement();
   }

   bool IsReady() { return mSetEvent.IsReady(); }

   const T& Get() const
   {
      mSetEvent.Wait();
      return value;
   }


protected:
   T value = {};
   mutable single_consumer_counter_event mSetEvent;
};

template<>
class future<void>
{
   public:
   future(): mSetEvent( 1 ) { }
   void Set()
   {
      EXPECTS( !IsReady() );
      mSetEvent.decrement();
   }

   bool IsReady() { return mSetEvent.IsReady(); }

   void Get() const
   {
      mSetEvent.Wait();
   }


protected:
   mutable single_consumer_counter_event mSetEvent;
};
}
