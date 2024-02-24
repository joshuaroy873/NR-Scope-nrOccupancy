#ifndef NRSCOPE_LOGGER_H
#define NRSCOPE_LOGGER_H

#include "nrscope/hdr/nrscope_def.h"
#include "nrscope/hdr/dci_decoder.h"

namespace NRScopeLog{
  typedef struct LogNode_ LogNode;
  struct LogNode_{
    double timestamp;
    int system_frame_idx;
    int slot_idx;
    srsran_sch_cfg_nr_t grant;
  };
  
  /***
   * ...
   * 
   * @param...
  */
  void init_logger(std::string filename_input);

  /***
   * ...
   * 
   * @param...
  */
  void push_node(LogNode input_node);

  /***
   * ...
   * 
   * @param...
  */
  void write_entry(LogNode input_node);

  /***
   * ...
   * 
   * @param...
  */
  void logger_thread();

   /***
   * ...
   * 
   * @param...
  */
  void exit_logger();
};


#endif