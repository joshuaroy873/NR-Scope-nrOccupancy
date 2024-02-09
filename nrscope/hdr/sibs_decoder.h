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
    SIBsDecoder();
    ~SIBsDecoder();

    int decode_and_parse_sib1_from_slot(srsran_ue_dl_nr_t* ue_dl,
                                        srsran_slot_cfg_t* slot,
                                        srsran_ue_dl_nr_sratescs_info arg_scs,
                                        srsran_carrier_nr_t* base_carrier,
                                        srsran_sch_hl_cfg_nr_t* pdsch_hl_cfg,
                                        srsran_softbuffer_rx_t* softbuffer,
                                        asn1::rrc_nr::sib1_s* sib1);

    // int decode_and_parse_sib2_from_slot()
};


#endif