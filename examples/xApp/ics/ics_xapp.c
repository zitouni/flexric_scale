#include "../../../src/xApp/e42_xapp_api.h"
#include "../../../src/util/alg_ds/alg/defer.h"
#include "../../../src/util/time_now_us.h"

#include<stdio.h>
#include<unistd.h>


void func (sm_ag_if_rd_t const* src){
    printf("[ICS xApp]: Length of the UE Stats: %d\n", src->ind.mac.msg.len_ue_stats);
    printf("[ICS xApp]: MAC Timestamp: %ld, UE RNTI: %X, UE WB CQI: %d, PSCH SNR: %.1f, \n", src->ind.mac.msg.tstamp,
            src->ind.mac.msg.ue_stats->rnti, src->ind.mac.msg.ue_stats->wb_cqi, src->ind.mac.msg.ue_stats->pusch_snr );
    exit(0);

}

int main(int argc, char** argv){

    fr_args_t a = init_fr_args(argc, argv);
    short func_id;
    const char* i_0 = "5_ms";

    init_xapp_api(&a);
    usleep(10000);

    e2_node_arr_xapp_t arr = e2_nodes_xapp_api();

    if (arr.len > 0){
        // printf("[ICS xAP] RAN FUNCTION: ID %d \n ", arr.n[0].rf[0].id);
    }
    // Set the RAN function equals to 142 MAC Stats
    func_id = 142;
    inter_xapp_e i = ms_5;

    printf("[ICS xApp]: RAN FUNCTION: ID %d \n ", arr.n[0].rf[0].id);
    // Returns a handle
    sm_ans_xapp_t ans = report_sm_xapp_api(&arr.n[0].id,  func_id, (void*)i_0, func);
    sleep(3);
    // Remove the handle previously returned
    rm_report_sm_xapp_api(ans.u.handle);

    while(try_stop_xapp_api()==false){
        usleep(1000);
    }


    free_e2_node_arr_xapp(&arr);

    return 0;
}




// e2_node_arr_xapp_t e2_nodes_xapp_api(void);

// typedef void (*sm_cb)(sm_ag_if_rd_t const*);

// typedef union{
//   char* reason;
//   int handle;
// } sm_ans_xapp_u;

// typedef struct{
//   sm_ans_xapp_u u;
//   bool success;
// } sm_ans_xapp_t;

// typedef enum{
//   ms_1,
//   ms_2,
//   ms_5,
//   ms_10,
//   ms_100,
//   ms_1000,

//   ms_end,
// } inter_xapp_e;

// // Returns a handle
// sm_ans_xapp_t report_sm_xapp_api(global_e2_node_id_t* id, uint32_t rf_id, void* data, sm_cb handler);

// // Remove the handle previously returned
// void rm_report_sm_xapp_api(int const handle);

// // Send control message
// // return void but sm_ag_if_ans_ctrl_t should be returned. Add it in the future if needed
// sm_ans_xapp_t control_sm_xapp_api(global_e2_node_id_t* id, uint32_t rf_id, void* wr);