# CMAKE generated file: DO NOT EDIT!
# Generated by "Unix Makefiles" Generator, CMake Version 3.22

# Delete rule output on recipe failure.
.DELETE_ON_ERROR:

#=============================================================================
# Special targets provided by cmake.

# Disable implicit rules so canonical targets will work.
.SUFFIXES:

# Disable VCS-based implicit rules.
% : %,v

# Disable VCS-based implicit rules.
% : RCS/%

# Disable VCS-based implicit rules.
% : RCS/%,v

# Disable VCS-based implicit rules.
% : SCCS/s.%

# Disable VCS-based implicit rules.
% : s.%

.SUFFIXES: .hpux_make_needs_suffix_list

# Command-line flag to silence nested $(MAKE).
$(VERBOSE)MAKESILENT = -s

#Suppress display of executed commands.
$(VERBOSE).SILENT:

# A target that is always out of date.
cmake_force:
.PHONY : cmake_force

#=============================================================================
# Set environment variables for the build.

# The shell in which to execute make rules.
SHELL = /bin/sh

# The CMake executable.
CMAKE_COMMAND = /usr/bin/cmake

# The command to remove a file.
RM = /usr/bin/cmake -E rm -f

# Escaping for special characters.
EQUALS = =

# The top-level source directory on which CMake was run.
CMAKE_SOURCE_DIR = /home/admin5g/oai-core_ran_ric_ue/openairinterface5g/openair2/E2AP

# The top-level build directory on which CMake was run.
CMAKE_BINARY_DIR = /home/admin5g/oai-core_ran_ric_ue/openairinterface5g/openair2/E2AP/flexric

# Include any dependencies generated for this target.
include flexric/src/lib/e2ap/v2_03/CMakeFiles/e2ap_ap_obj.dir/depend.make
# Include any dependencies generated by the compiler for this target.
include flexric/src/lib/e2ap/v2_03/CMakeFiles/e2ap_ap_obj.dir/compiler_depend.make

# Include the progress variables for this target.
include flexric/src/lib/e2ap/v2_03/CMakeFiles/e2ap_ap_obj.dir/progress.make

# Include the compile flags for this target's objects.
include flexric/src/lib/e2ap/v2_03/CMakeFiles/e2ap_ap_obj.dir/flags.make

flexric/src/lib/e2ap/v2_03/CMakeFiles/e2ap_ap_obj.dir/e2ap_ap_asn.o: flexric/src/lib/e2ap/v2_03/CMakeFiles/e2ap_ap_obj.dir/flags.make
flexric/src/lib/e2ap/v2_03/CMakeFiles/e2ap_ap_obj.dir/e2ap_ap_asn.o: src/lib/e2ap/v2_03/e2ap_ap_asn.c
flexric/src/lib/e2ap/v2_03/CMakeFiles/e2ap_ap_obj.dir/e2ap_ap_asn.o: flexric/src/lib/e2ap/v2_03/CMakeFiles/e2ap_ap_obj.dir/compiler_depend.ts
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/home/admin5g/oai-core_ran_ric_ue/openairinterface5g/openair2/E2AP/flexric/CMakeFiles --progress-num=$(CMAKE_PROGRESS_1) "Building C object flexric/src/lib/e2ap/v2_03/CMakeFiles/e2ap_ap_obj.dir/e2ap_ap_asn.o"
	cd /home/admin5g/oai-core_ran_ric_ue/openairinterface5g/openair2/E2AP/flexric/flexric/src/lib/e2ap/v2_03 && /usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -MD -MT flexric/src/lib/e2ap/v2_03/CMakeFiles/e2ap_ap_obj.dir/e2ap_ap_asn.o -MF CMakeFiles/e2ap_ap_obj.dir/e2ap_ap_asn.o.d -o CMakeFiles/e2ap_ap_obj.dir/e2ap_ap_asn.o -c /home/admin5g/oai-core_ran_ric_ue/openairinterface5g/openair2/E2AP/flexric/src/lib/e2ap/v2_03/e2ap_ap_asn.c

