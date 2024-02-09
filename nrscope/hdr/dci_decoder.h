#include "nrscope/hdr/nrscope_def.h"

typedef struct _DCIFeedback{
  std::vector<srsran_dci_dl_nr_t> dl_dcis;
  std::vector<srsran_dci_ul_nr_t> ul_dcis;
  std::vector<srsran_sch_cfg_nr_t> dl_grants;
  std::vector<srsran_sch_cfg_nr_t> ul_grants;
  std::vector<int> spare_dl_prbs;
  std::vector<int> spare_dl_tbs;
  std::vector<int> spare_dl_bits;
  std::vector<int> spare_ul_prbs;
  std::vector<int> spare_ul_tbs;
  std::vector<int> spare_ul_bits;

  int nof_dl_used_prbs = 0;
  int nof_dl_spare_prbs = 0;
  int nof_ul_used_prbs = 0;
  int nof_ul_spare_prbs = 0;

  int processing_time_us = 0;

} DCIFeedback;

class DCIDecoder{
  public:
    std::vector<float> dl_prb_rate;
    std::vector<float> ul_prb_rate;

    std::vector<float> dl_prb_bits_rate;
    std::vector<float> ul_prb_bits_rate;

    srsran_dci_dl_nr_t dci_dl_tmp[4];
    srsran_dci_ul_nr_t dci_ul_tmp[4];

    srsran_ue_dl_nr_t* ue_dl_tmp;
    srsran_slot_cfg_t* slot_tmp;

    srsran_dci_dl_nr_t* dci_dl;
    srsran_dci_ul_nr_t* dci_ul;

    DCIDecoder(uint32_t nof_known_rntis);
    ~DCIDecoder();

    DCIFeedback decode_and_parse_dci_from_slot(srsran_ue_dl_nr_t* ue_dl,
                                      srsran_slot_cfg_t* slot,
                                      srsran_ue_dl_nr_sratescs_info arg_scs,
                                      srsran_carrier_nr_t* carrier_dl,
                                      srsran_sch_hl_cfg_nr_t* pdsch_hl_cfg,
                                      srsran_sch_hl_cfg_nr_t* pusch_hl_cfg,
                                      srsran_softbuffer_rx_t* softbuffer,
                                      asn1::rrc_nr::rrc_setup_s* rrc_setup,
                                      asn1::rrc_nr::cell_group_cfg_s* master_cell_group,
                                      uint16_t* known_rntis,
                                      uint32_t nof_known_rntis,
                                      uint16_t target_rnti);
};