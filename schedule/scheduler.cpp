#include "scheduler.hpp"
using namespace co;

static thread_local Worker* gWorkerContext = nullptr;
static thread_local Scheduler* gScheduler = nullptr;
static thread_local bool gIsWorker = false;
static Scheduler* theScheduler = nullptr;
void Scheduler::Shutdown()
{
   mIsRunning.store( false, std::memory_order_relaxed );
}

bool Scheduler::IsRunning() const
{
   return mIsRunning.load(std::memory_order_relaxed);
}

Scheduler& Scheduler::Get()
{
   if(theScheduler == nullptr) {
      theScheduler = new Scheduler(QuerySystemCoreCount());
   }

   return *theScheduler;
}

Scheduler::~Scheduler()
{
   for(auto& workerThread: mWorkerThreads) {
      workerThread.join();
   }
}

uint Scheduler::GetThreadIndex() const
{

   return gWorkerContext ? gWorkerContext->threadId : Worker::kMainThread;
}

uint Scheduler::GetMainThreadIndex() const
{
   return Worker::kMainThread;
}

bool Scheduler::IsCurrentThreadWorker() const
{
   return gIsWorker;
}

Scheduler::Scheduler( uint workerCount )
   : mWorkerCount( workerCount )
{
   ASSERT_DIE( workerCount > 0 );

   gWorkerContext = new Worker{  Worker::kMainThread };
   mWorkerThreads.reserve( workerCount );
   mWorkerContexts = std::make_unique<Worker[]>( workerCount );
   mIsRunning = true;

   mFreeWorkerCount = workerCount;
   for(uint i = 0; i < workerCount; ++i) {
      mWorkerThreads.emplace_back( [this, i] { WorkerThreadEntry( i ); } );
   }
}


void Scheduler::WorkerThreadEntry( uint threadIndex )
{
   wchar_t name[100];
   swprintf_s( name, 100, L"co worker thread %u", threadIndex );
   SetThreadName( mWorkerThreads[threadIndex], name );

   auto& context = mWorkerContexts[threadIndex];
   context.threadId = threadIndex;
   gWorkerContext = &context;
   gScheduler = this;
   gIsWorker = true;
   while(true) {
      Job* op = FetchNextJob();
      if(op == nullptr) {
         std::this_thread::yield();
      } else {
         mFreeWorkerCount--;

         op->Resume();

         // whatever the state the op is, release the op, the ownership of the coroutine is either finished, or transfered to somewhere else.
         ReleaseOp( op );

         mFreeWorkerCount++;
      }

      if( !IsRunning() ) break;
   }
   gIsWorker = false;
}

void Scheduler::WorkerThreadEntry( const SysEvent& exitSignal )
{
   // this path only will run when it's blocked by something,
   // so instead, it will try to run something else at the same time.
   // In that sense, we need to first register itself as a free worker
   mFreeWorkerCount++;
   gIsWorker = true;

   while( true ) {
      Job* op = FetchNextJob();
      if( op == nullptr ) {
         std::this_thread::yield();
      }
      else {
         mFreeWorkerCount--;

         {
            op->Resume();

            // op could be either suspended or done, if it's done, we will also release the coroutine frame
            ReleaseOp( op );
         }

         mFreeWorkerCount++;
      }

      if( exitSignal.IsTriggered() ) break;
   }

   mFreeWorkerCount--;
   gIsWorker = false;

}

Scheduler::Job* Scheduler::FetchNextJob()
{
   Job* op = nullptr;
   mJobs.Dequeue( op );
   return op;
}

void Scheduler::EnqueueJob( Job* op )
{
   mJobs.Enqueue( op );
}
