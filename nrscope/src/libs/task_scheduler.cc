#include "nrscope/hdr/task_scheduler.h"

namespace NRScopeTask{
TaskSchedulerNRScope::TaskSchedulerNRScope(){
  task_scheduler_state.sib1_inited = false;
  task_scheduler_state.rach_inited = false;
  task_scheduler_state.dci_inited = false;

  task_scheduler_state.sib1_found = false;
  task_scheduler_state.rach_found = false;
  task_scheduler_state.sibs_vec_inited = false;

  task_scheduler_state.nof_known_rntis = 0;
  task_scheduler_state.known_rntis.resize(task_scheduler_state.nof_known_rntis);
}

TaskSchedulerNRScope::~TaskSchedulerNRScope(){
}

int TaskSchedulerNRScope::InitandStart(int32_t nof_threads, 
                                       uint32_t nof_rnti_worker_groups,
                                       uint8_t nof_bwps,
                                       cell_searcher_args_t args_t,
                                       uint32_t nof_workers_){
  task_scheduler_state.nof_threads = nof_threads;
  task_scheduler_state.nof_rnti_worker_groups = nof_rnti_worker_groups;
  task_scheduler_state.nof_bwps = nof_bwps;
  task_scheduler_state.args_t = args_t;
  task_scheduler_state.slot_sz = (uint32_t)(args_t.srate_hz / 1000.0f / 
    SRSRAN_NOF_SLOTS_PER_SF_NR(args_t.ssb_scs));
  nof_workers = nof_workers_;
  std::cout << "Starting workers..." << std::endl;
  for (uint32_t i = 0; i < nof_workers; i ++) {
    NRScopeWorker *worker = new NRScopeWorker();
    std::cout << "New worker " << i << " is going to start... "<< std::endl;
    if(worker->InitWorker(task_scheduler_state) < SRSRAN_SUCCESS) {
      ERROR("Error initializing worker %d", i);
      return NR_FAILURE;
    }
    workers.emplace_back(std::unique_ptr<NRScopeWorker> (worker));
  }

  std::cout << "Workers started..." << std::endl;

  scheduler_thread = std::thread{&TaskSchedulerNRScope::Run, this};
  scheduler_thread.detach();

  return SRSRAN_SUCCESS;
}

int TaskSchedulerNRScope::DecodeMIB(cell_searcher_args_t* args_t_, 
                        srsue::nr::cell_search::ret_t* cs_ret_,
                        srsue::nr::cell_search::cfg_t* srsran_searcher_cfg_t_,
                        float resample_ratio_,
                        uint32_t raw_srate_){
  args_t_->base_carrier.pci = cs_ret_->ssb_res.N_id;

  if(srsran_pbch_msg_nr_mib_unpack(&(cs_ret_->ssb_res.pbch_msg), 
      &task_scheduler_state.cell.mib) < SRSRAN_SUCCESS){
    ERROR("Error decoding MIB");
    return SRSRAN_ERROR;
  }

  char str[1024] = {};
  srsran_pbch_msg_nr_mib_info(&task_scheduler_state.cell.mib, str, 1024);
  printf("MIB: %s\n", str);
  // printf("MIB payload: ");
  // for (int i =0; i<SRSRAN_PBCH_MSG_NR_MAX_SZ; i++){
  //   printf("%hhu ", cs_ret_->ssb_res.pbch_msg.payload[i]);
  // }
  // printf("\n");

  /* already added the msb of k_ssb */
  task_scheduler_state.cell.k_ssb = task_scheduler_state.cell.mib.ssb_offset; 

  // srsran_coreset0_ssb_offset returns the offset_rb relative to ssb
  // nearly all bands in FR1 have min bandwidth 5 or 10 MHz, 
  // so there are only 5 entries here.  
  task_scheduler_state.coreset0_args_t.offset_rb = 
    srsran_coreset0_ssb_offset(task_scheduler_state.cell.mib.coreset0_idx, 
    args_t_->ssb_scs, task_scheduler_state.cell.mib.scs_common);

  task_scheduler_state.coreset0_t = {};
  /* srsran_coreset_zero returns the offset_rb relative to pointA */
  if(srsran_coreset_zero(cs_ret_->ssb_res.N_id, 
                        0,  
                        args_t_->ssb_scs, 
                        task_scheduler_state.cell.mib.scs_common, 
                        task_scheduler_state.cell.mib.coreset0_idx, 
                        &task_scheduler_state.coreset0_t) == SRSRAN_SUCCESS){
    char freq_res_str[SRSRAN_CORESET_FREQ_DOMAIN_RES_SIZE] = {};
    char coreset_info[512] = {};
    srsran_coreset_to_str(&task_scheduler_state.coreset0_t, coreset_info, 
      sizeof(coreset_info));
    printf("Coreset parameter: %s", coreset_info);
  }

  /* To find the position of coreset0, we need to use the offset between SSB 
    and CORESET0, because we don't know the ssb_pointA_freq_offset_Hz yet 
    required by the srsran_coreset_zero function.
    coreset0_t low bound freq = ssb center freq - 120 * scs (half of sc in ssb) - 
    ssb_subcarrierOffset(from MIB) * scs - entry->offset_rb * 12(sc in one rb) * scs */
  task_scheduler_state.cell.abs_ssb_scs = 
    SRSRAN_SUBC_SPACING_NR(args_t_->ssb_scs);
  task_scheduler_state.cell.abs_pdcch_scs = 
    SRSRAN_SUBC_SPACING_NR(task_scheduler_state.cell.mib.scs_common);
  
  srsran::srsran_band_helper bands;
  /* defined by standards */
  task_scheduler_state.coreset0_args_t.coreset0_lower_freq_hz = 
    srsran_searcher_cfg_t_->ssb_freq_hz - (SRSRAN_SSB_BW_SUBC / 2) *
    task_scheduler_state.cell.abs_ssb_scs - 
    task_scheduler_state.coreset0_args_t.offset_rb * NRSCOPE_NSC_PER_RB_NR * 
    task_scheduler_state.cell.abs_pdcch_scs - 
    task_scheduler_state.cell.k_ssb * 
    SRSRAN_SUBC_SPACING_NR(srsran_subcarrier_spacing_15kHz); 
  task_scheduler_state.coreset0_args_t.coreset0_center_freq_hz = 
    task_scheduler_state.coreset0_args_t.coreset0_lower_freq_hz + 
    srsran_coreset_get_bw(&task_scheduler_state.coreset0_t) / 2 * 
    task_scheduler_state.cell.abs_pdcch_scs * NRSCOPE_NSC_PER_RB_NR;

  coreset_zero_t_f_entry_nrscope coreset_zero_cfg;
  /* Get coreset_zero's position in time domain, check table 38.213, 13-11, 
    because USRP can only support FR1. */
  if(coreset_zero_t_f_nrscope(task_scheduler_state.cell.mib.ss0_idx, 
      task_scheduler_state.cell.mib.ssb_idx, 
      task_scheduler_state.coreset0_t.duration, &coreset_zero_cfg) < 
      SRSRAN_SUCCESS){
    ERROR("Error checking table 13-11");
    return SRSRAN_ERROR;
  }
  // std::cout << "After calling coreset_zero_t_f_nrscope" << std::endl;

  task_scheduler_state.cell.u = (int)args_t_->ssb_scs; 
  task_scheduler_state.coreset0_args_t.n_0 = (coreset_zero_cfg.O * 
    (int)pow(2, task_scheduler_state.cell.u) + 
    (int)floor(task_scheduler_state.cell.mib.ssb_idx * coreset_zero_cfg.M)) % 
    SRSRAN_NSLOTS_X_FRAME_NR(task_scheduler_state.cell.u);
  // sfn_c = 0, in even system frame, sfn_c = 1, in odd system frame    
  task_scheduler_state.coreset0_args_t.sfn_c = (int)(floor(coreset_zero_cfg.O * 
    pow(2, task_scheduler_state.cell.u) + 
    floor(task_scheduler_state.cell.mib.ssb_idx * coreset_zero_cfg.M)) / 
    SRSRAN_NSLOTS_X_FRAME_NR(task_scheduler_state.cell.u)) % 2;
  args_t_->base_carrier.nof_prb = 
    srsran_coreset_get_bw(&task_scheduler_state.coreset0_t);

  task_scheduler_state.args_t = *args_t_;
  task_scheduler_state.cs_ret = *cs_ret_;
  memcpy(&task_scheduler_state.srsran_searcher_cfg_t, srsran_searcher_cfg_t_, 
    sizeof(srsue::nr::cell_search::cfg_t));

  // initiate resampler here
  resample_ratio = resample_ratio_;
  float As=60.0f;
  resampler = msresamp_crcf_create(resample_ratio,As);
  resampler_delay = msresamp_crcf_get_delay(resampler);
  /* don't hardcode it; change later */
  pre_resampling_slot_sz = raw_srate_ / 1000 / 
    SRSRAN_NOF_SLOTS_PER_SF_NR(args_t_->ssb_scs); 
  temp_x_sz = pre_resampling_slot_sz + (int)ceilf(resampler_delay) + 10;
  temp_y_sz = (uint32_t)(temp_x_sz * resample_ratio * 2);
  temp_x = SRSRAN_MEM_ALLOC(std::complex<float>, temp_x_sz);
  temp_y = SRSRAN_MEM_ALLOC(std::complex<float>, temp_y_sz);

  return SRSRAN_SUCCESS;
}

int TaskSchedulerNRScope::MergeResults(){
  
  for (uint8_t b = 0; b < task_scheduler_state.nof_bwps; b++) {
    DCIFeedback new_result;
    results[b] = new_result;
    results[b].dl_grants.resize(task_scheduler_state.nof_known_rntis);
    results[b].ul_grants.resize(task_scheduler_state.nof_known_rntis);
    results[b].spare_dl_prbs.resize(task_scheduler_state.nof_known_rntis);
    results[b].spare_dl_tbs.resize(task_scheduler_state.nof_known_rntis);
    results[b].spare_dl_bits.resize(task_scheduler_state.nof_known_rntis);
    results[b].spare_ul_prbs.resize(task_scheduler_state.nof_known_rntis);
    results[b].spare_ul_tbs.resize(task_scheduler_state.nof_known_rntis);
    results[b].spare_ul_bits.resize(task_scheduler_state.nof_known_rntis);
    results[b].dl_dcis.resize(task_scheduler_state.nof_known_rntis);
    results[b].ul_dcis.resize(task_scheduler_state.nof_known_rntis);

    uint32_t rnti_s = 0;
    uint32_t rnti_e = 0;
    for(uint32_t i = 0; i < task_scheduler_state.nof_rnti_worker_groups; i++){
      if(rnti_s >= task_scheduler_state.nof_known_rntis){
        continue;
      }
      uint32_t n_rntis = task_scheduler_state.nof_sharded_rntis[i * 
        task_scheduler_state.nof_bwps];
      rnti_e = rnti_s + n_rntis;
      if(rnti_e > task_scheduler_state.nof_known_rntis){
        rnti_e = task_scheduler_state.nof_known_rntis;
      }

      uint32_t thread_id = i * task_scheduler_state.nof_bwps + b;
      results[b].nof_dl_used_prbs += 
        task_scheduler_state.sharded_results[thread_id].nof_dl_used_prbs;
      results[b].nof_ul_used_prbs += 
        task_scheduler_state.sharded_results[thread_id].nof_ul_used_prbs;

      for(uint32_t k = 0; k < n_rntis; k++) {
        results[b].dl_dcis[k+rnti_s] = 
          task_scheduler_state.sharded_results[thread_id].dl_dcis[k];
        results[b].ul_dcis[k+rnti_s] = 
          task_scheduler_state.sharded_results[thread_id].ul_dcis[k];
        results[b].dl_grants[k+rnti_s] = 
          task_scheduler_state.sharded_results[thread_id].dl_grants[k];
        results[b].ul_grants[k+rnti_s] = 
          task_scheduler_state.sharded_results[thread_id].ul_grants[k];
      }
      rnti_s = rnti_e;
    }

    std::cout << "End of nof_threads..." << std::endl;

    /* TO-DISCUSS: to obtain even more precise result, 
      here maybe we should total user payload prb in that bwp - used prb */
    results[b].nof_dl_spare_prbs = 
      task_scheduler_state.args_t.base_carrier.nof_prb * 
      (14 - 2) - results[b].nof_dl_used_prbs;
    for(uint32_t idx = 0; idx < task_scheduler_state.nof_known_rntis; idx ++){
      results[b].spare_dl_prbs[idx] = results[b].nof_dl_spare_prbs / 
        task_scheduler_state.nof_known_rntis;
      if(abs(results[b].spare_dl_prbs[idx]) > 
          task_scheduler_state.args_t.base_carrier.nof_prb * (14 - 2)){
        results[b].spare_dl_prbs[idx] = 0;
      }
      results[b].spare_dl_tbs[idx] = 
        (int) ((float)results[b].spare_dl_prbs[idx] * 
        task_scheduler_state.dl_prb_rate[idx]);
      results[b].spare_dl_bits[idx] = 
        (int) ((float)results[b].spare_dl_prbs[idx] * 
        task_scheduler_state.dl_prb_bits_rate[idx]);
    }

    results[b].nof_ul_spare_prbs = 
      task_scheduler_state.args_t.base_carrier.nof_prb * (14 - 2) - 
      results[b].nof_ul_used_prbs;
    for(uint32_t idx = 0; idx < task_scheduler_state.nof_known_rntis; idx ++){
      results[b].spare_ul_prbs[idx] = results[b].nof_ul_spare_prbs / 
        task_scheduler_state.nof_known_rntis;
      if(abs(results[b].spare_ul_prbs[idx]) > 
          task_scheduler_state.args_t.base_carrier.nof_prb * (14 - 2)){
        results[b].spare_ul_prbs[idx] = 0;
      }
      results[b].spare_ul_tbs[idx] = 
        (int) ((float)results[b].spare_ul_prbs[idx] * 
        task_scheduler_state.ul_prb_rate[idx]);
      results[b].spare_ul_bits[idx] = 
        (int) ((float)results[b].spare_ul_prbs[idx] * 
        task_scheduler_state.ul_prb_bits_rate[idx]);
    }
  }

  return SRSRAN_SUCCESS;
}

int TaskSchedulerNRScope::UpdateKnownRNTIs(){
  if(task_scheduler_state.new_rnti_number <= 0){
    return SRSRAN_SUCCESS;
  }

  task_scheduler_state.nof_known_rntis += task_scheduler_state.new_rnti_number;
  for(uint32_t i = 0; i < task_scheduler_state.new_rnti_number; i++){
    task_scheduler_state.known_rntis.emplace_back(
      task_scheduler_state.new_rntis_found[i]);
  }

  task_scheduler_state.new_rntis_found.clear();
  task_scheduler_state.new_rnti_number = 0;
  return SRSRAN_SUCCESS;
}

void TaskSchedulerNRScope::Run(){
  while(true) {
    /* Try to extract results from the global result queue*/

  }
}

int TaskSchedulerNRScope::AssignTask(srsran_slot_cfg_t* slot, cf_t* rx_buffer_){
  /* Find the first idle worker */
  bool found_worker = false;
  for (uint32_t i = 0; i < nof_workers; i ++) {
    bool busy;
    lock.lock();
    busy = workers[i].get()->busy;
    lock.unlock();
    if (!busy) {
      found_worker = true;

      /* Copy the rx_buffer_ to the worker's rx_buffer */
      workers[i].get()->CopySlotandBuffer(slot, rx_buffer_);

      /* Update the worker's state */
      workers[i].get()->SyncState(&task_scheduler_state);

      /* Set the worker's sem to let the task run */
      sem_post(&workers[i].get()->smph_has_job);

      break;
    }
  }
  
  if (!found_worker) {
    ERROR("No available worker, consider increasing the number of workers.");
    return SRSRAN_ERROR;
  } else {
    return SRSRAN_SUCCESS;
  }

}

}