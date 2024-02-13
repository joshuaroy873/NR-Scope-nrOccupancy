#include "nrscope/hdr/nrscope_def.h"
#include "nrscope/hdr/task_scheduler.h"

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
    srsran_carrier_nr_t base_carrier;
    srsran_ue_dl_nr_sratescs_info arg_scs;
    srsran_sch_hl_cfg_nr_t pdsch_hl_cfg;
    srsran_sch_hl_cfg_nr_t pusch_hl_cfg;
    srsran_softbuffer_rx_t softbuffer;
    srsran_dci_cfg_nr_t dci_cfg;
    srsran_ue_dl_nr_args_t ue_dl_args;
    srsran_pdcch_cfg_nr_t  pdcch_cfg;
    
    cell_search_result_t cell;
    srsran_coreset_t coreset0_t;
    srsran_search_space_t* search_space;

    asn1::rrc_nr::sib1_s sib1;
    asn1::rrc_nr::cell_group_cfg_s master_cell_group;
    asn1::rrc_nr::rrc_setup_s rrc_setup;

    srsran_carrier_nr_t carrier_dl;
    srsran_ue_dl_nr_t ue_dl_dci;
    srsue::nr::cell_search::cfg_t srsran_searcher_cfg_t;
    srsran_coreset_t coreset1_t; 


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

    int dci_decoder_and_reception_init(srsran_ue_dl_nr_sratescs_info arg_scs_,
                                       srsran_carrier_nr_t* base_carrier_,
                                       cell_search_result_t cell_,
                                       cf_t* input[SRSRAN_MAX_PORTS],
                                       srsran_coreset_t* coreset0_t_,
                                       asn1::rrc_nr::cell_group_cfg_s master_cell_group_,
                                       asn1::rrc_nr::rrc_setup_s rrc_setup_,
                                       srsue::nr::cell_search::cfg_t srsran_searcher_cfg_t_,
                                       asn1::rrc_nr::sib1_s sib1_);

    DCIFeedback decode_and_parse_dci_from_slot(srsran_slot_cfg_t* slot,
                                               uint16_t* known_rntis,
                                               uint32_t nof_known_rntis, 
                                               uint16_t target_rnti);

    int dci_thread(TaskSchedulerNRScope* task_scheduler_nrscope);
};