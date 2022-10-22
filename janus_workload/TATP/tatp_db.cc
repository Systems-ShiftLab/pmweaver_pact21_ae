/*
 Author: Vaibhav Gogte <vgogte@umich.edu>
         Aasheesh Kolli <akolli@umich.edu>

This file defines the various transactions in TATP.
*/

#include "tatp_db.h"
#include <cstdlib> // For rand
#include <iostream>
#include <string.h>
//#include <queue>
//#include <iostream>
#include "../common/common.h"
#define NUM_RNDM_SEEDS 1280

subscriber_entry *subscriber_table_entry_backup;
uint64_t *subscriber_table_entry_backup_valid;

int getRand() {
  return rand();
}

TATP_DB::TATP_DB(unsigned num_subscribers) {}
//Korakit
//this function is used in setup phase, no need to provide crash consistency
void TATP_DB::initialize(unsigned num_subscribers, int n) {
  std::cout << "total_subscribers = " << total_subscribers << " num_sub: " << num_subscribers << std::endl;
  total_subscribers = num_subscribers;
  num_threads = n;

  size_t stsz = num_subscribers*sizeof(subscriber_entry);
  size_t aitsz = 4*num_subscribers*sizeof(access_info_entry);
  size_t sftsz = 4*num_subscribers*sizeof(special_facility_entry);
  size_t cftsz = 3*4*num_subscribers*sizeof(call_forwarding_entry);
  size_t basz = n*sizeof(subscriber_entry); // backup array size
  size_t vasz = n*sizeof(uint64_t); // valid array size
  void* pool = aligned_malloc(64, stsz + aitsz + sftsz + cftsz + basz + vasz);
  subscriber_table = (subscriber_entry*)(pool);

  // A max of 4 access info entries per subscriber
  access_info_table = (access_info_entry*)(((size_t)pool) + stsz);

  // A max of 4 access info entries per subscriber
  special_facility_table = (special_facility_entry*) (((size_t)pool) + stsz + aitsz);

  // A max of 3 call forwarding entries per "special facility entry"
  call_forwarding_table= (call_forwarding_entry*) (((size_t)pool) + stsz + aitsz + sftsz);
  std::cout << "Table initialized at " << (void*)call_forwarding_table << std::endl;

  subscriber_table_entry_backup = (subscriber_entry*) (((size_t)pool) + stsz + aitsz + sftsz + cftsz);
  subscriber_table_entry_backup_valid = (uint64_t*) (((size_t)pool) + stsz + aitsz + sftsz + cftsz + basz);
  //Korakit
  //removed for single thread version
  /*
  lock_ = (pthread_mutex_t *)malloc(n*sizeof(pthread_mutex_t));

  for(int i=0; i<num_threads; i++) {
    pthread_mutex_init(&lock_[i], NULL);
  }
  */

  for(int i=0; i<4*num_subscribers; i++) {
    access_info_table[i].valid = false;
    special_facility_table[i].valid = false;
    for(int j=0; j<3; j++) {
      call_forwarding_table[3*i+j].valid = false;
    }
  }

  //rndm_seeds = new std::atomic<unsigned long>[NUM_RNDM_SEEDS];
  //rndm_seeds = (std::atomic<unsigned long>*) malloc(NUM_RNDM_SEEDS*sizeof(std::atomic<unsigned long>));
  subscriber_rndm_seeds = (unsigned long*) malloc(NUM_RNDM_SEEDS*sizeof(unsigned long));
  vlr_rndm_seeds = (unsigned long*) malloc(NUM_RNDM_SEEDS*sizeof(unsigned long));
  rndm_seeds = (unsigned long*) malloc(NUM_RNDM_SEEDS*sizeof(unsigned long));
  //sgetRand();
  for(int i=0; i<NUM_RNDM_SEEDS; i++) {
    subscriber_rndm_seeds[i] = getRand()%(NUM_RNDM_SEEDS*10)+1;
    vlr_rndm_seeds[i] = getRand()%(NUM_RNDM_SEEDS*10)+1;
    rndm_seeds[i] = getRand()%(NUM_RNDM_SEEDS*10)+1;
    //std::cout<<i<<" "<<rndm_seeds[i]<<std::endl;
  }
}

