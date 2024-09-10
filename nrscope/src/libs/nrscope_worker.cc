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
  busy = false;
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
  // std::cout << "Starting the worker..." << std::endl; 
  StartWorker();
  return SRSRAN_SUCCESS;
}

void NRScopeWorker::StartWorker(){
  // std::cout << "Creating the thread. " << std::endl;
  worker_thread = std::thread{&NRScopeWorker::Run, this};
  worker_thread.detach();
}

void NRScopeWorker::CopySlotandBuffer(srsran_slot_cfg_t* slot_, 
                                      cf_t* rx_buffer_) {
  slot = *slot_;
  srsran_vec_cf_copy(rx_buffer, rx_buffer_, worker_state.slot_sz);
}

int NRScopeWorker::InitSIBDecoder(){
  /* Will always be called before any tasks */
  if(sibs_decoder.SIBDecoderandReceptionInit(&worker_state, 
     rf_buffer_t.to_cf_t()) < SRSASN_SUCCESS){
    return NR_FAILURE;
  }
  return SRSRAN_SUCCESS;
}

int NRScopeWorker::InitRACHDecoder() {
  // std::thread rach_init_thread {&RachDecoder::rach_decoder_init, &rach_decoder, task_scheduler_nrscope.sib1, args_t.base_carrier};
  rach_decoder.RACHDecoderInit(&task_scheduler_nrscope);
  srsran::rf_buffer_t rf_buffer_wrapper(rx_buffer, pre_resampling_sf_sz);
  if(rach_decoder.rach_reception_init(arg_scs, &task_scheduler_nrscope, rf_buffer_wrapper.to_cf_t()) < SRSASN_SUCCESS){
    ERROR("RACHDecoder Init Error");
    return NR_FAILURE;
  }
  std::cout << "RACH Decoder Initialized.." << std::endl;
  task_scheduler_nrscope.rach_inited = true;
  return SRSRAN_SUCCESS;
}

int NRScopeWorker::SyncState(WorkState* task_scheduler_state) {
  
  worker_state.args_t = task_scheduler_state->args_t;
  worker_state.cell = task_scheduler_state->cell;
  worker_state.cs_ret = task_scheduler_state->cs_ret;
  memcpy(&worker_state.srsran_searcher_cfg_t, 
    &task_scheduler_state->srsran_searcher_cfg_t, 
    sizeof(srsue::nr::cell_search::cfg_t));
  worker_state.coreset0_args_t = task_scheduler_state->coreset0_args_t;
  memcpy(&worker_state.coreset0_t, &task_scheduler_state->coreset0_t, 
    sizeof(srsran_coreset_t));

  worker_state.arg_scs = task_scheduler_state->arg_scs;

  worker_state.sib1 = task_scheduler_state->sib1;
  worker_state.sibs.resize(task_scheduler_state->sibs.size());
  for(long unsigned int i = 0; i < task_scheduler_state->sibs.size(); i++) {
    worker_state.sibs[i] = task_scheduler_state->sibs[i];
  }

  worker_state.found_sib.resize(task_scheduler_state->found_sib.size());
  for(long unsigned int i = 0; i < task_scheduler_state->found_sib.size(); i++){
    worker_state.found_sib[i] = task_scheduler_state->found_sib[i];
  }

  worker_state.rrc_setup = task_scheduler_state->rrc_setup;
  worker_state.master_cell_group = task_scheduler_state->master_cell_group;
  worker_state.rrc_recfg = task_scheduler_state->rrc_recfg;

  /* SIB 1 decoded, we can start the RACH thread */
  worker_state.sib1_found = task_scheduler_state->sib1_found;
  worker_state.rach_found = task_scheduler_state->rach_found;

  worker_state.sibs_vec_inited = task_scheduler_state->sibs_vec_inited;
  worker_state.all_sibs_found = task_scheduler_state->all_sibs_found;

  if (!worker_state.sib1_inited && task_scheduler_state->sib1_inited) {
    InitSIBDecoder();
    worker_state.sib1_inited = task_scheduler_state->sib1_inited;
  }

  return SRSRAN_SUCCESS;

  // if (!worker_state.rach_inited && task_scheduler_state->rach_inited) {
  //   if (srsran_unlikely(!worker_state.sib1_found)) {
  //     /* Error happens */
  //     ERROR("SIB 1 is not found while trying to init RACH decoder.");
  //     return NR_FAILURE;
  //   }
  //   InitRACHDecoder();
  //   worker_state.rach_inited = task_scheduler_state->rach_inited;
  // }

  // bool dci_inited; // DCIDecoder is initialized.

  // uint32_t nof_known_rntis;
  // std::vector<uint16_t> known_rntis;

  /* Below this, these variables go to the result structure. */
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
}

void NRScopeWorker::Run() {
  while (true) {
    /* When there is a job, the semaphore is set and buffer is copied */
    sem_wait(&smph_has_job);
    lock.lock();
    busy = true;
    lock.unlock();

    /* If we accidentally assign the job without initializing the SIB decoder */
    if(!worker_state.sib1_inited){
      continue;
    }

    std::thread sibs_thread;
    /* If sib1 is not found, we run the sibs_thread; if it's found, we skip. */
    if (!worker_state.sib1_found) {
      sibs_thread = std::thread {&SIBsDecoder::DecodeandParseSIB1fromSlot, 
        &sibs_decoder, &slot, worker_state};
    }    

    // std::thread rach_thread {&RachDecoder::decode_and_parse_msg4_from_slot, 
    //   &rach_decoder, &slot, sib1_found, rach_inited, &rrc_setup, 
    //   &master_cell_group, &rach_found, &new_rnti_number, &new_rntis_found};

    // std::vector <std::thread> dci_threads;
    // if(dci_inited){
    //   for (uint32_t i = 0; i < nof_threads; i++){
    //     dci_threads.emplace_back(&DCIDecoder::decode_and_parse_dci_from_slot, 
    //       dci_decoders[i].get(), &slot, nof_known_rntis, nof_rnti_worker_groups,
    //       sharded_results, sharded_rntis, nof_sharded_rntis, known_rntis,
    //       dl_prb_rate, dl_prb_bits_rate, ul_prb_rate, ul_prb_bits_rate);
    //   }
    // }

    if(sibs_thread.joinable()){
      sibs_thread.join();
    }

    // if(rach_thread.joinable()){
    //   rach_thread.join();
    // }

    // if(dci_inited){
    //   for (uint32_t i = 0; i < worker_state.nof_threads; i++){
    //     if(dci_threads[i].joinable()){
    //       dci_threads[i].join();
    //     }
    //   }
    // }

    /* TODO: Post result to a global result queue */

    lock.lock();
    busy = false;
    lock.unlock();
  }
}
}