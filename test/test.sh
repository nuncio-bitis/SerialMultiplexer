###############################################################################
# Script to test the serial multiplexer utility.
# * Use socat to create two virtual serial port "sides"
#   (side 1 = a device, side 2 = other device).
# * Create 2 channels (A and B) on both sides of the serial port.
# * Run a test program for each of the four virtual ports.
# If all goes well, the output should show that only A-channel messages
# are exchanged between A channels, and B-channel messages only between
# B channels.
# i.e.
#   Side_1A <-> Side_2A
#   Side_1B <-> Side_2B
#
# May 2020
# J. Parziale
###############################################################################

devDest="dev"
systemType=$(uname -s)

# Mac won't allow creating pseudo devices under /dev, so use /tmp
if [[ $systemType == "Darwin" ]]; then
    devDest="tmp"
fi

dev1Base="ttyS11"
dev1="/${devDest}/${dev1Base}"
dev2Base="ttyS12"
dev2="/${devDest}/${dev2Base}"

###############################################################################

# Create 2 virtual ports connected together.
# These will act as two sides of the physical serial link.
echo
echo "=============================================================================="
echo "Starting pseudo serial ports..."
./test-serial-setup.sh ${dev1Base} ${dev2Base}
sleep 2

###############################################################################

echo
echo "=============================================================================="
echo "Creating side 1 virtual channels..."
# Side 1: Create two channels (A and B)
../bin/serial-mux -c10:/tmp/pty1A -c20:/tmp/pty1B ${dev1} &

echo
echo "=============================================================================="
echo "Creating side 2 virtual channels..."
# Side 2: Create two channels (A and B)
../bin/serial-mux -c10:/tmp/pty2A -c20:/tmp/pty2B ${dev2} &

sleep 2

###############################################################################
# We now have 2 "physical" ports, and 4 virtual ports.
# We'll create 4 program instances to handle the 4 virtual ports.

echo
echo "=============================================================================="
echo "Connecting A channels..."
../bin/sertest -c 10 "Side_1A" /tmp/pty1A &
../bin/sertest -c 10 "Side_2A" /tmp/pty2A &

echo
echo "=============================================================================="
echo "Connecting B channels..."
../bin/sertest -c 10 "Side_1B" /tmp/pty1B &
../bin/sertest -c 10 "Side_2B" /tmp/pty2B

###############################################################################

echo
echo "=============================================================================="
echo "Test programs finished. Take down serial-mux..."
pkill -INT serial-mux

sleep 2

echo
echo "=============================================================================="
echo "Killing pseudo serial ports..."
test-serial-setup.sh -x

echo
echo "=============================================================================="
rm -f nohup.out
echo "Done"
echo
