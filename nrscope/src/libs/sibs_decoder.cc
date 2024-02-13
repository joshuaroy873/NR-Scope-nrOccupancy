#include "nrscope/hdr/sibs_decoder.h"

SIBsDecoder::SIBsDecoder(){
  data_pdcch = srsran_vec_u8_malloc(SRSRAN_SLOT_MAX_NOF_BITS_NR);
  if (data_pdcch == NULL) {
    ERROR("Error malloc");
  }
}

SIBsDecoder::~SIBsDecoder(){
    
}

int SIBsDecoder::sib_decoder_and_reception_init(srsran_ue_dl_nr_sratescs_info arg_scs_,
                                                srsran_carrier_nr_t* base_carrier_,
                                                cell_search_result_t cell_,
                                                cf_t* input[SRSRAN_MAX_PORTS],
                                                srsran_coreset_t* coreset0_t_){
  memcpy(&coreset0_t, coreset0_t_, sizeof(srsran_coreset_t));

  dci_cfg.bwp_dl_initial_bw   = 275;
  dci_cfg.bwp_ul_initial_bw   = 275;
  dci_cfg.bwp_dl_active_bw    = 275;
  dci_cfg.bwp_ul_active_bw    = 275;
  dci_cfg.monitor_common_0_0  = true;
  dci_cfg.monitor_0_0_and_1_0 = true;
  dci_cfg.monitor_0_1_and_1_1 = true;
  // set coreset0 bandwidth
  dci_cfg.coreset0_bw = srsran_coreset_get_bw(&coreset0_t);

  pdcch_cfg.coreset_present[0] = true;
  search_space = &pdcch_cfg.search_space[0];
  pdcch_cfg.search_space_present[0]   = true;
  search_space->id                    = 0;
  search_space->coreset_id            = 0;
  search_space->type                  = srsran_search_space_type_common_0;
  search_space->formats[0]            = srsran_dci_format_nr_1_0;
  search_space->nof_formats           = 1;
  for (uint32_t L = 0; L < SRSRAN_SEARCH_SPACE_NOF_AGGREGATION_LEVELS_NR; L++) {
    search_space->nof_candidates[L] = srsran_pdcch_nr_max_candidates_coreset(&coreset0_t, L);
  }
  pdcch_cfg.coreset[0] = coreset0_t; 

  arg_scs = arg_scs_;
  memcpy(&base_carrier, base_carrier_, sizeof(srsran_carrier_nr_t));
  cell = cell_;
  pdsch_hl_cfg.typeA_pos = cell.mib.dmrs_typeA_pos;

  ue_dl_args.nof_rx_antennas               = 1;
  ue_dl_args.pdsch.sch.disable_simd        = false;
  ue_dl_args.pdsch.sch.decoder_use_flooded = false;
  ue_dl_args.pdsch.measure_evm             = true;
  ue_dl_args.pdcch.disable_simd            = false;
  ue_dl_args.pdcch.measure_evm             = true;
  ue_dl_args.nof_max_prb                   = 275;

  if (srsran_ue_dl_nr_init_nrscope(&ue_dl_sibs, input, &ue_dl_args, arg_scs)) {
    ERROR("Error UE DL");
    return SRSRAN_ERROR;
  }

  if (srsran_ue_dl_nr_set_carrier_nrscope(&ue_dl_sibs, &base_carrier, arg_scs)) {
    ERROR("Error setting SCH NR carrier");
    return SRSRAN_ERROR;
  }

  if (srsran_ue_dl_nr_set_pdcch_config(&ue_dl_sibs, &pdcch_cfg, &dci_cfg)) {
    ERROR("Error setting CORESET");
    return SRSRAN_ERROR;
  }

  if (srsran_softbuffer_rx_init_guru(&softbuffer, SRSRAN_SCH_NR_MAX_NOF_CB_LDPC, SRSRAN_LDPC_MAX_LEN_ENCODED_CB) <
      SRSRAN_SUCCESS) {
    ERROR("Error init soft-buffer");
    return SRSRAN_ERROR;
  }

  return SRSRAN_SUCCESS;
}

