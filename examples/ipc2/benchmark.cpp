#define SIMPPL_HAVE_VALIDATION
#define SIMPPL_HAVE_BOOST_FUSION

#include "simppl/stub.h"
#include "simppl/skeleton.h"
#include "simppl/dispatcher.h"
#include "simppl/interface.h"

#include <boost/fusion/include/define_struct.hpp>


using namespace std::placeholders;


static
unsigned int current_time_ms()
{
   struct timespec current_time ;
   clock_gettime(CLOCK_REALTIME, &current_time);

   return (current_time.tv_sec * 1000) + (current_time.tv_nsec / 1000000) ;
}


BOOST_FUSION_DEFINE_STRUCT(
   (), PODStruct,
   (int32_t, anInt32)
   (double, aDouble)
   (double, anotherDouble)
   (std::string, aString)
)


typedef std::vector<PODStruct> PODStructVector;



INTERFACE(DSIBenchmark)
{
   Request<> shutdown;
   
   Request<> callNoArg;
   Request<PODStruct> callStruct;
   Request<PODStructVector> callStructVector;
   
   Signal<> rCallNoArg;   // must be modeled like this here
   Response<PODStruct> rCallStruct;
   Response<PODStructVector> rCallStructVector;
   
   inline
   DSIBenchmark()
    : INIT_REQUEST(shutdown)
    , INIT_REQUEST(callNoArg)
    , INIT_REQUEST(callStruct)
    , INIT_REQUEST(callStructVector)
    , INIT_SIGNAL(rCallNoArg)
    , INIT_RESPONSE(rCallStruct)
    , INIT_RESPONSE(rCallStructVector)
   {
      //callNoArg >> rCallNoArg; not possible since the response is a signal
      callStruct >> rCallStruct;
      callStructVector >> rCallStructVector;
   }
};


struct Client : simppl::ipc::Stub<DSIBenchmark>
{
   Client(int calls = 1)   
    : simppl::ipc::Stub<DSIBenchmark>("bench", "unix:DSIBenchmark")    
    , mCalls(calls)
    , mCounter(0u)
    , mFactor(10u)
    , mStartTime(0u)
   {
      rCallNoArg >> std::bind(&Client::handleCallNoArg, this);
      rCallStruct >> std::bind(&Client::handleCallStruct, this, _1, _2);
      rCallStructVector >> std::bind(&Client::handleCallStructVector, this, _1, _2);
   
      mPodStruct.anInt32 = 1;
      mPodStruct.aDouble = 2;
      mPodStruct.anotherDouble = 3;
      mPodStruct.aString = "01234567890123456789";   
      
      connected >> std::bind(&Client::handleConnected, this);
   }
   
   void handleConnected()
   {
      rCallNoArg.attach();     
      
      std::cout << "Proxy connected to server" << std::endl;
      std::cout << "No arg: " << std::flush;
      
      mStartTime = current_time_ms();     
      callNoArg();
   }
   
   void handleCallNoArg()
   {
      if (mCounter++ < mCalls)
      {
         callNoArg();
      }
      else
      {
         snapshot();
         std::cout << "1 struct: " << std::flush;

         mCounter = 0u;
         mStartTime = current_time_ms();
         callStruct(mPodStruct);
      }
   }
   
   void handleCallStruct(const simppl::ipc::CallState& state, const PODStruct& s)
   {
      if (mCounter++ < mCalls)
      {
         callStruct(mPodStruct);
      }
      else
      {
         snapshot();
         std::cout << mFactor << " x struct: " << std::flush;
         while (mPodStructVector.size() < mFactor)
         {
            mPodStructVector.push_back(mPodStruct);
         }

         mCounter = 0u;
         mStartTime = current_time_ms();
         callStructVector(mPodStructVector);
      }
   }
   
   void handleCallStructVector(const simppl::ipc::CallState& state, const PODStructVector& v)
   {
      if (mCounter++ < mCalls)
      {
         callStructVector(mPodStructVector);
      }
      else
      {
         snapshot();
         if (mFactor < 1000u)
         {
            mFactor *= 10u;
            std::cout << mFactor << " x struct: " << std::flush;
            while (mPodStructVector.size() < mFactor)
            {
               mPodStructVector.push_back(mPodStruct);
            }
            mCounter = 0u;
            mStartTime = current_time_ms();
            callStructVector(mPodStructVector);
         }
         else
         {
            shutdown();         
            exit(1);
         }
      }
   }
   
   void snapshot()
   {
      const unsigned int totalTime = current_time_ms() - mStartTime;

      // a ping-pong loop has 2 messages
      std::cout << mCalls << " calls in " << totalTime << " ms, " <<(mCalls * 1000 * 2) / totalTime << " msgs/s"
                << std::endl;
   }
   
   unsigned int mCalls;
   unsigned int mCounter;
   unsigned int mFactor;
   unsigned int mStartTime;

   PODStruct mPodStruct;
   PODStructVector mPodStructVector;
};


struct Server : simppl::ipc::Skeleton<DSIBenchmark>
{
   Server()
    : simppl::ipc::Skeleton<DSIBenchmark>("bench")
   {
      shutdown >> std::bind(&Server::handleShutdown, this);
      callNoArg >> std::bind(&Server::handleNoArg, this);
      callStruct >> std::bind(&Server::handleStruct, this, _1);
      callStructVector >> std::bind(&Server::handleStructVector, this, _1);
   }
   
   void handleShutdown()
   {
      exit(0);
   }
   
   void handleNoArg()
   {
      rCallNoArg.emit();
   }
   
   void handleStruct(const PODStruct& s)
   {
      respondWith(rCallStruct(s));
   }
   
   void handleStructVector(const PODStructVector& v)
   {
      respondWith(rCallStructVector(v));
   }
};


#define PING_COUNT 100


int main(int argc, char** argv)
{   
   int calls = PING_COUNT;    // take this as default count of calls
   if (argc >= 2)
   {      
      if (!strncmp(argv[1], "--pings=", 8))
      {
         calls = atoi(argv[1] + 8);
         assert(calls > 0);
         
         simppl::ipc::Dispatcher d;
         Client c(calls);
         d.addClient(c);
         d.run();
         
         return EXIT_SUCCESS;
      }
      
      if (!strcmp(argv[1], "--server"))
      {
         simppl::ipc::Dispatcher d("unix:DSIBenchmark");
         Server s;
         d.addServer(s);
         d.run();
         
         return EXIT_SUCCESS;
      }
   }
   
   std::cout << "Invalid command line options: either '--server' or '--pings=<x>'" << std::endl;
   return EXIT_FAILURE;
}

