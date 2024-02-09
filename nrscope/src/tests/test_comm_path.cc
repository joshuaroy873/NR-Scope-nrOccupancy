// #include "nrscope/hdr/to_sunshine.h"
// #include "nrscope/hdr/to_moonlight.h"

// int main(){
//   to_moonlight::init_feedback_server(NGSCOPE_SERVER_PORT);
//   to_sunshine::init_feedback_client_udp();

//   usleep(MSG_QUEUE_POLL_TIME*100);

//   std::string data{"network_bandwidth:100"};
//   std::string data2{"network_bandwidth:200"};

//   to_sunshine::send_feedback(data, data.length());
//   usleep(MSG_QUEUE_POLL_TIME);
//   to_sunshine::send_feedback(data2, data2.length());
//   usleep(MSG_QUEUE_POLL_TIME);

//   to_moonlight::send_feedback(data, data.length());
//   usleep(MSG_QUEUE_POLL_TIME);
//   to_moonlight::send_feedback(data2, data2.length());
//   usleep(MSG_QUEUE_POLL_TIME);

//   to_sunshine::send_feedback(data, data.length());
//   usleep(MSG_QUEUE_POLL_TIME);
//   to_moonlight::send_feedback(data2, data2.length());
//   usleep(MSG_QUEUE_POLL_TIME);

//   usleep(MSG_QUEUE_POLL_TIME*100);
//   to_sunshine::terminate_feedback_client();
//   to_moonlight::terminate_feedback_server();
//   usleep(MSG_QUEUE_POLL_TIME*100);
// }