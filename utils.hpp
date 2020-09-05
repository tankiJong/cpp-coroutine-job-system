#pragma once

#define WIN32_LEAN_AND_MEAN
#include "Windows.h"
#include <atomic>
#include <thread>
#include <random>
//////////////////////////////////
///////////// MACROS /////////////
//////////////////////////////////

#define ASSERT_DIE(condition) {if(!(condition)) DebugBreak();}
#define ERROR_DIE(msg) DebugBreak();
#define ENSURES(condition) ASSERT_DIE(condition)
#define EXPECTS(condition) ASSERT_DIE(condition)


//////////////////////////////////
////////////// defs //////////////
//////////////////////////////////

using uint = std::uint32_t;


//////////////////////////////////
/////////// functions ////////////
//////////////////////////////////

inline uint QuerySystemCoreCount()
{
   SYSTEM_INFO info;
   GetSystemInfo( &info );
   return info.dwNumberOfProcessors;
}

inline void SetThreadName( std::thread& thread, const wchar_t* name )
{
   SetThreadDescription( thread.native_handle(), name );
}

namespace random {
inline float Between(float fromInclusive, float toInclusive)
{
   static thread_local std::mt19937 generator;
   std::uniform_real_distribution<> distribution(fromInclusive,toInclusive);
   return distribution(generator);
};
inline float Between01()
{
   return Between(0.f, 1.f);
};
};


//////////////////////////////////
//////////// SysEvent ////////////
//////////////////////////////////

class SysEvent
{
public:
   SysEvent( bool manualReset = false );
   ~SysEvent();
   bool Wait();
   void Trigger();
   void Reset();
   bool IsTriggered() const;
protected:
   void* mHandle;
   bool mManualReset;
   std::atomic<bool> mIsTriggered;
};



inline SysEvent::SysEvent( bool manualReset )
{
   mHandle = CreateEvent( NULL, manualReset, 0, nullptr );
   ASSERT_DIE( mHandle != nullptr );
   mManualReset = manualReset;
}

inline SysEvent::~SysEvent()
{
   if(mHandle != nullptr) {
      CloseHandle( mHandle );
   }
}

inline bool SysEvent::Wait()
{
   ASSERT_DIE( mHandle != nullptr );
   bool success = WaitForSingleObject( mHandle, INFINITE ) == WAIT_OBJECT_0;
   ASSERT_DIE( success );
   mIsTriggered = false;
   return success;
}

inline void SysEvent::Trigger()
{
   ASSERT_DIE( mHandle != nullptr );
   BOOL ret = SetEvent( mHandle );
   ASSERT_DIE( ret != 0 );
   mIsTriggered = ret != 0; 
}
inline void SysEvent::Reset()
{
   ASSERT_DIE( mHandle != nullptr );
   ResetEvent( mHandle );
   mIsTriggered = false;
}

inline bool SysEvent::IsTriggered() const
{
   return mIsTriggered.load();
}


