#include <iostream>
#include <string>
#include <unistd.h>

#include "nrscope/hdr/nrscope_def.h"
#include "nrscope/hdr/load_config.h"
#include "nrscope/hdr/asn_decoder.h"

#include "srsran/common/band_helper.h"
#include "srsran/phy/common/phy_common_nr.h"

int main(int argc, char** argv){

  // Initialise logging infrastructure
  srslog::init();

  std::vector<Radio> radios(1);

  radios[0].log_name = "scan.csv";
  radios[0].local_log = true;
  radios[0].nof_threads = 4;

  // All the radios have the same setting for local log or push to google
  if(radios[0].local_log){
    std::vector<std::string> log_names(1);
    for(int i = 0; i < 1; i++){
      log_names[i] = radios[i].log_name;
    }
    NRScopeLog::init_scan_logger(log_names);
  }

  std::vector<std::thread> radio_threads;

  for (auto& my_radio : radios) {
    radio_threads.emplace_back(&Radio::ScanThread, &my_radio);
  }

  for (auto& t : radio_threads) {
    if(t.joinable()){
      t.join();
    }
  } 

  return NR_SUCCESS;
}