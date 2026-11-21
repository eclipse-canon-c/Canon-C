#!/usr/bin/env bash
# VERIFY-048: the README's platform tiers, checked on AVR (avr-gcc 7,
# avr-libc, ATmega328P), whose size_t has 16 bits. The check is
# tools/tier_check.sh, whose defaults are AVR's (VERIFY-107).
exec "$(dirname "$0")/tier_check.sh" "$@"
