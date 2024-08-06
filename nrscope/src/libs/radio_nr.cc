#include "nrscope/hdr/radio_nr.h"
#include <liquid/liquid.h>

std::mutex lock_radio_nr;

int copy_c_to_cpp_complex_arr_and_zero_padding(cf_t* src, std::complex<float>* dst, uint32_t sz1, uint32_t sz2) {
  for (uint32_t i = 0; i < sz2; i++) {
    // indeed copy right? https://en.cppreference.com/w/cpp/numeric/complex/operator%3D
    dst[i] = i < sz1 ? src[i] : 0;
  }

  return 0;
}

int copy_cpp_to_c_complex_arr(std::complex<float>* src, cf_t* dst, uint32_t sz) {
  for (uint32_t i = 0; i < sz; i++) {
    // https://en.cppreference.com/w/cpp/numeric/complex 
    dst[i] = { src[i].real(), src[i].imag() };
  }

  return 0;
}

Radio::Radio() : 
  logger(srslog::fetch_basic_logger("PHY")), 
  srsran_searcher(logger),
  rf_buffer_t(1),
  slot_synchronizer(logger),
  task_scheduler_nrscope(),
  rach_decoder(),
  sibs_decoder(),
  // dci_decoder(100),
  harq_tracker(100)
{
  raido_shared = std::make_shared<srsran::radio>();
  radio = nullptr;

  nof_trials = 100;
  nof_trials_scan = 200;
  srsran_searcher_args_t.max_srate_hz = 30.72e6;
  srsran_searcher_args_t.ssb_min_scs = srsran_subcarrier_spacing_15kHz;
  srsran_searcher.init(srsran_searcher_args_t);

  ssb_cfg = {};
  ue_sync_nr = {};
  outcome = {};
  ue_sync_nr_args = {};
  sync_cfg = {};
}

Radio::~Radio() {
}

int Radio::RadioThread(){
  RadioInitandStart();
  return SRSRAN_SUCCESS;
}

int Radio::ScanThread(){
  ScanInitandStart();
  return SRSRAN_SUCCESS;
}

