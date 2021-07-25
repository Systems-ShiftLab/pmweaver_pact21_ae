/*
 Author: Vaibhav Gogte <vgogte@umich.edu>
         Aasheesh Kolli <akolli@umich.edu>

This file declares the entry types for each of the tables used in TPCC
*/

struct warehouse_entry {
  int w_id;
  char w_name[10];
  char w_street_1[20];
  char w_street_2[20];
  char w_city[20];
  char w_state[2];
  char w_zip[9];
  float w_tax;
  float w_ytd;
  char padding[32];
};

struct district_entry {
  short d_id;
  int d_w_id;
  char d_name[10];
  char d_street_1[20];
  char d_street_2[20];
  char d_city[20];
  char d_state[2];
  char d_zip[9];
  float d_tax;
  float d_ytd;
  int d_next_o_id;
  char padding[24];	//change padding from 4 to 24 to make it fits in 64-byte cacheline size
};

struct customer_entry {
  int c_id;
  short c_d_id;
  int c_w_id;
  char c_first[16];
  char c_middle[2];
  char c_last[16];
  char c_street_1[20];
  char c_street_2[20];
  char c_city[20];
  char c_state[2];
  char c_zip[9];
  char c_phone[16];
  long long c_since; // Seconds since 1st Jan 1900, 00:00:00
  char c_credit[2];
  float c_credit_lim;
  float c_discount;
  float c_balance;
  float c_ytd_payment;
  float c_payment_cnt;
  float c_delivery_cnt;
  char c_data[500];
  char padding[32];
};

struct history_entry {
  int h_c_id;
  short h_c_d_id;
  int h_c_w_id;
  short h_d_id;
  int h_w_id;
  long long h_date;
  float h_amount;
  char h_data[24];
};

struct new_order_entry {
  int no_o_id;
  short no_d_id;
  int no_w_id;
  char padding[52];	//change padding from 4 to 52 to make it fits in 64-byte cacheline size
};

struct order_entry {
  int o_id;
  short o_d_id;
  int o_w_id;
  int o_c_id;
  long long o_entry_d;
  short o_carrier_id;
  float o_ol_cnt;
  float o_all_local;
  char padding[24];
};

struct order_line_entry {
  int ol_o_id;
  short ol_d_id;
  int ol_w_id;
  short ol_number;
  int ol_i_id;
  int ol_supply_w_id;
  long long ol_delivery_d;
  float ol_quantity;
  float ol_amount;
  char ol_dist_info[24];
};

struct item_entry {
  int i_id;
  int i_im_id;
  char i_name[24];
  float i_price;
  char i_data[50];
  char padding[40];
};

struct stock_entry {
  int s_i_id;
  int s_w_id;
  float s_quantity;
  char s_dist_01[24];
  char s_dist_02[24];
  char s_dist_03[24];
  char s_dist_04[24];
  char s_dist_05[24];
  char s_dist_06[24];
  char s_dist_07[24];
  char s_dist_08[24];
  char s_dist_09[24];
  char s_dist_10[24];
  float s_ytd;
  float s_order_cnt;
  float s_remote_cnt;
  char s_data[50];
  char padding[4];
};

