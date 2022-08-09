all: clean _rebootex _tn _popsctrl install

clean:
	-rm "flash0/kd/popsctrl.prx"
	-rm "TN.BIN"
	-rm "FLASH0.TN"
	-rm tn/rebootex.h

_rebootex:
	make -C rebootex clean
	make -C rebootex
	gzip -9 -n rebootex/rebootex.bin
	bin2c rebootex/rebootex.bin.gz tn/rebootex.h rebootex
	rm rebootex/rebootex.bin.gz
	make -C rebootex clean

_tn:
	make -C tn clean
	make -C tn
	cp "tn/tn.bin" "TN.BIN"
	make -C tn clean

_popsctrl:
	make -C popsctrl clean
	make -C popsctrl
	psp-packer popsctrl/popsctrl.prx
	cp "popsctrl/popsctrl.prx" "flash0/kd/popsctrl.prx"
	make -C popsctrl clean

install:
	wine ./package_maker.exe
	-cp "FLASH0.TN" "C:\Users\Andy\Documents\PS Vita\PSAVEDATA\bff38b5612d69f24\SLUS00213\FLASH0.TN"
	-cp "TN.BIN" "C:\Users\Andy\Documents\PS Vita\PSAVEDATA\bff38b5612d69f24\SLUS00213\TN.BIN"