int Radio::ScanInitandStart(){

  // Static rf parameters
  rf_args.srate_hz = 11520000;
  rf_args.rx_gain = 30;
  rf_args.device_args = "type=x300";
  rf_args.nof_antennas = 1;
  rf_args.nof_carriers = 1;
  rf_args.log_level = "debug";
  rf_args.dl_freq = srsran_band_helper::get_freq_from_gscn(5279);

  // Static cell searcher parameters  
  args_t.srate_hz = rf_args.srate_hz;
  args_t.rf_device_name = rf_args.device_name;
  args_t.rf_device_args = rf_args.device_args;
  args_t.rf_log_level = "info";
  args_t.rf_rx_gain_dB = rf_args.rx_gain;
  args_t.rf_freq_offset_Hz = rf_args.freq_offset;
  args_t.phy_log_level   = "warning";
  args_t.stack_log_level = "warning";
  args_t.duration_ms = 1000;

  double ssb_bw_hz;
  double ssb_center_freq_min_hz;
  double ssb_center_freq_max_hz;

  // Store the double args_t.base_carrier.dl_center_frequency_hz and cs_args.center_freq_hz in a mirror int version
  // for precise diff calculation
  long long dl_center_frequency_hz_int_ver;
  long long cs_args_ssb_freq_hz_int_ver;

  uint32_t gscn_low;
  uint32_t gscn_high;
  uint32_t gscn_step;

  // initialize radio
  srsran_assert(raido_shared->init(rf_args, nullptr) == SRSRAN_SUCCESS, "Failed Radio initialisation");
  radio = std::move(raido_shared);
  radio->set_rx_srate(rf_args.srate_hz);
  radio->set_rx_gain(rf_args.rx_gain);
  std::cout << "Initialized radio; start cell scanning" << std::endl;

  // Traverse GSCN per band
  for (const srsran_band_helper::nr_band_ss_raster& ss_raster : srsran_band_helper::nr_band_ss_raster_table) {
    std::cout << "Start scaning band " << ss_raster.band << " with scs idx " << ss_raster.scs << std::endl;
    std::cout << "gscn " << ss_raster.gscn_first << " to gscn " << ss_raster.gscn_last << std::endl;

    // adjust the RF's central meas freq to the first GSCN point of the band
    cs_args_ssb_freq_hz_int_ver = (long long)srsran_band_helper::get_freq_from_gscn(ss_raster.gscn_first);
    dl_center_frequency_hz_int_ver = cs_args_ssb_freq_hz_int_ver;
    rf_args.dl_freq = cs_args_ssb_freq_hz_int_ver;
    args_t.base_carrier.dl_center_frequency_hz = rf_args.dl_freq;
    cs_args.center_freq_hz = args_t.base_carrier.dl_center_frequency_hz;
    cs_args.ssb_freq_hz = args_t.base_carrier.dl_center_frequency_hz;
    
    gscn_low = ss_raster.gscn_first;
    gscn_high = ss_raster.gscn_last;
    gscn_step = ss_raster.gscn_step;

    // Set ssb scs relevant logics
    ssb_scs = ss_raster.scs;
    args_t.set_ssb_from_band(ssb_scs);
    args_t.base_carrier.scs = args_t.ssb_scs;
    if(args_t.duplex_mode == SRSRAN_DUPLEX_MODE_TDD){
      args_t.base_carrier.ul_center_frequency_hz = args_t.base_carrier.dl_center_frequency_hz;
    }

    // Allocate receive buffer
    slot_sz = (uint32_t)(rf_args.srate_hz / 1000.0f / SRSRAN_NOF_SLOTS_PER_SF_NR(ssb_scs));
    rx_buffer = srsran_vec_cf_malloc(SRSRAN_NOF_SLOTS_PER_SF_NR(args_t.ssb_scs) * slot_sz);
    std::cout << "slot_sz: " << slot_sz << std::endl;
    std::cout << "rx_buffer size: " << SRSRAN_NOF_SLOTS_PER_SF_NR(args_t.ssb_scs) * slot_sz << std::endl;
    srsran_vec_zero(rx_buffer, SRSRAN_NOF_SLOTS_PER_SF_NR(args_t.ssb_scs) * slot_sz);

    cs_args.ssb_scs = args_t.ssb_scs;
    cs_args.ssb_pattern = args_t.ssb_pattern;
    cs_args.duplex_mode = args_t.duplex_mode;
    uint32_t ssb_scs_hz = SRSRAN_SUBC_SPACING_NR(cs_args.ssb_scs);

    // calculate the bandpass 
    std::cout << "Update RF's meas central freq to " << cs_args.ssb_freq_hz << std::endl;
    ssb_bw_hz = SRSRAN_SSB_BW_SUBC * SRSRAN_SUBC_SPACING_NR(cs_args.ssb_scs); // here might be a logic error
    ssb_center_freq_min_hz = args_t.base_carrier.dl_center_frequency_hz - (args_t.srate_hz * 0.7 - ssb_bw_hz) / 2.0;
    ssb_center_freq_max_hz = args_t.base_carrier.dl_center_frequency_hz + (args_t.srate_hz * 0.7 - ssb_bw_hz) / 2.0;
    std::cout << "Update min ssb center detect boundary to " << ssb_center_freq_min_hz << std::endl;
    std::cout << "Update max ssb center detect boundary to " << ssb_center_freq_max_hz << std::endl;

    // Set RF
    radio->release_freq(0);
    radio->set_rx_freq(0, (double)rf_args.dl_freq);
    
    // reset the cell searcher params
    srsran_searcher.init(srsran_searcher_args_t);

    // Traverse possible SSB freq in the band
    for(uint32_t gscn = gscn_low; gscn <= gscn_high; gscn = gscn + gscn_step){

      std::cout << "Start scaning GSCN number " << gscn << std::endl;
      // Get SSB center frequency for this GSCN point
      cs_args_ssb_freq_hz_int_ver = (long long)srsran_band_helper::get_freq_from_gscn(gscn);
      cs_args.ssb_freq_hz = cs_args_ssb_freq_hz_int_ver;
      std::cout << "Absolute freq " << cs_args.ssb_freq_hz << std::endl; 

      if (cs_args.ssb_freq_hz == 0) {
        std::cout << "Invalid GSCN to SSB freq? GSCN " << gscn << std::endl;
        continue;
      }

      bool offset_not_scs_aligned = false;
      bool not_in_bandpass_range = false;

      // Calculate frequency offset between the base-band center frequency and the SSB absolute frequency
      long long offset_hz = std::abs(cs_args_ssb_freq_hz_int_ver - dl_center_frequency_hz_int_ver);
       
      if (offset_hz % ssb_scs_hz != 0) {
        std::cout << "the offset " << offset_hz << " is NOT multiple of the subcarrier spacing " << ssb_scs_hz << std::endl;
        offset_not_scs_aligned = true;
      }

      // The SSB absolute frequency is invalid if it is outside the bandpass range 
      if ((cs_args.ssb_freq_hz < ssb_center_freq_min_hz) or (cs_args.ssb_freq_hz > ssb_center_freq_max_hz)) {
        std::cout << "SSB freq not in RF bandpass range" << std::endl;
        not_in_bandpass_range = true;
      }

      if (offset_not_scs_aligned || not_in_bandpass_range) {
        // update and measure
        std::cout << "Update RF's meas central freq to " << cs_args.ssb_freq_hz << std::endl;
        ssb_bw_hz = SRSRAN_SSB_BW_SUBC * SRSRAN_SUBC_SPACING_NR(cs_args.ssb_scs);
        ssb_center_freq_min_hz = args_t.base_carrier.dl_center_frequency_hz - (args_t.srate_hz * 0.7 - ssb_bw_hz) / 2.0;
        ssb_center_freq_max_hz = args_t.base_carrier.dl_center_frequency_hz + (args_t.srate_hz * 0.7 - ssb_bw_hz) / 2.0;
        std::cout << "Update min ssb center detect boundary to " << ssb_center_freq_min_hz << std::endl;
        std::cout << "Update max ssb center detect boundary to " << ssb_center_freq_max_hz << std::endl;

        rf_args.dl_freq = cs_args.ssb_freq_hz;
        args_t.base_carrier.dl_center_frequency_hz = rf_args.dl_freq;
        dl_center_frequency_hz_int_ver = cs_args_ssb_freq_hz_int_ver;
        // Set RF
        radio->release_freq(0);
        radio->set_rx_freq(0, (double)rf_args.dl_freq);
      }

      srsran_searcher_cfg_t.srate_hz = args_t.srate_hz;
      // Currently looks like there is some coarse correlation issue
      // that the next several GSCN can possibly detect the same ssb
      // Just need to add some "deduplicate" logic when using the scanned cell info
      // TO-DO: maybe fix this at a later point
      srsran_searcher_cfg_t.center_freq_hz = cs_args.ssb_freq_hz;
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
      args_t.base_carrier.ssb_center_freq_hz = cs_args.ssb_freq_hz;

      srsran::rf_buffer_t rf_buffer = {};
      rf_buffer.set_nof_samples(slot_sz);
      rf_buffer.set(0, rx_buffer);

      for(uint32_t trial=0; trial < nof_trials_scan; trial++){
        if (trial == 0) {
          srsran_vec_cf_zero(rx_buffer, slot_sz);
        }

        srsran::rf_timestamp_t& rf_timestamp = last_rx_time;

        if (not radio->rx_now(rf_buffer, rf_timestamp)) {
          return SRSRAN_ERROR;
        }
        *(last_rx_time.get_ptr(0)) = rf_timestamp.get(0);

        cs_ret = srsran_searcher.run_slot(rx_buffer, slot_sz);
        if(cs_ret.result == srsue::nr::cell_search::ret_t::CELL_FOUND){
          break;
        }
      }
      if(cs_ret.result == srsue::nr::cell_search::ret_t::CELL_FOUND){
        std::cout << "Cell Found! (maybe reported multiple times in the next several GSCN; see README)" << std::endl;
        std::cout << "N_id: " << cs_ret.ssb_res.N_id << std::endl;

        if (local_log) {
          ScanLogNode scan_log_node;
          scan_log_node.gscn = gscn;
          scan_log_node.freq = cs_args.ssb_freq_hz;
          scan_log_node.pci = cs_ret.ssb_res.N_id;
          NRScopeLog::push_node(scan_log_node, rf_index);
        }
      }
    }

    free(rx_buffer);
  }

  NRScopeLog::exit_logger();

  return SRSRAN_SUCCESS;
}

