#include "nrscope/hdr/radio_nr.h"

std::mutex lock_radio_nr;

Radio::Radio() : 
  logger(srslog::fetch_basic_logger("PHY")), 
  srsran_searcher(logger),
  rf_buffer_t(1),
  slot_synchronizer(logger),
  task_scheduler_nrscope(),
  rach_decoder(),
  sibs_decoder(),
  dci_decoder(4),
  harq_tracker(4)
{
  raido_shared = std::make_shared<srsran::radio>();
  radio = nullptr;

  nof_trials = 100;
  srsran_searcher_args_t.max_srate_hz = 30.72e6;
  srsran_searcher_args_t.ssb_min_scs = srsran_subcarrier_spacing_15kHz;
  srsran_searcher.init(srsran_searcher_args_t);

  ssb_cfg = {};

  ue_sync_nr = {};
  outcome = {};
  ue_sync_nr_args = {};
  sync_cfg = {};

  nof_known_rntis = 0;
}

Radio::~Radio() {
}

int Radio::RadioThread(){
  RadioInitandStart();
  return SRSRAN_SUCCESS;
}

int Radio::RadioInitandStart(){

  srsran_assert(raido_shared->init(rf_args, nullptr) == SRSRAN_SUCCESS, "Failed Radio initialisation");
  radio = std::move(raido_shared);

  // Cell Searcher parameters  
  args_t.srate_hz = rf_args.srate_hz;
  rf_args.dl_freq = args_t.base_carrier.dl_center_frequency_hz;
  args_t.rf_device_name = rf_args.device_name;
  args_t.rf_device_args = rf_args.device_args;
  args_t.rf_log_level = "info";
  args_t.rf_rx_gain_dB = rf_args.rx_gain;
  args_t.rf_freq_offset_Hz = rf_args.freq_offset;
  args_t.phy_log_level   = "warning";
  args_t.stack_log_level = "warning";
  args_t.duration_ms = 1000;

  // Set sampling rate
  radio->set_rx_srate(rf_args.srate_hz);
  // Set DL center frequency
  radio->set_rx_freq(0, (double)rf_args.dl_freq);
  // Set Rx gain
  radio->set_rx_gain(rf_args.rx_gain);
  
  args_t.set_ssb_from_band(ssb_scs);
  args_t.base_carrier.scs = args_t.ssb_scs;
  if(args_t.duplex_mode == SRSRAN_DUPLEX_MODE_TDD){
    args_t.base_carrier.ul_center_frequency_hz = args_t.base_carrier.dl_center_frequency_hz;
  }

  // Allocate receive buffer
  slot_sz = (uint32_t)(rf_args.srate_hz / 1000.0f / SRSRAN_NOF_SLOTS_PER_SF_NR(ssb_scs));
  rx_buffer = srsran_vec_cf_malloc(SRSRAN_NOF_SLOTS_PER_SF_NR(args_t.ssb_scs) * slot_sz);
  srsran_vec_zero(rx_buffer, SRSRAN_NOF_SLOTS_PER_SF_NR(args_t.ssb_scs) * slot_sz);

  cs_args.center_freq_hz = args_t.base_carrier.dl_center_frequency_hz;
  cs_args.ssb_freq_hz = args_t.base_carrier.dl_center_frequency_hz;
  cs_args.ssb_scs = args_t.ssb_scs;
  cs_args.ssb_pattern = args_t.ssb_pattern;
  cs_args.duplex_mode = args_t.duplex_mode;

  uint32_t band = bands.get_band_from_dl_freq_Hz(args_t.base_carrier.dl_center_frequency_hz);
  double ssb_bw_hz = SRSRAN_SSB_BW_SUBC * cs_args.ssb_scs;
  double ssb_center_freq_min_hz = args_t.base_carrier.dl_center_frequency_hz - (args_t.srate_hz * 0.7 - ssb_bw_hz) / 2.0;
  double ssb_center_freq_max_hz = args_t.base_carrier.dl_center_frequency_hz + (args_t.srate_hz * 0.7 - ssb_bw_hz) / 2.0;
  uint32_t ssb_scs_hz = SRSRAN_SUBC_SPACING_NR(cs_args.ssb_scs);
  
  srsran::srsran_band_helper::sync_raster_t ss = bands.get_sync_raster(band, cs_args.ssb_scs);
  srsran_assert(ss.valid(), "Invalid synchronization raster");

  while (not ss.end()) {
    // Get SSB center frequency
    cs_args.ssb_freq_hz = ss.get_frequency();
    // Advance SSB frequency raster
    ss.next();

    // Calculate frequency offset between the base-band center frequency and the SSB absolute frequency
    uint32_t offset_hz = (uint32_t)std::abs(std::round(cs_args.ssb_freq_hz - args_t.base_carrier.dl_center_frequency_hz));

    // The SSB absolute frequency is invalid if it is outside the range and 
    // the offset is NOT multiple of the subcarrier spacing
    if ((cs_args.ssb_freq_hz < ssb_center_freq_min_hz) or (cs_args.ssb_freq_hz > ssb_center_freq_max_hz) or
        (offset_hz % ssb_scs_hz != 0)) {
      // Skip this frequency
      continue;
    }

    srsran_searcher_cfg_t.srate_hz = args_t.srate_hz;
    srsran_searcher_cfg_t.center_freq_hz = cs_args.ssb_freq_hz; //args_t.base_carrier.dl_center_frequency_hz;
    srsran_searcher_cfg_t.ssb_freq_hz = cs_args.ssb_freq_hz;
    srsran_searcher_cfg_t.ssb_scs = args_t.ssb_scs;
    srsran_searcher_cfg_t.ssb_pattern = args_t.ssb_pattern;
    srsran_searcher_cfg_t.duplex_mode = args_t.duplex_mode;
    if (not srsran_searcher.start(srsran_searcher_cfg_t)) {
      std::cout << "Searcher: failed to start cell search" << std::endl;
      return NR_FAILURE;
    }
    // Set the searching frequency to ssb_freq
    // Because the srsRAN implementation use the center_freq_hz for cell searching
    cs_args.center_freq_hz = cs_args.ssb_freq_hz;
    std::cout << cs_args.ssb_freq_hz << std::endl;
    args_t.base_carrier.ssb_center_freq_hz = cs_args.ssb_freq_hz;

    radio->release_freq(0);
    radio->set_rx_freq(0, srsran_searcher_cfg_t.ssb_freq_hz);

    srsran::rf_buffer_t rf_buffer = {};
    rf_buffer.set_nof_samples(slot_sz);
    rf_buffer.set(0, rx_buffer + slot_sz);

    for(uint32_t trial=0; trial < nof_trials; trial++){
      if (trial == 0) {
        srsran_vec_cf_zero(rx_buffer, slot_sz);
      }
      srsran_vec_cf_copy(rx_buffer, rx_buffer + slot_sz, slot_sz);

      srsran::rf_timestamp_t& rf_timestamp = last_rx_time;

      if (not radio->rx_now(rf_buffer, rf_timestamp)) {
        return SRSRAN_ERROR;
      }
      *(last_rx_time.get_ptr(0)) = rf_timestamp.get(0);

      cs_ret = srsran_searcher.run_slot(rx_buffer, slot_sz);
      if(cs_ret.result == srsue::nr::cell_search::ret_t::CELL_FOUND ){
        // printf("found cell in this slot\n");
        break;
      }
    }
    if(cs_ret.result == srsue::nr::cell_search::ret_t::CELL_FOUND){
      std::cout << "Cell Found!" << std::endl;
      std::cout << "N_id: " << cs_ret.ssb_res.N_id << std::endl;
      std::cout << "Decoding MIB..." << std::endl;

      if(task_scheduler_nrscope.decode_mib(&args_t, &cs_ret, &srsran_searcher_cfg_t) < SRSRAN_SUCCESS){
        ERROR("Error init task scheduler");
        return NR_FAILURE;
      }

      if(SyncandDownlinkInit() < SRSRAN_SUCCESS){
        ERROR("Error decoding MIB");
        return NR_FAILURE;
      }

      if(InitTaskScheduler() < SRSASN_SUCCESS){
        ERROR("Error initializing task scheduler");
        return NR_FAILURE;
      }

      if(RadioCapture() < SRSASN_SUCCESS){
        ERROR("Error in RadioCapture");
        return NR_FAILURE;
      }

      if(SIB1Loop() < SRSRAN_SUCCESS){
        ERROR("Error in SIB1Loop");
        return NR_FAILURE;
      }

      if(MSG2and4Loop() < SRSRAN_SUCCESS){
        ERROR("Error in MSG2and4Loop");
        return NR_FAILURE;
      }

      if(DCILoop() < SRSRAN_SUCCESS){
        ERROR("Error in DCILoop");
        return NR_FAILURE;
      }
    }
  }
  return SRSRAN_SUCCESS;
}

