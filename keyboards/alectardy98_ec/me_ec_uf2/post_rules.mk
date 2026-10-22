$(INTERMEDIATE_OUTPUT)/wear_leveling_efl.o: FILE_SPECIFIC_CFLAGS += -fno-inline -Dbacking_store_init=a98_qmk_init -Dbacking_store_unlock=a98_qmk_unlock
$(INTERMEDIATE_OUTPUT)/lib/chibios/os/hal/ports/STM32/STM32F4xx/hal_efl_lld.o: FILE_SPECIFIC_CFLAGS += -fno-inline -Defl_lld_program=a98_hw_program -Defl_lld_start_erase_all=a98_unused_mass_erase -Defl_lld_start_erase_sector=a98_hw_erase -Defl_lld_start=a98_hw_start -Defl_lld_query_erase=a98_hw_query_erase
$(INTERMEDIATE_OUTPUT)/eeprom_wear_leveling.o: FILE_SPECIFIC_CFLAGS += -Deeprom_driver_init=a98_unused_eeprom_init
# Preserve the audited EEPROM schema across rebuilds. Only change these IDs
# for an intentionally incompatible Vial storage layout, not a new build date.
ifeq ($(KEYMAP),vial)
MATRIX_VIAL_STORAGE_ID := 0x009829ED
else ifeq ($(KEYMAP),vial_alec)
MATRIX_VIAL_STORAGE_ID := 0x0001198A
endif

ifneq ($(MATRIX_VIAL_STORAGE_ID),)
$(INTERMEDIATE_OUTPUT)/quantum/via.o: FILE_SPECIFIC_CFLAGS += -DA98_MATRIX_VIAL_BUILD_ID=$(MATRIX_VIAL_STORAGE_ID) -include keyboards/alectardy98_ec/me_ec_uf2/storage_version.h
else
# Leave archived keymaps outside this release's two verified schemas unchanged.
$(INTERMEDIATE_OUTPUT)/quantum/via.o: FILE_SPECIFIC_CFLAGS += -include keyboards/alectardy98_ec/common/runtime/storage_version.h
endif
