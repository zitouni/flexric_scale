# Find SQLite3 package
find_package(PkgConfig REQUIRED)
pkg_check_modules(SQLITE3 REQUIRED sqlite3)

add_executable(ics_xapp_kpm_moni
      ics_xapp_kpm_moni.c
)

add_executable(ics_xapp
        ics_xapp.c
)

# add_executable(latency_xapp
#       latency_xapp.c
#       db_metrics.c
# )


add_executable(oaibox_xapp_ue_measurements_ics
        oaibox_xapp_ue_measurements_ics.c
        ../../../../src/util/alg_ds/alg/defer.c
	../../../external/common/utils/hashtable/hashtable.c
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
# target_include_directories(latency_xapp 
#     PRIVATE
#     ${CMAKE_CURRENT_SOURCE_DIR}/../../src
#     ${CMAKE_CURRENT_SOURCE_DIR}/../../src/xApp
#     ${CMAKE_CURRENT_SOURCE_DIR}/../../src/sm/kpm_sm/kpm_sm_v02.03
#     ${SQLITE3_INCLUDE_DIRS}
# )


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