TATP_DB::~TATP_DB(){

  free(subscriber_rndm_seeds);
  free(vlr_rndm_seeds);
  free(rndm_seeds);
}
//Korakit
//this function is used in setup phase, no need to provide crash consistency
void TATP_DB::populate_tables(unsigned num_subscribers) {
  for(int i=0; i<num_subscribers; i++) {
    fill_subscriber_entry(i);
    int num_ai_types = getRand()%4 + 1; // num_ai_types varies from 1->4
    for(int j=1; j<=num_ai_types; j++) {
      fill_access_info_entry(i,j);
    }
    int num_sf_types = getRand()%4 + 1; // num_sf_types varies from 1->4
    for(int k=1; k<=num_sf_types; k++) {
      fill_special_facility_entry(i,k);
      int num_call_forwards = getRand()%4; // num_call_forwards varies from 0->3
      for(int p=0; p<num_call_forwards; p++) {
        fill_call_forwarding_entry(i,k,8*p);
      }
    }
  }
}
//Korakit
//this function is used in setup phase, no need to provide crash consistency
void TATP_DB::fill_subscriber_entry(unsigned _s_id) {
  subscriber_table[_s_id].s_id = _s_id;
  convert_to_string(_s_id, 15, subscriber_table[_s_id].sub_nbr);

  subscriber_table[_s_id].bit_1 = (short) (getRand()%2);
  subscriber_table[_s_id].bit_2 = (short) (getRand()%2);
  subscriber_table[_s_id].bit_3 = (short) (getRand()%2);
  subscriber_table[_s_id].bit_4 = (short) (getRand()%2);
  subscriber_table[_s_id].bit_5 = (short) (getRand()%2);
  subscriber_table[_s_id].bit_6 = (short) (getRand()%2);
  subscriber_table[_s_id].bit_7 = (short) (getRand()%2);
  subscriber_table[_s_id].bit_8 = (short) (getRand()%2);
  subscriber_table[_s_id].bit_9 = (short) (getRand()%2);
  subscriber_table[_s_id].bit_10 = (short) (getRand()%2);

  subscriber_table[_s_id].hex_1 = (short) (getRand()%16);
  subscriber_table[_s_id].hex_2 = (short) (getRand()%16);
  subscriber_table[_s_id].hex_3 = (short) (getRand()%16);
  subscriber_table[_s_id].hex_4 = (short) (getRand()%16);
  subscriber_table[_s_id].hex_5 = (short) (getRand()%16);
  subscriber_table[_s_id].hex_6 = (short) (getRand()%16);
  subscriber_table[_s_id].hex_7 = (short) (getRand()%16);
  subscriber_table[_s_id].hex_8 = (short) (getRand()%16);
  subscriber_table[_s_id].hex_9 = (short) (getRand()%16);
  subscriber_table[_s_id].hex_10 = (short) (getRand()%16);

  subscriber_table[_s_id].byte2_1 = (short) (getRand()%256);
  subscriber_table[_s_id].byte2_2 = (short) (getRand()%256);
  subscriber_table[_s_id].byte2_3 = (short) (getRand()%256);
  subscriber_table[_s_id].byte2_4 = (short) (getRand()%256);
  subscriber_table[_s_id].byte2_5 = (short) (getRand()%256);
  subscriber_table[_s_id].byte2_6 = (short) (getRand()%256);
  subscriber_table[_s_id].byte2_7 = (short) (getRand()%256);
  subscriber_table[_s_id].byte2_8 = (short) (getRand()%256);
  subscriber_table[_s_id].byte2_9 = (short) (getRand()%256);
  subscriber_table[_s_id].byte2_10 = (short) (getRand()%256);

  subscriber_table[_s_id].msc_location = getRand()%(2^32 - 1) + 1;
  subscriber_table[_s_id].vlr_location = getRand()%(2^32 - 1) + 1;
}
//Korakit
//this function is used in setup phase, no need to provide crash consistency
void TATP_DB::fill_access_info_entry(unsigned _s_id, short _ai_type) {

  int tab_indx = 4*_s_id + _ai_type - 1;

  access_info_table[tab_indx].s_id = _s_id;
  access_info_table[tab_indx].ai_type = _ai_type;

  access_info_table[tab_indx].data_1 = getRand()%256;
  access_info_table[tab_indx].data_2 = getRand()%256;

  make_upper_case_string(access_info_table[tab_indx].data_3, 3);
  make_upper_case_string(access_info_table[tab_indx].data_4, 5);

  access_info_table[tab_indx].valid = true;
}
//Korakit
//this function is used in setup phase, no need to provide crash consistency
void TATP_DB::fill_special_facility_entry(unsigned _s_id, short _sf_type) {

  int tab_indx = 4*_s_id + _sf_type - 1;

  special_facility_table[tab_indx].s_id = _s_id;
  special_facility_table[tab_indx].sf_type = _sf_type;

  special_facility_table[tab_indx].is_active = ((getRand()%100 < 85) ? 1 : 0);

  special_facility_table[tab_indx].error_cntrl = getRand()%256;
  special_facility_table[tab_indx].data_a = getRand()%256;

  make_upper_case_string(special_facility_table[tab_indx].data_b, 5);
  special_facility_table[tab_indx].valid = true;
}
//Korakit
//this function is used in setup phase, no need to provide crash consistency
void TATP_DB::fill_call_forwarding_entry(unsigned _s_id, short _sf_type, short _start_time) {
  if(_start_time == 0)
    return;

  int tab_indx = 4*_s_id + 3*(_sf_type-1) + (_start_time-8)/8;

  call_forwarding_table[tab_indx].s_id = _s_id;
  call_forwarding_table[tab_indx].sf_type = _sf_type;
  call_forwarding_table[tab_indx].start_time = _start_time - 8;

  call_forwarding_table[tab_indx].end_time = (_start_time - 8) + getRand()%8 + 1;

  convert_to_string(getRand()%1000, 15, call_forwarding_table[tab_indx].numberx);
}

