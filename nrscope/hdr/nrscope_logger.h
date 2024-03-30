#ifndef NRSCOPE_LOGGER_H
#define NRSCOPE_LOGGER_H

#include "nrscope/hdr/nrscope_def.h"
#include "nrscope/hdr/dci_decoder.h"

namespace NRScopeLog{
  /***
   * ...
   * 
   * @param...
  */
  void init_logger(std::vector<std::string> filename_input);

  /***
   * ...
   * 
   * @param...
  */
  void push_node(LogNode input_node, int rf_index);

  /***
   * ...
   * 
   * @param...
  */
  void write_entry(LogNode input_node, int rf_index);

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