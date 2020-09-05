#include "event.hpp"
#include "scheduler.hpp"

void co::single_consumer_counter_event::Wait()
{
   co::Scheduler::Get().RegisterAsTempWorker( mEvent );
}