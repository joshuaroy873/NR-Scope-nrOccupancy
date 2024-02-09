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

}

DCIDecoder::~DCIDecoder(){

}

DCIFeedback DCIDecoder::decode_and_parse_dci_from_slot(srsran_ue_dl_nr_t* ue_dl,
                                              srsran_slot_cfg_t* slot,
                                              srsran_ue_dl_nr_sratescs_info arg_scs,
                                              srsran_carrier_nr_t* carrier_dl,
                                              srsran_sch_hl_cfg_nr_t* pdsch_hl_cfg,
                                              srsran_sch_hl_cfg_nr_t* pusch_hl_cfg,
                                              srsran_softbuffer_rx_t* softbuffer,
                                              asn1::rrc_nr::rrc_setup_s* rrc_setup,
                                              asn1::rrc_nr::cell_group_cfg_s* master_cell_group,
                                              uint16_t* known_rntis,
                                              uint32_t nof_known_rntis, 
                                              uint16_t target_rnti){
  DCIFeedback result;

  result.dl_grants.resize(nof_known_rntis);
  result.ul_grants.resize(nof_known_rntis);
  result.spare_dl_prbs.resize(nof_known_rntis);
  result.spare_dl_tbs.resize(nof_known_rntis);
  result.spare_dl_bits.resize(nof_known_rntis);
  result.spare_ul_prbs.resize(nof_known_rntis);
  result.spare_ul_tbs.resize(nof_known_rntis);
  result.spare_ul_bits.resize(nof_known_rntis);
  result.dl_dcis.resize(nof_known_rntis);
  result.ul_dcis.resize(nof_known_rntis);

  // Set the buffer to 0s
  for(uint32_t idx = 0; idx < nof_known_rntis; idx++){
    memset(&dci_dl[idx], 0, sizeof(srsran_dci_dl_nr_t));
    memset(&dci_ul[idx], 0, sizeof(srsran_dci_dl_nr_t));
  }
  
  srsran_ue_dl_nr_estimate_fft_nrscope(ue_dl, slot, arg_scs);

  int total_dl_dci = 0;
  int total_ul_dci = 0;  
  
  struct timeval t0, t1;
  gettimeofday(&t0, NULL);

  // srsran_dci_nr_t* ue_dci_dl = (srsran_dci_nr_t*) malloc(sizeof(srsran_dci_nr_t) * (nof_known_rntis));
  // srsran_dci_nr_t* ue_dci_ul = (srsran_dci_nr_t*) malloc(sizeof(srsran_dci_nr_t) * (nof_known_rntis));

  for (uint32_t rnti_idx = 0; rnti_idx < nof_known_rntis; rnti_idx++){
    
    memcpy(ue_dl_tmp, ue_dl, sizeof(srsran_ue_dl_nr_t));
    memcpy(slot_tmp, slot, sizeof(srsran_slot_cfg_t));

    int nof_dl_dci = srsran_ue_dl_nr_find_dl_dci_nrscope_dciloop(ue_dl_tmp, slot_tmp, known_rntis[rnti_idx], 
                     srsran_rnti_type_c, dci_dl_tmp, 4);

    if (nof_dl_dci < SRSRAN_SUCCESS) {
      ERROR("Error in blind search");
    }

    int nof_ul_dci = srsran_ue_dl_nr_find_ul_dci(ue_dl_tmp, slot_tmp, known_rntis[rnti_idx], srsran_rnti_type_c, dci_ul_tmp, 4);

    // nof_dl_dci and nof_ul_dci are at most 1.
    if(nof_dl_dci > 0){
      dci_dl[rnti_idx] = dci_dl_tmp[0];
      // ue_dci_dl[rnti_idx] = ue_dl->dci;
      total_dl_dci += nof_dl_dci;
    }

    if(nof_ul_dci > 0){
      dci_ul[rnti_idx] = dci_ul_tmp[0];
      // ue_dci_ul[rnti_idx] = ue_dl->dci;
      total_ul_dci += nof_ul_dci;
    }
  }  

  // printf("total_dl_dci: %d\n", total_dl_dci);
  // printf("total_ul_dci: %d\n", total_ul_dci);

  if(total_dl_dci > 0){
    for (uint32_t dci_idx_dl = 0; dci_idx_dl < nof_known_rntis; dci_idx_dl++){
      // the rnti will not be copied if no dci found
      if(dci_dl[dci_idx_dl].ctx.rnti == known_rntis[dci_idx_dl]){
        result.dl_dcis[dci_idx_dl] = dci_dl[dci_idx_dl];
        char str[1024] = {};
        srsran_dci_dl_nr_to_str(&(ue_dl->dci), &dci_dl[dci_idx_dl], str, (uint32_t)sizeof(str));
        printf("Found DCI: %s\n", str);
        // The grant may not be decoded correctly, since srsRAN's code is not complete.
        // We can calculate the DL bandwidth for this subframe by ourselves.
        if(dci_dl[dci_idx_dl].ctx.format == srsran_dci_format_nr_1_1) {
          srsran_sch_cfg_nr_t pdsch_cfg = {};
          pdsch_hl_cfg->mcs_table = srsran_mcs_table_256qam;
          if (srsran_ra_dl_dci_to_grant_nr(carrier_dl, slot, pdsch_hl_cfg, &dci_dl[dci_idx_dl], 
                                          &pdsch_cfg, &pdsch_cfg.grant) < SRSRAN_SUCCESS) {
            ERROR("Error decoding PDSCH search");
            // return result;
          }
          srsran_sch_cfg_nr_info(&pdsch_cfg, str, (uint32_t)sizeof(str));
          printf("PDSCH_cfg:\n%s", str);

          result.dl_grants[dci_idx_dl] = pdsch_cfg;
          // result.total_dl_tbs += pdsch_cfg.grant.tb[0].tbs + pdsch_cfg.grant.tb[1].tbs;
          result.nof_dl_used_prbs += pdsch_cfg.grant.nof_prb * pdsch_cfg.grant.L;
          // std::cout << "nof prb: " << result.nof_dl_used_prbs << std::endl;

          dl_prb_rate[dci_idx_dl] = (float)(pdsch_cfg.grant.tb[0].tbs + pdsch_cfg.grant.tb[1].tbs) / (float)pdsch_cfg.grant.nof_prb / (float)pdsch_cfg.grant.L;
          dl_prb_bits_rate[dci_idx_dl] = (float)(pdsch_cfg.grant.tb[0].nof_bits + pdsch_cfg.grant.tb[1].nof_bits) / (float)pdsch_cfg.grant.nof_prb / (float)pdsch_cfg.grant.L;
        }
      }
    }
    result.nof_dl_spare_prbs = carrier_dl->nof_prb * (14 - 2) - result.nof_dl_used_prbs;
    for(uint32_t idx = 0; idx < nof_known_rntis; idx ++){
      result.spare_dl_prbs[idx] = result.nof_dl_spare_prbs / nof_known_rntis;
      if(abs(result.spare_dl_prbs[idx]) > carrier_dl->nof_prb * (14 - 2)){
        result.spare_dl_prbs[idx] = 0;
      }
      result.spare_dl_tbs[idx] = (int) ((float)result.spare_dl_prbs[idx] * dl_prb_rate[idx]);
      result.spare_dl_bits[idx] = (int) ((float)result.spare_dl_prbs[idx] * dl_prb_bits_rate[idx]);
    }
  }else{
    result.nof_dl_spare_prbs = carrier_dl->nof_prb * (14 - 2);
    for(uint32_t idx = 0; idx < nof_known_rntis; idx++){
      result.spare_dl_prbs[idx] = (int)((float)result.nof_dl_spare_prbs / (float)nof_known_rntis);
      result.spare_dl_tbs[idx] = (int) ((float)result.spare_dl_prbs[idx] * dl_prb_rate[idx]);
      result.spare_dl_bits[idx] = (int) ((float)result.spare_dl_prbs[idx] * dl_prb_bits_rate[idx]);
    }
  }
  
  if(total_ul_dci > 0){
    for (uint32_t dci_idx_ul = 0; dci_idx_ul < nof_known_rntis; dci_idx_ul++){
      if(dci_ul[dci_idx_ul].ctx.rnti == known_rntis[dci_idx_ul]){
        result.ul_dcis[dci_idx_ul] = dci_ul[dci_idx_ul];
        char str[1024] = {};
        srsran_dci_ul_nr_to_str(&(ue_dl->dci), &dci_ul[dci_idx_ul], str, (uint32_t)sizeof(str));
        printf("Found DCI: %s\n", str);
        // The grant may not be decoded correctly, since srsRAN's code is not complete. 
        // We can calculate the UL bandwidth for this subframe by ourselves.
        srsran_sch_cfg_nr_t pusch_cfg = {};
        pusch_hl_cfg->mcs_table = srsran_mcs_table_256qam;
        if (srsran_ra_ul_dci_to_grant_nr(carrier_dl, slot, pusch_hl_cfg, &dci_ul[dci_idx_ul], 
                                        &pusch_cfg, &pusch_cfg.grant) < SRSRAN_SUCCESS) {
          ERROR("Error decoding PUSCH search");
          // return result;
        }
        srsran_sch_cfg_nr_info(&pusch_cfg, str, (uint32_t)sizeof(str));
        printf("PUSCH_cfg:\n%s", str);

        result.ul_grants[dci_idx_ul] = pusch_cfg;
        // result.total_ul_tbs += pusch_cfg.grant.tb[0].tbs + pusch_cfg.grant.tb[1].tbs;
        result.nof_ul_used_prbs += pusch_cfg.grant.nof_prb * pusch_cfg.grant.L;
        // std::cout << "nof prb: " << result.nof_ul_used_prbs << std::endl;
        
        ul_prb_rate[dci_idx_ul] = (float)(pusch_cfg.grant.tb[0].tbs + pusch_cfg.grant.tb[1].tbs) / (float)pusch_cfg.grant.nof_prb / (float)pusch_cfg.grant.L;
        ul_prb_bits_rate[dci_idx_ul] = (float)(pusch_cfg.grant.tb[0].nof_bits + pusch_cfg.grant.tb[1].nof_bits) / (float)pusch_cfg.grant.nof_prb / (float)pusch_cfg.grant.L;
      }
    }
    result.nof_ul_spare_prbs = carrier_dl->nof_prb * (14 - 2) - result.nof_ul_used_prbs;
    for(uint32_t idx = 0; idx < nof_known_rntis; idx ++){
      result.spare_ul_prbs[idx] = result.nof_ul_spare_prbs / nof_known_rntis;
      result.spare_ul_tbs[idx] = (int) ((float)result.spare_ul_prbs[idx] * ul_prb_rate[idx]);
      result.spare_ul_bits[idx] = (int) ((float)result.spare_ul_prbs[idx] * ul_prb_bits_rate[idx]);
    }
  }else{
    result.nof_ul_spare_prbs = carrier_dl->nof_prb * (14 - 2);
    for(uint32_t idx = 0; idx < nof_known_rntis; idx ++){
      result.spare_ul_prbs[idx] = (int)((float)result.nof_ul_spare_prbs / (float)nof_known_rntis);
      if(abs(result.spare_ul_prbs[idx]) > carrier_dl->nof_prb * (14 - 2)){
        result.spare_ul_prbs[idx] = 0;
      }
      result.spare_ul_tbs[idx] = (int) ((float)result.spare_ul_prbs[idx] * ul_prb_rate[idx]);
      result.spare_ul_bits[idx] = (int) ((float)result.spare_ul_prbs[idx] * ul_prb_bits_rate[idx]);
    }
  }
  gettimeofday(&t1, NULL);  
  result.processing_time_us = t1.tv_usec - t0.tv_usec;   
  printf("time_spend:%ld (us)\n", t1.tv_usec - t0.tv_usec);

  return result;
}