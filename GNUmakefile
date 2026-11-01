# Keep the MY-2K's Python compatibility launcher scoped to its two final targets.
# All other keyboards continue to use the unchanged upstream Makefile.
ifneq ($(filter alectardy98/matrix_my2k_100ng/main:vial alectardy98/matrix_my2k_100ng/aux:aux,$(MAKECMDGOALS)),)
.PHONY: alectardy98/matrix_my2k_100ng/main\:vial alectardy98/matrix_my2k_100ng/aux\:aux
alectardy98/matrix_my2k_100ng/main\:vial:
	+@PATH="keyboards/alectardy98/matrix_my2k_100ng/tools:$$PATH" $(MAKE) --no-print-directory -f Makefile $@ QMK_BIN="keyboards/alectardy98/matrix_my2k_100ng/tools/qmk" SKIP_VERSION=yes
alectardy98/matrix_my2k_100ng/aux\:aux:
	+@$(MAKE) --no-print-directory -f keyboards/alectardy98/matrix_my2k_100ng/aux/build.mk
else
include Makefile
endif
