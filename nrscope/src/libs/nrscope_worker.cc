#include "nrscope/hdr/nrscope_worker.h"
#include <semaphore>
#include <chrono>

namespace NRScopeTask{
NRScopeWorker::NRScopeWorker() : 
  rf_buffer_t(1),
  rach_decoder(),
  sibs_decoder() 
{
  worker_state.sib1_inited = false;
  worker_state.rach_inited = false;
  worker_state.dci_inited = false;

  worker_state.sib1_found = false;
  worker_state.rach_found = false;
  worker_state.sibs_vec_inited = false;

  worker_state.nof_known_rntis = 0;
  worker_state.known_rntis.resize(worker_state.nof_known_rntis);
  sem_init(&smph_has_job, 0, 0); 
}

NRScopeWorker::~NRScopeWorker(){ }

int NRScopeWorker::InitWorker(WorkState task_scheduler_state){
  /* Copy initial values */
  worker_state.nof_threads = task_scheduler_state.nof_threads;
  worker_state.nof_rnti_worker_groups = 
    task_scheduler_state.nof_rnti_worker_groups;
  worker_state.nof_bwps = task_scheduler_state.nof_bwps;
  worker_state.args_t = task_scheduler_state.args_t;
  worker_state.slot_sz = task_scheduler_state.slot_sz;
  /* Size of one subframe */
  rx_buffer = srsran_vec_cf_malloc(SRSRAN_NOF_SLOTS_PER_SF_NR(
    worker_state.args_t.ssb_scs) * worker_state.slot_sz);

  /* An wrapper for the rx_buffer */
  rf_buffer_t = srsran::rf_buffer_t(rx_buffer, 
    SRSRAN_NOF_SLOTS_PER_SF_NR(worker_state.args_t.ssb_scs) * 
    worker_state.slot_sz);
  /* Start the worker thread */
  StartWorker();
  return SRSRAN_SUCCESS;
}

void NRScopeWorker::StartWorker(){
  std::thread worker_thread {&NRScopeWorker::Run, this};
}

int NRScopeWorker::InitSIBDecoder(){
  /* Will always be called before any tasks */
  if(sibs_decoder.sib_decoder_and_reception_init(&worker_state, 
     rf_buffer_t.to_cf_t()) < SRSASN_SUCCESS){
    return NR_FAILURE;
  }
  return SRSRAN_SUCCESS;
}

void NRScopeWorker::Run() {
  while (true) {
    /* When there is a job, the semaphore is set and buffer is copied */
    sem_wait(&smph_has_job);

    /* If we accidentally assign the job without initializing the SIB decoder */
    if(!worker_state.sib1_inited){
      continue;
    }

    std::thread sibs_thread;
    /* If sib1 is not found, we run the sibs_thread; if it's found, we skip. */
    if (!worker_state.sib1_found) {
      sibs_thread = std::thread {&SIBsDecoder::decode_and_parse_sib1_from_slot, 
        &sibs_decoder, &slot, &sibs_vec_inited, &all_sibs_found, found_sib,
        sibs, &sib1};
    }    

    std::thread rach_thread {&RachDecoder::decode_and_parse_msg4_from_slot, 
      &rach_decoder, &slot, sib1_found, rach_inited, &rrc_setup, 
      &master_cell_group, &rach_found, &new_rnti_number, &new_rntis_found};

    std::vector <std::thread> dci_threads;
    if(dci_inited){
      for (uint32_t i = 0; i < nof_threads; i++){
        dci_threads.emplace_back(&DCIDecoder::decode_and_parse_dci_from_slot, 
          dci_decoders[i].get(), &slot, nof_known_rntis, nof_rnti_worker_groups,
          sharded_results, sharded_rntis, nof_sharded_rntis, known_rntis,
          dl_prb_rate, dl_prb_bits_rate, ul_prb_rate, ul_prb_bits_rate);
      }
    }

    if(sibs_thread.joinable()){
      sibs_thread.join();
    }

    if(rach_thread.joinable()){
      rach_thread.join();
    }

    if(dci_inited){
      for (uint32_t i = 0; i < nof_threads; i++){
        if(dci_threads[i].joinable()){
          dci_threads[i].join();
        }
      }
    }

    /* TODO: Post result to a global result queue */

  }
}
}