int SIBsDecoder::decode_and_parse_sib1_from_slot(srsran_slot_cfg_t* slot,
                                                 asn1::rrc_nr::sib1_s* sib1){
  // if((coreset0_args_t.sfn_c == 0 && outcome.sfn % 2 == 0) || 
  //     (coreset0_args_t.sfn_c == 1 && outcome.sfn % 2 == 1)) {
  //   if((outcome.sf_idx) == (uint32_t)(coreset0_args_t.n_0 / 2) || 
  //       (outcome.sf_idx) == (uint32_t)(coreset0_args_t.n_0 / 2 + 1)){
  //       }
  //     }
  
  srsran_dci_dl_nr_t dci_sibs;
  // Check the fft plan and how does it manipulate the buffer
  srsran_ue_dl_nr_estimate_fft_nrscope(&ue_dl_sibs, slot, arg_scs);
  // Blind search
  int nof_found_dci = srsran_ue_dl_nr_find_dl_dci(&ue_dl_sibs, slot, 0xFFFF, 
                                                  srsran_rnti_type_si, &dci_sibs, 1);
  if (nof_found_dci < SRSRAN_SUCCESS) {
    ERROR("Error in blind search");
    return SRSRAN_ERROR;
  }
  // Print PDCCH blind search candidates
  for (uint32_t pdcch_idx = 0; pdcch_idx < ue_dl_sibs.pdcch_info_count; pdcch_idx++) {
    const srsran_ue_dl_nr_pdcch_info_t* info = &(ue_dl_sibs.pdcch_info[pdcch_idx]);
    printf("PDCCH: %s-rnti=0x%x, crst_id=%d, ss_type=%s, ncce=%d, al=%d, EPRE=%+.2f, RSRP=%+.2f, corr=%.3f; "
    "nof_bits=%d; crc=%s;\n",
    srsran_rnti_type_str_short(info->dci_ctx.rnti_type),
    info->dci_ctx.rnti,
    info->dci_ctx.coreset_id,
    srsran_ss_type_str(info->dci_ctx.ss_type),
    info->dci_ctx.location.ncce,
    info->dci_ctx.location.L,
    info->measure.epre_dBfs,
    info->measure.rsrp_dBfs,
    info->measure.norm_corr,
    info->nof_bits,
    info->result.crc ? "OK" : "KO");
  }
  if (nof_found_dci < 1) {
    printf("No DCI found :'(\n");
    return SRSRAN_ERROR;
  }

  char str[1024] = {};
  srsran_dci_dl_nr_to_str(&(ue_dl_sibs.dci), &dci_sibs, str, (uint32_t)sizeof(str));
  printf("Found DCI: %s\n", str);

  srsran_sch_cfg_nr_t pdsch_cfg = {};
  if (srsran_ra_dl_dci_to_grant_nr(&base_carrier, slot, &pdsch_hl_cfg, 
    &dci_sibs, &pdsch_cfg, &pdsch_cfg.grant) < SRSRAN_SUCCESS) {
    ERROR("Error decoding PDSCH search");
    return SRSRAN_ERROR;
  }
  srsran_sch_cfg_nr_info(&pdsch_cfg, str, (uint32_t)sizeof(str));
  printf("PDSCH_cfg:\n%s", str);
  pdsch_cfg.grant.tb[0].softbuffer.rx = &softbuffer; // Set softbuffer
  srsran_pdsch_res_nr_t pdsch_res = {}; // Prepare PDSCH result
  pdsch_res.tb[0].payload = data_pdcch;

  // Decode PDSCH
  if (srsran_ue_dl_nr_decode_pdsch(&ue_dl_sibs, slot, &pdsch_cfg, &pdsch_res) < SRSRAN_SUCCESS) {
    printf("Error decoding PDSCH search\n");
    return SRSRAN_ERROR;
  }
  if (!pdsch_res.tb[0].crc) {
    printf("Error decoding PDSCH (CRC)\n");
    return SRSRAN_ERROR;
  }
  printf("Decoded PDSCH (%d B)\n", pdsch_cfg.grant.tb[0].tbs / 8);
  srsran_vec_fprint_byte(stdout, pdsch_res.tb[0].payload, pdsch_cfg.grant.tb[0].tbs / 8);

   // check payload is not all null
  bool all_zero = true;
  for (int i = 0; i < pdsch_cfg.grant.tb[0].tbs / 8; ++i) {
    if (pdsch_res.tb[0].payload[i] != 0x0) {
      all_zero = false;
      break;
    }
  }
  if (all_zero) {
    ERROR("PDSCH payload is all zeros");
    return SRSRAN_ERROR;
  }
  std::cout << "Decoding SIB 1..." << std::endl;
  asn1::rrc_nr::bcch_dl_sch_msg_s dlsch_msg;
  asn1::cbit_ref dlsch_bref(pdsch_res.tb[0].payload, pdsch_cfg.grant.tb[0].tbs / 8);
  asn1::SRSASN_CODE err = dlsch_msg.unpack(dlsch_bref);
  *sib1 = dlsch_msg.msg.c1().sib_type1();
  std::cout << "SIB 1 Decoded." << std::endl;

  asn1::json_writer js;
  (*sib1).to_json(js);
  printf("Decoded SIB1: %s\n", js.to_string().c_str());
  return SRSRAN_SUCCESS;
}

int SIBsDecoder::sibs_thread(srsran_ue_dl_nr_sratescs_info arg_scs_, 
                             TaskSchedulerNRScope* task_scheduler_nrscope, 
                             cf_t* input[SRSRAN_MAX_PORTS]){
  // Start initializing the sib decoder
  if(sib_decoder_and_reception_init(arg_scs_, &(task_scheduler_nrscope->args_t.base_carrier), task_scheduler_nrscope->cell, 
     input, &(task_scheduler_nrscope->coreset0_t)) < SRSASN_SUCCESS){
    ERROR("SIBsDecoder Init Error");
    return NR_FAILURE;
  }

  while(true){
    sib1_task_element this_slot = task_scheduler_nrscope->sib1_queue.front();
    task_scheduler_nrscope->sib1_queue.pop();
    // get slot and 
    if((task_scheduler_nrscope->coreset0_args_t.sfn_c == 0 && this_slot.outcome.sfn % 2 == 0) || 
      (task_scheduler_nrscope->coreset0_args_t.sfn_c == 1 && this_slot.outcome.sfn % 2 == 1)) {
      if((this_slot.outcome.sf_idx) == (uint32_t)(task_scheduler_nrscope->coreset0_args_t.n_0 / 2) || 
          (this_slot.outcome.sf_idx) == (uint32_t)(task_scheduler_nrscope->coreset0_args_t.n_0 / 2 + 1)){
        if(decode_and_parse_sib1_from_slot(&this_slot.slot, &task_scheduler_nrscope->sib1) == SRSASN_SUCCESS){
          return SRSASN_SUCCESS;
        }else{
          continue;
        }
      } 
    }
  }

}