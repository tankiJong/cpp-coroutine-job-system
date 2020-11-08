#pragma once
#include <atomic>
#include <thread>
#include <vector>
#include <coroutine>

#include "LockQueue.hpp"
#include "../utils.hpp"
using uint = std::uint32_t;

namespace co {
struct Worker
{
   static constexpr uint kMainThread = 0xff;
   uint threadId;
};

using job_id_t = int64_t;

enum class eOpState: uint
{
   UnKnown,
   Created,
   Scheduled,
   Processing,
   Suspended, // pending reschedule
   Done,
   Canceled,
};


/**
 * \brief All coroutine promise types should derive from this
 */
struct promise_base
{
   friend class Scheduler;
   friend struct final_awaitable;

   inline static std::atomic<int> sAllocated = 0;

   enum class ParentScheduleStatus : uint8_t 
   {
      Closed = 0, // It's closed, cannot assign parent to it
      Open, // It can accept parent
      Assigned, // Parent is assigned
   };

   promise_base() noexcept
      : mOwner( nullptr )
    , mState( eOpState::Created )
    , mJobId( sJobID.fetch_add( 1 ) )
    , mHasParent( ParentScheduleStatus::Open )
   {
      // sAllocated++;
   }

   struct final_awaitable
   {
      bool await_ready() { return false; }

      template<typename Promise>
      void await_suspend( std::coroutine_handle<Promise> handle );

      void await_resume() {}
   };

   ~promise_base()
   {
      // in case it is resumed in final_suspend as continuation, it's remain suspended
      // if it's never scheduled, it's in created state
      // sAllocated--;
   }
   void unhandled_exception() { ERROR_DIE( "unhandled exception in promise_base" ); }

   /////////////////////////////////////////////////////
   /////// scheduler related api start from here ///////
   /////////////////////////////////////////////////////

   bool Ready() const { return mState.load( std::memory_order_relaxed ) == eOpState::Done; }

   bool Cancel()
   {
      mState.store( eOpState::Canceled );
      return true;
   }

   bool SetExecutor( Scheduler& scheduler )
   {
      if(mOwner == nullptr) {
         mOwner = &scheduler;
         return true;
      }
      return mOwner == &scheduler;
   }

   bool IsScheduled() const { return mOwner != nullptr;  }
   void MarkWaited() { mAwaiter.fetch_add(1, std::memory_order_release); }
   void UnMarkWaited() { mAwaiter.fetch_sub(1, std::memory_order_release); }
   bool AnyWaited() const { return mAwaiter.load(std::memory_order_acquire) > 0;  }
   int  WaiterCount() const { return mAwaiter.load( std::memory_order_acquire ); }

   template<typename Promise>
   bool SetContinuation(const std::coroutine_handle<Promise>& parent)
   {
      promise_base& parentPromise = parent.promise();

      auto expectedState = eOpState::Processing;
      bool updated = parentPromise.SetState( expectedState, eOpState::Suspended );
      ENSURES( updated || expectedState == eOpState::Suspended);

      mParent = parent;
      mScheduleParent = &ScheduleParentTyped<Promise>;
      // Expect the status is `open`. This means it is safe to resume the parent coroutine.
      // If it's not, that means it is `` already goes through `ScheduleParent`, which is triggered in final_suspend
      ParentScheduleStatus oldStatus = ParentScheduleStatus::Open;
      bool expected = mHasParent.compare_exchange_strong(oldStatus, ParentScheduleStatus::Assigned);
      ENSURES(expected || (!expected && oldStatus == ParentScheduleStatus::Closed));
      return expected;
   }

   bool SetState(eOpState&& expectState, eOpState newState)
   {
      return mState.compare_exchange_strong( expectState, newState, std::memory_order_release);
   }

   bool SetState(eOpState& expectState, eOpState newState)
   {
      return mState.compare_exchange_strong( expectState, newState, std::memory_order_release);
   }

   eOpState State() const { return mState.load( std::memory_order_acquire ); }
   void ScheduleParent()
   {
      auto status = mHasParent.exchange(ParentScheduleStatus::Closed, std::memory_order_acq_rel);
      ENSURES(status == ParentScheduleStatus::Assigned || status == ParentScheduleStatus::Open);
      if(status == ParentScheduleStatus::Assigned) {
         mScheduleParent( *this );
      }
   }

protected:
   Scheduler*            mOwner = nullptr;
   std::atomic<int> mAwaiter = 0;
   std::atomic<eOpState> mState;
   job_id_t mJobId{};
   std::coroutine_handle<> mParent;
   std::atomic<ParentScheduleStatus> mHasParent;
   void(*mScheduleParent)(promise_base&);
   inline static std::atomic<job_id_t> sJobID;