int Radio::RadioInitandStart(){

  srsran_assert(raido_shared->init(rf_args, nullptr) == SRSRAN_SUCCESS, "Failed Radio initialisation");
  radio = std::move(raido_shared);

  // Cell Searcher parameters  
  args_t.srate_hz = rf_args.srsran_srate_hz;
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
  std::cout << "usrp srate_hz: " << rf_args.srate_hz << std::endl;
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
  slot_sz = (uint32_t)(rf_args.srsran_srate_hz / 1000.0f / SRSRAN_NOF_SLOTS_PER_SF_NR(ssb_scs));
  rx_buffer = srsran_vec_cf_malloc(SRSRAN_NOF_SLOTS_PER_SF_NR(args_t.ssb_scs) * slot_sz * 4);
  std::cout << "slot_sz: " << slot_sz << std::endl;
  // std::cout << "rx_buffer size: " << SRSRAN_NOF_SLOTS_PER_SF_NR(args_t.ssb_scs) * slot_sz << std::endl;
  srsran_vec_zero(rx_buffer, SRSRAN_NOF_SLOTS_PER_SF_NR(args_t.ssb_scs) * slot_sz);
  uint32_t actual_slot_sz = 0; // the actual slot size after resampling 


  // Allocate pre-resampling receive buffer
  pre_resampling_slot_sz = (uint32_t)(rf_args.srate_hz / 1000.0f / SRSRAN_NOF_SLOTS_PER_SF_NR(ssb_scs));
  pre_resampling_rx_buffer = srsran_vec_cf_malloc(SRSRAN_NOF_SLOTS_PER_SF_NR(args_t.ssb_scs) * pre_resampling_slot_sz);
  std::cout << "pre_resampling_slot_sz: " << pre_resampling_slot_sz << std::endl;
  srsran_vec_zero(pre_resampling_rx_buffer, SRSRAN_NOF_SLOTS_PER_SF_NR(args_t.ssb_scs) * pre_resampling_slot_sz);

  cs_args.center_freq_hz = args_t.base_carrier.dl_center_frequency_hz;
  cs_args.ssb_freq_hz = args_t.base_carrier.dl_center_frequency_hz;
  cs_args.ssb_scs = args_t.ssb_scs;
  cs_args.ssb_pattern = args_t.ssb_pattern;
  cs_args.duplex_mode = args_t.duplex_mode;

  uint32_t ssb_scs_hz = SRSRAN_SUBC_SPACING_NR(cs_args.ssb_scs);
  double ssb_bw_hz = SRSRAN_SSB_BW_SUBC * ssb_scs_hz;
  double ssb_center_freq_min_hz = args_t.base_carrier.dl_center_frequency_hz - (args_t.srate_hz * 0.7 - ssb_bw_hz) / 2.0;
  double ssb_center_freq_max_hz = args_t.base_carrier.dl_center_frequency_hz + (args_t.srate_hz * 0.7 - ssb_bw_hz) / 2.0;

  uint32_t band = bands.get_band_from_dl_freq_Hz_and_scs(args_t.base_carrier.dl_center_frequency_hz, cs_args.ssb_scs);
  srsran::srsran_band_helper::sync_raster_t ss = bands.get_sync_raster(band, cs_args.ssb_scs);
  srsran_assert(ss.valid(), "Invalid synchronization raster");

  // initialize resampling tool
  float r = (float)rf_args.srsran_srate_hz/(float)rf_args.srate_hz;       // resampling rate (output/input)
  std::cout << "[xuyang debug] r (resampling rate): " << r << std::endl;
  float As=60.0f;         // resampling filter stop-band attenuation [dB]
  msresamp_crcf q = msresamp_crcf_create(r,As);
  float delay = msresamp_crcf_get_delay(q);
  std::cout << "[xuyang debug] resample delay: " << delay << std::endl;

  // add a few zero padding
  uint32_t temp_x_sz = SRSRAN_NOF_SLOTS_PER_SF_NR(args_t.ssb_scs) * pre_resampling_slot_sz + (int)ceilf(delay) + 10;
  std::complex<float> temp_x[temp_x_sz];

  uint32_t temp_y_sz = (uint32_t)(temp_x_sz * r * 2);
  std::complex<float> temp_y[temp_y_sz];

  FILE *fp_time_series_pre_resample;
  fp_time_series_pre_resample = fopen("./time_series_pre_resample.txt", "w");
  FILE *fp_time_series_post_resample;
  fp_time_series_post_resample = fopen("./time_series_post_resample.txt", "w");

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
      // std::cout << "[xuyang debug] skipped freq: " << cs_args.ssb_freq_hz << std::endl;
      // Skip this frequency
      continue;
    }

    // xuyang debug: skip all other nearby measure and just focus on the wanted SSB freq
    if (offset_hz > 1) {
      continue;
    }

    std::cout << "[xuyang debug] freq to find ssb: " << cs_args.ssb_freq_hz << std::endl;

    srsran_searcher_cfg_t.srate_hz = args_t.srate_hz; // which is indeed the srsran srate
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
    // std::cout << cs_args.ssb_freq_hz << std::endl;
    args_t.base_carrier.ssb_center_freq_hz = cs_args.ssb_freq_hz;

    radio->release_freq(0);
    radio->set_rx_freq(0, srsran_searcher_cfg_t.ssb_freq_hz);

    srsran::rf_buffer_t rf_buffer = {};
    rf_buffer.set_nof_samples(pre_resampling_slot_sz);
    std::cout << "[xuyang debug] pre_resampling_slot_sz: " << pre_resampling_slot_sz << std::endl;
    rf_buffer.set(0, pre_resampling_rx_buffer);// + slot_sz);

    for(uint32_t trial=0; trial < nof_trials; trial++){
      if (trial == 0) {
        srsran_vec_cf_zero(rx_buffer, slot_sz);
        srsran_vec_cf_zero(pre_resampling_rx_buffer, pre_resampling_slot_sz);
      }
      // srsran_vec_cf_copy(rx_buffer, rx_buffer + slot_sz, slot_sz);

      srsran::rf_timestamp_t& rf_timestamp = last_rx_time;

      if (not radio->rx_now(rf_buffer, rf_timestamp)) {
        return SRSRAN_ERROR;
      }

      // srsran_vec_fprint_c(stdout, rx_buffer, slot_sz);

      // HERE WE DO RESAMPLING: 33330000 to the familiar 23040000
      // from pre_resampling_rx_buffer to rx_buffer
      struct timeval t0, t1;

      // std::cout << "[xuyang debug] BEFORE RESAMPLING: " << std::endl;
      srsran_vec_fprint2_c(fp_time_series_pre_resample, pre_resampling_rx_buffer, pre_resampling_slot_sz);
      
      // std::cout << "[xuyang debug] started liquid resampling" << std::endl;
      copy_c_to_cpp_complex_arr_and_zero_padding(pre_resampling_rx_buffer, temp_x, pre_resampling_slot_sz, temp_x_sz);

      gettimeofday(&t0, NULL);  

      msresamp_crcf_execute(q, temp_x, pre_resampling_slot_sz, temp_y, &actual_slot_sz);

      gettimeofday(&t1, NULL);  
      // result.processing_time_us = t1.tv_usec - t0.tv_usec;   
      std::cout << "[ssb search] time_spend: " << (t1.tv_usec - t0.tv_usec) << "(us)" << std::endl;
      copy_cpp_to_c_complex_arr(temp_y, rx_buffer, actual_slot_sz);

      
      // std::cout << "[xuyang debug] resampled; actual_slot_sz: " << actual_slot_sz << std::endl;
      // std::cout << "[xuyang debug] AFTER RESAMPLING: " << std::endl;
      srsran_vec_fprint2_c(fp_time_series_post_resample, rx_buffer, actual_slot_sz);

      *(last_rx_time.get_ptr(0)) = rf_timestamp.get(0);

      cs_ret = srsran_searcher.run_slot(rx_buffer, slot_sz);
      // std::cout << "Slot_sz: " << slot_sz << std::endl;
      if(cs_ret.result == srsue::nr::cell_search::ret_t::CELL_FOUND ){
        // printf("found cell in this slot\n");
        srsran_vec_fprint_c(stdout, rx_buffer, slot_sz);
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

      if(RadioCapture() < SRSASN_SUCCESS){
        ERROR("Error in RadioCapture");
        return NR_FAILURE;
      }
    }
  }

  fclose(fp_time_series_pre_resample);
  fclose(fp_time_series_post_resample);
  msresamp_crcf_destroy(q);
  return SRSRAN_SUCCESS;
}

