#ifndef LOAD_CONFIG_H
#define LOAD_CONFIG_H

#include <iostream>
#include <yaml-cpp/yaml.h>

#include "nrscope/hdr/radio_nr.h"

// using namespace std;

int get_nof_usrp(std::string file_name);
int load_config(std::vector<Radio>&, std::string file_name);

#endif