TOPDIR := $(realpath ../..)

SOURCES := ahx.asm
CLEAN-FILES := AHX-Replayer000.BIN.patched

include $(TOPDIR)/build/lib.mk

AHX-Replayer000.BIN.patched: AHX-Replayer000.BIN AHX-Replayer000.diff
	@echo "[DIR] $(addprefix $(DIR),$^) -> $(DIR)$@"
	$(BINPATCH) $^ $@

ahx.o: ahx.asm $(TOPDIR)/include/ahx.h AHX-Replayer000.BIN.patched
