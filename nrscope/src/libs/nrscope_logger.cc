#include "nrscope/hdr/nrscope_logger.h"

namespace NRScopeLog{
  std::string filename;
  std::queue<LogNode> log_queue;
  std::thread log_thread;
  std::mutex lock;
  char buff[2048];
  bool run_log;

  void init_logger(std::string filename_input){
    filename = filename_input;
    FILE* pFile = fopen(filename.c_str(), "a");
    // Transform the input_node into one log entry row.
    fprintf(pFile, "%s\n", "timestamp,system_frame_index,slot_index,rnti,rnti_type,k,mapping,time_start,time_length,"
      "frequency_start,frequency_length,nof_dmrs_cdm_groups,beta_dmrs,nof_layers,n_scid,tb_scaling_field,"
      "modulation,mcs_index,transport_block_size,code_rate,redundancy_version,new_data_indicator,"
      "nof_re,nof_bits,mcs_table,xoverhead");
    fclose(pFile);
    run_log = true;
    log_thread = std::thread{logger_thread};
    printf("Finished csv head writing...\n");
  }

  // Called to add node into the queue
  void push_node(LogNode input_node){
    lock.lock();
    log_queue.push(input_node);
    lock.unlock();
  }

  void write_entry(LogNode input_node){
    // Write the data in a CSV format.
    memset(buff, 0, sizeof(buff));

    int first_prb = SRSRAN_MAX_PRB_NR;
    for (int i = 0; i < SRSRAN_MAX_PRB_NR && first_prb == SRSRAN_MAX_PRB_NR; i++) {
      if (input_node.grant.grant.prb_idx[i]) {
        first_prb = i;
      }
    }

    snprintf(buff, sizeof(buff), "%f,%d,%d,%d,%s,%d,%s,%d,%d,%d,%d,%d,%f,%d,%d,%d,%s,%d,%d,%f,%d,%d,%d,%d,%s,%s", 
            input_node.timestamp,
            input_node.system_frame_idx,
            input_node.slot_idx,
            input_node.grant.grant.rnti,
            srsran_rnti_type_str(input_node.grant.grant.rnti_type),
            input_node.grant.grant.k,
            sch_mapping_to_str(input_node.grant.grant.mapping),
            input_node.grant.grant.S,
            input_node.grant.grant.L,
            first_prb,
            input_node.grant.grant.nof_prb,
            input_node.grant.grant.nof_dmrs_cdm_groups_without_data,
            input_node.grant.grant.beta_dmrs,
            input_node.grant.grant.nof_layers,
            input_node.grant.grant.n_scid,
            input_node.grant.grant.tb_scaling_field,
            srsran_mod_string(input_node.grant.grant.tb[0].mod),
            input_node.grant.grant.tb[0].mcs,
            input_node.grant.grant.tb[0].tbs,
            input_node.grant.grant.tb[0].R,
            input_node.grant.grant.tb[0].rv,
            input_node.grant.grant.tb[0].ndi,
            input_node.grant.grant.tb[0].nof_re,
            input_node.grant.grant.tb[0].nof_bits,
            srsran_mcs_table_to_str(input_node.grant.sch_cfg.mcs_table),
            sch_xoverhead_to_str(input_node.grant.sch_cfg.xoverhead)
    );
    FILE* pFile = fopen(filename.c_str(), "a");
    
    // Transform the input_node into one log entry row.
    fprintf(pFile, "%s\n", buff);
    fclose(pFile);
  }

  void logger_thread(){
    struct sigaction sigIntHandler;

    sigIntHandler.sa_handler = my_sig_handler;
    sigemptyset(&sigIntHandler.sa_mask);
    sigIntHandler.sa_flags = 0;

    sigaction(SIGINT, &sigIntHandler, NULL);

    while(run_log){
      // printf("Queue length: %ld\n", log_queue.size());
      if(log_queue.size() > 0){
        lock.lock();
        LogNode new_node = log_queue.front();
        // Write to local disk
        write_entry(new_node);
        printf("new_node_timestamp: %f\n", new_node.timestamp);
        log_queue.pop();
        lock.unlock();
      }else{
        usleep(1000);
      }
    }
  }

  void exit_logger(){
    lock.lock();
    run_log = false;
    lock.unlock();
  }
};