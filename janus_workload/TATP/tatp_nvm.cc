/*
 Author: Vaibhav Gogte <vgogte@umich.edu>
         Aasheesh Kolli <akolli@umich.edu>


This file is the TATP benchmark, performs various transactions as per the specifications.
*/

#include "tatp_db.h"
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <iostream>
#include <cstdint>
#include <assert.h>
#include <sys/time.h>
#include <string>
#include <fstream>
#include "../common/common.h"
//#include "/home/smahar/git/transparent_txopt/helper.h"
#include "../m5ops.h"

//Korakit
//might need to change parameters
#define NUM_SUBSCRIBERS 1000	//100000
#define NUM_OPS_PER_CS 2 
#define NUM_OPS 3000	//10000000
#define NUM_THREADS 1 

TATP_DB* my_tatp_db;
//#include "../DCT/rdtsc.h"

void init_db() {
  unsigned num_subscribers = NUM_SUBSCRIBERS;
  // my_tatp_db = (TATP_DB *)aligned_malloc(64, sizeof(TATP_DB));
  my_tatp_db = new TATP_DB(num_subscribers);
  my_tatp_db->initialize(num_subscribers,NUM_THREADS);
  fprintf(stderr, "Created tatp db at %p\n", (void *)my_tatp_db);
}


void* update_locations(void* args, int coreid) {
  //  TraceBegin();
#ifdef GEM5
  m5_work_begin(coreid,0);
#endif
  int thread_id = *((int*)args);
  for(int i=0; i<NUM_OPS/NUM_THREADS; i++) {
    long subId = my_tatp_db->get_sub_id();
    uint64_t vlr = my_tatp_db->get_random_vlr(0);
    my_tatp_db->backup_location(thread_id, subId);
    my_tatp_db->update_location(subId, vlr);
    my_tatp_db->discard_backup(thread_id, subId);
  }
#ifdef GEM5
  m5_work_end(coreid,0);
#endif
  //  TraceEnd();
  return NULL;
}

int main(int argc, char* argv[]) {

  //printf("in main\n");

  //struct timeval tv_start;
  //struct timeval tv_end;
  //std::ofstream fexec;
  //fexec.open("exec.csv",std::ios_base::app);
  // Korakit: move to the init
  // LIU
  

  init_db();

  // LIU: remove output
  //std::cout<<"done with initialization"<<std::endl;

  my_tatp_db->populate_tables(NUM_SUBSCRIBERS);
  // LIU: remove output
  //std::cout<<"done with populating tables"<<std::endl;

  pthread_t threads[NUM_THREADS];
  int id[NUM_THREADS];
  srand(0);
  //Korakit
  //exit to count instructions after initialization
  //we use memory trace from the beginning to this to test the compression ratio
  //as update locations(the actual test) only do one update

  // LIU
  // gettimeofday(&tv_start, NULL);
  std::cout << "Done with init" << std::endl;
  // abort();

  for(int i=0; i<NUM_THREADS; i++) {
    id[i] = i;
    update_locations((void*)&id[i], atoi(argv[1]));
  }

//Korakit
//Not necessary for single threaded version
/*
  for(int i=0; i<NUM_THREADS; i++) {
    pthread_join(threads[i], NULL);
  }
*/

  // LIU: remove the output  
/*
  gettimeofday(&tv_end, NULL);
  fprintf(stderr, "time elapsed %ld us\n",
          tv_end.tv_usec - tv_start.tv_usec +
              (tv_end.tv_sec - tv_start.tv_sec) * 1000000);



  fexec << "TATP" << ", " << std::to_string((tv_end.tv_usec - tv_start.tv_usec) + (tv_end.tv_sec - tv_start.tv_sec) * 1000000) << std::endl;


  fexec.close();
  free(my_tatp_db);

  std::cout<<"done with threads"<<std::endl;
*/

  return 0;
}
