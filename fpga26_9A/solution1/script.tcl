############################################################
## This file is generated automatically by Vitis HLS.
## Please DO NOT edit it.
## Copyright 1986-2022 Xilinx, Inc. All Rights Reserved.
## Copyright 2022-2023 Advanced Micro Devices, Inc. All Rights Reserved.
############################################################
open_project fpga26_9A
set_top algo_top
add_files src/algo_top.cpp
open_solution "solution1" -flow_target vivado
set_part {xc7z015clg485-2}
create_clock -period 20 -name default
#source "./fpga26_9A/solution1/directives.tcl"
#csim_design
csynth_design
#cosim_design
export_design -format ip_catalog
