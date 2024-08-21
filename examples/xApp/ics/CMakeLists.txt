add_executable(ics_xapp_kpm_moni
      ics_xapp_kpm_moni.c
)

add_executable(ics_xapp
        ics_xapp.c
)

add_executable(oaibox_xapp_ue_measurements_ics
        oaibox_xapp_ue_measurements_ics.c
        ../../../../src/util/alg_ds/alg/defer.c
        ../../../../../../common/utils/hashtable/hashtable.c
)
add_executable(ics_xapp_gtp_mac_rlc_pdcp_moni 
                ics_xapp_gtp_mac_rlc_pdcp_moni.c
                ../../../../src/util/alg_ds/alg/defer.c
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

target_link_libraries(ics_xapp_gtp_mac_rlc_pdcp_moni
        PUBLIC
        e42_xapp
        -pthread
        -lsctp
        -ldl
)
