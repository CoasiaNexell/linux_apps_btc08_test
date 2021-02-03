#!/bin/sh

############################################################################
## gpio export
echo 127 > /sys/class/gpio/export	# RESET
echo 126 > /sys/class/gpio/export	# IRQ_GN
echo 125 > /sys/class/gpio/export	# IRQ_OON

echo 'out' > /sys/class/gpio/gpio127/direction	# RESET  (WRITE)
echo 'in' > /sys/class/gpio/gpio126/direction	# IRQ_GN (READ)
echo 'in' > /sys/class/gpio/gpio125/direction	# IRQ_OON(READ)

############################################################################
## release reset
echo 1 > /sys/class/gpio/gpio127/value	# RESET