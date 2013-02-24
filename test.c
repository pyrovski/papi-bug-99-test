#define _GNU_SOURCE
#include <sched.h>

#include <assert.h>
#include <papi.h>
#include <pthread.h>

#include <stdio.h>
#include <stdlib.h>
#include <syscall.h> //needed for getTid
#include <sys/types.h>
#include <unistd.h>

typedef struct myinfo
{

  int volatile * volatile killSig; /* @brief address of kill signal that we spin on*/
  pthread_t join_id;
  PAPI_thread_id_t parent; /* @brief tid used by papi to access the counters*/

} myInfo;

static inline pid_t getTid (void)
{

  pid_t answer = syscall (__NR_gettid);
  return answer;

}

void initPapiHelper ( int * EventSet, myInfo * handle)
{

  int codes[1] = {0};
  int ret_code;
  printf("Initing Papi counters\n");

  if (PAPI_event_name_to_code ("UNHALTED_CORE_CYCLES", &codes[0]) != PAPI_OK)
    {

      fprintf (stderr,"PAPI even_name_to_code failed!\n");
      exit (1);

    }

  /* Check to see if the PAPI natives are available */
  if ((PAPI_query_event (codes[0]) != PAPI_OK))
    {

      fprintf (stderr,"PAPI counters aren't sufficient to measure boundedness!\n");
      exit (1);

    }

  ret_code=PAPI_create_eventset ( EventSet );
  if(PAPI_OK != ret_code)
    {

      fprintf (stderr,"Creating the PAPI create eventset failed:%d %s\n",ret_code, PAPI_strerror(ret_code));
      exit (1);

    }

  ret_code=PAPI_add_events (*EventSet,codes, 1);
  if(PAPI_OK != ret_code)
    {

      fprintf (stderr,"Adding the PAPI add eventset failed: %d %s\n",ret_code, PAPI_strerror(ret_code));
      exit (1);

    }

  // This ensures that papi counters only reads the parent thread counters
  ret_code=PAPI_attach (*EventSet, handle->parent);
  if(PAPI_OK != ret_code)
    {

      fprintf (stderr,"Attaching the PAPI eventset failed: %d %s\n",ret_code, PAPI_strerror(ret_code));
      exit (1);

    }


  return;

}

void * profilerThread (void * ContextPtr)
{

  /*Contexts and signal variables*/
  volatile int killSignal=0; //kill signal used to have destructor function from other thread kill this thread
  myInfo * temp = ContextPtr; // handle to data needed from other thread
  myInfo * myHandle= temp;

  /*Papi related variables*/
  int EventSet = PAPI_NULL;
  long_long values[1]= {0};
  int ret_code=0;

  //call the papi helper to init stuff
  initPapiHelper ( &EventSet, myHandle);

  // we send back out kill signal address to the other thread to let it continue running
  myHandle->killSig=&killSignal;

  ret_code=PAPI_start (EventSet);
  if(PAPI_OK != ret_code)
    {

      printf (" PAPI start failed: %s\n",PAPI_strerror(ret_code));

    }

  while (killSignal==0)
    {


      // get PAPI info from counters
      if(PAPI_OK != PAPI_accum (EventSet,values))
        {

	  printf (" PAPI accum failed: %s\n",PAPI_strerror(ret_code));

        }
      if(values[0])
	printf ("Total Cycles is %lld\n",values[0]);
      fflush (stdout);

      //reset my counter
      values[0]=0;

      /*Uncomment/comment to expose bug/

        usleep (1);

      */

    }
  return NULL; //gets rid of a warning

}

void pinCore()
{

  cpu_set_t cpuset;


  int cpuDest = 0; //right now we'll just do core 0

  /* Set affinity mask to include CPU 0 */
  CPU_ZERO(&cpuset);
  CPU_SET(cpuDest, &cpuset);


  pid_t me = getpid ();
  int s =sched_setaffinity (me, sizeof(cpu_set_t), &cpuset);
  if (s != 0)
    {

      printf("Sched_setaffinity signaled a failure to pin to core 0\n");
      fflush(stdout);

    }

}


int main ()
{

  myInfo * handle;
  //    /*Uncomment/comment to expose bug/
  pinCore();
  printf("Start now!\n");
  fflush(stdout);
  //we initialize papi
  int retval=PAPI_library_init (PAPI_VER_CURRENT);
  if (retval != PAPI_VER_CURRENT)
    {

      printf ("PAPI library init error!\n");
      exit (1);

    }

  //register the thread specifier

  if (PAPI_thread_init (getTid) != PAPI_OK)

    {

      printf ("Thread init function didn't register properly\n");

    }

  //allocate our context on the heap, check that we succeeded, and initialize it
  handle= malloc (sizeof ( * handle));
  assert ( handle != NULL );
  handle->killSig=NULL;
  handle->parent=PAPI_thread_id ();
    
    
  assert(pthread_create (&(handle->join_id),NULL,profilerThread,handle)==0);

  //    /Uncomment/comment to expose bug*/
  //pinCore();

  //spin waiting for my child to init
  while (handle->killSig==NULL){}
  while (1)
    {


    }
  return 0;

}

