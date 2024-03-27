#include "nrscope/hdr/dci_decoder.h"

DCIDecoder::DCIDecoder(uint32_t nof_known_rntis){
  dl_prb_rate.resize(nof_known_rntis);
  ul_prb_rate.resize(nof_known_rntis);

  dl_prb_bits_rate.resize(nof_known_rntis);
  ul_prb_bits_rate.resize(nof_known_rntis);

  for(uint32_t i = 0; i < nof_known_rntis; i++){
    dl_prb_rate[i] = 0.0;
    ul_prb_rate[i] = 0.0;
    dl_prb_bits_rate[i] = 0.0;
    ul_prb_bits_rate[i] = 0.0;
  }

  ue_dl_tmp = (srsran_ue_dl_nr_t*) malloc(sizeof(srsran_ue_dl_nr_t));
  slot_tmp = (srsran_slot_cfg_t*) malloc(sizeof(srsran_slot_cfg_t));

  dci_dl = (srsran_dci_dl_nr_t*) malloc(sizeof(srsran_dci_dl_nr_t) * (nof_known_rntis));
  dci_ul = (srsran_dci_ul_nr_t*) malloc(sizeof(srsran_dci_ul_nr_t) * (nof_known_rntis));

  // data_pdcch = srsran_vec_u8_malloc(SRSRAN_SLOT_MAX_NOF_BITS_NR);
  // if (data_pdcch == NULL) {
  //   ERROR("Error malloc");
  // }

}

DCIDecoder::~DCIDecoder(){

}


