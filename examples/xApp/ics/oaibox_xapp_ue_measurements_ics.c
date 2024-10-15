#include "../../../src/xApp/e42_xapp_api.h"
#include "../../../src/util/alg_ds/alg/defer.h"
#include "../../../src/util/time_now_us.h"
#include "../../../external/common/utils/hashtable/hashtable.h"





#include <arpa/inet.h>
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <time.h>
#include <unistd.h>
#include <math.h>
#include <netdb.h>
#include <sys/socket.h>
#include <unistd.h>


#define TICK_INTERVAL 10
#define UPDATE_INTERVAL 1000
#define MAX_MOBILES_PER_GNB 40

int16_t ticker = 0;
uint64_t    current_timestamp;

static bool keepRunning = true;
void intHandler() {
    keepRunning = false;
}

static pthread_mutex_t mac_stats_mutex;
hash_table_t *ue_mac_stats_by_rnti_ht;

static void print_mac_ue_stats(mac_ue_stats_impl_t *ue_mac_stats) {
    // Add the fields you want to print from the ue_mac_stats structure
    printf("RNTI: %x, WB CQI: %d, PUSCH SNR: %f\n", ue_mac_stats->rnti, ue_mac_stats->wb_cqi, ue_mac_stats->pusch_snr);
}

static void sm_cb_mac(sm_ag_if_rd_t const* rd)
{
    assert(rd != NULL);
    assert(rd->type == INDICATION_MSG_AGENT_IF_ANS_V0);
    assert(rd->ind.type == MAC_STATS_V0);

    for (u_int32_t i = 0; i < rd->ind.mac.msg.len_ue_stats; i++) {
        pthread_mutex_lock(&mac_stats_mutex);
        mac_ue_stats_impl_t *ue_mac_stats = malloc(sizeof(*ue_mac_stats));
        *ue_mac_stats = rd->ind.mac.msg.ue_stats[i];
        hashtable_insert(ue_mac_stats_by_rnti_ht, rd->ind.mac.msg.ue_stats[i].rnti, ue_mac_stats);
        print_mac_ue_stats(ue_mac_stats); // Print the newly inserted node
        pthread_mutex_unlock(&mac_stats_mutex);
    }
    
}


int main(int argc, char* argv[])
{
    fr_args_t args = init_fr_args(argc, argv);

    struct sigaction act;
    act.sa_handler = intHandler;
    sigaction(SIGINT, &act, NULL);

    pthread_mutexattr_t attr = {0};
    pthread_mutex_init(&mac_stats_mutex, &attr);
    ue_mac_stats_by_rnti_ht = hashtable_create(MAX_MOBILES_PER_GNB, NULL, free);

    // Init the xApp
    init_xapp_api(&args);
    sleep(1);

    e2_node_arr_xapp_t nodes = e2_nodes_xapp_api();
    defer({free_e2_node_arr_xapp(&nodes); });

    assert(nodes.len > 0);

    printf("Connected E2 nodes = %d\n", nodes.len);

    // MAC indication
    const char* i_0 = "10_ms";
    sm_ans_xapp_t* mac_handle = NULL;
    sm_ans_xapp_t* gtp_handle = NULL;

    if (nodes.len > 0) {
        mac_handle = calloc( nodes.len, sizeof(sm_ans_xapp_t) );
        assert(mac_handle  != NULL);
        gtp_handle = calloc( nodes.len, sizeof(sm_ans_xapp_t) );
        assert(gtp_handle  != NULL);
    }

    for (int i = 0; i < nodes.len; i++) {
        e2_node_connected_xapp_t* n = &nodes.n[i];

        if (n->id.type == ngran_gNB) {
            mac_handle[i] = report_sm_xapp_api(&nodes.n[i].id, 142, (void*)i_0, sm_cb_mac);
            assert(mac_handle[i].success == true);
        } 
    }
    while (keepRunning) {
        usleep(1000);  // Sleep for 500ms
    }
    for (int i = 0; i < nodes.len; ++i) {
        if(mac_handle[i].u.handle != 0 )
            rm_report_sm_xapp_api(mac_handle[i].u.handle);
        if(gtp_handle[i].u.handle != 0)
            rm_report_sm_xapp_api(gtp_handle[i].u.handle);
    }

    if (nodes.len > 0) {
        free(mac_handle);
        free(gtp_handle);
    }

    hashtable_destroy(&ue_mac_stats_by_rnti_ht);

    while (try_stop_xapp_api() == false)
        usleep(500);

    pthread_mutex_destroy(&mac_stats_mutex);
    printf("OAIBOX UE Measurements xAPP Completed\n");
}
