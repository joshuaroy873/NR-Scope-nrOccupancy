#include "nrscope/hdr/radio_nr.h"

std::mutex lock_radio_nr;

Radio::Radio() : 
  logger(srslog::fetch_basic_logger("PHY")), 
  srsran_searcher(logger),
  rf_buffer_t(1),
  slot_synchronizer(logger),
  rach_decoder(),
  sibs_decoder(),
  dci_decoder(4),
  harq_tracker(4)
{
  r = std::make_shared<srsran::radio>();
  radio = nullptr;

  nof_trials = 100;
  srsran_searcher_args_t.max_srate_hz = 30.72e6;
  srsran_searcher_args_t.ssb_min_scs = srsran_subcarrier_spacing_15kHz;
  srsran_searcher.init(srsran_searcher_args_t);

  pdcch_cfg = {};
  pdsch_hl_cfg = {};
  pusch_hl_cfg = {};
  dci_cfg = {};
  ue_dl = {};
  ue_dl_args = {};
  ssb_cfg = {};

  ue_sync_nr = {};
  softbuffer = {};
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

  srsran_assert(r->init(rf_args, nullptr) == SRSRAN_SUCCESS, "Failed Radio initialisation");
  radio = std::move(r);

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

      rx_uplink_buffer = srsran_vec_cf_malloc(SRSRAN_NOF_SLOTS_PER_SF_NR(args_t.ssb_scs) * slot_sz);
      
      if(DecodeMIB() < SRSRAN_SUCCESS){
        ERROR("Error decoding MIB");
        return NR_FAILURE;
      }

      if(SyncandDownlinkInit() < SRSRAN_SUCCESS){
        ERROR("Error decoding MIB");
        return NR_FAILURE;
      }

      if(InitTaskScheduler() < SRSRAN_SUCCESS){
        ERROR("Error init task scheduler");
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

int Radio::DecodeMIB(){
  args_t.base_carrier.pci = cs_ret.ssb_res.N_id;

  if(srsran_pbch_msg_nr_mib_unpack(&cs_ret.ssb_res.pbch_msg, &cell.mib) < SRSRAN_SUCCESS){
    ERROR("Error decoding MIB");
    return SRSRAN_ERROR;
  }
  printf("MIB payload: ");
  for (int i =0; i<SRSRAN_PBCH_MSG_NR_MAX_SZ; i++){
    printf("%hhu ", cs_ret.ssb_res.pbch_msg.payload[i]);
  }
  printf("\n");
  std::cout << "cell.mib.ssb_offset: " << cell.mib.ssb_offset << std::endl;
  std::cout << "((int)cs_ret.ssb_res.pbch_msg.k_ssb_msb): " << ((int)cs_ret.ssb_res.pbch_msg.k_ssb_msb) << std::endl;

  cell.k_ssb = cell.mib.ssb_offset; // already added the msb of k_ssb
  rf_buffer_t = srsran::rf_buffer_t(rx_buffer, SRSRAN_NOF_SLOTS_PER_SF_NR(args_t.ssb_scs) * slot_sz);

  // srsran_coreset0_ssb_offset returns the offset_rb relative to ssb
  // nearly all bands in FR1 have min bandwidth 5 or 10 MHz, so there are only 5 entries here.  
  coreset0_args_t.offset_rb = srsran_coreset0_ssb_offset(cell.mib.coreset0_idx, 
    args_t.ssb_scs, cell.mib.scs_common);
  // std::cout << "Coreset offset in rbs related to SSB: " << coreset0_args_t.offset_rb << std::endl;

  coreset0_t = {};
  // srsran_coreset_zero returns the offset_rb relative to pointA
  if(srsran_coreset_zero(cs_ret.ssb_res.N_id, 
                         0, //cell.k_ssb * SRSRAN_SUBC_SPACING_NR(srsran_subcarrier_spacing_15kHz), 
                         args_t.ssb_scs, 
                         cell.mib.scs_common, 
                         cell.mib.coreset0_idx, 
                         &coreset0_t) == SRSRAN_SUCCESS){
    char freq_res_str[SRSRAN_CORESET_FREQ_DOMAIN_RES_SIZE] = {};
    char coreset_info[512] = {};
    srsran_coreset_to_str(&coreset0_t, coreset_info, sizeof(coreset_info));
    printf("Coreset parameter: %s", coreset_info);
  }
  // To find the position of coreset0, we need to use the offset between SSB and CORESET0,
  // because we don't know the ssb_pointA_freq_offset_Hz yet required by the srsran_coreset_zero function.
  // coreset0_t low bound freq = ssb center freq - 120 * scs (half of sc in ssb) - 
  // ssb_subcarrierOffset(from MIB) * scs - entry->offset_rb * 12(sc in one rb) * scs
  cell.abs_ssb_scs = SRSRAN_SUBC_SPACING_NR(args_t.ssb_scs);
  cell.abs_pdcch_scs = SRSRAN_SUBC_SPACING_NR(cell.mib.scs_common);
  
  srsran::srsran_band_helper bands;
  coreset0_args_t.coreset0_lower_freq_hz = srsran_searcher_cfg_t.ssb_freq_hz - (SRSRAN_SSB_BW_SUBC / 2) *
    cell.abs_ssb_scs - coreset0_args_t.offset_rb * NRSCOPE_NSC_PER_RB_NR * cell.abs_pdcch_scs - 
    cell.k_ssb * SRSRAN_SUBC_SPACING_NR(srsran_subcarrier_spacing_15kHz); // defined by standards
  coreset0_args_t.coreset0_center_freq_hz = coreset0_args_t.coreset0_lower_freq_hz + srsran_coreset_get_bw(&coreset0_t) / 2 * 
    cell.abs_pdcch_scs * NRSCOPE_NSC_PER_RB_NR;

  // std::cout << "k_ssb: " << cell.k_ssb << std::endl;
  // std::cout << "ssb freq hz: " << srsran_searcher_cfg_t.ssb_freq_hz << std::endl;
  // std::cout << "coreset0_lower freq hz: " << coreset0_args_t.coreset0_lower_freq_hz << std::endl;
  // std::cout << "coreset0 center freq hz: " << coreset0_args_t.coreset0_center_freq_hz << std::endl;
  // std::cout << "ssb_lower_freq hz: " << srsran_searcher_cfg_t.ssb_freq_hz - (SRSRAN_SSB_BW_SUBC / 2) *
  //   cell.abs_ssb_scs << std::endl;
  // std::cout << "coreset0_bw: " << srsran_coreset_get_bw(&coreset0_t) << std::endl;
  // std::cout << "coreset0_nof_symb: " << coreset0_t.duration << std::endl;
  // std::cout << "scs_common: " << cell.mib.scs_common << std::endl;
  // // the total used prb for coreset0 is 48 -> 17.28 MHz bw
  
  // std::cout << "mib pdcch-configSIB1.coreset0_idx: " << cell.mib.coreset0_idx << std::endl;
  // std::cout << "mib pdcch-configSIB1.searchSpaceZero: " << cell.mib.ss0_idx << std::endl;
  // printf("mib ssb-index: %u\n", cell.mib.ssb_idx);

  coreset_zero_t_f_entry_nrscope coreset_zero_cfg;
  // get coreset_zero's position in time domain
  // check table 38.213, 13-11, because USRP can only support FR1.
  if(coreset_zero_t_f_nrscope(cell.mib.ss0_idx, cell.mib.ssb_idx, coreset0_t.duration, &coreset_zero_cfg) < 
    SRSRAN_SUCCESS){
    ERROR("Error checking table 13-11");
    return SRSRAN_ERROR;
  }

  cell.u = (int)args_t.ssb_scs; 
  coreset0_args_t.n_0 = (coreset_zero_cfg.O * (int)pow(2, cell.u) + 
    (int)floor(cell.mib.ssb_idx * coreset_zero_cfg.M)) % SRSRAN_NSLOTS_X_FRAME_NR(cell.u);
  // sfn_c = 0, in even system frame, sfn_c = 1, in odd system frame    
  coreset0_args_t.sfn_c = (int)(floor(coreset_zero_cfg.O * pow(2, cell.u) + 
    floor(cell.mib.ssb_idx * coreset_zero_cfg.M)) / SRSRAN_NSLOTS_X_FRAME_NR(cell.u)) % 2;
  args_t.base_carrier.nof_prb = srsran_coreset_get_bw(&coreset0_t);

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
  // this part of bw is set according to the coreset0_bw
  dci_cfg.bwp_dl_initial_bw   = 275;
  dci_cfg.bwp_ul_initial_bw   = 275;
  dci_cfg.bwp_dl_active_bw    = 275;
  dci_cfg.bwp_ul_active_bw    = 275;
  dci_cfg.monitor_common_0_0  = true;
  dci_cfg.monitor_0_0_and_1_0 = true;
  dci_cfg.monitor_0_1_and_1_1 = true;
  // set coreset0 bandwidth
  dci_cfg.coreset0_bw = srsran_coreset_get_bw(&coreset0_t);

  ue_dl_args.nof_rx_antennas               = 1;
  ue_dl_args.pdsch.sch.disable_simd        = false;
  ue_dl_args.pdsch.sch.decoder_use_flooded = false;
  ue_dl_args.pdsch.measure_evm             = true;
  ue_dl_args.pdcch.disable_simd            = false;
  ue_dl_args.pdcch.measure_evm             = true;
  ue_dl_args.nof_max_prb                   = 275;

  // Setup PDSCH DMRS (also signaled through MIB)
  pdsch_hl_cfg.typeA_pos = cell.mib.dmrs_typeA_pos;
  pusch_hl_cfg.typeA_pos = cell.mib.dmrs_typeA_pos;
  
  // pdcch_cfg.coreset_present[0] = true;
  // search_space = &pdcch_cfg.search_space[0];
  // pdcch_cfg.search_space_present[0]   = true;
  // search_space->id                    = 0;
  // search_space->coreset_id            = 0;
  // search_space->type                  = srsran_search_space_type_common_0;
  // search_space->formats[0]            = srsran_dci_format_nr_1_0;
  // search_space->nof_formats           = 1;
  // for (uint32_t L = 0; L < SRSRAN_SEARCH_SPACE_NOF_AGGREGATION_LEVELS_NR; L++) {
  //   search_space->nof_candidates[L] = srsran_pdcch_nr_max_candidates_coreset(&coreset0_t, L);
  // }
  // pdcch_cfg.coreset[0] = coreset0_t; 

  // it appears the srsRAN is build on 15kHz scs, we need to use the srate and 
  // scs to calculate the correct subframe size 
  arg_scs.srate = args_t.srate_hz;
  arg_scs.scs = cell.mib.scs_common;

  arg_scs.coreset_offset_scs = (cs_args.ssb_freq_hz - coreset0_args_t.coreset0_center_freq_hz) / cell.abs_pdcch_scs;// + 12;
  arg_scs.coreset_slot = (uint32_t)coreset0_args_t.n_0;
  arg_scs.phase_diff_first_second_half = 0;
  // std::cout << "arg_scs.coreset_offset_scs: " << arg_scs.coreset_offset_scs << std::endl; 

  // // we need to set ue_dl.sf_symbols here to set the out_buffer correctly.
  // if (srsran_ue_dl_nr_init_nrscope(&ue_dl, rf_buffer_t.to_cf_t(), &ue_dl_args, arg_scs)) {
  //   ERROR("Error UE DL");
  //   return SRSRAN_ERROR;
  // }

  // if (srsran_ue_dl_nr_set_carrier_nrscope(&ue_dl, &args_t.base_carrier, arg_scs)) {
  //   ERROR("Error setting SCH NR carrier");
  //   return SRSRAN_ERROR;
  // }

  // if (srsran_ue_dl_nr_set_pdcch_config(&ue_dl, &pdcch_cfg, &dci_cfg)) {
  //   ERROR("Error setting CORESET");
  //   return SRSRAN_ERROR;
  // }

  // if (srsran_softbuffer_rx_init_guru(&softbuffer, SRSRAN_SCH_NR_MAX_NOF_CB_LDPC, SRSRAN_LDPC_MAX_LEN_ENCODED_CB) <
  //     SRSRAN_SUCCESS) {
  //   ERROR("Error init soft-buffer");
  //   return SRSRAN_ERROR;
  // }

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
  ssb_cfg.srate_hz       = args_t.srate_hz;
  ssb_cfg.center_freq_hz = cs_args.ssb_freq_hz;
  ssb_cfg.ssb_freq_hz    = cs_args.ssb_freq_hz;
  ssb_cfg.scs            = cs_args.ssb_scs;
  ssb_cfg.pattern        = cs_args.ssb_pattern;
  ssb_cfg.duplex_mode    = cs_args.duplex_mode;
  ssb_cfg.periodicity_ms = 20; // for all in FR1

  sync_cfg.N_id = cs_ret.ssb_res.N_id;
  sync_cfg.ssb = ssb_cfg;
  sync_cfg.ssb.srate_hz = args_t.srate_hz;
  if (srsran_ue_sync_nr_set_cfg(&ue_sync_nr, &sync_cfg) < SRSRAN_SUCCESS) {
    printf("SYNC: failed to set cell configuration for N_id %d", sync_cfg.N_id);
    logger.error("SYNC: failed to set cell configuration for N_id %d", sync_cfg.N_id);
    return SRSRAN_ERROR;
  }

  return SRSRAN_SUCCESS;
}

int Radio::InitTaskScheduler(){


  return SRSASN_SUCCESS;
}

int Radio::SIB1Loop(){
  std::cout << "SIB1 Loop Starts..." << std::endl;     

  if(sibs_decoder.sib_decoder_and_reception_init(arg_scs, &(args_t.base_carrier), cell, 
     rf_buffer_t.to_cf_t(), &dci_cfg, &ue_dl_args, &coreset0_t) < SRSASN_SUCCESS){
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

  char str[1024] = {};

  // pdcch_cfg.search_space_present[0]      = true;
  // pdcch_cfg.search_space[0].id           = 1;
  // pdcch_cfg.search_space[0].coreset_id   = 0;
  // pdcch_cfg.search_space[0].type         = srsran_search_space_type_common_1;
  // pdcch_cfg.search_space[0].formats[0]   = srsran_dci_format_nr_1_0;
  // pdcch_cfg.search_space[0].nof_formats  = 1;

  // pdcch_cfg.coreset[0] = coreset0_t; 

  // pdcch_cfg.search_space[0].nof_candidates[0] = sib1.serving_cell_cfg_common.dl_cfg_common.
  //                                      init_dl_bwp.pdcch_cfg_common.setup().common_search_space_list[0].
  //                                      nrof_candidates.aggregation_level1;
  // pdcch_cfg.search_space[0].nof_candidates[1] = sib1.serving_cell_cfg_common.dl_cfg_common.
  //                                      init_dl_bwp.pdcch_cfg_common.setup().common_search_space_list[0].
  //                                      nrof_candidates.aggregation_level2;
  // pdcch_cfg.search_space[0].nof_candidates[2] = sib1.serving_cell_cfg_common.dl_cfg_common.
  //                                      init_dl_bwp.pdcch_cfg_common.setup().common_search_space_list[0].
  //                                      nrof_candidates.aggregation_level4;
  // pdcch_cfg.search_space[0].nof_candidates[3] = sib1.serving_cell_cfg_common.dl_cfg_common.
  //                                      init_dl_bwp.pdcch_cfg_common.setup().common_search_space_list[0].
  //                                      nrof_candidates.aggregation_level8;
  // pdcch_cfg.search_space[0].nof_candidates[4] = sib1.serving_cell_cfg_common.dl_cfg_common.
  //                                      init_dl_bwp.pdcch_cfg_common.setup().common_search_space_list[0].
  //                                      nrof_candidates.aggregation_level16;

  if(rach_decoder.rach_reception_init(arg_scs, &(args_t.base_carrier), cell, 
     rf_buffer_t.to_cf_t(), &dci_cfg, &ue_dl_args, &coreset0_t) < SRSASN_SUCCESS){
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
  char str[1024] = {};

  // parameter settings
  pdcch_cfg.search_space_present[0]      = true;
  pdcch_cfg.search_space_present[1]      = false;
  pdcch_cfg.search_space_present[2]      = false;
  pdcch_cfg.ra_search_space_present      = false;
  if(master_cell_group.sp_cell_cfg.sp_cell_cfg_ded.init_dl_bwp.pdcch_cfg.is_setup()){
    pdcch_cfg.search_space[0].id = master_cell_group.sp_cell_cfg.sp_cell_cfg_ded.init_dl_bwp.pdcch_cfg.setup().
                                   search_spaces_to_add_mod_list[0].search_space_id;
    pdcch_cfg.search_space[0].coreset_id = master_cell_group.sp_cell_cfg.sp_cell_cfg_ded.init_dl_bwp.pdcch_cfg.setup().
                                           ctrl_res_set_to_add_mod_list[0].ctrl_res_set_id;
    pdcch_cfg.search_space[0].type = srsran_search_space_type_ue;
    if(master_cell_group.sp_cell_cfg.sp_cell_cfg_ded.init_dl_bwp.pdcch_cfg.setup().
       search_spaces_to_add_mod_list[0].search_space_type.ue_specific().dci_formats.formats0_minus1_and_minus1_minus1){
      pdcch_cfg.search_space[0].formats[0] = srsran_dci_format_nr_1_1;
      pdcch_cfg.search_space[0].formats[1] = srsran_dci_format_nr_0_1;
      dci_cfg.monitor_0_0_and_1_0 = false;
      dci_cfg.monitor_common_0_0 = false;
    }else if(master_cell_group.sp_cell_cfg.sp_cell_cfg_ded.init_dl_bwp.pdcch_cfg.setup().search_spaces_to_add_mod_list[0].
             search_space_type.ue_specific().dci_formats.formats0_minus0_and_minus1_minus0){
      pdcch_cfg.search_space[0].formats[0] = srsran_dci_format_nr_1_0; 
      pdcch_cfg.search_space[0].formats[1] = srsran_dci_format_nr_0_0;
      dci_cfg.monitor_0_1_and_1_1 = false;
    }
    pdcch_cfg.search_space[0].nof_formats  = 2;
    pdcch_cfg.coreset[0] = coreset0_t; 
  }else{
    // Use some default settings
    pdcch_cfg.search_space[0].id           = 2;
    pdcch_cfg.search_space[0].coreset_id   = 1;
    pdcch_cfg.search_space[0].type         = srsran_search_space_type_ue;
    pdcch_cfg.search_space[0].formats[0]   = srsran_dci_format_nr_1_1; // from RRCSetup
    pdcch_cfg.search_space[0].formats[1]   = srsran_dci_format_nr_0_1; // from RRCSetup
    dci_cfg.monitor_0_0_and_1_0 = false;
    dci_cfg.monitor_common_0_0 = false;
    pdcch_cfg.search_space[0].nof_formats  = 2;
    pdcch_cfg.coreset[0] = coreset0_t; 
  }
  
  // all the Coreset information is from RRCSetup
  srsran_coreset_t coreset1_t = {}; 
  coreset1_t.id = master_cell_group.sp_cell_cfg.sp_cell_cfg_ded.init_dl_bwp.pdcch_cfg.setup().
                  ctrl_res_set_to_add_mod_list[0].ctrl_res_set_id; 
  coreset1_t.duration = master_cell_group.sp_cell_cfg.sp_cell_cfg_ded.init_dl_bwp.pdcch_cfg.setup().
                        ctrl_res_set_to_add_mod_list[0].dur;
  for(int i = 0; i < 45; i++){
    coreset1_t.freq_resources[i] = master_cell_group.sp_cell_cfg.sp_cell_cfg_ded.init_dl_bwp.pdcch_cfg.
                                   setup().ctrl_res_set_to_add_mod_list[0].freq_domain_res.get(45-i-1);
  }
  coreset1_t.offset_rb = 0;
  if (master_cell_group.sp_cell_cfg.sp_cell_cfg_ded.init_dl_bwp.pdcch_cfg.
      setup().ctrl_res_set_to_add_mod_list[0].precoder_granularity == 
      asn1::rrc_nr::ctrl_res_set_s::precoder_granularity_opts::same_as_reg_bundle){
    coreset1_t.precoder_granularity = srsran_coreset_precoder_granularity_reg_bundle;
  }else if (master_cell_group.sp_cell_cfg.sp_cell_cfg_ded.init_dl_bwp.pdcch_cfg.
            setup().ctrl_res_set_to_add_mod_list[0].precoder_granularity == 
            asn1::rrc_nr::ctrl_res_set_s::precoder_granularity_opts::all_contiguous_rbs){
    coreset1_t.precoder_granularity = srsran_coreset_precoder_granularity_contiguous;
  }
  if (master_cell_group.sp_cell_cfg.sp_cell_cfg_ded.init_dl_bwp.pdcch_cfg.
      setup().ctrl_res_set_to_add_mod_list[0].cce_reg_map_type.type() == 
      asn1::rrc_nr::ctrl_res_set_s::cce_reg_map_type_c_::types_opts::non_interleaved ||
      master_cell_group.sp_cell_cfg.sp_cell_cfg_ded.init_dl_bwp.pdcch_cfg.
      setup().ctrl_res_set_to_add_mod_list[0].cce_reg_map_type.type() == 
      asn1::rrc_nr::ctrl_res_set_s::cce_reg_map_type_c_::types_opts::nulltype){
    coreset1_t.mapping_type = srsran_coreset_mapping_type_non_interleaved; 
    coreset1_t.interleaver_size = srsran_coreset_bundle_size_n2; // doesn't matter, fill a random value
    coreset1_t.shift_index = 0; // doesn't matter, fill a random value
    coreset1_t.reg_bundle_size = srsran_coreset_bundle_size_n6; // doesen't matter, fill a random value
  }else{
    coreset1_t.mapping_type = srsran_coreset_mapping_type_interleaved; 
    switch(master_cell_group.sp_cell_cfg.sp_cell_cfg_ded.init_dl_bwp.pdcch_cfg.
           setup().ctrl_res_set_to_add_mod_list[0].cce_reg_map_type.interleaved().interleaver_size){
      case asn1::rrc_nr::ctrl_res_set_s::cce_reg_map_type_c_::interleaved_s_::interleaver_size_e_::n2:
        coreset1_t.interleaver_size = srsran_coreset_bundle_size_n2;
        break;
      case asn1::rrc_nr::ctrl_res_set_s::cce_reg_map_type_c_::interleaved_s_::interleaver_size_e_::n3:
        coreset1_t.interleaver_size = srsran_coreset_bundle_size_n3;
        break;
      case asn1::rrc_nr::ctrl_res_set_s::cce_reg_map_type_c_::interleaved_s_::interleaver_size_e_::n6:
        coreset1_t.interleaver_size = srsran_coreset_bundle_size_n6;
        break;
      case asn1::rrc_nr::ctrl_res_set_s::cce_reg_map_type_c_::interleaved_s_::reg_bundle_size_e_::nulltype:
        ERROR("Interleaved size not found, set as bundle_size_n6\n");
        coreset1_t.reg_bundle_size = srsran_coreset_bundle_size_n6;
        break;
      default:
        ERROR("Interleaved size not found, set as bundle_size_n6\n");
        coreset1_t.reg_bundle_size = srsran_coreset_bundle_size_n6;
        break;
    }
    coreset1_t.shift_index = master_cell_group.sp_cell_cfg.sp_cell_cfg_ded.init_dl_bwp.pdcch_cfg.
     setup().ctrl_res_set_to_add_mod_list[0].cce_reg_map_type.interleaved().shift_idx;
    switch(master_cell_group.sp_cell_cfg.sp_cell_cfg_ded.init_dl_bwp.pdcch_cfg.
           setup().ctrl_res_set_to_add_mod_list[0].cce_reg_map_type.interleaved().reg_bundle_size){
      case asn1::rrc_nr::ctrl_res_set_s::cce_reg_map_type_c_::interleaved_s_::reg_bundle_size_e_::n2:
        coreset1_t.reg_bundle_size = srsran_coreset_bundle_size_n2;
        break;
      case asn1::rrc_nr::ctrl_res_set_s::cce_reg_map_type_c_::interleaved_s_::reg_bundle_size_e_::n3:
        coreset1_t.reg_bundle_size = srsran_coreset_bundle_size_n3;
        break;
      case asn1::rrc_nr::ctrl_res_set_s::cce_reg_map_type_c_::interleaved_s_::reg_bundle_size_e_::n6:
        coreset1_t.reg_bundle_size = srsran_coreset_bundle_size_n6;
        break;
      case asn1::rrc_nr::ctrl_res_set_s::cce_reg_map_type_c_::interleaved_s_::reg_bundle_size_e_::nulltype:
        ERROR("Reg bundle size not found, set as bundle_size_n6\n");
        coreset1_t.reg_bundle_size = srsran_coreset_bundle_size_n6;
        break;
      default:
        ERROR("Reg bundle size not found, set as bundle_size_n6\n");
        coreset1_t.reg_bundle_size = srsran_coreset_bundle_size_n6;
        break;
    }
  }
  coreset1_t.dmrs_scrambling_id_present = master_cell_group.sp_cell_cfg.sp_cell_cfg_ded.init_dl_bwp.pdcch_cfg.
                                          setup().ctrl_res_set_to_add_mod_list[0].pdcch_dmrs_scrambling_id_present;
  if (coreset1_t.dmrs_scrambling_id_present){
    coreset1_t.dmrs_scrambling_id = master_cell_group.sp_cell_cfg.sp_cell_cfg_ded.init_dl_bwp.pdcch_cfg.
                                    setup().ctrl_res_set_to_add_mod_list[0].pdcch_dmrs_scrambling_id;
  }
  printf("coreset_dmrs_scrambling id: %u\n", coreset1_t.dmrs_scrambling_id);

  pdcch_cfg.coreset[1] = coreset1_t;
  pdcch_cfg.coreset_present[1] = true;

  char coreset_info[512] = {};
  srsran_coreset_to_str(&coreset1_t, coreset_info, sizeof(coreset_info));
  printf("Coreset %d parameter: %s", coreset1_t.id, coreset_info);

  // For FR1 offset_to_point_a uses prbs with 15kHz scs. 
  double pointA = srsran_searcher_cfg_t.ssb_freq_hz - (SRSRAN_SSB_BW_SUBC / 2) *
    cell.abs_ssb_scs - cell.k_ssb * SRSRAN_SUBC_SPACING_NR(srsran_subcarrier_spacing_15kHz) 
    - sib1.serving_cell_cfg_common.dl_cfg_common.freq_info_dl.offset_to_point_a * 
    SRSRAN_SUBC_SPACING_NR(srsran_subcarrier_spacing_15kHz) * NRSCOPE_NSC_PER_RB_NR;
  std::cout << "pointA: " << pointA << std::endl;

  double coreset1_center_freq_hz = pointA + srsran_coreset_get_bw(&coreset1_t) / 2 * 
    cell.abs_pdcch_scs * NRSCOPE_NSC_PER_RB_NR;
  std::cout << "previous offset: " << arg_scs.coreset_offset_scs << std::endl;
  arg_scs.coreset_offset_scs = (cs_args.ssb_freq_hz - coreset1_center_freq_hz) / cell.abs_pdcch_scs;
  std::cout << "current offset: " << arg_scs.coreset_offset_scs << std::endl;
  
   // set ra search space directly from the RRC Setup
  pdcch_cfg.search_space[0].nof_candidates[0] = master_cell_group.sp_cell_cfg.sp_cell_cfg_ded.init_dl_bwp.
                                                pdcch_cfg.setup().search_spaces_to_add_mod_list[0].
                                                nrof_candidates.aggregation_level1;
  pdcch_cfg.search_space[0].nof_candidates[1] = master_cell_group.sp_cell_cfg.sp_cell_cfg_ded.init_dl_bwp.
                                                pdcch_cfg.setup().search_spaces_to_add_mod_list[0].
                                                nrof_candidates.aggregation_level2;
  pdcch_cfg.search_space[0].nof_candidates[2] = master_cell_group.sp_cell_cfg.sp_cell_cfg_ded.init_dl_bwp.
                                                pdcch_cfg.setup().search_spaces_to_add_mod_list[0].
                                                nrof_candidates.aggregation_level4;
  pdcch_cfg.search_space[0].nof_candidates[3] = master_cell_group.sp_cell_cfg.sp_cell_cfg_ded.init_dl_bwp.
                                                pdcch_cfg.setup().search_spaces_to_add_mod_list[0].
                                                nrof_candidates.aggregation_level8;
  pdcch_cfg.search_space[0].nof_candidates[4] = master_cell_group.sp_cell_cfg.sp_cell_cfg_ded.init_dl_bwp.
                                                pdcch_cfg.setup().search_spaces_to_add_mod_list[0].
                                                nrof_candidates.aggregation_level16;

  dci_cfg.carrier_indicator_size = 0; // for carrier aggregation, we don't consider this situation.
  dci_cfg.enable_sul = false; // by default
  dci_cfg.enable_hopping = false; // by default

  /// Format 0_1 specific configuration (for PUSCH only)
  ///< Number of UL BWPs excluding the initial UL BWP, mentioned in the TS as N_BWP_RRC
  dci_cfg.nof_ul_bwp = master_cell_group.sp_cell_cfg.sp_cell_cfg_ded.ul_cfg.ul_bwp_to_add_mod_list.size(); 
  ///< Number of dedicated PUSCH time domain resource assigment, set to 0 for default
  dci_cfg.nof_ul_time_res = master_cell_group.sp_cell_cfg.sp_cell_cfg_ded.ul_cfg.init_ul_bwp.pusch_cfg.setup().
                            pusch_time_domain_alloc_list_present ? 
                            master_cell_group.sp_cell_cfg.sp_cell_cfg_ded.ul_cfg.init_ul_bwp.pusch_cfg.setup().
                            pusch_time_domain_alloc_list.setup().size() : 
                            (sib1.serving_cell_cfg_common.ul_cfg_common_present ? 
                            (sib1.serving_cell_cfg_common.ul_cfg_common.init_ul_bwp.pusch_cfg_common_present ?
                            sib1.serving_cell_cfg_common.ul_cfg_common.init_ul_bwp.pusch_cfg_common.setup().pusch_time_domain_alloc_list.size() : 0) : 0);     
  dci_cfg.nof_srs = master_cell_group.sp_cell_cfg.sp_cell_cfg_ded.ul_cfg.init_ul_bwp.srs_cfg_present ? 
                    master_cell_group.sp_cell_cfg.sp_cell_cfg_ded.ul_cfg.init_ul_bwp.srs_cfg.setup().
                    srs_res_to_add_mod_list.size() : 0;             ///< Number of configured SRS resources

  ///< Set to the maximum number of layers for PUSCH
  if(master_cell_group.sp_cell_cfg.sp_cell_cfg_ded.ul_cfg.init_ul_bwp.pusch_cfg.setup().max_rank_present){
    dci_cfg.nof_ul_layers = master_cell_group.sp_cell_cfg.sp_cell_cfg_ded.ul_cfg.init_ul_bwp.pusch_cfg.setup().max_rank;       
  } else {
    dci_cfg.nof_ul_layers = 1;
  }
  
  ///< determined by maxCodeBlockGroupsPerTransportBlock for PUSCH
  if(master_cell_group.sp_cell_cfg.sp_cell_cfg_ded.pdsch_serving_cell_cfg.setup().code_block_group_tx_present){
    switch(master_cell_group.sp_cell_cfg.sp_cell_cfg_ded.pdsch_serving_cell_cfg.setup().
           code_block_group_tx.setup().max_code_block_groups_per_transport_block){
      case asn1::rrc_nr::pdsch_code_block_group_tx_s::max_code_block_groups_per_transport_block_opts::n2:
        dci_cfg.pusch_nof_cbg = 2;
        break;
      case asn1::rrc_nr::pdsch_code_block_group_tx_s::max_code_block_groups_per_transport_block_opts::n4:
        dci_cfg.pusch_nof_cbg = 4;
        break;
      case asn1::rrc_nr::pdsch_code_block_group_tx_s::max_code_block_groups_per_transport_block_opts::n6:
        dci_cfg.pusch_nof_cbg = 6;
        break;
      case asn1::rrc_nr::pdsch_code_block_group_tx_s::max_code_block_groups_per_transport_block_opts::n8:
        dci_cfg.pusch_nof_cbg = 8;
        break;
      default:
        ERROR("None type or not found pusch_nof_cbg, setting to 0.\n");
        dci_cfg.pusch_nof_cbg = 0;
        break;
    }
  } else {
    dci_cfg.pusch_nof_cbg = 0;
  }
  
  if(master_cell_group.sp_cell_cfg.sp_cell_cfg_ded.csi_meas_cfg_present){
    dci_cfg.report_trigger_size = master_cell_group.sp_cell_cfg.sp_cell_cfg_ded.csi_meas_cfg.setup().report_trigger_size;
  }else{
    dci_cfg.report_trigger_size = 0; ///< determined by reportTriggerSize
  }

  if(master_cell_group.sp_cell_cfg.sp_cell_cfg_ded.ul_cfg.init_ul_bwp.pusch_cfg.setup().transform_precoder == 
     asn1::rrc_nr::pusch_cfg_s::transform_precoder_opts::disabled){
    dci_cfg.enable_transform_precoding = false;      ///< Set to true if PUSCH transform precoding is enabled
  } else if (master_cell_group.sp_cell_cfg.sp_cell_cfg_ded.ul_cfg.init_ul_bwp.pusch_cfg.setup().transform_precoder == 
     asn1::rrc_nr::pusch_cfg_s::transform_precoder_opts::enabled){
    dci_cfg.enable_transform_precoding = true;      ///< Set to true if PUSCH transform precoding is enabled
  }
  
  dci_cfg.pusch_tx_config_non_codebook = false;    ///< Set to true if PUSCH txConfig is set to non-codebook
  if(master_cell_group.sp_cell_cfg.sp_cell_cfg_ded.ul_cfg.init_ul_bwp.pusch_cfg.setup().tx_cfg_present){
    if(master_cell_group.sp_cell_cfg.sp_cell_cfg_ded.ul_cfg.init_ul_bwp.pusch_cfg.setup().tx_cfg.value == 
      master_cell_group.sp_cell_cfg.sp_cell_cfg_ded.ul_cfg.init_ul_bwp.pusch_cfg.setup().tx_cfg.codebook){
      dci_cfg.pusch_tx_config_non_codebook = false;
    }else if (master_cell_group.sp_cell_cfg.sp_cell_cfg_ded.ul_cfg.init_ul_bwp.pusch_cfg.setup().tx_cfg.value == 
      master_cell_group.sp_cell_cfg.sp_cell_cfg_ded.ul_cfg.init_ul_bwp.pusch_cfg.setup().tx_cfg.non_codebook){
      dci_cfg.pusch_tx_config_non_codebook = true;
    }
  }
  
  if(master_cell_group.sp_cell_cfg.sp_cell_cfg_ded.ul_cfg.init_ul_bwp.pusch_cfg.setup().dmrs_ul_for_pusch_map_type_a.setup().phase_tracking_rs_present){
    dci_cfg.pusch_ptrs = true;                      ///< Set to true if PT-RS are enabled for PUSCH transmissionß
  } else {
    dci_cfg.pusch_ptrs = false;
  }

  if(master_cell_group.sp_cell_cfg.sp_cell_cfg_ded.ul_cfg.init_ul_bwp.pusch_cfg.setup().uci_on_pusch.setup().beta_offsets_present){
    if(master_cell_group.sp_cell_cfg.sp_cell_cfg_ded.ul_cfg.init_ul_bwp.pusch_cfg.setup().uci_on_pusch.setup().beta_offsets.type() == 
       asn1::rrc_nr::uci_on_pusch_s::beta_offsets_c_::types_opts::dynamic_type){
      dci_cfg.pusch_dynamic_betas = true;             ///< Set to true if beta offsets operation is not semi-static
    } else if(master_cell_group.sp_cell_cfg.sp_cell_cfg_ded.ul_cfg.init_ul_bwp.pusch_cfg.setup().uci_on_pusch.setup().beta_offsets.type() == 
       asn1::rrc_nr::uci_on_pusch_s::beta_offsets_c_::types_opts::semi_static){
      dci_cfg.pusch_dynamic_betas = false;             ///< Set to true if beta offsets operation is not semi-static
    } else{
      dci_cfg.pusch_dynamic_betas = false;
    }
  }

  ///< PUSCH resource allocation type
  if(master_cell_group.sp_cell_cfg.sp_cell_cfg_ded.ul_cfg.init_ul_bwp.pusch_cfg_present){
    switch(master_cell_group.sp_cell_cfg.sp_cell_cfg_ded.ul_cfg.init_ul_bwp.pusch_cfg.setup().res_alloc){
      case asn1::rrc_nr::pusch_cfg_s::res_alloc_opts::res_alloc_type0:
        dci_cfg.pusch_alloc_type = srsran_resource_alloc_type0;
        break;
      case asn1::rrc_nr::pusch_cfg_s::res_alloc_opts::res_alloc_type1:
        dci_cfg.pusch_alloc_type = srsran_resource_alloc_type1;
        break;
      case asn1::rrc_nr::pusch_cfg_s::res_alloc_opts::dynamic_switch:
        dci_cfg.pusch_alloc_type = srsran_resource_alloc_dynamic;
        break;
      case asn1::rrc_nr::pusch_cfg_s::res_alloc_opts::nulltype:
        ERROR("No PUSCH resource allocation found, use type 1\n");
        dci_cfg.pusch_alloc_type = srsran_resource_alloc_type1;
        break;
    }
  } else {
    ERROR("No PUSCH resource allocation found, use type 1\n");
    dci_cfg.pusch_alloc_type = srsran_resource_alloc_type1;
  }
  dci_cfg.pusch_dmrs_type = srsran_dmrs_sch_type_1;  ///< PUSCH DMRS type
  dci_cfg.pusch_dmrs_max_len = srsran_dmrs_sch_len_1; ///< PUSCH DMRS maximum length

  /// Format 1_1 specific configuration (for PDSCH only)
  switch (master_cell_group.phys_cell_group_cfg.pdsch_harq_ack_codebook){
    case asn1::rrc_nr::phys_cell_group_cfg_s::pdsch_harq_ack_codebook_opts::dynamic_value:
      dci_cfg.harq_ack_codebok = srsran_pdsch_harq_ack_codebook_dynamic;
      break;
    case asn1::rrc_nr::phys_cell_group_cfg_s::pdsch_harq_ack_codebook_opts::semi_static:
      dci_cfg.harq_ack_codebok = srsran_pdsch_harq_ack_codebook_semi_static;
      break;
    default:
      ERROR("harq_ack_code none.\n");
      dci_cfg.harq_ack_codebok = srsran_pdsch_harq_ack_codebook_none;
      break;
  }  

  // For DCI 0_1
  ///< Set to true if HARQ-ACK codebook is set to dynamic with 2 sub-codebooks
  dci_cfg.dynamic_dual_harq_ack_codebook = false;

  dci_cfg.nof_dl_bwp             = master_cell_group.sp_cell_cfg.sp_cell_cfg_ded.dl_bwp_to_add_mod_list.size();
  dci_cfg.nof_dl_time_res        = master_cell_group.sp_cell_cfg.sp_cell_cfg_ded.init_dl_bwp.pdsch_cfg.setup().
                                   pdsch_time_domain_alloc_list_present ? 
                                   master_cell_group.sp_cell_cfg.sp_cell_cfg_ded.init_dl_bwp.pdsch_cfg.setup().
                                   pdsch_time_domain_alloc_list.setup().size() : 0; 

  dci_cfg.nof_aperiodic_zp       = master_cell_group.sp_cell_cfg.sp_cell_cfg_ded.init_dl_bwp.pdsch_cfg.setup().
                                   aperiodic_zp_csi_rs_res_sets_to_add_mod_list.size();
  dci_cfg.pdsch_nof_cbg          = master_cell_group.sp_cell_cfg.sp_cell_cfg_ded.init_dl_bwp.pdsch_cfg.setup().
                                   max_nrof_code_words_sched_by_dci_present ? 
                                   master_cell_group.sp_cell_cfg.sp_cell_cfg_ded.init_dl_bwp.pdsch_cfg.setup().
                                   max_nrof_code_words_sched_by_dci : 0;
  dci_cfg.nof_dl_to_ul_ack       = master_cell_group.sp_cell_cfg.sp_cell_cfg_ded.ul_cfg.init_ul_bwp.pucch_cfg.setup().
                                   dl_data_to_ul_ack.size();
  dci_cfg.pdsch_inter_prb_to_prb = master_cell_group.sp_cell_cfg.sp_cell_cfg_ded.init_dl_bwp.pdsch_cfg.setup().
                                   vrb_to_prb_interleaver_present;
  dci_cfg.pdsch_rm_pattern1      = master_cell_group.sp_cell_cfg.sp_cell_cfg_ded.init_dl_bwp.pdsch_cfg.setup().rate_match_pattern_group1.size();
  dci_cfg.pdsch_rm_pattern2      = master_cell_group.sp_cell_cfg.sp_cell_cfg_ded.init_dl_bwp.pdsch_cfg.setup().rate_match_pattern_group2.size();
  dci_cfg.pdsch_2cw = false; // set to false initially and if maxofcodewordscheduledbydci is 2, set to true.
  if (master_cell_group.sp_cell_cfg.sp_cell_cfg_ded.init_dl_bwp.pdsch_cfg.setup().max_nrof_code_words_sched_by_dci_present){
    if (master_cell_group.sp_cell_cfg.sp_cell_cfg_ded.init_dl_bwp.pdsch_cfg.setup().max_nrof_code_words_sched_by_dci == 
        asn1::rrc_nr::pdsch_cfg_s::max_nrof_code_words_sched_by_dci_opts::n2){
      dci_cfg.pdsch_2cw = true;
    }
  }

  // Only consider one serving cell
  dci_cfg.multiple_scell = false; 
  dci_cfg.pdsch_tci = master_cell_group.sp_cell_cfg.sp_cell_cfg_ded.init_dl_bwp.pdcch_cfg.setup().ctrl_res_set_to_add_mod_list[0].
                      tci_present_in_dci_present ? true : false; 
  dci_cfg.pdsch_cbg_flush = master_cell_group.sp_cell_cfg.sp_cell_cfg_ded.pdsch_serving_cell_cfg.setup().
                            code_block_group_tx_present ? true : false; 

  dci_cfg.pdsch_dynamic_bundling = false; 
  if(master_cell_group.sp_cell_cfg.sp_cell_cfg_ded.init_dl_bwp.pdsch_cfg.setup().prb_bundling_type.type() == 
    asn1::rrc_nr::pdsch_cfg_s::prb_bundling_type_c_::types_opts::dynamic_bundling){
    dci_cfg.pdsch_dynamic_bundling = true;
    ERROR("PRB dynamic bundling not implemented, which can cause being unable to find DCIs. We are working on it.");
  }

  switch(master_cell_group.sp_cell_cfg.sp_cell_cfg_ded.init_dl_bwp.pdsch_cfg.setup().res_alloc){
    case asn1::rrc_nr::pdsch_cfg_s::res_alloc_opts::res_alloc_type0:
      dci_cfg.pdsch_alloc_type = srsran_resource_alloc_type0;
      break;
    case asn1::rrc_nr::pdsch_cfg_s::res_alloc_opts::res_alloc_type1:
      dci_cfg.pdsch_alloc_type = srsran_resource_alloc_type1;
      break;
    case asn1::rrc_nr::pdsch_cfg_s::res_alloc_opts::dynamic_switch:
      dci_cfg.pdsch_alloc_type = srsran_resource_alloc_dynamic;
      break;
    default:
      ERROR("pdsch alloc type not found, using type1.\n");
      dci_cfg.pdsch_alloc_type = srsran_resource_alloc_type1;
      break;
  }
  std::cout << "pdsch resource alloc: " << dci_cfg.pdsch_alloc_type << std::endl;

  // using default dmrs type = 1
  dci_cfg.pdsch_dmrs_type        = srsran_dmrs_sch_type_1; // by default
  dci_cfg.pdsch_dmrs_max_len     = srsran_dmrs_sch_len_1; // by default

  pdsch_hl_cfg.alloc = dci_cfg.pdsch_alloc_type;
  pusch_hl_cfg.alloc = dci_cfg.pusch_alloc_type;

  // config according to the SIB 1's UL and DL BWP size
  dci_cfg.bwp_dl_initial_bw = sib1.serving_cell_cfg_common.dl_cfg_common.freq_info_dl.scs_specific_carrier_list[0].carrier_bw;
  dci_cfg.bwp_dl_active_bw = sib1.serving_cell_cfg_common.dl_cfg_common.freq_info_dl.scs_specific_carrier_list[0].carrier_bw;
  dci_cfg.bwp_ul_initial_bw = sib1.serving_cell_cfg_common.ul_cfg_common.freq_info_ul.scs_specific_carrier_list[0].carrier_bw; 
  dci_cfg.bwp_ul_active_bw = sib1.serving_cell_cfg_common.ul_cfg_common.freq_info_ul.scs_specific_carrier_list[0].carrier_bw; 

  args_t.base_carrier.nof_prb = srsran_coreset_get_bw(&coreset1_t);
  srsran_carrier_nr_t carrier_dl = args_t.base_carrier;
  carrier_dl.nof_prb = dci_cfg.bwp_dl_active_bw; // Use a dummy carrier for resource calculation.
  carrier_dl.max_mimo_layers = master_cell_group.sp_cell_cfg.sp_cell_cfg_ded.pdsch_serving_cell_cfg_present ?
                               master_cell_group.sp_cell_cfg.sp_cell_cfg_ded.pdsch_serving_cell_cfg.setup().max_mimo_layers_present ?
                               master_cell_group.sp_cell_cfg.sp_cell_cfg_ded.pdsch_serving_cell_cfg.setup().max_mimo_layers : 1 : 1;
  
  // update the nof_prb for carrier settings.
  if (srsran_ue_dl_nr_set_carrier_nrscope(&ue_dl, &args_t.base_carrier, arg_scs)) {
    ERROR("Error setting SCH NR carrier");
    return SRSRAN_ERROR;
  }

  if (srsran_ue_dl_nr_set_pdcch_config(&ue_dl, &pdcch_cfg, &dci_cfg)) {
    ERROR("Error setting CORESET");
    return SRSRAN_ERROR;
  }

  if (srsran_softbuffer_rx_init_guru(&softbuffer, SRSRAN_SCH_NR_MAX_NOF_CB_LDPC, SRSRAN_LDPC_MAX_LEN_ENCODED_CB) <
      SRSRAN_SUCCESS) {
    ERROR("Error init soft-buffer");
    return SRSRAN_ERROR;
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
        auto result = dci_decoder.decode_and_parse_dci_from_slot(&ue_dl, &slot, arg_scs, &carrier_dl, &pdsch_hl_cfg, &pusch_hl_cfg, 
                                                    &softbuffer, &rrc_setup, &master_cell_group, known_rntis, nof_known_rntis, known_rntis[0]);

        // determine if this is new data, if it's a retransmission, we don't count it into the overall TBS
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


void Radio::WriteLogFile(std::string filename, const char* szString)
{
  FILE* pFile = fopen(filename.c_str(), "a");
  fprintf(pFile, "%s\n", szString);
  fclose(pFile);
}