#include "nrscope/hdr/nrscope_def.h"

double get_now_timestamp_in_double(){
  auto time = std::chrono::system_clock::now().time_since_epoch();
  std::chrono::seconds seconds = std::chrono::duration_cast< std::chrono::seconds >(time);
  std::chrono::milliseconds ms = std::chrono::duration_cast< std::chrono::milliseconds >(time);
  std::chrono::microseconds us = std::chrono::duration_cast< std::chrono::microseconds >(time);

  return (double) seconds.count() + ((double) (ms.count() % 1000)/1000.0) + ((double) (us.count() % 1000000)/1000000.0);
}

const char* sch_mapping_to_str(srsran_sch_mapping_type_t mapping)
{
  switch (mapping) {
    case srsran_sch_mapping_type_A:
      return "A";
    case srsran_sch_mapping_type_B:
      return "B";
    default:; // Do nothing
  }
  return "invalid";
}

const char* sch_xoverhead_to_str(srsran_xoverhead_t xoverhead)
{
  switch (xoverhead) {
    case srsran_xoverhead_0:
      return "0";
    case srsran_xoverhead_6:
      return "6";
    case srsran_xoverhead_12:
      return "12";
    case srsran_xoverhead_18:
      return "18";
    default:; // Do nothing
  }
  return "invalid";
}

void my_sig_handler(int s){
  printf("Caught signal %d\n",s);
  exit(0); 
}