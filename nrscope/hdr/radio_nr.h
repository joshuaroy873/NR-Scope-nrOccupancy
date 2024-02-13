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
#include "nrscope/hdr/task_scheduler.h"

class Radio{
  public:
    int rf_index;
    srsran::rf_args_t                             rf_args;
    std::shared_ptr<srsran::radio>                raido_shared;
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
    srsran_ue_sync_nr_args_t                      ue_sync_nr_args;
    srsran_ue_sync_nr_cfg_t                       sync_cfg;

    srsue::nr::slot_sync                          slot_synchronizer;
    srsran_ue_sync_nr_t                           ue_sync_nr;
    srsran_ue_sync_nr_outcome_t                   outcome;
    
    srsran_ssb_cfg_t ssb_cfg;

    TaskSchedulerNRScope task_scheduler_nrscope;
    RachDecoder rach_decoder; // processing for uplink in rach
    SIBsDecoder sibs_decoder;
    DCIDecoder dci_decoder;
    HarqTracker harq_tracker;

    uint16_t known_rntis[200];
    uint32_t nof_known_rntis;

    std::string dl_log_name;
    std::string ul_log_name;

    int tbs_array[200]; // maintain a 1s window to calculate kbps
    int spare_array[200]; // maintain a 1s window to calculate spare kbps
    int mcs_array[200];

    Radio();  //constructor
    ~Radio(); //deconstructor

    int RadioThread();
    int RadioInitandStart();
    int SyncandDownlinkInit();

    int StartTasks();

    int RadioCapture();

    int SIB1Loop(); // Decode SIB 1
    int MSG2and4Loop(); // Decode MSG 4
    int DCILoop(); // Decode DCIs 

    void WriteLogFile(std::string filename, const char* szString);
    // int RadioStop();
};


#endif