static int slot_sync_recv_callback(void* ptr, cf_t** buffer, uint32_t nsamples, srsran_timestamp_t* ts)
{
  if (ptr == nullptr) {
    return SRSRAN_ERROR_INVALID_INPUTS;
  }
  srsran::radio* radio = (srsran::radio*)ptr;

  cf_t* buffer_ptr[SRSRAN_MAX_CHANNELS] = {};
  buffer_ptr[0]                         = buffer[0];
  srsran::rf_buffer_t rf_buffer(buffer_ptr, nsamples);

  srsran::rf_timestamp_t a;
  srsran::rf_timestamp_t &rf_timestamp = a;
  *ts = a.get(0);

  return radio->rx_now(rf_buffer, rf_timestamp);
}

int Radio::SyncandDownlinkInit(){
  //***** DL args Config Start *****//
  rf_buffer_t = srsran::rf_buffer_t(rx_buffer, SRSRAN_NOF_SLOTS_PER_SF_NR(task_scheduler_nrscope.args_t.ssb_scs) * slot_sz);

  // it appears the srsRAN is build on 15kHz scs, we need to use the srate and 
  // scs to calculate the correct subframe size 
  arg_scs.srate = task_scheduler_nrscope.args_t.srate_hz;
  arg_scs.scs = task_scheduler_nrscope.cell.mib.scs_common;

  arg_scs.coreset_offset_scs = (cs_args.ssb_freq_hz - task_scheduler_nrscope.coreset0_args_t.coreset0_center_freq_hz) / task_scheduler_nrscope.cell.abs_pdcch_scs;// + 12;
  arg_scs.coreset_slot = (uint32_t)task_scheduler_nrscope.coreset0_args_t.n_0;
  arg_scs.phase_diff_first_second_half = 0;
  //***** DL args Config End *****//

  //***** Slot Sync Start *****//
  ue_sync_nr_args.max_srate_hz    = srsran_searcher_args_t.max_srate_hz;
  ue_sync_nr_args.min_scs         = srsran_searcher_args_t.ssb_min_scs;
  ue_sync_nr_args.nof_rx_channels = 1;
  ue_sync_nr_args.disable_cfo     = false;
  ue_sync_nr_args.pbch_dmrs_thr   = 0.5;
  ue_sync_nr_args.cfo_alpha       = 0.1;
  ue_sync_nr_args.recv_obj        = radio.get();
  ue_sync_nr_args.recv_callback   = slot_sync_recv_callback;

  if (srsran_ue_sync_nr_init(&ue_sync_nr, &ue_sync_nr_args) < SRSRAN_SUCCESS) {
    std::cout << "Error initiating UE SYNC NR object" << std::endl;
    logger.error("Error initiating UE SYNC NR object");
    return SRSRAN_ERROR;
  }

  // Be careful of all the frequency setting (SSB/center downlink and etc.)!
  ssb_cfg.srate_hz       = task_scheduler_nrscope.args_t.srate_hz;
  ssb_cfg.center_freq_hz = cs_args.ssb_freq_hz;
  ssb_cfg.ssb_freq_hz    = cs_args.ssb_freq_hz;
  ssb_cfg.scs            = cs_args.ssb_scs;
  ssb_cfg.pattern        = cs_args.ssb_pattern;
  ssb_cfg.duplex_mode    = cs_args.duplex_mode;
  ssb_cfg.periodicity_ms = 20; // for all in FR1

  sync_cfg.N_id = task_scheduler_nrscope.cs_ret.ssb_res.N_id;
  sync_cfg.ssb = ssb_cfg;
  sync_cfg.ssb.srate_hz = task_scheduler_nrscope.args_t.srate_hz;
  if (srsran_ue_sync_nr_set_cfg(&ue_sync_nr, &sync_cfg) < SRSRAN_SUCCESS) {
    printf("SYNC: failed to set cell configuration for N_id %d", sync_cfg.N_id);
    logger.error("SYNC: failed to set cell configuration for N_id %d", sync_cfg.N_id);
    return SRSRAN_ERROR;
  }

  return SRSRAN_SUCCESS;
}

