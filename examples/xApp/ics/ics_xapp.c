#include "../../../src/xApp/e42_xapp_api.h"
#include "../../../src/util/alg_ds/alg/defer.h"
#include "../../../src/util/time_now_us.h"

#include <stdio.h>
#include <unistd.h>

void func(sm_ag_if_rd_t const* src)
{
  printf("[ICS xApp]: Length of the UE Stats: %d\n", src->ind.mac.msg.len_ue_stats);
  printf("[ICS xApp]: MAC Timestamp: %ld, UE RNTI: %X, UE WB CQI: %d, PSCH SNR: %.1f, \n",
         src->ind.mac.msg.tstamp,
         src->ind.mac.msg.ue_stats->rnti,
         src->ind.mac.msg.ue_stats->wb_cqi,
         src->ind.mac.msg.ue_stats->pusch_snr);
}

int main(int argc, char** argv)
{
  fr_args_t a = init_fr_args(argc, argv);
  short func_id;
  const char* i_0 = "100_ms";

  init_xapp_api(&a);
  usleep(10000000);

  e2_node_arr_xapp_t arr = e2_nodes_xapp_api();

  if (arr.len > 0) {
    // printf("[ICS xAP] RAN FUNCTION: ID %d \n ", arr.n[0].rf[0].id);
  }
  // Set the RAN function equals to 142 MAC Stats
  func_id = 142;
  // inter_xapp_e i = ms_5;

  printf("[ICS xApp]: RAN FUNCTION: ID %d \n ", arr.n[0].rf[0].id);
  // Returns a handle
  sm_ans_xapp_t ans = report_sm_xapp_api(&arr.n[0].id, func_id, (void*)i_0, func);
  sleep(3);
  // Remove the handle previously returned
  rm_report_sm_xapp_api(ans.u.handle);

  while (try_stop_xapp_api() == false) {
    usleep(1000000);
  }

  free_e2_node_arr_xapp(&arr);

  return 0;
}
