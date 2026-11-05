set(JLINK_DEVICE STM32F411CC)
set(BOARD_BOOT_LD "${CMAKE_CURRENT_LIST_DIR}/boot.ld")
function(update_board TARGET)
  target_sources(${TARGET} PUBLIC
    ${ST_CMSIS}/Source/Templates/gcc/startup_stm32f411xe.s
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/validation.c)
  target_include_directories(${TARGET} PUBLIC ${TOP}/src)
  target_compile_definitions(${TARGET} PUBLIC STM32F411xE HSE_VALUE=8000000U)
  # Replace only the port's Pro X-specific 64 KiB vector validator. Its USB,
  # reset and handoff code remains unchanged; this board has 128 KiB SRAM.
  set_source_files_properties(${TOP}/ports/stm32f4/boards.c PROPERTIES
    COMPILE_DEFINITIONS "board_app_valid=virgo_unused_app_valid;board_dfu_init=virgo_port_dfu_init")
  set_source_files_properties(${TOP}/ports/stm32f4/board_flash.c PROPERTIES
    COMPILE_DEFINITIONS "board_flash_write=virgo_port_flash_write")
endfunction()
