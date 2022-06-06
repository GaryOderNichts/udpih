#-------------------------------------------------------------------------------
.SUFFIXES:
#-------------------------------------------------------------------------------

.PHONY: all clean linux pico arm_kernel arm_user

all: linux pico
	@echo "\033[92mDone!\033[0m"

arm_user:
	@$(MAKE) --no-print-directory -C $(CURDIR)/arm_user

arm_kernel: arm_user
	@$(MAKE) --no-print-directory -C $(CURDIR)/arm_kernel

linux: arm_kernel
	@echo "\033[92mBuilding $@...\033[0m"
	@$(MAKE) --no-print-directory -C $(CURDIR)/linux

pico: arm_kernel
	@echo "\033[92mBuilding $@...\033[0m"
	@$(MAKE) --no-print-directory -C $(CURDIR)/pico

clean:
	@echo "\033[92mCleaning $@...\033[0m"
	@$(MAKE) --no-print-directory -C $(CURDIR)/arm_user clean
	@$(MAKE) --no-print-directory -C $(CURDIR)/arm_kernel clean
	@$(MAKE) --no-print-directory -C $(CURDIR)/linux clean
	@$(MAKE) --no-print-directory -C $(CURDIR)/pico clean
