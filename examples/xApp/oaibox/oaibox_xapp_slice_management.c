#include "../../../src/xApp/e42_xapp_api.h"
#include "../../../src/util/alg_ds/alg/defer.h"
#include "../../../src/util/time_now_us.h"

#include <unistd.h>
#include <stdio.h>

void print_usage(const char *prog_name) {
    printf("Usage: %s --active <true|false> --sliceId <int> --dlBitrate <uint64> --ulBitrate <uint64>\n", prog_name);
}

bool parse_bool(const char *str) {
    if (strcmp(str, "true") == 0) return true;
    if (strcmp(str, "false") == 0) return false;
    printf("Invalid boolean value: %s\n", str);
    exit(EXIT_FAILURE);
}

void parse_args(int argc, char *argv[], mac_ctrl_req_data_t *args) {
    if (argc != 9) {
        print_usage(argv[0]);
        exit(EXIT_FAILURE);
    }

    for (int i = 1; i < argc; i += 2) {
        if (strcmp(argv[i], "--active") == 0) {
            args->msg.active = parse_bool(argv[i + 1]);
        } else if (strcmp(argv[i], "--sliceId") == 0) {
            args->msg.sliceId = atoi(argv[i + 1]);
        } else if (strcmp(argv[i], "--dlBitrate") == 0) {
            args->msg.dl_agg_bitrate = strtoull(argv[i + 1], NULL, 10);
        } else if (strcmp(argv[i], "--ulBitrate") == 0) {
            args->msg.ul_agg_bitrate = strtoull(argv[i + 1], NULL, 10);
        } else {
            print_usage(argv[0]);
            exit(EXIT_FAILURE);
        }
    }
}

int main(int argc, char* argv[]) {
    fr_args_t args = init_fr_args(0, NULL);

    mac_ctrl_req_data_t slice_conf_args;
    parse_args(argc, argv, &slice_conf_args);

    // Init the xApp
    init_xapp_api(&args);
    sleep(1);

    e2_node_arr_xapp_t nodes = e2_nodes_xapp_api();
    defer({free_e2_node_arr_xapp(&nodes);});

    assert(nodes.len > 0);

    if (nodes.len > 0) {

        for (int i = 0; i < nodes.len; i++) {
            e2_node_connected_xapp_t *n = &nodes.n[i];

            if (n->id.type == ngran_gNB || n->id.type == ngran_gNB_DU) {
                mac_ctrl_req_data_t wr = {.hdr.dummy = 1, .msg.action = 42, .msg.active = slice_conf_args.msg.active, .msg.sliceId = slice_conf_args.msg.sliceId, .msg.dl_agg_bitrate = slice_conf_args.msg.dl_agg_bitrate, .msg.ul_agg_bitrate = slice_conf_args.msg.ul_agg_bitrate };
                sm_ans_xapp_t const a = control_sm_xapp_api(&nodes.n[i].id, 142, &wr);
            }
        }
    }

    while (try_stop_xapp_api() == false)
        usleep(500);
}