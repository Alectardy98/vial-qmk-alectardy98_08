# Keep the extension local to the requested Vial keymaps. Other xwhatsit builds
# retain their original utility protocol and constant calibration path.
ifneq ($(filter vial vial_alec,$(KEYMAP)),)
XWHATSIT_RUNTIME_CALIBRATION ?= yes
ifeq ($(XWHATSIT_RUNTIME_CALIBRATION),yes)
OPT_DEFS += -DXWHATSIT_RUNTIME_CALIBRATION
SRC += keyboards/xwhatsit/calibration_config.c
endif
endif

# Copyright 2020 Purdea Andrei
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.

# Build Options
#   change yes to no to disable
#

CUSTOM_MATRIX=lite
SRC += keyboards/xwhatsit/matrix.c keyboards/xwhatsit/util_comm.c
HAPTIC_ENABLE = yes
HAPTIC_DRIVER = solenoid

# Required by QMK AVR send_string timer entropy helper.
OPT_DEFS += -include avr/io.h
OPT_DEFS += -Wno-error=unused-but-set-variable