int Radio::InitTaskScheduler(){
  task_scheduler_nrscope.decoders_init();
  
  return SRSASN_SUCCESS;
}

int Radio::SIB1Loop(){
  std::cout << "SIB1 Loop Starts..." << std::endl;     

  if(sibs_decoder.sib_decoder_and_reception_init(arg_scs, &(task_scheduler_nrscope.args_t.base_carrier), task_scheduler_nrscope.cell, 
     rf_buffer_t.to_cf_t(), &(task_scheduler_nrscope.coreset0_t)) < SRSASN_SUCCESS){
    ERROR("SIBsDecoder Init Error");
    return NR_FAILURE;
  }

  // Do sync and buffer moving.
  while(true){
    outcome.timestamp = last_rx_time.get(0);
    if (srsran_ue_sync_nr_zerocopy(&ue_sync_nr, rf_buffer_t.to_cf_t(), &outcome) < SRSRAN_SUCCESS) {
      std::cout << "SYNC: error in zerocopy" << std::endl;
      logger.error("SYNC: error in zerocopy");
      return false;
    }
    // If in sync, update slot index. The synced data is stored in rf_buffer_t.to_cf_t()[0]
    if (outcome.in_sync){
      std::cout << "System frame idx: " << outcome.sfn << std::endl;
      std::cout << "Subframe idx: " << outcome.sf_idx << std::endl;
      // Find right slot position for SIB 1.
      for(int slot_idx = 0; slot_idx < SRSRAN_NOF_SLOTS_PER_SF_NR(arg_scs.scs); slot_idx++){
        srsran_slot_cfg_t slot = {0};
        slot.idx = (outcome.sf_idx) * SRSRAN_NSLOTS_PER_FRAME_NR(arg_scs.scs) / 10 + slot_idx;
        // Move rx_buffer
        srsran_vec_cf_copy(rx_buffer, rx_buffer + slot_idx*slot_sz, slot_sz);    
        if((coreset0_args_t.sfn_c == 0 && outcome.sfn % 2 == 0) || 
           (coreset0_args_t.sfn_c == 1 && outcome.sfn % 2 == 1)) {
          if((outcome.sf_idx) == (uint32_t)(coreset0_args_t.n_0 / 2) || 
             (outcome.sf_idx) == (uint32_t)(coreset0_args_t.n_0 / 2 + 1)){
            
            if(sibs_decoder.decode_and_parse_sib1_from_slot(&slot, &sib1) == SRSASN_SUCCESS){
              return SRSASN_SUCCESS;
            }else{
              continue;
            }
          } 
        }
      } 
    } 
  }
  return SRSRAN_SUCCESS;
}