int DCIDecoder::dci_decoder_and_reception_init(srsran_ue_dl_nr_sratescs_info arg_scs_,
                                               TaskSchedulerNRScope* task_scheduler_nrscope,
                                               cf_t* input[SRSRAN_MAX_PORTS]){ 
  
  memcpy(&base_carrier, &task_scheduler_nrscope->args_t.base_carrier, sizeof(srsran_carrier_nr_t));
  arg_scs = arg_scs_; 
  cell = task_scheduler_nrscope->cell;

  ue_dl_args.nof_rx_antennas               = 1;
  ue_dl_args.pdsch.sch.disable_simd        = false;
  ue_dl_args.pdsch.sch.decoder_use_flooded = false;
  ue_dl_args.pdsch.measure_evm             = true;
  ue_dl_args.pdcch.disable_simd            = false;
  ue_dl_args.pdcch.measure_evm             = true;
  ue_dl_args.nof_max_prb                   = 275;

  memcpy(&coreset0_t, &task_scheduler_nrscope->coreset0_t, sizeof(srsran_coreset_t));
  sib1 = task_scheduler_nrscope->sib1;
  master_cell_group = task_scheduler_nrscope->master_cell_group;
  rrc_setup = task_scheduler_nrscope->rrc_setup;

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

  pdcch_cfg.coreset[coreset1_t.id] = coreset1_t;
  pdcch_cfg.coreset_present[coreset1_t.id] = true;

  char coreset_info[512] = {};
  srsran_coreset_to_str(&coreset1_t, coreset_info, sizeof(coreset_info));
  printf("Coreset %d parameter: %s", coreset1_t.id, coreset_info);

  // For FR1 offset_to_point_a uses prbs with 15kHz scs. 
  srsran_searcher_cfg_t = task_scheduler_nrscope->srsran_searcher_cfg_t;
  double pointA = srsran_searcher_cfg_t.ssb_freq_hz - (SRSRAN_SSB_BW_SUBC / 2) *
    cell.abs_ssb_scs - cell.k_ssb * SRSRAN_SUBC_SPACING_NR(srsran_subcarrier_spacing_15kHz) 
    - sib1.serving_cell_cfg_common.dl_cfg_common.freq_info_dl.offset_to_point_a * 
    SRSRAN_SUBC_SPACING_NR(srsran_subcarrier_spacing_15kHz) * NRSCOPE_NSC_PER_RB_NR;
  std::cout << "pointA: " << pointA << std::endl;

  double coreset1_center_freq_hz = pointA + srsran_coreset_get_bw(&coreset1_t) / 2 * 
    cell.abs_pdcch_scs * NRSCOPE_NSC_PER_RB_NR;
  std::cout << "previous offset: " << arg_scs.coreset_offset_scs << std::endl;
  arg_scs.coreset_offset_scs = (base_carrier.ssb_center_freq_hz - coreset1_center_freq_hz) / cell.abs_pdcch_scs;
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

  pdsch_hl_cfg.typeA_pos = cell.mib.dmrs_typeA_pos;
  pusch_hl_cfg.typeA_pos = cell.mib.dmrs_typeA_pos;
  pdsch_hl_cfg.alloc = dci_cfg.pdsch_alloc_type;
  pusch_hl_cfg.alloc = dci_cfg.pusch_alloc_type;

  for (uint32_t pdsch_time_id = 0; pdsch_time_id < master_cell_group.sp_cell_cfg.sp_cell_cfg_ded.init_dl_bwp.pdsch_cfg.setup().pdsch_time_domain_alloc_list.setup().size(); pdsch_time_id++){
    if(master_cell_group.sp_cell_cfg.sp_cell_cfg_ded.init_dl_bwp.pdsch_cfg.setup().pdsch_time_domain_alloc_list.setup()[pdsch_time_id].k0_present){
      pdsch_hl_cfg.common_time_ra[pdsch_time_id].k = master_cell_group.sp_cell_cfg.sp_cell_cfg_ded.init_dl_bwp.pdsch_cfg.setup().pdsch_time_domain_alloc_list.setup()[pdsch_time_id].k0;
    }
    pdsch_hl_cfg.common_time_ra[pdsch_time_id].sliv = master_cell_group.sp_cell_cfg.sp_cell_cfg_ded.init_dl_bwp.pdsch_cfg.setup().pdsch_time_domain_alloc_list.setup()[pdsch_time_id].start_symbol_and_len;
    switch(master_cell_group.sp_cell_cfg.sp_cell_cfg_ded.init_dl_bwp.pdsch_cfg.setup().pdsch_time_domain_alloc_list.setup()[pdsch_time_id].map_type){
      case asn1::rrc_nr::pdsch_time_domain_res_alloc_s::map_type_e_::type_a:
        pdsch_hl_cfg.common_time_ra[pdsch_time_id].mapping_type = srsran_sch_mapping_type_A;
        break;
      case asn1::rrc_nr::pdsch_time_domain_res_alloc_s::map_type_e_::type_b:
        pdsch_hl_cfg.common_time_ra[pdsch_time_id].mapping_type = srsran_sch_mapping_type_B;
        break;
      case asn1::rrc_nr::pdsch_time_domain_res_alloc_s::map_type_e_::nulltype:
        break;
      default:
        break;
    }
  }
  pdsch_hl_cfg.nof_common_time_ra = master_cell_group.sp_cell_cfg.sp_cell_cfg_ded.init_dl_bwp.pdsch_cfg.setup().pdsch_time_domain_alloc_list.setup().size();

  for (uint32_t pusch_time_id = 0; pusch_time_id < master_cell_group.sp_cell_cfg.sp_cell_cfg_ded.ul_cfg.init_ul_bwp.pusch_cfg.setup().pusch_time_domain_alloc_list.setup().size(); pusch_time_id++){
    if(master_cell_group.sp_cell_cfg.sp_cell_cfg_ded.ul_cfg.init_ul_bwp.pusch_cfg.setup().pusch_time_domain_alloc_list.setup()[pusch_time_id].k2_present){
      pusch_hl_cfg.common_time_ra[pusch_time_id].k = master_cell_group.sp_cell_cfg.sp_cell_cfg_ded.ul_cfg.init_ul_bwp.pusch_cfg.setup().pusch_time_domain_alloc_list.setup()[pusch_time_id].k2;
    }
    pusch_hl_cfg.common_time_ra[pusch_time_id].sliv = master_cell_group.sp_cell_cfg.sp_cell_cfg_ded.ul_cfg.init_ul_bwp.pusch_cfg.setup().pusch_time_domain_alloc_list.setup()[pusch_time_id].start_symbol_and_len;
    switch(master_cell_group.sp_cell_cfg.sp_cell_cfg_ded.ul_cfg.init_ul_bwp.pusch_cfg.setup().pusch_time_domain_alloc_list.setup()[pusch_time_id].map_type){
      case asn1::rrc_nr::pusch_time_domain_res_alloc_s::map_type_e_::type_a:
        pusch_hl_cfg.common_time_ra[pusch_time_id].mapping_type = srsran_sch_mapping_type_A;
        break;
      case asn1::rrc_nr::pusch_time_domain_res_alloc_s::map_type_e_::type_b:
        pusch_hl_cfg.common_time_ra[pusch_time_id].mapping_type = srsran_sch_mapping_type_B;
        break;
      case asn1::rrc_nr::pusch_time_domain_res_alloc_s::map_type_e_::nulltype:
        break;
      default:
        break;
    }
  }
  pusch_hl_cfg.nof_common_time_ra = master_cell_group.sp_cell_cfg.sp_cell_cfg_ded.init_dl_bwp.pdsch_cfg.setup().pdsch_time_domain_alloc_list.setup().size();

  // config according to the SIB 1's UL and DL BWP size
  dci_cfg.bwp_dl_initial_bw = sib1.serving_cell_cfg_common.dl_cfg_common.freq_info_dl.scs_specific_carrier_list[0].carrier_bw;
  dci_cfg.bwp_dl_active_bw = sib1.serving_cell_cfg_common.dl_cfg_common.freq_info_dl.scs_specific_carrier_list[0].carrier_bw;
  dci_cfg.bwp_ul_initial_bw = sib1.serving_cell_cfg_common.ul_cfg_common.freq_info_ul.scs_specific_carrier_list[0].carrier_bw; 
  dci_cfg.bwp_ul_active_bw = sib1.serving_cell_cfg_common.ul_cfg_common.freq_info_ul.scs_specific_carrier_list[0].carrier_bw; 

  base_carrier.nof_prb = srsran_coreset_get_bw(&coreset1_t);
  carrier_dl = base_carrier;
  carrier_dl.nof_prb = dci_cfg.bwp_dl_active_bw; // Use a dummy carrier for resource calculation.
  carrier_dl.max_mimo_layers = master_cell_group.sp_cell_cfg.sp_cell_cfg_ded.pdsch_serving_cell_cfg_present ?
                               master_cell_group.sp_cell_cfg.sp_cell_cfg_ded.pdsch_serving_cell_cfg.setup().max_mimo_layers_present ?
                               master_cell_group.sp_cell_cfg.sp_cell_cfg_ded.pdsch_serving_cell_cfg.setup().max_mimo_layers : 1 : 1;
  
  // update the nof_prb for carrier settings.

  if (srsran_ue_dl_nr_init_nrscope(&ue_dl_dci, input, &ue_dl_args, arg_scs)) {
    ERROR("Error UE DL");
    return SRSRAN_ERROR;
  }

  if (srsran_ue_dl_nr_set_carrier_nrscope(&ue_dl_dci, &base_carrier, arg_scs)) {
    ERROR("Error setting SCH NR carrier");
    return SRSRAN_ERROR;
  }

  if (srsran_ue_dl_nr_set_pdcch_config(&ue_dl_dci, &pdcch_cfg, &dci_cfg)) {
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


int DCIDecoder::decode_and_parse_dci_from_slot(srsran_slot_cfg_t* slot,
                                               TaskSchedulerNRScope* task_scheduler_nrscope){
  if(!task_scheduler_nrscope->rach_found or !task_scheduler_nrscope->dci_inited){
    std::cout << "RACH not found or DCI decoder not initialized, quitting..." << std::endl;
    return SRSRAN_SUCCESS;
  }

  DCIFeedback new_result;
  task_scheduler_nrscope->result = new_result;

  task_scheduler_nrscope->result.dl_grants.resize(task_scheduler_nrscope->nof_known_rntis);
  task_scheduler_nrscope->result.ul_grants.resize(task_scheduler_nrscope->nof_known_rntis);
  task_scheduler_nrscope->result.spare_dl_prbs.resize(task_scheduler_nrscope->nof_known_rntis);
  task_scheduler_nrscope->result.spare_dl_tbs.resize(task_scheduler_nrscope->nof_known_rntis);
  task_scheduler_nrscope->result.spare_dl_bits.resize(task_scheduler_nrscope->nof_known_rntis);
  task_scheduler_nrscope->result.spare_ul_prbs.resize(task_scheduler_nrscope->nof_known_rntis);
  task_scheduler_nrscope->result.spare_ul_tbs.resize(task_scheduler_nrscope->nof_known_rntis);
  task_scheduler_nrscope->result.spare_ul_bits.resize(task_scheduler_nrscope->nof_known_rntis);
  task_scheduler_nrscope->result.dl_dcis.resize(task_scheduler_nrscope->nof_known_rntis);
  task_scheduler_nrscope->result.ul_dcis.resize(task_scheduler_nrscope->nof_known_rntis);

  // Set the buffer to 0s
  for(uint32_t idx = 0; idx < task_scheduler_nrscope->nof_known_rntis; idx++){
    memset(&dci_dl[idx], 0, sizeof(srsran_dci_dl_nr_t));
    memset(&dci_ul[idx], 0, sizeof(srsran_dci_dl_nr_t));
  }
  
  srsran_ue_dl_nr_estimate_fft_nrscope(&ue_dl_dci, slot, arg_scs);

  int total_dl_dci = 0;
  int total_ul_dci = 0;  

  for (uint32_t rnti_idx = 0; rnti_idx < task_scheduler_nrscope->nof_known_rntis; rnti_idx++){
    
    memcpy(ue_dl_tmp, &ue_dl_dci, sizeof(srsran_ue_dl_nr_t));
    memcpy(slot_tmp, slot, sizeof(srsran_slot_cfg_t));

    int nof_dl_dci = srsran_ue_dl_nr_find_dl_dci_nrscope_dciloop(ue_dl_tmp, slot_tmp, task_scheduler_nrscope->known_rntis[rnti_idx], 
                     srsran_rnti_type_c, dci_dl_tmp, 4);

    if (nof_dl_dci < SRSRAN_SUCCESS) {
      ERROR("Error in blind search");
    }

    int nof_ul_dci = srsran_ue_dl_nr_find_ul_dci(ue_dl_tmp, slot_tmp, task_scheduler_nrscope->known_rntis[rnti_idx], srsran_rnti_type_c, dci_ul_tmp, 4);

    if(nof_dl_dci > 0){
      dci_dl[rnti_idx] = dci_dl_tmp[0];
      total_dl_dci += nof_dl_dci;
    }

    if(nof_ul_dci > 0){
      dci_ul[rnti_idx] = dci_ul_tmp[0];
      total_ul_dci += nof_ul_dci;
    }
  }  

  if(total_dl_dci > 0){
    for (uint32_t dci_idx_dl = 0; dci_idx_dl < task_scheduler_nrscope->nof_known_rntis; dci_idx_dl++){
      // the rnti will not be copied if no dci found
      if(dci_dl[dci_idx_dl].ctx.rnti == task_scheduler_nrscope->known_rntis[dci_idx_dl]){
        task_scheduler_nrscope->result.dl_dcis[dci_idx_dl] = dci_dl[dci_idx_dl];
        char str[1024] = {};
        srsran_dci_dl_nr_to_str(&(ue_dl_dci.dci), &dci_dl[dci_idx_dl], str, (uint32_t)sizeof(str));
        printf("DCIDecoder -- Found DCI: %s\n", str);
        // The grant may not be decoded correctly, since srsRAN's code is not complete.
        // We can calculate the DL bandwidth for this subframe by ourselves.
        if(dci_dl[dci_idx_dl].ctx.format == srsran_dci_format_nr_1_1) {
          srsran_sch_cfg_nr_t pdsch_cfg = {};
          pdsch_hl_cfg.mcs_table = srsran_mcs_table_256qam;

          if (srsran_ra_dl_dci_to_grant_nr(&carrier_dl, slot, &pdsch_hl_cfg, &dci_dl[dci_idx_dl], 
                                          &pdsch_cfg, &pdsch_cfg.grant) < SRSRAN_SUCCESS) {
            ERROR("Error decoding PDSCH search");
            // return result;
          }
          srsran_sch_cfg_nr_info(&pdsch_cfg, str, (uint32_t)sizeof(str));
          printf("DCIDecoder -- PDSCH_cfg:\n%s", str);


          /* Trying to decode the RRC Reconfiguration*/
          // if (srsran_softbuffer_rx_init_guru(&softbuffer, SRSRAN_SCH_NR_MAX_NOF_CB_LDPC, SRSRAN_LDPC_MAX_LEN_ENCODED_CB) <
          //     SRSRAN_SUCCESS) {
          //   ERROR("SIBDecoder -- Error init soft-buffer");
          //   return SRSRAN_ERROR;
          // }

          // // Reset the data_pdcch to zeros
          // srsran_vec_u8_zero(data_pdcch, SRSRAN_SLOT_MAX_NOF_BITS_NR);
          
          // pdsch_cfg.grant.tb[0].softbuffer.rx = &softbuffer; // Set softbuffer
          // pdsch_res = {}; // Prepare PDSCH result
          // pdsch_res.tb[0].payload = data_pdcch;

          // // Decode PDSCH
          // if (srsran_ue_dl_nr_decode_pdsch(ue_dl_tmp, slot, &pdsch_cfg, &pdsch_res) < SRSRAN_SUCCESS) {
          //   printf("DCIDecoder -- Error decoding PDSCH search\n");
          //   return SRSRAN_ERROR;
          // }
          // if (!pdsch_res.tb[0].crc) {
          //   printf("DCIDecoder -- Error decoding PDSCH (CRC)\n");
          //   return SRSRAN_ERROR;
          // }
          // printf("DCIDecoder Decoded PDSCH (%d B)\n", pdsch_cfg.grant.tb[0].tbs / 8);
          // srsran_vec_fprint_byte(stdout, pdsch_res.tb[0].payload, pdsch_cfg.grant.tb[0].tbs / 8);

          // // check payload is not all null
          // bool all_zero = true;
          // for (int i = 0; i < pdsch_cfg.grant.tb[0].tbs / 8; ++i) {
          //   if (pdsch_res.tb[0].payload[i] != 0x0) {
          //     all_zero = false;
          //     break;
          //   }
          // }
          // if (all_zero) {
          //   ERROR("PDSCH payload is all zeros");
          //   return SRSRAN_ERROR;
          // }
          // std::cout << "Try to decode RRC_RECFG..." << std::endl;
          // asn1::rrc_nr::bcch_dl_sch_msg_s dlsch_msg;
          // asn1::cbit_ref dlsch_bref(pdsch_res.tb[0].payload, pdsch_cfg.grant.tb[0].tbs / 8);
          // asn1::SRSASN_CODE err = task_scheduler_nrscope->rrc_recfg.unpack(dlsch_bref);

          // asn1::json_writer jw;
          // task_scheduler_nrscope->rrc_recfg.to_json(jw);
          // printf("Decoded RRC_RECFG: %s\n", jw.to_string().c_str());
          /* Trying to decode the RRC Reconfiguration*/


          task_scheduler_nrscope->result.dl_grants[dci_idx_dl] = pdsch_cfg;
          task_scheduler_nrscope->result.nof_dl_used_prbs += pdsch_cfg.grant.nof_prb * pdsch_cfg.grant.L;

          dl_prb_rate[dci_idx_dl] = (float)(pdsch_cfg.grant.tb[0].tbs + pdsch_cfg.grant.tb[1].tbs) / (float)pdsch_cfg.grant.nof_prb / (float)pdsch_cfg.grant.L;
          dl_prb_bits_rate[dci_idx_dl] = (float)(pdsch_cfg.grant.tb[0].nof_bits + pdsch_cfg.grant.tb[1].nof_bits) / (float)pdsch_cfg.grant.nof_prb / (float)pdsch_cfg.grant.L;
        }
      }
    }
    task_scheduler_nrscope->result.nof_dl_spare_prbs = carrier_dl.nof_prb * (14 - 2) - task_scheduler_nrscope->result.nof_dl_used_prbs;
    for(uint32_t idx = 0; idx < task_scheduler_nrscope->nof_known_rntis; idx ++){
      task_scheduler_nrscope->result.spare_dl_prbs[idx] = task_scheduler_nrscope->result.nof_dl_spare_prbs / task_scheduler_nrscope->nof_known_rntis;
      if(abs(task_scheduler_nrscope->result.spare_dl_prbs[idx]) > carrier_dl.nof_prb * (14 - 2)){
        task_scheduler_nrscope->result.spare_dl_prbs[idx] = 0;
      }
      task_scheduler_nrscope->result.spare_dl_tbs[idx] = (int) ((float)task_scheduler_nrscope->result.spare_dl_prbs[idx] * dl_prb_rate[idx]);
      task_scheduler_nrscope->result.spare_dl_bits[idx] = (int) ((float)task_scheduler_nrscope->result.spare_dl_prbs[idx] * dl_prb_bits_rate[idx]);
    }
  }else{
    task_scheduler_nrscope->result.nof_dl_spare_prbs = carrier_dl.nof_prb * (14 - 2);
    for(uint32_t idx = 0; idx < task_scheduler_nrscope->nof_known_rntis; idx++){
      task_scheduler_nrscope->result.spare_dl_prbs[idx] = (int)((float)task_scheduler_nrscope->result.nof_dl_spare_prbs / (float)task_scheduler_nrscope->nof_known_rntis);
      task_scheduler_nrscope->result.spare_dl_tbs[idx] = (int) ((float)task_scheduler_nrscope->result.spare_dl_prbs[idx] * dl_prb_rate[idx]);
      task_scheduler_nrscope->result.spare_dl_bits[idx] = (int) ((float)task_scheduler_nrscope->result.spare_dl_prbs[idx] * dl_prb_bits_rate[idx]);
    }
  }
  
  if(total_ul_dci > 0){
    for (uint32_t dci_idx_ul = 0; dci_idx_ul < task_scheduler_nrscope->nof_known_rntis; dci_idx_ul++){
      if(dci_ul[dci_idx_ul].ctx.rnti == task_scheduler_nrscope->known_rntis[dci_idx_ul]){
        task_scheduler_nrscope->result.ul_dcis[dci_idx_ul] = dci_ul[dci_idx_ul];
        char str[1024] = {};
        srsran_dci_ul_nr_to_str(&(ue_dl_dci.dci), &dci_ul[dci_idx_ul], str, (uint32_t)sizeof(str));
        printf("DCIDecoder -- Found DCI: %s\n", str);
        // The grant may not be decoded correctly, since srsRAN's code is not complete. 
        // We can calculate the UL bandwidth for this subframe by ourselves.
        srsran_sch_cfg_nr_t pusch_cfg = {};
        pusch_hl_cfg.mcs_table = srsran_mcs_table_256qam;
        if (srsran_ra_ul_dci_to_grant_nr(&carrier_dl, slot, &pusch_hl_cfg, &dci_ul[dci_idx_ul], 
                                        &pusch_cfg, &pusch_cfg.grant) < SRSRAN_SUCCESS) {
          ERROR("Error decoding PUSCH search");
          // return result;
        }
        srsran_sch_cfg_nr_info(&pusch_cfg, str, (uint32_t)sizeof(str));
        printf("DCIDecoder -- PUSCH_cfg:\n%s", str);

        task_scheduler_nrscope->result.ul_grants[dci_idx_ul] = pusch_cfg;
        task_scheduler_nrscope->result.nof_ul_used_prbs += pusch_cfg.grant.nof_prb * pusch_cfg.grant.L;
        
        ul_prb_rate[dci_idx_ul] = (float)(pusch_cfg.grant.tb[0].tbs + pusch_cfg.grant.tb[1].tbs) / (float)pusch_cfg.grant.nof_prb / (float)pusch_cfg.grant.L;
        ul_prb_bits_rate[dci_idx_ul] = (float)(pusch_cfg.grant.tb[0].nof_bits + pusch_cfg.grant.tb[1].nof_bits) / (float)pusch_cfg.grant.nof_prb / (float)pusch_cfg.grant.L;
      }
    }
    task_scheduler_nrscope->result.nof_ul_spare_prbs = carrier_dl.nof_prb * (14 - 2) - task_scheduler_nrscope->result.nof_ul_used_prbs;
    for(uint32_t idx = 0; idx < task_scheduler_nrscope->nof_known_rntis; idx ++){
      task_scheduler_nrscope->result.spare_ul_prbs[idx] = task_scheduler_nrscope->result.nof_ul_spare_prbs / task_scheduler_nrscope->nof_known_rntis;
      task_scheduler_nrscope->result.spare_ul_tbs[idx] = (int) ((float)task_scheduler_nrscope->result.spare_ul_prbs[idx] * ul_prb_rate[idx]);
      task_scheduler_nrscope->result.spare_ul_bits[idx] = (int) ((float)task_scheduler_nrscope->result.spare_ul_prbs[idx] * ul_prb_bits_rate[idx]);
    }
  }else{
    task_scheduler_nrscope->result.nof_ul_spare_prbs = carrier_dl.nof_prb * (14 - 2);
    for(uint32_t idx = 0; idx < task_scheduler_nrscope->nof_known_rntis; idx ++){
      task_scheduler_nrscope->result.spare_ul_prbs[idx] = (int)((float)task_scheduler_nrscope->result.nof_ul_spare_prbs / (float)task_scheduler_nrscope->nof_known_rntis);
      if(abs(task_scheduler_nrscope->result.spare_ul_prbs[idx]) > carrier_dl.nof_prb * (14 - 2)){
        task_scheduler_nrscope->result.spare_ul_prbs[idx] = 0;
      }
      task_scheduler_nrscope->result.spare_ul_tbs[idx] = (int) ((float)task_scheduler_nrscope->result.spare_ul_prbs[idx] * ul_prb_rate[idx]);
      task_scheduler_nrscope->result.spare_ul_bits[idx] = (int) ((float)task_scheduler_nrscope->result.spare_ul_prbs[idx] * ul_prb_bits_rate[idx]);
    }
  }

  return SRSRAN_SUCCESS;
}