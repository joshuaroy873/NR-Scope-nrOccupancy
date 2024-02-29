#include "nrscope/hdr/to_google.h"

namespace ToGoogle{
  std::thread google_thread;
  std::mutex to_google_lock;
  std::queue<LogNode> to_google_queue;

  bool run_google;
  
  int list_length;
  int list_count;

  std::string google_credential;
  std::string google_project_id;
  std::string google_dataset_id;


  void init_to_google(std::string google_credential_input, std::string google_dataset_id_input){
    run_google = true;
    google_credential = google_credential_input;
    // google_project_id = google_project_id_input;
    google_dataset_id = google_dataset_id_input;
    google_thread = std::thread(to_google_thread);
  }

  void push_google_node(LogNode input_log){
    to_google_lock.lock();
    to_google_queue.push(input_log);
    to_google_lock.unlock();
  }

  void to_google_thread(){
    struct sigaction sigIntHandler;

    sigIntHandler.sa_handler = my_sig_handler;
    sigemptyset(&sigIntHandler.sa_mask);
    sigIntHandler.sa_flags = 0;

    sigaction(SIGINT, &sigIntHandler, NULL);

    PyObject *pName, *pModule, *pCreate, *pPush;
    PyObject *pClient, *pDict, *pList;
    PyObject *pInt, *pDouble, *pStr, *pCredential, *pProjectID, *pDatasetID, *pInput;
    setenv("PYTHONPATH", ".", 0);

    Py_Initialize();
    pName = PyUnicode_FromString((char*)"to_google");

    pModule = PyImport_Import(pName);
    Py_DECREF(pName);

    if (pModule != NULL) {
      pCreate = PyObject_GetAttrString(pModule, "create_table_with_position_and_time");
      pPush = PyObject_GetAttrString(pModule, "push_data_to_table");
      /* pFunc is a new reference */

      if (pCreate && PyCallable_Check(pCreate)) {
        pCredential = PyUnicode_FromString(google_credential.c_str());
        // pProjectID = PyUnicode_FromString(google_project_id.c_str());
        pDatasetID = PyUnicode_FromString(google_dataset_id.c_str());
			  pInput = PyTuple_Pack(2, pCredential, pDatasetID);
        pClient = PyObject_CallObject(pCreate, pInput);
      }else {
        Py_DECREF(pCreate);
        Py_DECREF(pModule);
        PyErr_Print();
        fprintf(stderr,"Call failed\n");
        return;
      }
    }else {
      if (PyErr_Occurred())
          PyErr_Print();
      fprintf(stderr, "Cannot find function.\n");
    }
    list_count = 0;
    list_length = 4000;
    pList = PyList_New(list_length);
    
    while(run_google){
      if(to_google_queue.size()>0){
        std::cout << "Pushing to queue..." << std::endl;
        LogNode new_entry;
        to_google_lock.lock();
        new_entry = to_google_queue.front();
        to_google_queue.pop();
        to_google_lock.unlock();

        std::cout << "timestamp: " << new_entry.timestamp << std::endl;

        int first_prb = SRSRAN_MAX_PRB_NR;
        for (int i = 0; i < SRSRAN_MAX_PRB_NR && first_prb == SRSRAN_MAX_PRB_NR; i++) {
          if (new_entry.grant.grant.prb_idx[i]) {
            first_prb = i;
          }
        }
        // Initialize and set values of dictionary.
        pDict = PyDict_New();
        pDouble = PyFloat_FromDouble(new_entry.timestamp);
        PyDict_SetItemString(pDict, "timestamp", pDouble);
        pInt = PyLong_FromLong(new_entry.system_frame_idx);
        PyDict_SetItemString(pDict, "system_frame_index", pInt);
        pInt = PyLong_FromLong(new_entry.slot_idx);
        PyDict_SetItemString(pDict, "slot_index", pInt);
        pInt = PyLong_FromLong(new_entry.grant.grant.rnti);
        PyDict_SetItemString(pDict, "rnti", pInt);
        pStr = PyUnicode_FromString(srsran_rnti_type_str(new_entry.grant.grant.rnti_type));
        PyDict_SetItemString(pDict, "rnti_type", pStr);
        pStr = PyUnicode_FromString(new_entry.dci_format.c_str());
        PyDict_SetItemString(pDict, "dci_format", pStr);
        pInt = PyLong_FromLong(new_entry.grant.grant.k);
        PyDict_SetItemString(pDict, "k", pInt);
        pStr = PyUnicode_FromString(sch_mapping_to_str(new_entry.grant.grant.mapping));
        PyDict_SetItemString(pDict, "mapping", pStr);
        pInt = PyLong_FromLong(new_entry.grant.grant.S);
        PyDict_SetItemString(pDict, "time_start", pInt);
        pInt = PyLong_FromLong(new_entry.grant.grant.L);
        PyDict_SetItemString(pDict, "time_length", pInt);
        pInt = PyLong_FromLong(first_prb);
        PyDict_SetItemString(pDict, "frequency_start", pInt);
        pInt = PyLong_FromLong(new_entry.grant.grant.nof_prb);
        PyDict_SetItemString(pDict, "frequency_length", pInt);
        pInt = PyLong_FromLong(new_entry.grant.grant.nof_dmrs_cdm_groups_without_data);
        PyDict_SetItemString(pDict, "nof_dmrs_cdm_groups", pInt);
        pDouble = PyFloat_FromDouble(new_entry.grant.grant.beta_dmrs);
        PyDict_SetItemString(pDict, "beta_dmrs", pDouble);
        pInt = PyLong_FromLong(new_entry.grant.grant.nof_layers);
        PyDict_SetItemString(pDict, "nof_layers", pInt);
        pInt = PyLong_FromLong(new_entry.grant.grant.n_scid);
        PyDict_SetItemString(pDict, "n_scid", pInt);
        pInt = PyLong_FromLong(new_entry.grant.grant.tb_scaling_field);
        PyDict_SetItemString(pDict, "tb_scaling_field", pInt);
        pStr = PyUnicode_FromString(srsran_mod_string(new_entry.grant.grant.tb[0].mod));
        PyDict_SetItemString(pDict, "modulation", pStr);
        pInt = PyLong_FromLong(new_entry.grant.grant.tb[0].mcs);
        PyDict_SetItemString(pDict, "mcs_index", pInt);
        pInt = PyLong_FromLong(new_entry.grant.grant.tb[0].tbs);
        PyDict_SetItemString(pDict, "transport_block_size", pInt);
        pDouble = PyFloat_FromDouble(new_entry.grant.grant.tb[0].R);
        PyDict_SetItemString(pDict, "code_rate", pDouble);
        pInt = PyLong_FromLong(new_entry.grant.grant.tb[0].rv);
        PyDict_SetItemString(pDict, "redundancy_version", pInt);
        pInt = PyLong_FromLong(new_entry.grant.grant.tb[0].ndi);
        PyDict_SetItemString(pDict, "new_data_indicator", pInt);
        pInt = PyLong_FromLong(new_entry.grant.grant.tb[0].nof_re);
        PyDict_SetItemString(pDict, "nof_re", pInt);
        pInt = PyLong_FromLong(new_entry.grant.grant.tb[0].nof_bits);
        PyDict_SetItemString(pDict, "nof_bits", pInt);
        pStr = PyUnicode_FromString(srsran_mcs_table_to_str(new_entry.grant.sch_cfg.mcs_table));
        PyDict_SetItemString(pDict, "mcs_table", pStr);
        pStr = PyUnicode_FromString(srsran_mcs_table_to_str(new_entry.grant.sch_cfg.mcs_table));
        PyDict_SetItemString(pDict, "xoverhead", pStr);

        PyList_SetItem(pList, list_count, pDict);
        list_count += 1;
        std::cout << "Current list count: " << list_count << std::endl;

        if(list_count == list_length){
          list_count = 0;
          if(pList != NULL){
            PyTuple_SetItem(pClient, 3, pList);
            printf("Pushing to google...\n");
            PyObject_CallObject(pPush, pClient);
            pList = PyList_New(list_length);
            // Py_DECREF(pClient);
          }else{
            fprintf(stderr,"Creating new dict failed\n");
          }
        }
      }else{
        printf("No node in the queue...\n");
        usleep(1000);
      }
    }
  }     

  void exit_to_google(){
    to_google_lock.lock();
    run_google = false;
    to_google_lock.unlock();
  }
};