static int slot_sync_recv_callback_find_state(void* ptr, cf_t** buffer, uint32_t nsamples, srsran_timestamp_t* ts)
{
  if (ptr == nullptr) {
  return SRSRAN_ERROR_INVALID_INPUTS;
  }
  srsran::radio* radio = (srsran::radio*)ptr;

  cf_t* buffer_ptr[SRSRAN_MAX_CHANNELS] = {};
  buffer_ptr[0]                         = buffer[0];
  nsamples = (float)(nsamples)/((float)23040000/(float)25000000);
  std::cout << "[xuyang debug 3] nsamples: " << nsamples << std::endl;
  srsran::rf_buffer_t rf_buffer(buffer_ptr, nsamples);

  srsran::rf_timestamp_t a;
  srsran::rf_timestamp_t &rf_timestamp = a;
  *ts = a.get(0);

  bool res = radio->rx_now(rf_buffer, rf_timestamp);

  // FILE *fp_sib1_time_series;
  // fp_sib1_time_series = fopen("./time_sib1_series.txt", "w");

  // srsran_vec_fprint2_c(fp_sib1_time_series, buffer[0], nsamples);

  // fclose(fp_sib1_time_series);

  return res;

  //-------------------------- NO RESAMPLE ABOVE VS RESAMPLE BELOW (TIME COMSUMING) --------------------------

  // if (ptr == nullptr) {
  //   return SRSRAN_ERROR_INVALID_INPUTS;
  // }

  // float r = (float)23040000/(float)25000000;
  // float As=60.0f;

  // msresamp_crcf resampler = msresamp_crcf_create(r,As);
  // float delay = msresamp_crcf_get_delay(resampler);

  // // uint32_t pre_resampling_sf_sz = (int)ceilf((float)nsamples/r);
  // uint32_t pre_resampling_sf_sz = (int)((float)nsamples/r);
  // // std::cout << "[xuyang debug 2] pre_resampling_sf_sz: " << pre_resampling_sf_sz << std::endl;
  // // temp buffers for resampling
  // uint32_t temp_x_sz = pre_resampling_sf_sz + (int)ceilf(delay) + 10;
  // std::complex<float> temp_x[temp_x_sz];
  // uint32_t temp_y_sz = (uint32_t)(temp_x_sz * r * 2);
  // std::complex<float> temp_y[temp_y_sz];

  // uint32_t actual_sf_sz = 0;

  // // Allocate pre-resampling receive buffer
  // cf_t* pre_rs_rx_buffer = srsran_vec_cf_malloc(pre_resampling_sf_sz * 2);
  // srsran_vec_zero(pre_rs_rx_buffer, pre_resampling_sf_sz * 2);

  // srsran::radio* radio = (srsran::radio*)ptr;

  // cf_t* buffer_ptr[SRSRAN_MAX_CHANNELS] = {};
  // // buffer_ptr[0]                         = buffer[0];
  // buffer_ptr[0]                         = pre_rs_rx_buffer;
  // // srsran::rf_buffer_t rf_buffer(buffer_ptr, nsamples);
  // srsran::rf_buffer_t rf_buffer(buffer_ptr, pre_resampling_sf_sz);

  // srsran::rf_timestamp_t a;
  // srsran::rf_timestamp_t &rf_timestamp = a;
  // *ts = a.get(0);

  // bool res = radio->rx_now(rf_buffer, rf_timestamp);

  // copy_c_to_cpp_complex_arr_and_zero_padding(pre_rs_rx_buffer, temp_x, pre_resampling_sf_sz, temp_x_sz);
  // msresamp_crcf_execute(resampler, temp_x, pre_resampling_sf_sz, temp_y, &actual_sf_sz);
  // // std::cout << "[xuyang debug 2] actual_sf_sz: " << actual_sf_sz << std::endl;
  // copy_cpp_to_c_complex_arr(temp_y, buffer[0], actual_sf_sz);

  // msresamp_crcf_destroy(resampler);

  // return res;
}