flexric/src/lib/e2ap/v2_03/CMakeFiles/e2ap_ap_obj.dir/e2ap_ap_asn.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing C source to CMakeFiles/e2ap_ap_obj.dir/e2ap_ap_asn.i"
	cd /home/admin5g/oai-core_ran_ric_ue/openairinterface5g/openair2/E2AP/flexric/flexric/src/lib/e2ap/v2_03 && /usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -E /home/admin5g/oai-core_ran_ric_ue/openairinterface5g/openair2/E2AP/flexric/src/lib/e2ap/v2_03/e2ap_ap_asn.c > CMakeFiles/e2ap_ap_obj.dir/e2ap_ap_asn.i

flexric/src/lib/e2ap/v2_03/CMakeFiles/e2ap_ap_obj.dir/e2ap_ap_asn.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling C source to assembly CMakeFiles/e2ap_ap_obj.dir/e2ap_ap_asn.s"
	cd /home/admin5g/oai-core_ran_ric_ue/openairinterface5g/openair2/E2AP/flexric/flexric/src/lib/e2ap/v2_03 && /usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -S /home/admin5g/oai-core_ran_ric_ue/openairinterface5g/openair2/E2AP/flexric/src/lib/e2ap/v2_03/e2ap_ap_asn.c -o CMakeFiles/e2ap_ap_obj.dir/e2ap_ap_asn.s

