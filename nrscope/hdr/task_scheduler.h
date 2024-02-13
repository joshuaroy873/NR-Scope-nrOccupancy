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
    asn1::rrc_nr::sib2_s sib2;
    asn1::rrc_nr::rrc_setup_s rrc_setup;
    asn1::rrc_nr::cell_group_cfg_s master_cell_group;
    // srsran::mac_rar_pdu_nr rar_pdu; // rar pdu

    bool sib1_found; // SIB 1 decoded, we can start the RACH thread

    std::queue<sib1_task_element> sib1_queue;
    std::queue<rach_task_element> rach_queue;
    std::queue<dci_task_element> dci_queue;

    uint32_t nof_known_rntis;
    std::vector<uint16_t> known_rntis;

    TaskSchedulerNRScope();

    int decode_mib(cell_searcher_args_t* args_t_, 
                   srsue::nr::cell_search::ret_t* cs_ret_,
                   srsue::nr::cell_search::cfg_t* srsran_searcher_cfg_t);

    int push_queue(srsran_ue_sync_nr_outcome_t outcome_, srsran_slot_cfg_t slot_);
};


#endif