int Radio::MSG2and4Loop(){
  std::cout << "MSG2 and 4 Loop starts...(please ignore the errors messages)" << std::endl;
  rach_decoder.rach_decoder_init(sib1, args_t.base_carrier);

  if(rach_decoder.rach_reception_init(arg_scs, &(task_scheduler_nrscope.args_t.base_carrier), task_scheduler_nrscope.cell, 
     rf_buffer_t.to_cf_t(), &(task_scheduler_nrscope.coreset0_t)) < SRSASN_SUCCESS){
    ERROR("RACHDecoder Init Error");
    return NR_FAILURE;
  }

  while(true){
    outcome.timestamp = last_rx_time.get(0);

    if (srsran_ue_sync_nr_zerocopy(&ue_sync_nr, rf_buffer_t.to_cf_t(), &outcome) < SRSRAN_SUCCESS) {
      std::cout << "SYNC: error in zerocopy" << std::endl;
      logger.error("SYNC: error in zerocopy");
      return false;
    }

    if (outcome.in_sync){
      std::cout << "System frame idx: " << outcome.sfn << std::endl;
      std::cout << "Subframe idx: " << outcome.sf_idx << std::endl;

      for(int slot_idx = 0; slot_idx < SRSRAN_NOF_SLOTS_PER_SF_NR(arg_scs.scs); slot_idx++){
        srsran_slot_cfg_t slot = {0};
        slot.idx = (outcome.sf_idx) * SRSRAN_NOF_SLOTS_PER_SF_NR(arg_scs.scs) + slot_idx;
        std::cout << "Slot idx: " << slot.idx << std::endl;
        // Processing for each slot
        srsran_vec_cf_copy(rx_buffer, rx_buffer + slot_idx*slot_sz, slot_sz);

        if(rach_decoder.decode_and_parse_msg4_from_slot(&slot, &rrc_setup, &master_cell_group, known_rntis, &nof_known_rntis) == SRSASN_SUCCESS){
          return SRSASN_SUCCESS;
        }else{
          continue;
        }
      }
    }
  }
  return SRSRAN_SUCCESS;
}

