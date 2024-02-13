#ifndef SIBS_DECODER_H
#define SIBS_DECODER_H

#include "nrscope/hdr/nrscope_def.h"
#include "nrscope/hdr/task_scheduler.h"

class SIBsDecoder{
  public:
    uint8_t* data_pdcch;
    srsran_ue_dl_nr_sratescs_info arg_scs;
    srsran_carrier_nr_t base_carrier;
    srsran_sch_hl_cfg_nr_t pdsch_hl_cfg;
    srsran_softbuffer_rx_t softbuffer;
    asn1::rrc_nr::sib1_s sib1;

    srsran_ue_dl_nr_t ue_dl_sibs;
    srsran_dci_cfg_nr_t dci_cfg;
    srsran_ue_dl_nr_args_t ue_dl_args;
    srsran_pdcch_cfg_nr_t  pdcch_cfg;
    
    srsran_coreset_t coreset0_t;
    srsran_search_space_t* search_space;

    cell_search_result_t cell;


    SIBsDecoder();
    ~SIBsDecoder();

    int sib_decoder_and_reception_init(srsran_ue_dl_nr_sratescs_info arg_scs_,
                                       srsran_carrier_nr_t* base_carrier_,
                                       cell_search_result_t cell,
                                       cf_t* input[SRSRAN_MAX_PORTS],
                                       srsran_coreset_t* coreset0_t_);

    int decode_and_parse_sib1_from_slot(srsran_slot_cfg_t* slot,
                                        asn1::rrc_nr::sib1_s* sib1);

    int sibs_thread(srsran_ue_dl_nr_sratescs_info arg_scs_, 
                    TaskSchedulerNRScope* task_scheduler_nrscope, 
                    cf_t* input[SRSRAN_MAX_PORTS]);
};


#endif