void TATP_DB::convert_to_string(unsigned number, int num_digits, char* string_ptr) {
  
  char digits[10] = {'0','1','2','3','4','5','6','7','8','9'};
  int quotient = number;
  int reminder = 0;
  int num_digits_converted=0;
  int divider = 1;
  while((quotient != 0) && (num_digits_converted<num_digits)) {
    divider = 10^(num_digits_converted+1);
    reminder = quotient%divider; quotient = quotient/divider;
    string_ptr[num_digits-1 - num_digits_converted] = digits[reminder];
    num_digits_converted++;
  }
  if(num_digits_converted < num_digits) {
    string_ptr[num_digits-1 - num_digits_converted] = digits[0];
    num_digits_converted++;
  }
  return;
}

void TATP_DB::make_upper_case_string(char* string_ptr, int num_chars) {
  char alphabets[26] = {'A','B','C','D','E','F','G','H','I','J','K','L','M','N',
                                'O','P','Q','R','S','T','U','V','W','X','Y','Z'};
  for(int i=0; i<num_chars; i++) {
    string_ptr[i] = alphabets[getRand()%26];
  }
  return;
}

void TATP_DB::update_subscriber_data(int threadId) {
  unsigned rndm_s_id = getRand() % total_subscribers;
  short rndm_sf_type = getRand() % 4 + 1;
  unsigned special_facility_tab_indx = 4*rndm_s_id + rndm_sf_type -1;

  if(special_facility_table[special_facility_tab_indx].valid) {

//FIXME: There is a potential data race here, do not use this function yet
  //Korakit
  //removed for single thread version
  //pthread_mutex_lock(&lock_[rndm_s_id]);
    subscriber_table[rndm_s_id].bit_1 = getRand()%2;
    special_facility_table[special_facility_tab_indx].data_a = getRand()%256;
  //Korakit
  //removed for single thread version
  //pthread_mutex_unlock(&lock_[rndm_s_id]);

  }
  return;
}

