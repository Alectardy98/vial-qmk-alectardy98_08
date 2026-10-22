# Keep the actual QMK backend; interpose validation, not EEPROM algorithms.
$(INTERMEDIATE_OUTPUT)/wear_leveling_efl.o: FILE_SPECIFIC_CFLAGS += -fno-inline -Dbacking_store_init=s2_qmk_init -Dbacking_store_read=s2_qmk_read -Dbacking_store_unlock=s2_qmk_unlock
# Standard ChibiOS implementations remain behind sector-boundary wrappers.
$(INTERMEDIATE_OUTPUT)/lib/chibios/os/hal/ports/STM32/STM32F4xx/hal_efl_lld.o: FILE_SPECIFIC_CFLAGS += -fno-inline -Defl_lld_program=s2_hw_program -Defl_lld_start_erase_all=s2_unused_erase_all -Defl_lld_start_erase_sector=s2_hw_erase_sector -Defl_lld_start=s2_hw_start -Defl_lld_query_erase=s2_hw_query_erase
$(INTERMEDIATE_OUTPUT)/eeprom_wear_leveling.o: FILE_SPECIFIC_CFLAGS += -Deeprom_driver_init=s2_unused_eeprom_init

# Keyboard-local pin: include generated version.h once, then override for VIA.
$(INTERMEDIATE_OUTPUT)/quantum/via.o: FILE_SPECIFIC_CFLAGS += -include keyboards/mmstudio/class0413_ec/modules/storage_version.h

$(INTERMEDIATE_OUTPUT)/solenoid.o: FILE_SPECIFIC_CFLAGS += -fno-inline -Dusb_device_state_get_configure_state=ec_solenoid_setup_usb_state -Dsolenoid_fire_handler=ec_qmk_solenoid_fire_handler
$(INTERMEDIATE_OUTPUT)/quantum/haptic.o: FILE_SPECIFIC_CFLAGS += -Deeconfig_read_haptic=ec_read_haptic_config
$(INTERMEDIATE_OUTPUT)/quantum/audio/audio.o: FILE_SPECIFIC_CFLAGS += -fno-inline -Daudio_init=ec_standard_audio_init

# Include the native music core exactly once through the local safety shim.
override MUSIC_ENABLE = no

# Keep native Clicky, avoid newlib rand()'s large heap/assert/stdio dependency.
$(INTERMEDIATE_OUTPUT)/quantum/process_keycode/process_clicky.o: FILE_SPECIFIC_CFLAGS += -Drand=ec_clicky_rand

# Schedule existing LTRANS partitions with at most two compiler workers.
# Retain LTO/default partitioning; no dependence on a make jobserver.
ifeq ($(strip $(LTO_ENABLE)), yes)
EXTRALDFLAGS += -flto=2
endif

# Single LTO partition permits cross-module size optimization without dropping features.
EXTRALDFLAGS += -flto-partition=one


OPT_DEFS += -include keyboards/mmstudio/class0413_ec/modules/time_math.h