/**
 * Here in track state, we need to process really fast, so we will grab the samples and
 * resample later. Also we will skip those sync check (which seems not exercising its duty correctly either)
 */
static int slot_sync_recv_callback_track_state(void* ptr, cf_t** buffer, uint32_t nsamples, srsran_timestamp_t* ts)
{
  if (ptr == nullptr) {
    return SRSRAN_ERROR_INVALID_INPUTS;
  }
  srsran::radio* radio = (srsran::radio*)ptr;

  cf_t* buffer_ptr[SRSRAN_MAX_CHANNELS] = {};
  buffer_ptr[0]                         = buffer[0];
  nsamples = (float)(nsamples)/((float)23040000/(float)25000000);
  std::cout << "[xuyang debug 3] nsamples: " << nsamples << std::endl;
  srsran::rf_buffer_t rf_buffer(buffer_ptr, nsamples);

  srsran::rf_timestamp_t a;
  srsran::rf_timestamp_t &rf_timestamp = a;
  *ts = a.get(0);

  bool res = radio->rx_now(rf_buffer, rf_timestamp);

  // FILE *fp_sib1_time_series;
  // fp_sib1_time_series = fopen("./time_sib1_series.txt", "w");

  // srsran_vec_fprint2_c(fp_sib1_time_series, buffer[0], nsamples);

  // fclose(fp_sib1_time_series);

  return res;
}

