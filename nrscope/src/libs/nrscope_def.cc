#include "nrscope/hdr/nrscope_def.h"

double get_now_timestamp_in_double(){
  auto time = std::chrono::system_clock::now().time_since_epoch();
  std::chrono::seconds seconds = std::chrono::duration_cast< std::chrono::seconds >(time);
  std::chrono::milliseconds ms = std::chrono::duration_cast< std::chrono::milliseconds >(time);
  std::chrono::microseconds us = std::chrono::duration_cast< std::chrono::microseconds >(time);

  return (double) seconds.count() + ((double) (ms.count() % 1000)/1000.0) + ((double) (us.count() % 1000000)/1000000.0);
}