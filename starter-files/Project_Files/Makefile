.PHONY: ip hls_project hls_sim clean cleanall vivado_project bitstream extract_bitstream help
PROJECT_NAME := RadioAstro

help:
	@echo ""
	@echo "MAKEFILE targets"
	@echo ""
	@echo "VITIS HLS targets"
	@echo ""
	@echo "hls_project: Just creates the Vitis HLS project"
	@echo "hls_sim: Creates the Vitis HLS project and runs the C++ simulation"
	@echo "ip: Creates the Vitis HLS project, synthesizes the design and exports the IP core"
	@echo ""
	@echo "VIVADO targets"
	@echo ""
	@echo "vivado_project: Just creates the Vivado project"
	@echo "bitstream: Creates the Vivado project and runs synthesis up to bitstream generation"
	@echo "extract_bitstream: If the Vivado bitstream has already been generated, extracts it to this folder"
	@echo ""
	@echo "Generic targets"
	@echo ""
	@echo "clean: Deletes log files and Vitis HLS and Vivado projects. Deletes the files in the IP Catalog (keeps the IP catalog ZIP file)"
	@echo "cleanall: Additionally, deletes the bitstream files and the IP catalog ZIP file"
	@echo "help: Shows this help message"
	@echo ""


ip: IP-catalog/$(PROJECT_NAME)_HW_HLS.zip HLS/*.cpp HLS/*.h

IP-catalog/$(PROJECT_NAME)_HW_HLS.zip: HLS/*.cpp HLS/*.h
	rm -rf $(PROJECT_NAME)_HW_HLS
	vitis_hls -f $(PROJECT_NAME)_HLS_impl.tcl

hls_project: $(PROJECT_NAME)_HW_HLS/hls.app

$(PROJECT_NAME)_HW_HLS/hls.app:
	rm -rf $(PROJECT_NAME)_HW_HLS
	vitis_hls -f $(PROJECT_NAME)_HLS.tcl

hls_sim:
	rm -rf $(PROJECT_NAME)_HW_HLS
	vitis_hls -f $(PROJECT_NAME)_HLS_sim.tcl


vivado_project: ip $(PROJECT_NAME)_HW_Vivado/

$(PROJECT_NAME)_HW_Vivado/:
	cd IP-catalog; unzip ./$(PROJECT_NAME)_HW_HLS.zip; cd ..
	vivado -mode batch -source $(PROJECT_NAME)_Vivado.tcl -tclargs --project_name $(PROJECT_NAME)_HW_Vivado

bitstream: vivado_project $(PROJECT_NAME)_HW.bit $(PROJECT_NAME)_HW.hwh

$(PROJECT_NAME)_HW.bit $(PROJECT_NAME)_HW.hwh:
	vivado -mode batch -source $(PROJECT_NAME)_Vivado_impl.tcl -tclargs --project_name $(PROJECT_NAME)_HW_Vivado
	cp ./$(PROJECT_NAME)_HW_Vivado/$(PROJECT_NAME)_HW_Vivado.runs/impl_1/design_1_wrapper.bit $(PROJECT_NAME)_HW.bit
	cp ./$(PROJECT_NAME)_HW_Vivado/$(PROJECT_NAME)_HW_Vivado.gen/sources_1/bd/design_1/hw_handoff/design_1.hwh $(PROJECT_NAME)_HW.hwh
	#cp ./$(PROJECT_NAME)_HW_Vivado/$(PROJECT_NAME)_HW_Vivado.runs/impl_1/design_1_wrapper.ltx $(PROJECT_NAME)_HW.ltx
	@md5sum $(PROJECT_NAME)_HW.bit
	@md5sum $(PROJECT_NAME)_HW.hwh


extract_bitstream:
	cp ./$(PROJECT_NAME)_HW_Vivado/$(PROJECT_NAME)_HW_Vivado.runs/impl_1/design_1_wrapper.bit $(PROJECT_NAME)_HW.bit
	cp ./$(PROJECT_NAME)_HW_Vivado/$(PROJECT_NAME)_HW_Vivado.gen/sources_1/bd/design_1/hw_handoff/design_1.hwh $(PROJECT_NAME)_HW.hwh
	#cp ./$(PROJECT_NAME)_HW_Vivado/$(PROJECT_NAME)_HW_Vivado.runs/impl_1/design_1_wrapper.ltx $(PROJECT_NAME)_HW.ltx
	@md5sum $(PROJECT_NAME)_HW.bit
	@md5sum $(PROJECT_NAME)_HW.hwh


clean:
	rm -rf NA/ .Xil
	rm -f vivado*.jou vivado*.log vivado*.str vitis_hls.log
	rm -f $(PROJECT_NAME)_HW_Vivado_def_val.txt $(PROJECT_NAME)_HW_Vivado_dump.txt
	rm -rf $(PROJECT_NAME)_HW_HLS $(PROJECT_NAME)_HW_Vivado
	rm -f IP-catalog/component.xml
	rm -rf IP-catalog/constraints/ IP-catalog/doc/ IP-catalog/drivers/ IP-catalog/hdl/ IP-catalog/misc/ IP-catalog/xgui/

cleanall: clean
	rm -f IP-catalog/$(PROJECT_NAME)_HW_HLS.zip
	rm -f *.bit *.hwh *.ltx

