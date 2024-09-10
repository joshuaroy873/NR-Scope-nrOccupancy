#ifndef TASK_SCHEDULER_H
#define TASK_SCHEDULER_H

#include <liquid/liquid.h>

/* A class that stores some intermediate results and schedules the SIB, RACH and DCI loops. */

#include "nrscope/hdr/nrscope_def.h"
#include "nrscope/hdr/nrscope_worker.h"

namespace NRScopeTask{
class TaskSchedulerNRScope{
public:
  /* Task scheduler stores all the latest global params that each worker
  should sync with it each time the task is assigned. */
  // cell_searcher_args_t args_t;
  // cell_search_result_t cell;
  // srsue::nr::cell_search::ret_t cs_ret;
  // srsue::nr::cell_search::cfg_t srsran_searcher_cfg_t;
  // coreset0_args coreset0_args_t;
  // srsran_coreset_t coreset0_t;

  // asn1::rrc_nr::sib1_s sib1;
  // std::vector<asn1::rrc_nr::sys_info_s> sibs;
  // std::vector<int> found_sib; 

  // asn1::rrc_nr::rrc_setup_s rrc_setup;
  // asn1::rrc_nr::cell_group_cfg_s master_cell_group;
  // asn1::rrc_nr::rrc_recfg_s rrc_recfg;

  // bool sib1_found; // SIB 1 decoded, we can start the RACH thread
  // bool rach_found;

  // bool sibs_vec_inited; // Is the vector for other SIBs set according to SIB?
  // bool all_sibs_found; // All SIBs are decoded, we can stop the SIB thread from now.

  // bool sib1_inited; // SIBsDecoder is initialized.
  // bool rach_inited; // RACHDecoder is initialized.
  // bool dci_inited; // DCIDecoder is initialized.

  // uint32_t nof_known_rntis;
  // std::vector<uint16_t> known_rntis;

  // std::vector<uint32_t> nof_sharded_rntis;
  // std::vector <std::vector <uint16_t> > sharded_rntis;
  // std::vector <DCIFeedback> sharded_results;
  // uint32_t nof_threads;
  // uint32_t nof_rnti_worker_groups;
  // uint8_t nof_bwps;

  // std::vector <float> dl_prb_rate;
  // std::vector <float> ul_prb_rate;
  // std::vector <float> dl_prb_bits_rate;
  // std::vector <float> ul_prb_bits_rate;

  // uint32_t new_rnti_number;
  // std::vector<uint16_t> new_rntis_found;
  WorkState task_scheduler_state;
  uint32_t nof_workers;
  std::thread scheduler_thread;

  std::vector<std::unique_ptr <NRScopeWorker> > workers;
  std::mutex lock;

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

  int MergeResults();
  int UpdateKnownRNTIs();

  /* Assign the current slot to one worker*/
  int AssignTask(srsran_slot_cfg_t* slot, cf_t* rx_buffer_);

  std::vector <DCIFeedback> get_results(){
    return results;
  }

  // per bwp DCI decoding result for current TTI
  std::vector <DCIFeedback> results;

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