long TATP_DB::get_sub_id() {
    return ((long)get_random_s_id(0)) % total_subscribers;
}

void TATP_DB::backup_location(int thread_id, long subId) {
    /* Backup the location */
    memcpy(&subscriber_table_entry_backup[thread_id], &subscriber_table[subId], sizeof(subscriber_entry));
    flush_caches(&subscriber_table_entry_backup[thread_id], sizeof(subscriber_entry));
    s_fence();

    /* Set the valid bit to 1 */
    subscriber_table_entry_backup_valid[thread_id] = 1;
    flush_caches(&subscriber_table_entry_backup_valid[thread_id], sizeof(uint64_t));
    s_fence();

    return;
}

void TATP_DB::discard_backup(int thread_id, long subId) {
    subscriber_table_entry_backup_valid[thread_id] = 0;
    flush_caches(&subscriber_table_entry_backup_valid[thread_id], sizeof(uint64_t));
    s_fence();
}

void TATP_DB::update_location(long subId, uint64_t vlr) {
    int threadId = 0;
    subscriber_table[subId].vlr_location = vlr;
    flush_caches(&subscriber_table[subId], sizeof(subscriber_table[subId]));
    s_fence();

    return;
}

void TATP_DB::insert_call_forwarding(int threadId) {
  return;
}

void TATP_DB::delete_call_forwarding(int threadId) {
  return;
}

void TATP_DB::print_results() {
  //std::cout<<"TxType:0 successful txs = "<<txCounts[0][0]<<std::endl;
  //std::cout<<"TxType:0 failed txs = "<<txCounts[0][1]<<std::endl;
  //std::cout<<"TxType:1 successful txs = "<<txCounts[1][0]<<std::endl;
  //std::cout<<"TxType:1 failed txs = "<<txCounts[1][1]<<std::endl;
  //std::cout<<"TxType:2 successful txs = "<<txCounts[2][0]<<std::endl;
  //std::cout<<"TxType:2 failed txs = "<<txCounts[2][1]<<std::endl;
  //std::cout<<"TxType:3 successful txs = "<<txCounts[3][0]<<std::endl;
  //std::cout<<"TxType:3 failed txs = "<<txCounts[3][1]<<std::endl;
}

unsigned long TATP_DB::get_random(int thread_id) {
  //return (getRand()%65536 | min + getRand()%(max - min + 1)) % (max - min + 1) + min;
  unsigned long tmp;
  tmp = rndm_seeds[thread_id*10] = (rndm_seeds[thread_id*10] * 16807) % 2147483647;
  return tmp;
}

unsigned long TATP_DB::get_random(int thread_id, int min, int max) {
  //return (getRand()%65536 | min + getRand()%(max - min + 1)) % (max - min + 1) + min;
  unsigned long tmp;
  tmp = rndm_seeds[thread_id*10] = (rndm_seeds[thread_id*10] * 16807) % 2147483647;
  return (min+tmp%(max-min+1));
}

unsigned long TATP_DB::get_random_s_id(int thread_id) {
  unsigned long tmp;
  tmp = subscriber_rndm_seeds[thread_id*10] = (subscriber_rndm_seeds[thread_id*10] * 16807) % 2147483647;
  return (1 + tmp%(total_subscribers));
}

unsigned long TATP_DB::get_random_vlr(int thread_id) {
  unsigned long tmp;
  tmp = vlr_rndm_seeds[thread_id*10] = (vlr_rndm_seeds[thread_id*10] * 16807)%2147483647;
  return (1 + tmp%(2^32));
}