int Radio::DCILoop(){
  std::cout << "DCI Loop starts...(please ignore the error messages)" << std::endl;

  if(dci_decoder.dci_decoder_and_reception_init(arg_scs, &(task_scheduler_nrscope.args_t.base_carrier), task_scheduler_nrscope.cell, 
     rf_buffer_t.to_cf_t(), &(task_scheduler_nrscope.coreset0_t), master_cell_group, rrc_setup , task_scheduler_nrscope.srsran_searcher_cfg_t, sib1) < SRSASN_SUCCESS){
    ERROR("RACHDecoder Init Error");
    return NR_FAILURE;
  }

  nof_known_rntis = 4;
  known_rntis[1] = 17026;
  known_rntis[2] = 17034;
  known_rntis[3] = 17043;

  char buff_dl[1024];
  char buff_ul[1024];
  char dci_eval_dl[1024];
  char dci_eval_ul[1024];
  char buff_feedback[200];

  int* rnti_dl_prbs = (int*)malloc(sizeof(int) * nof_known_rntis);
  int* rnti_dl_tbs = (int*)malloc(sizeof(int) * nof_known_rntis);
  int* rnti_dl_bits = (int*)malloc(sizeof(int) * nof_known_rntis);

  int* rnti_ul_prbs = (int*)malloc(sizeof(int) * nof_known_rntis);
  int* rnti_ul_tbs = (int*)malloc(sizeof(int) * nof_known_rntis);
  int* rnti_ul_bits = (int*)malloc(sizeof(int) * nof_known_rntis);

  bool* is_dl_new_data = (bool*)malloc(sizeof(bool) * nof_known_rntis);
  bool* is_ul_new_data = (bool*)malloc(sizeof(bool) * nof_known_rntis);

  float mean_mcs = 0;
  int all_dl_prb = 0;
  
  int tbs_array_idx = 0;
  while(true){
    auto timestamp = get_now_timestamp_in_double(); 
    outcome.timestamp = last_rx_time.get(0);
    if (srsran_ue_sync_nr_zerocopy(&ue_sync_nr, rf_buffer_t.to_cf_t(), &outcome) < SRSRAN_SUCCESS) {
      std::cout << "SYNC: error in zerocopy" << std::endl;
      logger.error("SYNC: error in zerocopy");
      return NR_FAILURE;
    }
    // If in sync, update slot index. The synced data is stored in rf_buffer_t.to_cf_t()[0]
    if (outcome.in_sync){      
      for(int slot_idx = 0; slot_idx < SRSRAN_NOF_SLOTS_PER_SF_NR(arg_scs.scs); slot_idx++){
        srsran_slot_cfg_t slot = {0};
        timestamp = timestamp + 0.0005 * slot_idx;
        slot.idx = (outcome.sf_idx) * SRSRAN_NSLOTS_PER_FRAME_NR(arg_scs.scs) / 10 + slot_idx;
        std::cout << std::endl;
        std::cout << "<----- System frame idx: " << outcome.sfn << " ----->" << std::endl;
        std::cout << "<----- Subframe idx: " << outcome.sf_idx << " ----->" << std::endl;
        std::cout << "<----- Slot idx: " << slot.idx << " ----->" << std::endl;
        std::cout << std::endl;
        srsran_vec_cf_copy(rx_buffer, rx_buffer + slot_idx*slot_sz, slot_sz);
        auto result = dci_decoder.decode_and_parse_dci_from_slot(&slot, known_rntis, nof_known_rntis, known_rntis[0]);

        // move the following into logger class
        for(uint32_t idx = 0; idx < nof_known_rntis; idx++){
          is_dl_new_data[idx] = false;
          is_ul_new_data[idx] = false;
        }

        for(uint32_t idx = 0; idx < nof_known_rntis; idx++){
          if(result.dl_grants[idx].grant.rnti == known_rntis[idx]){
            is_dl_new_data[idx] = harq_tracker.is_new_data(idx, result.dl_grants[idx].grant.tb[0].ndi, result.dl_dcis[idx].pid, true);
          }
        }

        for(uint32_t idx = 0; idx < nof_known_rntis; idx++){
          if(result.ul_grants[idx].grant.rnti == known_rntis[idx]){
            is_ul_new_data[idx] = harq_tracker.is_new_data(idx, result.ul_grants[idx].grant.tb[0].ndi, result.ul_dcis[idx].pid, false);
          }
        }

        // prb, tbs, bits calculation
        for(uint32_t idx = 0; idx < nof_known_rntis; idx++){
          if(result.dl_grants[idx].grant.rnti == known_rntis[idx] && is_dl_new_data[idx]){
            rnti_dl_prbs[idx] = result.dl_grants[idx].grant.L * result.dl_grants[idx].grant.nof_prb;
            rnti_dl_tbs[idx] = result.dl_grants[idx].grant.tb[0].tbs + result.dl_grants[idx].grant.tb[1].tbs;
            rnti_dl_bits[idx] = result.dl_grants[idx].grant.tb[0].nof_bits + result.dl_grants[idx].grant.tb[1].nof_bits;
          }else{
            rnti_dl_prbs[idx] = 0;
            rnti_dl_tbs[idx] = 0;
            rnti_dl_bits[idx] = 0;
          }
        }  

        for(uint32_t idx = 0; idx < nof_known_rntis; idx++){
          if(result.ul_grants[idx].grant.rnti == known_rntis[idx] && is_ul_new_data[idx]){
            rnti_ul_prbs[idx] = result.ul_grants[idx].grant.L * result.ul_grants[idx].grant.nof_prb;
            rnti_ul_tbs[idx] = result.ul_grants[idx].grant.tb[0].tbs + result.ul_grants[idx].grant.tb[1].tbs;
            rnti_ul_bits[idx] = result.ul_grants[idx].grant.tb[0].nof_bits + result.ul_grants[idx].grant.tb[1].nof_bits;
          }else{
            rnti_ul_prbs[idx] = 0;
            rnti_ul_tbs[idx] = 0;
            rnti_ul_bits[idx] = 0;
          }
        }              

        printf("writing log..\n");
        // int buff_dl_pos = snprintf(buff_dl, sizeof(buff_dl), "%f ", timestamp);
        // int buff_ul_pos = snprintf(buff_ul, sizeof(buff_ul), "%f ", timestamp);
        // for(uint32_t idx = 0; idx < nof_known_rntis; idx++){
        //   buff_dl_pos += snprintf(buff_dl + buff_dl_pos, sizeof(buff_dl) - buff_dl_pos, "%d %d %d %d %d %d ", rnti_dl_prbs[idx], 
        //     rnti_dl_tbs[idx], rnti_dl_bits[idx], result.spare_dl_prbs[idx], result.spare_dl_tbs[idx], result.spare_dl_bits[idx]);
        //   buff_ul_pos += snprintf(buff_ul + buff_ul_pos, sizeof(buff_ul) - buff_ul_pos, "%d %d %d %d %d %d ", rnti_ul_prbs[idx], 
        //     rnti_ul_tbs[idx], rnti_ul_bits[idx], result.spare_ul_prbs[idx], result.spare_ul_tbs[idx], result.spare_ul_bits[idx]);
        // }
        // buff_dl_pos += snprintf(buff_dl + buff_dl_pos, sizeof(buff_dl) - buff_dl_pos, "%d ", result.processing_time_us);
        // buff_ul_pos += snprintf(buff_ul + buff_ul_pos, sizeof(buff_ul) - buff_ul_pos, "%d ", result.processing_time_us);

        int buff_dl_pos = snprintf(buff_dl, sizeof(buff_dl), "%f ", timestamp);
        int buff_ul_pos = snprintf(buff_ul, sizeof(buff_ul), "%f ", timestamp);
        for(uint32_t idx = 0; idx < nof_known_rntis; idx++){
          buff_dl_pos += snprintf(buff_dl + buff_dl_pos, sizeof(buff_dl) - buff_dl_pos, "%d ", rnti_dl_prbs[idx]);
          buff_ul_pos += snprintf(buff_ul + buff_ul_pos, sizeof(buff_ul) - buff_ul_pos, "%d ", rnti_ul_prbs[idx]);
        }
        // buff_dl_pos += snprintf(buff_dl + buff_dl_pos, sizeof(buff_dl) - buff_dl_pos, "%d ", result.processing_time_us);
        // buff_ul_pos += snprintf(buff_ul + buff_ul_pos, sizeof(buff_ul) - buff_ul_pos, "%d ", result.processing_time_us);
      
        // WriteLogFile(dl_log_name, buff_dl);
        // WriteLogFile(ul_log_name, buff_ul);

        // DCI evaluation, DCI detection rate, PRB, TBS
        // int dci_dl_pos = snprintf(dci_eval_dl, sizeof(dci_eval_dl), "%f %d.%d ", timestamp, outcome.sfn, outcome.sf_idx*SRSRAN_NOF_SLOTS_PER_SF_NR(arg_scs.scs)+slot_idx);
        // int dci_ul_pos = snprintf(dci_eval_ul, sizeof(dci_eval_dl), "%f %d.%d ", timestamp, outcome.sfn, outcome.sf_idx*SRSRAN_NOF_SLOTS_PER_SF_NR(arg_scs.scs)+slot_idx);
        // for(uint32_t idx = 0; idx < nof_known_rntis; idx++){
        //   dci_dl_pos += snprintf(dci_eval_dl + dci_dl_pos, sizeof(dci_eval_dl) - dci_dl_pos, "%u %d %d ", known_rntis[idx], rnti_dl_prbs[idx], rnti_dl_tbs[idx]);
        //   dci_ul_pos += snprintf(dci_eval_ul + dci_ul_pos, sizeof(dci_eval_ul) - dci_ul_pos, "%u %d %d ", known_rntis[idx], rnti_ul_prbs[idx], rnti_ul_tbs[idx]);
        // }

        // WriteLogFile(dl_log_name, dci_eval_dl);
        // WriteLogFile(ul_log_name, dci_eval_ul);

        // determine if it's a uplink slot, a lazy way: if all the dl prbs are 0, we treat it as an uplink frame 
        // (we should us the ul/dl frame structure in SIB 1)
        all_dl_prb = 0;
        for(uint32_t i = 0; i < nof_known_rntis; i++){
          all_dl_prb += rnti_dl_prbs[i];
        }

      } 
    }
  }
  return SRSRAN_SUCCESS;
}

// Logger class's function
void Radio::WriteLogFile(std::string filename, const char* szString)
{
  FILE* pFile = fopen(filename.c_str(), "a");
  fprintf(pFile, "%s\n", szString);
  fclose(pFile);
}