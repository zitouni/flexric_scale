# Find SQLite3 package
find_package(PkgConfig REQUIRED)
pkg_check_modules(SQLITE3 REQUIRED sqlite3)

add_executable(ics_xapp_kpm_moni
      ics_xapp_kpm_moni.c
)

add_executable(ics_xapp
        ics_xapp.c
)

add_executable(oaibox_xapp_ue_measurements_ics
        oaibox_xapp_ue_measurements_ics.c
        ../../../../src/util/alg_ds/alg/defer.c
	../../../external/common/utils/hashtable/hashtable.c
)
# hiper_ran_xapp
add_executable(hiper_ran_xapp 
                hiper_ran_xapp.c
                hiper_ran_tcp_client.c
                ../../../../src/util/alg_ds/alg/defer.c
                ../../../../external/common/utils/hashtable/hashtable.c
              )

add_executable(ics_xapp_gtp_mac_rlc_pdcp_moni 
                ics_xapp_gtp_mac_rlc_pdcp_moni.c
                ../../../../src/util/alg_ds/alg/defer.c
              )

add_executable(ics_xapp_gtp_mac_rlc_pdcp_moni_extend 
              ics_xapp_gtp_mac_rlc_pdcp_moni_extend.c
              ../../../../src/util/alg_ds/alg/defer.c
              ../../../../external/common/utils/hashtable/hashtable.c
            )

# Specify the include directories
include_directories(ics_xapp
    ../../../../../../common/utils
    ../../../../../../common
)


target_link_libraries(ics_xapp
        PUBLIC
        e42_xapp
        -pthread
        -lsctp
        -ldl
)


target_link_libraries(oaibox_xapp_ue_measurements_ics
        PUBLIC
        e42_xapp
        -pthread
        -lsctp
        -ldl
)
target_link_libraries(ics_xapp_kpm_moni
        PUBLIC
        e42_xapp
        -pthread
        -lsctp
        -ldl
)

# # Link SQLite3 to latency_xapp
# target_include_directories(latency_xapp PRIVATE ${SQLITE3_INCLUDE_DIRS})
# target_link_libraries(latency_xapp 
#     PUBLIC
#     e42_xapp
#     -pthread
#     -lsctp
#     -ldl
#     ${SQLITE3_LIBRARIES}
# )

target_link_libraries(ics_xapp_gtp_mac_rlc_pdcp_moni_extend
        PUBLIC
        e42_xapp
        -pthread
        -lsctp
        -ldl
)

target_link_libraries(ics_xapp_gtp_mac_rlc_pdcp_moni
        PUBLIC
        e42_xapp
        -pthread
        -lsctp
        -ldl
)

target_link_libraries(hiper_ran_xapp
        PUBLIC
        e42_xapp
        -pthread
        -lsctp
        -ldl
)