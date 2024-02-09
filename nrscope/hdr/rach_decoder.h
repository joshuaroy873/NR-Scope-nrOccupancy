#include <mutex>
#include <iostream>

#include "srsran/config.h"
#include "srsran/asn1/rrc_nr.h"
#include "srsran/asn1/asn1_utils.h"
#include "srsran/common/band_helper.h"

#include "nrscope/hdr/nrscope_def.h"

// typedef struct {
//   bool is_sib1_found = false;
//   bool is_msg1_found = false;
//   bool is_msg2_found = false;
//   bool is_msg3_found = false;
//   bool is_msg4_found = false;

//   //msg content definition.
// }nrscope_dl_ul_exchange;


class RachDecoder{
  public:
    // bool new_subframe_flag;
    asn1::rrc_nr::sib1_s sib1; 
    srsran_carrier_nr_t base_carrier;
    // nrscope_dl_ul_exchange rach_dl_ul_info;
    prach_nr_config_t prach_cfg_nr;
    srsran_prach_t prach;
    srsran_prach_cfg_t prach_cfg;

    uint16_t *ra_rnti;
    uint32_t nof_ra_rnti;

    uint8_t* data_pdcch;

    RachDecoder();
    ~RachDecoder();
    int RachDecoderInit(asn1::rrc_nr::sib1_s sib1_input, srsran_carrier_nr_t carrier_input);

    int decode_and_parse_msg4_from_slot(srsran_ue_dl_nr_t* ue_dl,
                                        srsran_slot_cfg_t* slot,
                                        srsran_ue_dl_nr_sratescs_info arg_scs,
                                        srsran_carrier_nr_t* base_carrier,
                                        srsran_sch_hl_cfg_nr_t* pdsch_hl_cfg,
                                        srsran_softbuffer_rx_t* softbuffer,
                                        asn1::rrc_nr::rrc_setup_s* rrc_setup,
                                        asn1::rrc_nr::cell_group_cfg_s* master_cell_group,
                                        uint16_t* known_rntis,
                                        uint32_t* nof_known_rntis);
};