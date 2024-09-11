#ifndef TASK_SCHEDULER_H
#define TASK_SCHEDULER_H

#include <liquid/liquid.h>

#include "nrscope/hdr/nrscope_def.h"
#include "nrscope/hdr/nrscope_worker.h"

namespace NRScopeTask{
class TaskSchedulerNRScope{
public:
  /* Task scheduler stores all the latest global params that each worker
  should sync with it each time the task is assigned. */
  WorkState task_scheduler_state;
  uint32_t nof_workers;
  std::thread scheduler_thread;

  std::vector<std::unique_ptr <NRScopeWorker> > workers;
  /* Slot results reorder buffer */
  std::vector<SlotResult> slot_results;

  TaskSchedulerNRScope();
  ~TaskSchedulerNRScope();

  /* Start the workers and the receiving result thread, called before the 
  radio reception. */
  int InitandStart(int32_t nof_threads, 
                   uint32_t nof_rnti_worker_groups,
                   uint8_t nof_bwps,
                   cell_searcher_args_t args_t,
                   uint32_t nof_workers_);

  int DecodeMIB(cell_searcher_args_t* args_t_, 
                srsue::nr::cell_search::ret_t* cs_ret_,
                srsue::nr::cell_search::cfg_t* srsran_searcher_cfg_t,
                float resample_ratio_,
                uint32_t raw_srate_);

  int UpdateKnownRNTIs();

  /* Assign the current slot to one worker*/
  int AssignTask(srsran_slot_cfg_t* slot, 
                 srsran_ue_sync_nr_outcome_t* outcome,
                 cf_t* rx_buffer_);

  // resampler tools
  float resample_ratio;
  msresamp_crcf resampler;
  float resampler_delay;
  uint32_t temp_x_sz;
  uint32_t temp_y_sz;
  std::complex<float> * temp_x;
  std::complex<float> * temp_y;
  uint32_t pre_resampling_slot_sz;

private:
  void Run();
};

}

#endif