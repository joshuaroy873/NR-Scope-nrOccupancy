#ifndef NGSCOPE_DEF_H
#define NGSCOPE_DEF_H

/******************************************/
/* Some common definition and radom things*/
/******************************************/

#include <assert.h>
#include <math.h>
#include <pthread.h>
#include <semaphore.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/time.h>
#include <unistd.h>
#include <stdint.h>
#include "srsran/srslog/logger.h"
#include <iostream>
#include <cmath>
#include <pthread.h>
#include <complex>
#include <sys/time.h>

#include "srsran/common/band_helper.h"
#include "srsran/common/band_helper.h"
#include "srsran/common/crash_handler.h"
#include "srsran/common/string_helpers.h"
#include "srsue/hdr/phy/phy_nr_sa.h"
#include "srsue/hdr/phy/nr/cell_search.h"
#include "test/phy/dummy_ue_stack.h"
#include "srsue/hdr/stack/ue_stack_nr.h"
#include <boost/program_options.hpp>
#include <boost/program_options/parsers.hpp>
#include "srsran/asn1/rrc_nr.h"
#include "srsran/asn1/asn1_utils.h"
#include "srsran/mac/mac_rar_pdu_nr.h"
#include "srsran/phy/phch/prach.h"
#include "srsran/srsran.h"

#define MAX_NOF_DCI_DECODER 4
#define MAX_NOF_RF_DEV 4
#define NOF_LOG_SF 32
#define NOF_LOG_SUBF NOF_LOG_SF * 10
#define MAX_MSG_PER_SUBF 10
#define MAX_DCI_BUFFER 10

#define NR_FAILURE -1
#define NR_SUCCESS 0

struct cell_searcher_args_t {
  // Generic parameters
  double                      srate_hz        = 11.52e6;
  srsran_carrier_nr_t         base_carrier    = SRSRAN_DEFAULT_CARRIER_NR;
  srsran_ssb_pattern_t        ssb_pattern;
  srsran_subcarrier_spacing_t ssb_scs;
  srsran_duplex_mode_t        duplex_mode     = SRSRAN_DUPLEX_MODE_TDD;
  uint32_t                    duration_ms     = 1000;
  std::string                 phy_log_level   = "warning";
  std::string                 stack_log_level = "warning";

  // RF parameters
  std::string rf_device_name    = "auto";
  std::string rf_device_args    = "auto";
  std::string rf_log_level      = "info";
  float       rf_rx_gain_dB     = 20.0f;
  float       rf_freq_offset_Hz = 0.0f;

  void set_ssb_from_band(srsran_subcarrier_spacing_t scs_input)
  {
    srsran::srsran_band_helper bands;

    // Deduce band number
    uint16_t band = bands.get_band_from_dl_freq_Hz(base_carrier.dl_center_frequency_hz);

    srsran_assert(band != UINT16_MAX, "Invalid band");
    
    // Deduce point A in Hz
    double pointA_Hz =
        bands.get_abs_freq_point_a_from_center_freq(base_carrier.nof_prb, base_carrier.dl_center_frequency_hz);

    // Deduce DL center frequency ARFCN
    uint32_t pointA_arfcn = bands.freq_to_nr_arfcn(pointA_Hz);
    srsran_assert(pointA_arfcn != 0, "Invalid frequency");

    // Select a valid SSB subcarrier spacing
    ssb_scs = scs_input;
    // ssb_scs = srsran_subcarrier_spacing_30kHz;

    // Deduce SSB center frequency ARFCN
    // uint32_t ssb_arfcn = bands.get_abs_freq_ssb_arfcn(band, ssb_scs, pointA_arfcn);
    // srsran_assert(ssb_arfcn, "Invalid SSB center frequency");

    duplex_mode                     = bands.get_duplex_mode(band);
    ssb_pattern = bands.get_ssb_pattern(band, ssb_scs);
    
    // ssb_pattern                     = bands.get_ssb_pattern(band, ssb_scs);
    // base_carrier.ssb_center_freq_hz = bands.nr_arfcn_to_freq(ssb_arfcn);
  }
};

struct cell_search_result_t {
  bool                        found           = false;
  double                      ssb_abs_freq_hz = 0.0f;
  srsran_subcarrier_spacing_t ssb_scs         = srsran_subcarrier_spacing_15kHz;
  srsran_ssb_pattern_t        ssb_pattern     = SRSRAN_SSB_PATTERN_A;
  srsran_duplex_mode_t        duplex_mode     = SRSRAN_DUPLEX_MODE_FDD;
  srsran_mib_nr_t             mib             = {};
  uint32_t                    pci             = 0;
  uint32_t                    k_ssb           = 0;
  double                      abs_ssb_scs     = 0.0;
  double                      abs_pdcch_scs   = 0.0;
  int                         u = (int) ssb_scs;
  };


/**
  * Get the UNIX timestamp for now in microsecond.
  *
  * @return double type UNIX timestamp in microsecond.
  */
double get_now_timestamp_in_double();
#endif
