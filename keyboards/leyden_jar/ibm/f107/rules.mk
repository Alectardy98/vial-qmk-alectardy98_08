include keyboards/leyden_jar/ibm/common/rules.mk

# A noisy capacitive pad must not restart debounce for unrelated keys.
DEBOUNCE_TYPE = sym_defer_pk
