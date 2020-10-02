// cpp-coroutine-job.cpp : Defines the entry point for the application.
//

#include <iostream>
#include "utils.hpp"
#include "schedule/algorithms.hpp"
#include "schedule/scheduler.hpp"
#include "schedule/task.hpp"

using namespace std;

using namespace co;

constexpr uint kLabCount = 5;
constexpr uint kFactoryCount = 10;

deferred_token<> LabDevelopVaccine(uint index, float chance, single_consumer_counter_event& e)
{
	// printf( "lab %u doing research...\n", index );

	do
	{
		std::this_thread::sleep_for( 1s );
		float v = random::Between01();
		if( v > chance )
		{
			printf( "lab %u did not find a vaccine.... Retry....\n", index );
		} else
		{
			printf( "lab %u found a vaccine!\n", index );
			break;
		}
	} while(!e.IsReady());

	e.decrement();

	co_return;
}


deferred_token<> Factory(std::atomic<uint>& stock, bool& terminationSignal)
{
	while(!terminationSignal)
	{
		std::this_thread::sleep_for( 1s );
		uint vaccine = random::Between( 50, 100 );
		stock += vaccine;
		// printf( "A factory produced %u vaccine\n", vaccine );
	}
	co_return;
}

deferred_token<> TryMakeVaccine()
{

	printf( "Trying to find a vaccine......\n" );

	single_consumer_counter_event counter(1);

	for(uint i = 0; i < kLabCount; i++)
	{
		float chance = random::Between( 0.01f, 0.2f );
		LabDevelopVaccine(i, chance, counter).Launch();
	}
	
	co_await counter;

	// printf( "Found a vaccine!\n");
}

deferred_token<> ProduceVaccine(std::atomic<uint>& vaccineStock, bool& vaccineProductionTermniationSignal)
{
	std::vector<deferred_token<>> factories;
	for(uint i = 0; i < kFactoryCount; i++)
	{
		factories.push_back(Factory( vaccineStock, vaccineProductionTermniationSignal ));
	}
	co_await parallel_for(std::move(factories));
}

deferred_token<> ClinicApplyVaccine(uint& peopleNeedVaccine, std::atomic<uint>& stock, bool& vaccineProductionTermniationSignal)
{
	while(peopleNeedVaccine > 0)
	{
		if( stock == 0 ) continue;
		peopleNeedVaccine--;
		stock--;
	}

	vaccineProductionTermniationSignal = true;
	co_return;
}

auto worldSavedString = R"(

____    __    ____  ______   .______      __       _______          _______    ___   ____    ____  _______  _______  
\   \  /  \  /   / /  __  \  |   _  \    |  |     |       \        /       |  /   \  \   \  /   / |   ____||       \ 
 \   \/    \/   / |  |  |  | |  |_)  |   |  |     |  .--.  |      |   (----` /  ^  \  \   \/   /  |  |__   |  .--.  |
  \            /  |  |  |  | |      /    |  |     |  |  |  |       \   \    /  /_\  \  \      /   |   __|  |  |  |  |
   \    /\    /   |  `--'  | |  |\  \---.|  `----.|  '--'  |   .----)   |  /  _____  \  \    /    |  |____ |  '--'  |
    \__/  \__/     \______/  | _| `.____||_______||_______/    |_______/  /__/     \__\  \__/     |_______||_______/ 
                                                                                                                        
)";
task<> ApplyImmunization(uint healthPeople)
{
	uint i = 0;

	std::atomic<uint> vaccineStock = 0;
	bool vaccineProductionTermniationSignal = false;

	[&]() -> deferred_token<>
	{
		while( !vaccineProductionTermniationSignal )
		{
			std::this_thread::sleep_for( 1s );
			printf( "\n\n============== current status ================\n" );
			printf( "People left: %u\n", healthPeople );
			printf( "vaccine left: %u\n", vaccineStock.load() );
			printf( "==============================================\n\n\n" );
		}

		co_return;
	}().Launch();

	std::vector<deferred_token<>> saveWorldSteps;

	std::vector<deferred_token<>> step2;
	step2.push_back( ProduceVaccine( vaccineStock, vaccineProductionTermniationSignal ) );
	step2.push_back( ClinicApplyVaccine( healthPeople, vaccineStock, vaccineProductionTermniationSignal ) );
	
	saveWorldSteps.push_back(TryMakeVaccine());
	saveWorldSteps.push_back( parallel_for( std::move( step2 ) ) );

	co_await sequential_for( std::move( saveWorldSteps ) );

	printf( worldSavedString );
	
}



int main()
{
	Scheduler::Get();
	uint healthyPeople = 3000;
	
	auto saveWorld = ApplyImmunization( healthyPeople );
	saveWorld.Result();

	printf( "\n\nDone!" );
	scanf( "%d" );
	return 0;
}
