#ifndef NRSCOPE_WORKER_H
#define NRSCOPE_WORKER_H

#include "nrscope/hdr/nrscope_def.h"
#include "nrscope/hdr/rach_decoder.h"
#include "nrscope/hdr/sibs_decoder.h"
#include "nrscope/hdr/dci_decoder.h"

namespace NRScopeTask{

class NRScopeWorker{
public:
  cf_t* rx_buffer;
  srsran::rf_buffer_t rf_buffer_t;

  WorkState worker_state;
  RachDecoder rach_decoder; 
  SIBsDecoder sibs_decoder;
  std::vector<std::unique_ptr <DCIDecoder> > dci_decoders;

  /* Job indicator */
  sem_t smph_has_job; 
  bool busy;
  std::mutex lock;

  /* Worker thread */
  std::thread worker_thread;

  /* For DCI sharded results */
  std::vector<uint32_t> nof_sharded_rntis;
  std::vector <std::vector <uint16_t> > sharded_rntis;
  std::vector <DCIFeedback> sharded_results;
  std::vector <DCIFeedback> results;
  std::vector <float> dl_prb_rate;
  std::vector <float> dl_prb_bits_rate;
  std::vector <float> ul_prb_rate;
  std::vector <float> ul_prb_bits_rate;

  srsran_slot_cfg_t slot; /* Current slot. */
  
  NRScopeWorker();
  ~NRScopeWorker();

  /* This is called right after entering the radio_nr.cc,
    The cell's information is set to the worker,
    so the worker can set its buffer.*/
  int InitWorker(WorkState task_scheduler_state);

  /* Start the worker thread */
  void StartWorker();

  /* Set worker's state the same as the scheduler's state, 
    and initilize the decoders if needed. This function will be called
    before the job starts, so don't need to consider about the thread
    safe thing. */
  int SyncState(WorkState* task_scheduler_state);

  /* Copy the buffer and the slot structure from the task_scheduler */
  void CopySlotandBuffer(srsran_slot_cfg_t* slot_, cf_t* rx_buffer_);

  int InitSIBDecoder();
  int InitRACHDecoder();
  int InitDCIDecoders();

  int MergeResults();
  
private:
  void Run();
};

}

#endif