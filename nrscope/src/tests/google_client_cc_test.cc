#include "nrscope/hdr/nrscope_logger.h"
#include "nrscope/hdr/nrscope_def.h"

int main(){
  std::string filename("./a.csv");
  NRScopeLog::init_logger(filename);
  printf("Finished log creating...\n");
  NRScopeLog::LogNode a;
  // a.timestamp = 0.0;
  while(true){
    NRScopeLog::push_node(a);
    printf("Waiting for the thread...\n");
  }
    
  return 0;
}