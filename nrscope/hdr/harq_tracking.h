#ifndef HARQ_TRACKING_H
#define HARQ_TRACKING_H

#include <stdint.h>
#include "cassert"
#include "vector"



class HarqTracker{
  public:
    int nof_known_rntis;
    std::vector<std::vector<int>> dl_ue_ndi;
    std::vector<std::vector<int>> ul_ue_ndi;

    HarqTracker(int nof_know_rntis);
    ~HarqTracker();

    /**
    * To determine if the received data is new data or a retransmission
    *
    * @param ue_id ue_id in the known_rnti list
    * @param ndi new data indicator from the DCI
    * @param harq_id harq_id from the DCI
    * @param is_dl 1 for this is a dl DCI, 0 for this is ul DCI
    * @return True for this is a new data, false for this is a retransmission
    */
    bool is_new_data(int ue_id, int ndi, int harq_id, bool is_dl);
};

#endif