e2ap_ap_obj: flexric/src/lib/e2ap/v2_03/CMakeFiles/e2ap_ap_obj.dir/e2ap_ap_asn.o
e2ap_ap_obj: flexric/src/lib/e2ap/v2_03/e2ap_types/CMakeFiles/e2ap_types_obj.dir/ric_subscription_request.o
e2ap_ap_obj: flexric/src/lib/e2ap/v2_03/e2ap_types/CMakeFiles/e2ap_types_obj.dir/ric_subscription_response.o
e2ap_ap_obj: flexric/src/lib/e2ap/v2_03/e2ap_types/CMakeFiles/e2ap_types_obj.dir/ric_subscription_failure.o
e2ap_ap_obj: flexric/src/lib/e2ap/v2_03/e2ap_types/CMakeFiles/e2ap_types_obj.dir/ric_indication.o
e2ap_ap_obj: flexric/src/lib/e2ap/v2_03/e2ap_types/CMakeFiles/e2ap_types_obj.dir/ric_subscription_delete_request.o
e2ap_ap_obj: flexric/src/lib/e2ap/v2_03/e2ap_types/CMakeFiles/e2ap_types_obj.dir/ric_subscription_delete_response.o
e2ap_ap_obj: flexric/src/lib/e2ap/v2_03/e2ap_types/CMakeFiles/e2ap_types_obj.dir/ric_subscription_delete_failure.o
e2ap_ap_obj: flexric/src/lib/e2ap/v2_03/e2ap_types/CMakeFiles/e2ap_types_obj.dir/ric_control_request.o
e2ap_ap_obj: flexric/src/lib/e2ap/v2_03/e2ap_types/CMakeFiles/e2ap_types_obj.dir/ric_control_ack.o
e2ap_ap_obj: flexric/src/lib/e2ap/v2_03/e2ap_types/CMakeFiles/e2ap_types_obj.dir/ric_control_failure.o
e2ap_ap_obj: flexric/src/lib/e2ap/v2_03/e2ap_types/CMakeFiles/e2ap_types_obj.dir/e2ap_error_indication.o
e2ap_ap_obj: flexric/src/lib/e2ap/v2_03/e2ap_types/CMakeFiles/e2ap_types_obj.dir/e2_setup_request.o
e2ap_ap_obj: flexric/src/lib/e2ap/v2_03/e2ap_types/CMakeFiles/e2ap_types_obj.dir/e2_setup_response.o
e2ap_ap_obj: flexric/src/lib/e2ap/v2_03/e2ap_types/CMakeFiles/e2ap_types_obj.dir/e2_setup_failure.o
e2ap_ap_obj: flexric/src/lib/e2ap/v2_03/e2ap_types/CMakeFiles/e2ap_types_obj.dir/e42_setup_request.o
e2ap_ap_obj: flexric/src/lib/e2ap/v2_03/e2ap_types/CMakeFiles/e2ap_types_obj.dir/e42_setup_response.o
e2ap_ap_obj: flexric/src/lib/e2ap/v2_03/e2ap_types/CMakeFiles/e2ap_types_obj.dir/e42_ric_control_request.o
e2ap_ap_obj: flexric/src/lib/e2ap/v2_03/e2ap_types/CMakeFiles/e2ap_types_obj.dir/e2_node_connected.o
e2ap_ap_obj: flexric/src/lib/e2ap/v2_03/e2ap_types/CMakeFiles/e2ap_types_obj.dir/e42_ric_subscription_request.o
e2ap_ap_obj: flexric/src/lib/e2ap/v2_03/e2ap_types/CMakeFiles/e2ap_types_obj.dir/e42_ric_subscription_delete_request.o
e2ap_ap_obj: flexric/src/lib/e2ap/v2_03/e2ap_types/CMakeFiles/e2ap_types_obj.dir/e2ap_reset_request.o
e2ap_ap_obj: flexric/src/lib/e2ap/v2_03/e2ap_types/CMakeFiles/e2ap_types_obj.dir/e2ap_reset_response.o
e2ap_ap_obj: flexric/src/lib/e2ap/v2_03/e2ap_types/CMakeFiles/e2ap_types_obj.dir/ric_service_update.o
e2ap_ap_obj: flexric/src/lib/e2ap/v2_03/e2ap_types/CMakeFiles/e2ap_types_obj.dir/ric_service_update_ack.o
e2ap_ap_obj: flexric/src/lib/e2ap/v2_03/e2ap_types/CMakeFiles/e2ap_types_obj.dir/ric_service_update_failure.o
e2ap_ap_obj: flexric/src/lib/e2ap/v2_03/e2ap_types/CMakeFiles/e2ap_types_obj.dir/ric_service_query.o
e2ap_ap_obj: flexric/src/lib/e2ap/v2_03/e2ap_types/CMakeFiles/e2ap_types_obj.dir/e2_node_configuration_update.o
e2ap_ap_obj: flexric/src/lib/e2ap/v2_03/e2ap_types/CMakeFiles/e2ap_types_obj.dir/common/e2ap_ran_function_id_rev.o
e2ap_ap_obj: flexric/src/lib/e2ap/v2_03/e2ap_types/CMakeFiles/e2ap_types_obj.dir/common/e2ap_ran_function_id.o
e2ap_ap_obj: flexric/src/lib/e2ap/v2_03/e2ap_types/CMakeFiles/e2ap_types_obj.dir/common/ric_action.o
e2ap_ap_obj: flexric/src/lib/e2ap/v2_03/e2ap_types/CMakeFiles/e2ap_types_obj.dir/common/ric_gen_id.o
e2ap_ap_obj: flexric/src/lib/e2ap/v2_03/e2ap_types/CMakeFiles/e2ap_types_obj.dir/common/ric_action_admitted.o
e2ap_ap_obj: flexric/src/lib/e2ap/v2_03/e2ap_types/CMakeFiles/e2ap_types_obj.dir/common/ric_action_not_admitted.o
e2ap_ap_obj: flexric/src/lib/e2ap/v2_03/e2ap_types/CMakeFiles/e2ap_types_obj.dir/common/e2ap_criticality_diagnostics.o
e2ap_ap_obj: flexric/src/lib/e2ap/v2_03/e2ap_types/CMakeFiles/e2ap_types_obj.dir/common/e2ap_cause.o
e2ap_ap_obj: flexric/src/lib/e2ap/v2_03/e2ap_types/CMakeFiles/e2ap_types_obj.dir/common/e2ap_global_node_id.o
e2ap_ap_obj: flexric/src/lib/e2ap/v2_03/e2ap_types/CMakeFiles/e2ap_types_obj.dir/common/e2ap_global_ric_id.o
e2ap_ap_obj: flexric/src/lib/e2ap/v2_03/e2ap_types/CMakeFiles/e2ap_types_obj.dir/common/e2ap_ran_function.o
e2ap_ap_obj: flexric/src/lib/e2ap/v2_03/e2ap_types/CMakeFiles/e2ap_types_obj.dir/common/e2ap_time_to_wait.o
e2ap_ap_obj: flexric/src/lib/e2ap/v2_03/e2ap_types/CMakeFiles/e2ap_types_obj.dir/common/e2ap_rejected_ran_function.o
e2ap_ap_obj: flexric/src/lib/e2ap/v2_03/e2ap_types/CMakeFiles/e2ap_types_obj.dir/common/e2ap_node_component_config_update.o
e2ap_ap_obj: flexric/src/lib/e2ap/v2_03/e2ap_types/CMakeFiles/e2ap_types_obj.dir/common/e2ap_node_comp_config_add_ack.o
e2ap_ap_obj: flexric/src/lib/e2ap/v2_03/e2ap_types/CMakeFiles/e2ap_types_obj.dir/common/e2ap_node_comp_conf_add_ack.o
e2ap_ap_obj: flexric/src/lib/e2ap/v2_03/e2ap_types/CMakeFiles/e2ap_types_obj.dir/common/e2ap_node_comp_id.o
e2ap_ap_obj: flexric/src/lib/e2ap/v2_03/e2ap_types/CMakeFiles/e2ap_types_obj.dir/common/transport_layer_info.o
e2ap_ap_obj: flexric/src/lib/e2ap/v2_03/e2ap_types/CMakeFiles/e2ap_types_obj.dir/common/e2ap_plmn.o
e2ap_ap_obj: flexric/src/lib/e2ap/v2_03/e2ap_types/CMakeFiles/e2ap_types_obj.dir/common/ric_subsequent_action.o
e2ap_ap_obj: flexric/src/lib/e2ap/v2_03/e2ap_types/CMakeFiles/e2ap_types_obj.dir/common/e2ap_node_component_config_add.o
e2ap_ap_obj: flexric/src/lib/e2ap/v2_03/e2ap_types/CMakeFiles/e2ap_types_obj.dir/common/e2ap_node_comp_conf.o
e2ap_ap_obj: flexric/src/lib/e2ap/v2_03/enc/CMakeFiles/e2ap_msg_enc_obj.dir/e2ap_msg_enc_asn.o
e2ap_ap_obj: flexric/src/lib/e2ap/v2_03/dec/CMakeFiles/e2ap_msg_dec_obj.dir/e2ap_msg_dec_asn.o
e2ap_ap_obj: flexric/src/lib/e2ap/v2_03/free/CMakeFiles/e2ap_msg_free_obj.dir/e2ap_msg_free.o
e2ap_ap_obj: flexric/src/util/CMakeFiles/e2_conv_obj.dir/conversions.o
e2ap_ap_obj: flexric/src/lib/3gpp/ie/CMakeFiles/3gpp_derived_ie_obj.dir/e2sm_gummei.o
e2ap_ap_obj: flexric/src/lib/3gpp/ie/CMakeFiles/3gpp_derived_ie_obj.dir/e2ap_gnb_id.o
e2ap_ap_obj: flexric/src/lib/3gpp/ie/CMakeFiles/3gpp_derived_ie_obj.dir/enb.o
e2ap_ap_obj: flexric/src/lib/3gpp/ie/CMakeFiles/3gpp_derived_ie_obj.dir/en_gnb.o
e2ap_ap_obj: flexric/src/lib/3gpp/ie/CMakeFiles/3gpp_derived_ie_obj.dir/eutra_cgi.o
e2ap_ap_obj: flexric/src/lib/3gpp/ie/CMakeFiles/3gpp_derived_ie_obj.dir/global_enb_id.o
e2ap_ap_obj: flexric/src/lib/3gpp/ie/CMakeFiles/3gpp_derived_ie_obj.dir/global_gnb_id.o
e2ap_ap_obj: flexric/src/lib/3gpp/ie/CMakeFiles/3gpp_derived_ie_obj.dir/global_ng_enb_id.o
e2ap_ap_obj: flexric/src/lib/3gpp/ie/CMakeFiles/3gpp_derived_ie_obj.dir/global_ng_ran_node_id.o
e2ap_ap_obj: flexric/src/lib/3gpp/ie/CMakeFiles/3gpp_derived_ie_obj.dir/gnb.o
e2ap_ap_obj: flexric/src/lib/3gpp/ie/CMakeFiles/3gpp_derived_ie_obj.dir/gnb_cu_up.o
e2ap_ap_obj: flexric/src/lib/3gpp/ie/CMakeFiles/3gpp_derived_ie_obj.dir/gnb_du.o
e2ap_ap_obj: flexric/src/lib/3gpp/ie/CMakeFiles/3gpp_derived_ie_obj.dir/guami.o
e2ap_ap_obj: flexric/src/lib/3gpp/ie/CMakeFiles/3gpp_derived_ie_obj.dir/ng_enb.o
e2ap_ap_obj: flexric/src/lib/3gpp/ie/CMakeFiles/3gpp_derived_ie_obj.dir/ng_enb_du.o
e2ap_ap_obj: flexric/src/lib/3gpp/ie/CMakeFiles/3gpp_derived_ie_obj.dir/nr_cgi.o
e2ap_ap_obj: flexric/src/lib/3gpp/ie/CMakeFiles/3gpp_derived_ie_obj.dir/plmn_identity.o
e2ap_ap_obj: flexric/src/lib/3gpp/ie/CMakeFiles/3gpp_derived_ie_obj.dir/rrc_msg_id.o
e2ap_ap_obj: flexric/src/lib/3gpp/ie/CMakeFiles/3gpp_derived_ie_obj.dir/s_nssai.o
e2ap_ap_obj: flexric/src/lib/e2ap/v2_03/CMakeFiles/e2ap_ap_obj.dir/build.make
.PHONY : e2ap_ap_obj

