#ifndef SIBS_DECODER_H
#define SIBS_DECODER_H

// #include "srsran/common/band_helper.h"
// #include "srsran/common/crash_handler.h"
// #include "srsran/common/string_helpers.h"
// #include "srsue/hdr/phy/phy_nr_sa.h"
// #include "srsue/hdr/phy/nr/cell_search.h"
// #include "test/phy/dummy_ue_stack.h"
// #include "srsue/hdr/stack/ue_stack_nr.h"
// #include <boost/program_options.hpp>
// #include <boost/program_options/parsers.hpp>
// #include "srsran/asn1/rrc_nr.h"
// #include "srsran/asn1/asn1_utils.h"
// #include "srsran/mac/mac_rar_pdu_nr.h"

#include "nrscope/hdr/nrscope_def.h"

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
    srsran_search_space_t search_space;

    cell_search_result_t cell;


    SIBsDecoder();
    ~SIBsDecoder();

    int sib_decoder_and_reception_init(srsran_ue_dl_nr_sratescs_info arg_scs_,
                                       srsran_carrier_nr_t* base_carrier_,
                                       cell_search_result_t cell,
                                       cf_t* input[SRSRAN_MAX_PORTS],
                                       srsran_dci_cfg_nr_t* dci_cfg_,
                                       srsran_ue_dl_nr_args_t* ue_dl_args_,
                                       srsran_coreset_t* coreset0_t_);

    int decode_and_parse_sib1_from_slot(srsran_slot_cfg_t* slot,
                                        asn1::rrc_nr::sib1_s* sib1);

    // int decode_and_parse_sib2_from_slot()
};


#endif