   template<typename Promise>
   static void ScheduleParentTyped( promise_base& self );
};



/**
 * \brief Scheduler, manage workers, enqueue/dispatch jobs
 */
class Scheduler
{
public:


   /**
    * \brief Base type to describe a job
    */
   struct Job
   {
      virtual promise_base* Promise() = 0;

      virtual ~Job()
      {
         if(mShouldRelease) {
            mCoroutine.destroy();
         }

      }
      Job(const std::coroutine_handle<>& handle): mCoroutine( handle ) {}
      // bool RescheduleOp(Scheduler& newScheduler)
      // {
      //    promise_base* promise = Promise();
      //    EXPECTS( promise->mState == eOpState::Suspended );
      //
      //    eOpState expectedState = eOpState::Suspended;
      //    bool updated = promise->mState.compare_exchange_strong( expectedState, eOpState::Scheduled );
      //
      //    // TODO: this might be a time bomb
      //    // this can be potentially troublesome because after setting the state, it's possible we fail to enqueue the job.
      //    // but let's assume it will successful for now
      //    newScheduler.EnqueueJob( this );
      // }

      bool ScheduleOp(Scheduler& newScheduler)
      {
         // TODO: this might be a time bomb
         // this can be potentially troublesome because after setting the state, it's possible we fail to enqueue the job.
         // but let's assume it will successful for now
         newScheduler.EnqueueJob( this );
         return true;
      }

      void Resume()
      {
         promise_base& promise = *Promise();

         if( promise.mState == eOpState::Canceled ) return;
         mCoroutine.resume();
      }

      bool Done() { return mCoroutine.done(); }

      std::coroutine_handle<> mCoroutine;
      bool mShouldRelease = false;
	};

   
   /**
    * \brief templated job type that can access promise
    * \tparam P Promise Type
    */
   template<typename P>
   struct JobT: public Job
   {
      JobT(const std::coroutine_handle<P>& coro)
         : Job( coro )
      {
         coro.promise().MarkWaited();
      }

      promise_base* Promise() override
      {
         return &std::coroutine_handle<P>::from_address( mCoroutine.address() ).promise();
      };

      ~JobT()
      {
         bool shouldRelease = Promise()->WaiterCount() == 1 && mCoroutine.done();
         mShouldRelease = shouldRelease;
         if(!shouldRelease) {
            Promise()->UnMarkWaited();
         }
      }
   };

   static Scheduler& Get();
   ~Scheduler();

   void Shutdown();
   bool IsRunning() const;

   uint GetThreadIndex() const;
   uint GetMainThreadIndex() const;
   bool IsCurrentThreadWorker() const;

   void EnqueueJob(Job* op);

   size_t EstimateFreeWorkerCount() const { return mFreeWorkerCount.load(std::memory_order_relaxed); }

   template<typename Promise>
   Job* AllocateOp(const std::coroutine_handle<Promise>& handle)
   {
      return new JobT<Promise>( handle );
   }

   void ReleaseOp(Job* op)
   {
      delete op;
   }

   template<typename Promise>
   void Schedule( const std::coroutine_handle<Promise>& handle )
   {
      bool assigned = handle.promise().SetExecutor( *this );
      if(assigned) {
         Job* op = AllocateOp( handle );
         op->ScheduleOp( *this );
      }
   }

   void RegisterAsTempWorker( const SysEvent& exitSignal ) { WorkerThreadEntry( exitSignal ); }

protected:

   explicit Scheduler(uint workerCount);

   void WorkerThreadEntry(uint threadIndex);
   void WorkerThreadEntry( const SysEvent& exitSignal );
   Job* FetchNextJob();

   ////////// data ///////////

   uint mWorkerCount = 0;
   std::vector<std::thread> mWorkerThreads;
   std::unique_ptr<Worker[]> mWorkerContexts;
   std::atomic<bool> mIsRunning;
   LockQueue<Job*> mJobs;
   std::atomic_size_t mFreeWorkerCount;
};

template< typename Promise > void promise_base::ScheduleParentTyped( promise_base& self )
{
   auto parent = std::coroutine_handle<Promise>::from_address( self.mParent.address() );
   self.mOwner->Schedule( parent );
}

template< typename Promise > void promise_base::final_awaitable::await_suspend(
   std::coroutine_handle<Promise> handle )
{
   // we expect that should be derived from promise_base
   static_assert(std::is_base_of<promise_base, Promise>::value, "Promise should be derived from promise_base");
   promise_base& promise = handle.promise();
   promise.ScheduleParent();
   promise.SetState( eOpState::Processing, eOpState::Done );
}
}
