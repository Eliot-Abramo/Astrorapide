## This file is a general .xdc for the PYNQ-Z2 board 
## Single Ended Analog Inputs
##NOTE: The ar_an_p pins can be used as single ended analog inputs with voltages from 0-3.3V (Arduino Analog pins a[0]-a[5]). 
##      These signals should only be connected to the XADC core. When using these pins as digital I/O, use pins a[0]-a[5].

set_property -dict { PACKAGE_PIN E17   IOSTANDARD LVCMOS33 } [get_ports { Vp_Vn_0_p }]; #IO_L3P_T0_DQS_AD1P_35 Sch=ar_an0_p
set_property -dict { PACKAGE_PIN D18   IOSTANDARD LVCMOS33 } [get_ports { Vp_Vn_0_n }]; #IO_L3P_T0_DQS_AD1P_35 Sch=ar_an0_n
#set_property -dict { PACKAGE_PIN E18   IOSTANDARD LVCMOS33 } [get_ports { ar_an1_p }]; #IO_L5N_T0_AD9P_35 Sch=ar_an1_p
#set_property -dict { PACKAGE_PIN E19   IOSTANDARD LVCMOS33 } [get_ports { ar_an1_n }]; #IO_L5N_T0_AD9N_35 Sch=ar_an1_n
#set_property -dict { PACKAGE_PIN K14   IOSTANDARD LVCMOS33 } [get_ports { ar_an2_p }]; #IO_L20P_T3_AD6P_35 Sch=ar_an2_p
#set_property -dict { PACKAGE_PIN J14   IOSTANDARD LVCMOS33 } [get_ports { ar_an2_n }]; #IO_L20P_T3_AD6N_35 Sch=ar_an2_n
#set_property -dict { PACKAGE_PIN K16   IOSTANDARD LVCMOS33 } [get_ports { ar_an3_p }]; #IO_L24P_T3_AD15P_35 Sch=ar_an3_p
#set_property -dict { PACKAGE_PIN J16   IOSTANDARD LVCMOS33 } [get_ports { ar_an3_n }]; #IO_L24P_T3_AD15N_35 Sch=ar_an3_n
#set_property -dict { PACKAGE_PIN J20   IOSTANDARD LVCMOS33 } [get_ports { ar_an4_p }]; #IO_L17P_T2_AD5P_35 Sch=ar_an4_p
#set_property -dict { PACKAGE_PIN H20   IOSTANDARD LVCMOS33 } [get_ports { ar_an4_n }]; #IO_L17P_T2_AD5P_35 Sch=ar_an4_n
#set_property -dict { PACKAGE_PIN G19   IOSTANDARD LVCMOS33 } [get_ports { ar_an5_p }]; #IO_L18P_T2_AD13P_35 Sch=ar_an5_p
#set_property -dict { PACKAGE_PIN G20   IOSTANDARD LVCMOS33 } [get_ports { ar_an5_n }]; #IO_L18P_T2_AD13P_35 Sch=ar_an5_n

##Arduino Digital I/O 

#set_property -dict { PACKAGE_PIN T14   IOSTANDARD LVCMOS33 } [get_ports { ar[0] }]; #IO_L5P_T0_34 Sch=ar[0]
#set_property -dict { PACKAGE_PIN U12   IOSTANDARD LVCMOS33 } [get_ports { ar[1] }]; #IO_L2N_T0_34 Sch=ar[1]
#set_property -dict { PACKAGE_PIN U13   IOSTANDARD LVCMOS33 } [get_ports { ar[2] }]; #IO_L3P_T0_DQS_PUDC_B_34 Sch=ar[2]
#set_property -dict { PACKAGE_PIN V13   IOSTANDARD LVCMOS33 } [get_ports { ar[3] }]; #IO_L3N_T0_DQS_34 Sch=ar[3]
#set_property -dict { PACKAGE_PIN V15   IOSTANDARD LVCMOS33 } [get_ports { ar[4] }]; #IO_L10P_T1_34 Sch=ar[4]
#set_property -dict { PACKAGE_PIN T15   IOSTANDARD LVCMOS33 } [get_ports { ar[5] }]; #IO_L5N_T0_34 Sch=ar[5]
#set_property -dict { PACKAGE_PIN R16   IOSTANDARD LVCMOS33 } [get_ports { ar[6] }]; #IO_L19P_T3_34 Sch=ar[6]
#set_property -dict { PACKAGE_PIN U17   IOSTANDARD LVCMOS33 } [get_ports { ar[7] }]; #IO_L9N_T1_DQS_34 Sch=ar[7]
#set_property -dict { PACKAGE_PIN V17   IOSTANDARD LVCMOS33 } [get_ports { ar[8] }]; #IO_L21P_T3_DQS_34 Sch=ar[8]
#set_property -dict { PACKAGE_PIN V18   IOSTANDARD LVCMOS33 } [get_ports { ar[9] }]; #IO_L21N_T3_DQS_34 Sch=ar[9]
#set_property -dict { PACKAGE_PIN T16   IOSTANDARD LVCMOS33 } [get_ports { ar[10] }]; #IO_L9P_T1_DQS_34 Sch=ar[10]
#set_property -dict { PACKAGE_PIN R17   IOSTANDARD LVCMOS33 } [get_ports { ar[11] }]; #IO_L19N_T3_VREF_34 Sch=ar[11]
#set_property -dict { PACKAGE_PIN P18   IOSTANDARD LVCMOS33 } [get_ports { ar[12] }]; #IO_L23N_T3_34 Sch=ar[12]
#set_property -dict { PACKAGE_PIN N17   IOSTANDARD LVCMOS33 } [get_ports { ar[13] }]; #IO_L23P_T3_34 Sch=ar[13]
#set_property -dict { PACKAGE_PIN Y13   IOSTANDARD LVCMOS33 } [get_ports { a }]; #IO_L20N_T3_13 Sch=a

##Arduino Digital I/O On Outer Analog Header
##NOTE: These pins should be used when using the analog header signals A0-A5 as digital I/O 

#set_property -dict { PACKAGE_PIN Y11   IOSTANDARD LVCMOS33 } [get_ports { a[0] }]; #IO_L18N_T2_13 Sch=a[0]
#set_property -dict { PACKAGE_PIN Y12   IOSTANDARD LVCMOS33 } [get_ports { a[1] }]; #IO_L20P_T3_13 Sch=a[1]
#set_property -dict { PACKAGE_PIN W11   IOSTANDARD LVCMOS33 } [get_ports { a[2] }]; #IO_L18P_T2_13 Sch=a[2]
#set_property -dict { PACKAGE_PIN V11   IOSTANDARD LVCMOS33 } [get_ports { a[3] }]; #IO_L21P_T3_DQS_13 Sch=a[3]
#set_property -dict { PACKAGE_PIN T5    IOSTANDARD LVCMOS33 } [get_ports { a[4] }]; #IO_L19P_T3_13 Sch=a[4]
#set_property -dict { PACKAGE_PIN U10   IOSTANDARD LVCMOS33 } [get_ports { a[5] }]; #IO_L12N_T1_MRCC_13 Sch=a[5]