int Radio::SyncandDownlinkInit(){
  //***** DL args Config Start *****//
  rf_buffer_t = srsran::rf_buffer_t(rx_buffer, SRSRAN_NOF_SLOTS_PER_SF_NR(task_scheduler_nrscope.args_t.ssb_scs) * slot_sz * 2);
  // it appears the srsRAN is build on 15kHz scs, we need to use the srate and 
  // scs to calculate the correct subframe size 
  arg_scs.srate = task_scheduler_nrscope.args_t.srate_hz;
  arg_scs.scs = task_scheduler_nrscope.cell.mib.scs_common;

  arg_scs.coreset_offset_scs = (cs_args.ssb_freq_hz - task_scheduler_nrscope.coreset0_args_t.coreset0_center_freq_hz) / task_scheduler_nrscope.cell.abs_pdcch_scs;// + 12;
  arg_scs.coreset_slot = (uint32_t)task_scheduler_nrscope.coreset0_args_t.n_0;
  // arg_scs.phase_diff_first_second_half = 0;
  //***** DL args Config End *****//

  //***** Slot Sync Start *****//
  ue_sync_nr_args.max_srate_hz    = srsran_searcher_args_t.max_srate_hz;
  ue_sync_nr_args.min_scs         = srsran_searcher_args_t.ssb_min_scs;
  ue_sync_nr_args.nof_rx_channels = 1;
  ue_sync_nr_args.disable_cfo     = false;
  ue_sync_nr_args.pbch_dmrs_thr   = 0.5;
  ue_sync_nr_args.cfo_alpha       = 0.1;
  ue_sync_nr_args.recv_obj        = radio.get();
  ue_sync_nr_args.recv_callback   = slot_sync_recv_callback_find_state;
  ue_sync_nr_args.recv_callback2   = slot_sync_recv_callback_track_state;
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

int Radio::RadioCapture(){
  if(!task_scheduler_nrscope.sib1_inited){
    // std::thread sib_init_thread {&SIBsDecoder::sib_decoder_and_reception_init, &sibs_decoder, arg_scs, &task_scheduler_nrscope, rf_buffer_t.to_cf_t()};
    if(sibs_decoder.sib_decoder_and_reception_init(arg_scs, &task_scheduler_nrscope, rf_buffer_t.to_cf_t()) < SRSASN_SUCCESS){
      ERROR("SIBsDecoder Init Error");
      return NR_FAILURE;
    }
    std::cout << "SIB Decoder Initializing..." << std::endl;
  }

  // FILE *fp2;
  // fp2 = fopen("/home/wanhr/Documents/codes/cpp/srsRAN_4G/build/srsue/src/SIB_debug_slot_idx.txt", "r");
  // long int file_position = 0;
  // long int slot_idx_position = 0;

  resampler_kit rk;
  prepare_resampler(&rk);

  while(true){
    outcome.timestamp = last_rx_time.get(0);  

    struct timeval t0, t1;
    gettimeofday(&t0, NULL);    

    if (srsran_ue_sync_nr_zerocopy_twinrx(&ue_sync_nr, rf_buffer_t.to_cf_t(), &outcome, rk) < SRSRAN_SUCCESS) {
      std::cout << "SYNC: error in zerocopy" << std::endl;
      logger.error("SYNC: error in zerocopy");
      return false;
    }
    // If in sync, update slot index. The synced data is stored in rf_buffer_t.to_cf_t()[0]
    if (outcome.in_sync){
      std::cout << "System frame idx: " << outcome.sfn << std::endl;
      std::cout << "Subframe idx: " << outcome.sf_idx << std::endl;
      std::cout << "Sync delay: " << outcome.delay_us << std::endl;

      for(int slot_idx = 0; slot_idx < SRSRAN_NOF_SLOTS_PER_SF_NR(arg_scs.scs); slot_idx++){
        srsran_slot_cfg_t slot = {0};
        slot.idx = (outcome.sf_idx) * SRSRAN_NSLOTS_PER_FRAME_NR(arg_scs.scs) / 10 + slot_idx;
        // Move rx_buffer
        srsran_vec_cf_copy(rx_buffer, rx_buffer + slot_idx*pre_resampling_slot_sz, pre_resampling_slot_sz);  
        

        // fseek(fp, file_position * sizeof(cf_t), SEEK_SET);
        // // uint32_t a = fread(ue_dl.fft[0].cfg.in_buffer, sizeof(cf_t), ue_dl.fft[0].sf_sz, fp);
        // uint32_t a = fread(&sibs_decoder.ue_dl_sibs.fft[0].cfg.in_buffer, sizeof(cf_t), sibs_decoder.ue_dl_sibs.fft[0].sf_sz, fp);
        // file_position += sibs_decoder.ue_dl_sibs.fft[0].sf_sz;

        // fseek(fp2, slot_idx_position * sizeof(uint32_t), SEEK_SET);
        // // uint32_t a = fread(ue_dl.fft[0].cfg.in_buffer, sizeof(cf_t), ue_dl.fft[0].sf_sz, fp);
        // uint32_t b = fread(&slot.idx, sizeof(uint32_t), 1, fp2);
        // slot_idx_position += 1;

        // 1) Inform the 3 loops to attend to this slot by puting the slot and outcome into a queue
        //     Problem: When the thread try to attend to the data and get the slot index from the queue, 
        //              the buffer may already be flushed away to the next slot.
        // 2) Call the non-blocking functions for each processing here and join, which might be more reasonable.

        if(!task_scheduler_nrscope.rach_inited and task_scheduler_nrscope.sib1_found){
          // std::thread rach_init_thread {&RachDecoder::rach_decoder_init, &rach_decoder, task_scheduler_nrscope.sib1, args_t.base_carrier};
          rach_decoder.rach_decoder_init(&task_scheduler_nrscope);

          if(rach_decoder.rach_reception_init(arg_scs, &task_scheduler_nrscope, rf_buffer_t.to_cf_t()) < SRSASN_SUCCESS){
            ERROR("RACHDecoder Init Error");
            return NR_FAILURE;
          }
          std::cout << "RACH Decoder Initialized.." << std::endl;
          task_scheduler_nrscope.rach_inited = true;
        }

        if(!task_scheduler_nrscope.dci_inited and task_scheduler_nrscope.rach_found){
          std::cout << "Initializing DCI decoder..." << std::endl;
          task_scheduler_nrscope.sharded_results.resize(nof_threads);
          task_scheduler_nrscope.nof_sharded_rntis.resize(nof_threads);
          task_scheduler_nrscope.sharded_rntis.resize(nof_threads);
          task_scheduler_nrscope.nof_threads = nof_threads;
          task_scheduler_nrscope.nof_rnti_worker_groups = nof_rnti_worker_groups;
          task_scheduler_nrscope.nof_bwps = nof_bwps;
          task_scheduler_nrscope.results.resize(nof_bwps);

          for(uint32_t i = 0; i < nof_rnti_worker_groups; i++){
            // for each rnti worker group, for each bwp, spawn a decoder
            for(uint8_t j = 0; j < nof_bwps; j++){
              DCIDecoder *decoder = new DCIDecoder(100);
              if(decoder->dci_decoder_and_reception_init(arg_scs, &task_scheduler_nrscope, rf_buffer_t.to_cf_t(), j) < SRSASN_SUCCESS){
                ERROR("DCIDecoder Init Error");
                return NR_FAILURE;
              }
              decoder->dci_decoder_id = i * nof_bwps + j;
              decoder->rnti_worker_group_id = i;
              dci_decoders.push_back(std::unique_ptr<DCIDecoder> (decoder));
            }
          }
          
          std::cout << "DCI Decoder Initialized.." << std::endl;
          task_scheduler_nrscope.dci_inited = true;
        }

        // Then start each type of decoder, TODO
        task_scheduler_nrscope.dl_prb_rate.resize(task_scheduler_nrscope.nof_known_rntis);
        task_scheduler_nrscope.ul_prb_rate.resize(task_scheduler_nrscope.nof_known_rntis);
        task_scheduler_nrscope.dl_prb_bits_rate.resize(task_scheduler_nrscope.nof_known_rntis);
        task_scheduler_nrscope.ul_prb_bits_rate.resize(task_scheduler_nrscope.nof_known_rntis);

        // To save computing resources for dci decoders: assume SIB1 info should be static
        std::thread sibs_thread;
        if (!task_scheduler_nrscope.sib1_found) {
          sibs_thread = std::thread {&SIBsDecoder::decode_and_parse_sib1_from_slot, &sibs_decoder, &slot, &task_scheduler_nrscope, rx_buffer};
        }
        std::thread rach_thread {&RachDecoder::decode_and_parse_msg4_from_slot, &rach_decoder, &slot, &task_scheduler_nrscope};

        std::vector <std::thread> dci_threads;
        if(task_scheduler_nrscope.dci_inited){
          for (uint32_t i = 0; i < nof_threads; i++){
            dci_threads.emplace_back(&DCIDecoder::decode_and_parse_dci_from_slot, dci_decoders[i].get(), &slot, &task_scheduler_nrscope);
          }
        }

        if(sibs_thread.joinable()){
          sibs_thread.join();
        }

        if(rach_thread.joinable()){
          rach_thread.join();
        }

        if(task_scheduler_nrscope.dci_inited){
          for (uint32_t i = 0; i < nof_threads; i++){
            if(dci_threads[i].joinable()){
              dci_threads[i].join();
            }
          }
        }

        if(task_scheduler_nrscope.dci_inited){
          task_scheduler_nrscope.merge_results();
          std::vector <DCIFeedback> results = task_scheduler_nrscope.get_results();

          for (uint8_t b = 0; b < nof_bwps; b++) {
            DCIFeedback result = results[b];
            if((result.dl_grants.size()>0 or result.ul_grants.size()>0)){
              for (uint32_t i = 0; i < task_scheduler_nrscope.nof_known_rntis; i++){
                if(result.dl_grants[i].grant.rnti == task_scheduler_nrscope.known_rntis[i]){
                  LogNode log_node;
                  log_node.slot_idx = slot.idx;
                  log_node.system_frame_idx = outcome.sfn;
                  log_node.timestamp = get_now_timestamp_in_double();
                  log_node.grant = result.dl_grants[i];
                  log_node.dci_format = srsran_dci_format_nr_string(result.dl_dcis[i].ctx.format);
                  log_node.dl_dci = result.dl_dcis[i];
                  log_node.bwp_id = result.dl_dcis[i].bwp_id;
                  if(local_log){
                    NRScopeLog::push_node(log_node, rf_index);
                  }
                  if(to_google){
                    ToGoogle::push_google_node(log_node, rf_index);
                  }
                }

                if(result.ul_grants[i].grant.rnti == task_scheduler_nrscope.known_rntis[i]){
                  LogNode log_node;
                  log_node.slot_idx = slot.idx;
                  log_node.system_frame_idx = outcome.sfn;
                  log_node.timestamp = get_now_timestamp_in_double();
                  log_node.grant = result.ul_grants[i];
                  log_node.dci_format = srsran_dci_format_nr_string(result.ul_dcis[i].ctx.format);
                  log_node.ul_dci = result.ul_dcis[i];
                  log_node.bwp_id = result.ul_dcis[i].bwp_id;
                  if(local_log){
                    NRScopeLog::push_node(log_node, rf_index);
                  }
                  if(to_google){
                    ToGoogle::push_google_node(log_node, rf_index);
                  }
                }
              } 
            }
          }
        }
        task_scheduler_nrscope.update_known_rntis();
        gettimeofday(&t1, NULL);  
        // result.processing_time_us = t1.tv_usec - t0.tv_usec;   
        std::cout << "time_spend: " << (t1.tv_usec - t0.tv_usec) << "(us)" << std::endl;
      } 
    } 
  }
  // fclose(fp);
  // fclose(fp2);
  return SRSRAN_SUCCESS;
}
