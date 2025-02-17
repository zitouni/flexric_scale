#ifndef DB_METRICS_H
#define DB_METRICS_H

#include <sqlite3.h>
#include <stdbool.h>
#include <stdint.h>
#include "../../../src/xApp/e2_node_arr_xapp.h"
#include "../../../src/sm/kpm_sm/kpm_sm_v02.03/ie/kpm_data_ie.h"
// Initialize the database connection
bool init_metrics_db(const char* db_path);

// Store metrics in the database
void store_detailed_metrics(const kpm_ind_data_t* ind,
                            int64_t latency, // latency in microseconds
                            int counter,
                            const e2_node_arr_xapp_t* nodes,
                            int64_t measurement_timestamp,
                            char* time_str); // Added parameter for measurement timestamp

// Close the database connection
void close_metrics_db(void);

#endif // DB_METRICS_H
