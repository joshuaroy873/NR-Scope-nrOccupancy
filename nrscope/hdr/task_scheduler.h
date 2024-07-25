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

    asn1::rrc_nr::sib1_s sib1;
    std::vector<asn1::rrc_nr::sys_info_s> sibs;
    std::vector<int> found_sib; 

    asn1::rrc_nr::rrc_setup_s rrc_setup;
    asn1::rrc_nr::cell_group_cfg_s master_cell_group;

    asn1::rrc_nr::rrc_recfg_s rrc_recfg;
    // srsran::mac_rar_pdu_nr rar_pdu; // rar pdu

    bool sib1_found; // SIB 1 decoded, we can start the RACH thread
    bool rach_found;

    bool sibs_vec_inited; // Is the vector for other SIBs set according to SIB?
    bool all_sibs_found; // All SIBs are decoded, we can stop the SIB thread from now.

    bool sib1_inited; // SIBsDecoder is initialized.
    bool rach_inited; // RACHDecoder is initialized.
    bool dci_inited; // DCIDecoder is initialized.

    uint32_t nof_known_rntis;
    std::vector<uint16_t> known_rntis;

    std::vector<uint32_t> nof_sharded_rntis;
    std::vector <std::vector <uint16_t> > sharded_rntis;
    std::vector <DCIFeedback> sharded_results;
    uint32_t nof_threads;
    uint32_t nof_rnti_worker_groups;
    uint8_t nof_bwps;

    std::vector <float> dl_prb_rate;
    std::vector <float> ul_prb_rate;
    std::vector <float> dl_prb_bits_rate;
    std::vector <float> ul_prb_bits_rate;

    uint32_t new_rnti_number;
    std::vector<uint16_t> new_rntis_found;

    std::mutex lock;

    TaskSchedulerNRScope();
    ~TaskSchedulerNRScope();

    int decode_mib(cell_searcher_args_t* args_t_, 
                   srsue::nr::cell_search::ret_t* cs_ret_,
                   srsue::nr::cell_search::cfg_t* srsran_searcher_cfg_t);

    int merge_results();

    int update_known_rntis();

    DCIFeedback get_result(){
      return result;
    }

  private:
    DCIFeedback result; // DCI decoding result for current TTI
};


#endif