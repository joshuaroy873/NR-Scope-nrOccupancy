#ifndef TASK_SCHEDULER_H
#define TASK_SCHEDULER_H

/* A class that stores some intermediate results and schedules the SIB, RACH and DCI loops. */

#include "nrscope/hdr/nrscope_def.h"

class TaskSchedulerNRScope{
  public:
    cell_searcher_args_t args_t;
    cell_search_result_t cell;
    srsue::nr::cell_search::ret_t cs_ret;
    srsue::nr::cell_search::cfg_t srsran_searcher_cfg_t;
    coreset0_args coreset0_args_t;
    srsran_coreset_t coreset0_t;

    TaskSchedulerNRScope();

    int decode_mib(cell_searcher_args_t* args_t_, 
                   srsue::nr::cell_search::ret_t* cs_ret_,
                   srsue::nr::cell_search::cfg_t* srsran_searcher_cfg_t);
    int decoders_init();
};


#endif