/*
 Author: Vaibhav Gogte <vgogte@umich.edu>
         Aasheesh Kolli <akolli@umich.edu>


This file declares the TATP data base and the different transactions supported by
the database.
*/

#include <cstdint>
//#include <atomic>
#include <pthread.h>

#include "tableEntries.h"

class TATP_DB{

  private:
    long total_subscribers; // Holds the number of subscribers
    int num_threads;
    subscriber_entry* subscriber_table; // Pointer to the subscriber table
    access_info_entry* access_info_table; // Pointer to the access info table
    special_facility_entry* special_facility_table; // Pointer to the special facility table
    call_forwarding_entry* call_forwarding_table; // Pointer to the call forwarding table
    pthread_mutex_t* lock_; // Lock per subscriber to protect the update
    //std::atomic<long>** txCounts; // Array of tx counts, success and fails
    unsigned long* subscriber_rndm_seeds;
    unsigned long* vlr_rndm_seeds;
    unsigned long* rndm_seeds;


  public:
    TATP_DB(unsigned num_subscribers); // Constructs and sizes tables as per num_subscribers
    ~TATP_DB();

    void initialize(unsigned num_subscribers, int n);

    void populate_tables(unsigned num_subscribers); // Populates the various tables

    void fill_subscriber_entry(unsigned _s_id); // Fills subscriber table entry given subscriber id
    void fill_access_info_entry(unsigned _s_id, short _ai_type); // Fills access info table entry given subscriber id and ai_type
    void fill_special_facility_entry(unsigned _s_id, short _sf_type); // Fills special facility table entry given subscriber id and sf_type
    void fill_call_forwarding_entry(unsigned _s_id, short _sf_type, short _start_time); // Fills call forwarding table entry given subscriber id, sf_type and start type

    void convert_to_string(unsigned number, int num_digits, char* string_ptr);
    void make_upper_case_string(char* string_ptr, int num_chars);

    void update_subscriber_data(int threadId); // Tx: updates a random subscriber data
    long get_sub_id();
    void backup_location(long subId);
    void discard_backup(long subId);
    void update_location(long subId, uint64_t vlr); // Tx: updates location for a random subscriber
    void insert_call_forwarding(int threadId); // Tx: Inserts into call forwarding table for a random user
    void delete_call_forwarding(int threadId); // Tx: Deletes call forwarding for a random user

    unsigned long get_random(int thread_id, int min, int max);
    unsigned long get_random(int thread_id);
    unsigned long get_random_s_id(int thread_id);
    unsigned long get_random_vlr(int thread_id);

    void print_results();
};

//DS for logging info to recover from a failed update_subscriber_data Tx
struct recovery_update_subscriber_data {
  char txType; // will be '0'
  unsigned s_id; // the subscriber id being updated
  short sf_type; // the sf_type being modified
  short bit_1; // the old bit_! value
  short data_a; // the old data_a value
  char padding[5];
};

struct recovery_update_location {
  char txType; // will be '1'
  unsigned s_id; // the subcriber whose location is being updated
  unsigned vlr_location; // the old vlr location
  char padding[7];
};
