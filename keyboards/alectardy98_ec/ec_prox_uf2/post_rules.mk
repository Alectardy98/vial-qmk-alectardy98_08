# Vial is required by this fork, but upstream's metadata parser rejects it in
# rules.mk. This hook runs before common_features.mk selects the Vial sources.
VIAL_ENABLE = yes

# Schedule the existing LTO partitions with up to three workers. No partition,
# optimization or warning-suppression flags change.
EXTRALDFLAGS += -flto=3

$(INTERMEDIATE_OUTPUT)/wear_leveling_efl.o: FILE_SPECIFIC_CFLAGS += -fno-inline -Dbacking_store_init=a98_qmk_init -Dbacking_store_unlock=a98_qmk_unlock
$(INTERMEDIATE_OUTPUT)/lib/chibios/os/hal/ports/STM32/STM32F4xx/hal_efl_lld.o: FILE_SPECIFIC_CFLAGS += -fno-inline -Defl_lld_program=a98_hw_program -Defl_lld_start_erase_all=a98_unused_mass_erase -Defl_lld_start_erase_sector=a98_hw_erase -Defl_lld_start=a98_hw_start -Defl_lld_query_erase=a98_hw_query_erase
$(INTERMEDIATE_OUTPUT)/eeprom_wear_leveling.o: FILE_SPECIFIC_CFLAGS += -Deeprom_driver_init=a98_unused_eeprom_init
$(INTERMEDIATE_OUTPUT)/quantum/via.o: FILE_SPECIFIC_CFLAGS += -include keyboards/alectardy98_ec/ec_prox_uf2/storage_version.h


UF2_FAMILY = 0x57755a57
