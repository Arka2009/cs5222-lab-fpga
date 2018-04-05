# Directories
BUILD_DIR = build
SCRIPT_DIR = tcl

# Executables
VIVADO_HLS = vivado_hls
VIVADO = vivado

.PHONY: all setup ip bit clean

all: setup ip bit

setup:
	if [ -d $(BUILD_DIR) ]; then echo "/build directory already exists. Please enter: make clean"; exit; fi
	mkdir $(BUILD_DIR)

ip:
	cd $(BUILD_DIR) && $(VIVADO_HLS) -f ../$(SCRIPT_DIR)/hls.tcl
	mv $(BUILD_DIR)/accel/solution0/syn/report/mmult_hw_csynth.rpt $(PWD)/csynth.rpt

bit:
	cd $(BUILD_DIR) && $(VIVADO) -mode tcl -source ../$(SCRIPT_DIR)/classifier.tcl -nojournal -nolog
	#echo "Dont waste your time generating the bitstream, for all test cases !!!"

clean:
	rm -rf $(BUILD_DIR) csynth.rpt
