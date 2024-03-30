#include "nrscope/hdr/nrscope_logger.h"
#include "nrscope/hdr/nrscope_def.h"
#include "nrscope/hdr/to_google.h"

int main(){
  std::vector<std::string> filename = {"./a.csv", "./b.csv"};
  NRScopeLog::init_logger(filename);
  printf("Finished log creating...\n");
  LogNode a;
  // a.timestamp = 0.0;
  while(true){
    NRScopeLog::push_node(a, 0);
    printf("Waiting for the thread...\n");
  }
    
  return 0;
}