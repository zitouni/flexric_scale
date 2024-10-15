add_executable(oaibox_xapp_ue_measurements
        oaibox_xapp_ue_measurements.c
        ../../../../src/util/alg_ds/alg/defer.c
        ../../../../external/common/utils/hashtable/hashtable.c
)

add_executable(oaibox_xapp_slice_management
        oaibox_xapp_slice_management.c
        ../../../../src/util/alg_ds/alg/defer.c
)

target_link_libraries(oaibox_xapp_ue_measurements
        PUBLIC
        e42_xapp
        -pthread
        -lsctp
        -ldl
)

target_link_libraries(oaibox_xapp_slice_management
        PUBLIC
        e42_xapp
        -pthread
        -lsctp
        -ldl
)
