  # Create interface ports
  set Vp_Vn_0 [ create_bd_intf_port -mode Slave -vlnv xilinx.com:interface:diff_analog_io_rtl:1.0 Vp_Vn_0 ]

  # Create instance: xadc_wiz_0, and set properties
  set xadc_wiz_0 [ create_bd_cell -type ip -vlnv xilinx.com:ip:xadc_wiz:3.3 xadc_wiz_0 ]
  set_property -dict [list \
    CONFIG.ACQUISITION_TIME {10} \
    CONFIG.ADC_CONVERSION_RATE {39} \
    CONFIG.ADC_OFFSET_AND_GAIN_CALIBRATION {false} \
    CONFIG.CHANNEL_AVERAGING {256} \
    CONFIG.CHANNEL_ENABLE_VP_VN {false} \
    CONFIG.ENABLE_CALIBRATION_AVERAGING {false} \
    CONFIG.ENABLE_EXTERNAL_MUX {false} \
    CONFIG.ENABLE_RESET {false} \
    CONFIG.ENABLE_VCCDDRO_ALARM {false} \
    CONFIG.ENABLE_VCCPAUX_ALARM {false} \
    CONFIG.ENABLE_VCCPINT_ALARM {false} \
    CONFIG.EXTERNAL_MUXADDR_ENABLE {false} \
    CONFIG.EXTERNAL_MUX_CHANNEL {VAUXP0_VAUXN0} \
    CONFIG.INTERFACE_SELECTION {Enable_AXI} \
    CONFIG.OT_ALARM {false} \
    CONFIG.POWER_DOWN_ADCB {false} \
    CONFIG.SENSOR_OFFSET_AND_GAIN_CALIBRATION {false} \
    CONFIG.SEQUENCER_MODE {Off} \
    CONFIG.SINGLE_CHANNEL_ACQUISITION_TIME {true} \
    CONFIG.SINGLE_CHANNEL_SELECTION {VP_VN} \
    CONFIG.TIMING_MODE {Continuous} \
    CONFIG.USER_TEMP_ALARM {false} \
    CONFIG.VCCAUX_ALARM {false} \
    CONFIG.VCCINT_ALARM {false} \
    CONFIG.XADC_STARUP_SELECTION {single_channel} \
  ] $xadc_wiz_0


  # Create interface connections
  connect_bd_intf_net -intf_net Vp_Vn_0_1 [get_bd_intf_ports Vp_Vn_0] [get_bd_intf_pins xadc_wiz_0/Vp_Vn]

