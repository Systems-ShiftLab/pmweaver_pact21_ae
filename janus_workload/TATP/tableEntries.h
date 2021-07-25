/*
 Author: Vaibhav Gogte <vgogte@umich.edu>
         Aasheesh Kolli <akolli@umich.edu>

This file defines the table entries used by TATP.
*/


struct subscriber_entry {
  unsigned s_id; // Subscriber id
  char sub_nbr[15]; // Subscriber number, s_id in 15 digit string, zeros padded
  short bit_1, bit_2, bit_3, bit_4, bit_5, bit_6, bit_7, bit_8, bit_9, bit_10; // randomly generated values 0/1
  short hex_1, hex_2, hex_3, hex_4, hex_5, hex_6, hex_7, hex_8, hex_9, hex_10; // randomly generated values 0->15
  short byte2_1, byte2_2, byte2_3, byte2_4, byte2_5, byte2_6, byte2_7, byte2_8, byte2_9, byte2_10; // randomly generated values 0->255
  unsigned msc_location; // Randomly generated value 1->((2^32)-1)
  unsigned vlr_location; // Randomly generated value 1->((2^32)-1)
  char padding[40];
};

struct access_info_entry {
  unsigned s_id; //Subscriber id
  short ai_type; // Random value 1->4. A subscriber can have a max of 4 and all unique
  short data_1, data_2; // Randomly generated values 0->255
  char data_3[3]; // random 3 char string. All upper case alphabets
  char data_4[5]; // random 5 char string. All upper case alphabets
  bool valid;
  bool padding_1[7];
  char padding_2[4+32];
};

struct special_facility_entry {
  unsigned s_id; //Subscriber id
  short sf_type; // Random value 1->4. A subscriber can have a max of 4 and all unique
  short is_active; // 0(15%)/1(85%)
  short error_cntrl; // Randomly generated values 0->255
  short data_a; // Randomly generated values 0->255
  char data_b[5]; // random 5 char string. All upper case alphabets
  char padding_1[7];
  bool valid;
  bool padding_2[4+32];
};

struct call_forwarding_entry {
  unsigned s_id; // Subscriber id from special facility
  short sf_type; // sf_type from special facility table
  int start_time; // 0 or 8 or 16
  int end_time; // start_time+N, N randomly generated 1->8
  char numberx[15]; // randomly generated 15 digit string
  char padding_1[7];
  bool valid;
  bool padding_2[24];
};