# Rule to build all files generated by this target.
flexric/src/lib/e2ap/v2_03/CMakeFiles/e2ap_ap_obj.dir/build: e2ap_ap_obj
.PHONY : flexric/src/lib/e2ap/v2_03/CMakeFiles/e2ap_ap_obj.dir/build

flexric/src/lib/e2ap/v2_03/CMakeFiles/e2ap_ap_obj.dir/clean:
	cd /home/admin5g/oai-core_ran_ric_ue/openairinterface5g/openair2/E2AP/flexric/flexric/src/lib/e2ap/v2_03 && $(CMAKE_COMMAND) -P CMakeFiles/e2ap_ap_obj.dir/cmake_clean.cmake
.PHONY : flexric/src/lib/e2ap/v2_03/CMakeFiles/e2ap_ap_obj.dir/clean

flexric/src/lib/e2ap/v2_03/CMakeFiles/e2ap_ap_obj.dir/depend:
	cd /home/admin5g/oai-core_ran_ric_ue/openairinterface5g/openair2/E2AP/flexric && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" /home/admin5g/oai-core_ran_ric_ue/openairinterface5g/openair2/E2AP /home/admin5g/oai-core_ran_ric_ue/openairinterface5g/openair2/E2AP/flexric/src/lib/e2ap/v2_03 /home/admin5g/oai-core_ran_ric_ue/openairinterface5g/openair2/E2AP/flexric /home/admin5g/oai-core_ran_ric_ue/openairinterface5g/openair2/E2AP/flexric/flexric/src/lib/e2ap/v2_03 /home/admin5g/oai-core_ran_ric_ue/openairinterface5g/openair2/E2AP/flexric/flexric/src/lib/e2ap/v2_03/CMakeFiles/e2ap_ap_obj.dir/DependInfo.cmake --color=$(COLOR)
.PHONY : flexric/src/lib/e2ap/v2_03/CMakeFiles/e2ap_ap_obj.dir/depend

