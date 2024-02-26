#define PY_SSIZE_T_CLEAN
#include <Python.h>

int main(int argc, char *argv[])
{
	PyObject *pName, *pModule, *pCreate, *pPush;
	PyObject *pClient, *pDict, *pList;
	PyObject *pInt, *pDouble, *pStr;

	setenv("PYTHONPATH", ".", 0);

	Py_Initialize();
	pName = PyUnicode_FromString((char*)"bigquery_table_create");

	pModule = PyImport_Import(pName);
	Py_DECREF(pName);

	if (pModule != NULL) {
		pCreate = PyObject_GetAttrString(pModule, "create_table_with_position_and_time");
		pPush = PyObject_GetAttrString(pModule, "push_data_to_table");
		/* pFunc is a new reference */

		if (pCreate && PyCallable_Check(pCreate)) {
			printf("Creating table...\n");
			pClient = PyObject_CallObject(pCreate, NULL);

			if (pClient != NULL) {
				if (pPush && PyCallable_Check(pPush)){
					// Initialize and set values of dictionary.
					pDict = PyDict_New();
					pDouble = PyFloat_FromDouble(100.1);
					PyDict_SetItemString(pDict, "timestamp", pDouble);
					pInt = PyLong_FromLong(100);
					PyDict_SetItemString(pDict, "system_frame_index", pInt);
					pInt = PyLong_FromLong(12);
					PyDict_SetItemString(pDict, "slot_index", pInt);
					pInt = PyLong_FromLong(17042);
					PyDict_SetItemString(pDict, "rnti", pInt);
					pStr = PyUnicode_FromString("c-rnti");
					PyDict_SetItemString(pDict, "rnti_type", pStr);
					pInt = PyLong_FromLong(0);
					PyDict_SetItemString(pDict, "k", pInt);
					pStr = PyUnicode_FromString("A");
					PyDict_SetItemString(pDict, "mapping", pStr);
					pInt = PyLong_FromLong(0);
					PyDict_SetItemString(pDict, "time_start", pInt);
					pInt = PyLong_FromLong(12);
					PyDict_SetItemString(pDict, "time_length", pInt);
					pInt = PyLong_FromLong(0);
					PyDict_SetItemString(pDict, "frequency_start", pInt);
					pInt = PyLong_FromLong(1);
					PyDict_SetItemString(pDict, "frequency_length", pInt);
					pInt = PyLong_FromLong(1);
					PyDict_SetItemString(pDict, "nof_dmrs_cdm_groups", pInt);
					pDouble = PyFloat_FromDouble(1.14);
					PyDict_SetItemString(pDict, "beta_dmrs", pDouble);
					pInt = PyLong_FromLong(2);
					PyDict_SetItemString(pDict, "nof_layers", pInt);
					pInt = PyLong_FromLong(1);
					PyDict_SetItemString(pDict, "n_scid", pInt);
					pInt = PyLong_FromLong(1);
					PyDict_SetItemString(pDict, "tb_scaling_field", pInt);
					pStr = PyUnicode_FromString("16QAM");
					PyDict_SetItemString(pDict, "modulation", pStr);
					pInt = PyLong_FromLong(8);
					PyDict_SetItemString(pDict, "mcs_index", pInt);
					pInt = PyLong_FromLong(512);
					PyDict_SetItemString(pDict, "transport_block_size", pInt);
					pDouble = PyFloat_FromDouble(0.516);
					PyDict_SetItemString(pDict, "code_rate", pDouble);
					pInt = PyLong_FromLong(1);
					PyDict_SetItemString(pDict, "redundancy_version", pInt);
					pInt = PyLong_FromLong(1);
					PyDict_SetItemString(pDict, "new_data_indicator", pInt);
					pInt = PyLong_FromLong(12);
					PyDict_SetItemString(pDict, "nof_re", pInt);
					pInt = PyLong_FromLong(132);
					PyDict_SetItemString(pDict, "nof_bits", pInt);
					pStr = PyUnicode_FromString("256QAM");
					PyDict_SetItemString(pDict, "mcs_table", pStr);
					pInt = PyLong_FromLong(0);
					PyDict_SetItemString(pDict, "xoverhead", pInt);

					pList = PyList_New(8000);
					for (int i = 0; i < 8000; i++){
						PyList_SetItem(pList, i, pDict);
					}
					if(pDict != NULL && pList != NULL){
						PyTuple_SetItem(pClient, 3, pList);
						PyObject_CallObject(pPush, pClient);
						printf("Pushing data...\n");
						Py_DECREF(pClient);
					}else{
						fprintf(stderr,"Creating new dict failed\n");
					}
				}else{
					Py_DECREF(pPush);
					fprintf(stderr,"Call push failed\n");
				}
			}
			else {
				Py_DECREF(pCreate);
				Py_DECREF(pModule);
				PyErr_Print();
				fprintf(stderr,"Call failed\n");
				return 1;
			}
		}
		else {
			if (PyErr_Occurred())
					PyErr_Print();
			fprintf(stderr, "Cannot find function \"%s\"\n", argv[2]);
		}
		Py_XDECREF(pCreate);
		Py_XDECREF(pPush);
		Py_DECREF(pModule);
	}
	else {
		PyErr_Print();
		fprintf(stderr, "Failed to load \"%s\"\n", argv[1]);
		return 1;
	}
	if (Py_FinalizeEx() < 0) {
		return 120;
	}
	return 0;
}
