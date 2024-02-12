#ifndef RADIO_H
#define RADIO_H

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
#include "cstdio"

#include "nrscope/hdr/nrscope_def.h"
#include "nrscope/hdr/rach_decoder.h"
#include "nrscope/hdr/sibs_decoder.h"
#include "nrscope/hdr/dci_decoder.h"
// #include "nrscope/hdr/to_sunshine.h"
// #include "nrscope/hdr/to_moonlight.h"
#include "nrscope/hdr/harq_tracking.h"

struct coreset0_args{
  uint32_t                    offset_rb       = 0; // CORESET offset rb
  double                      coreset0_lower_freq_hz = 0.0;
  double                      coreset0_center_freq_hz = 0.0;
  int                         n_0 = 0;
  int                         sfn_c = 0;
};

class Radio{
  public:
    int rf_index;
    srsran::rf_args_t                             rf_args;
    std::shared_ptr<srsran::radio>                r;
    std::shared_ptr<srsran::radio_interface_phy>  radio;

    srslog::basic_logger&                         logger;

    srsran::rf_buffer_t                           rf_buffer_t;
    cf_t*                                         rx_buffer;
    cf_t*                                         rx_uplink_buffer;
    uint32_t                                      slot_sz;
    srsran::rf_timestamp_t                        last_rx_time;

    cell_searcher_args_t                          args_t;
    srsue::phy_nr_sa::cell_search_args_t          cs_args;
    srsran_subcarrier_spacing_t                   ssb_scs;
    srsran::srsran_band_helper                    bands;
    srsran_ue_dl_nr_sratescs_info                 arg_scs;

    srsue::nr::cell_search                        srsran_searcher; // from cell_search.cc
    srsue::nr::cell_search::cfg_t                 srsran_searcher_cfg_t;
    srsue::nr::cell_search::args_t                srsran_searcher_args_t;
    srsue::nr::cell_search::ret_t                 cs_ret;
    uint32_t                                      nof_trials;
    cell_search_result_t                          cell;

    coreset0_args                                 coreset0_args_t;
    srsran_coreset_t                              coreset0_t;
    srsran_search_space_t*                        search_space;
    srsran_ue_sync_nr_args_t                      ue_sync_nr_args;
    srsran_ue_sync_nr_cfg_t                       sync_cfg;

    srsue::nr::slot_sync                          slot_synchronizer;
    srsran_ue_sync_nr_t                           ue_sync_nr;
    srsran_ue_sync_nr_outcome_t                   outcome;
    srsran_softbuffer_rx_t                        softbuffer;
    
    double pointA;
    srsran_dci_dl_nr_t dci_1_0_coreset0;
    srsran_pdcch_cfg_nr_t  pdcch_cfg;   // pdcch config for cell search and RACH
    srsran_sch_hl_cfg_nr_t pdsch_hl_cfg;
    srsran_sch_hl_cfg_nr_t pusch_hl_cfg;
    srsran_dci_cfg_nr_t dci_cfg;
    srsran_ue_dl_nr_t ue_dl;
    srsran_ue_dl_nr_args_t ue_dl_args;
    srsran_ssb_cfg_t ssb_cfg;
    srsran_sch_cfg_nr_t pdsch_cfg;  

    asn1::rrc_nr::sib1_s sib1;
    asn1::rrc_nr::sib2_s sib2;

    RachDecoder rach_decoder; // processing for uplink in rach
    SIBsDecoder sibs_decoder;
    DCIDecoder dci_decoder;

    srsran_search_space_t* ra_search_space;
    srsran::mac_rar_pdu_nr rar_pdu; // rar pdu
    asn1::rrc_nr::rrc_setup_s rrc_setup;
    asn1::rrc_nr::cell_group_cfg_s master_cell_group;

    // srsran_search_space_t* tc_search_space; // used with TC-RNTI
    srsran_pdcch_cfg_nr_t  pdcch_cfg_data; // pdcch config for data communication

    uint16_t rnti_lists[200];
    uint16_t known_rntis[200];
    uint32_t nof_known_rntis;

    std::string dl_log_name;
    std::string ul_log_name;

    int tbs_array[200]; // maintain a 1s window to calculate kbps
    int spare_array[200]; // maintain a 1s window to calculate spare kbps
    int mcs_array[200];

    HarqTracker harq_tracker;

    Radio();  //constructor
    ~Radio(); //deconstructor

    int RadioThread();
    int RadioInitandStart();
    int DecodeMIB();
    int SyncandDownlinkInit();
    int InitTaskScheduler();

    int SIB1Loop(); // downlink channel
    int MSG2and4Loop(); // downlink RAR
    int DCILoop();

    void WriteLogFile(std::string filename, const char* szString);
    // int RadioStop();
};


#endif