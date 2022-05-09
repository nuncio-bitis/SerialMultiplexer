###############################################################################
# Create a virtual serial port pair that are linked to each other (Tx <-> Rx).
# Useful for testing two programs that talk to each other over a serial port.
#
# May 2020
# J. Parziale
###############################################################################

# File to hold the device paths/names to delete on '-x' option.
infoFile="/tmp/socat_info.txt"

devDest="dev"
systemType=$(uname -s)

# Mac won't allow creating pseudo devices under /dev, so use /tmp
if [[ $systemType == "Darwin" ]]; then
    devDest="tmp"
fi

device0Name_="/${devDest}/ttyS30"
device1Name_="/${devDest}/ttyS31"

###############################################################################

if [[ "$1" == "-h" ]]; then
    echo
    echo "Usage: $(basename $0) [-h] [-x] [<devName0> <devname1>]"
    echo "      -h : Print this help message."
    echo "      -x : Close current-running session (if any)"
    echo "      <devName0> <devname1> : Specify port names manually."
    echo "          In this case, /${devDest}/<devName0> and /${devDest}/<devname1> will be created."
    echo "          Without specification, the defaults are \"$device0Name_\" and \"$device1Name_\""
    echo
    exit 0
fi

###############################################################################

# Get the process ID of a running instance
socatPid=$(ps -ef | grep socat | grep -v "grep" | awk '{ print $2 }')

# Check if the '-x' option was used to kill a running process.
if [[ "$1" == "-x" ]]; then
    # Check if an instance is actually running
    if [[ -z $socatPid ]]; then
        echo
        echo "WARNING: No instance of socat is running, no action taken."
        echo
        exit 0
    fi
    echo "Killing process $socatPid."
    sudo kill -KILL $socatPid
    #if [[ $devDest == "tmp" ]]; then
        sudo rm -f $(cat ${infoFile})
        rm -f ${infoFile}
    #fi
    exit 0
fi

###############################################################################
# Check if an instance is already running, to avoid multiple instances.

if [[ -n $socatPid ]]; then
    echo
    echo "ERROR: socat is already running; pid=$socatPid"
    echo
    exit 1
fi

###############################################################################
# Use device names specified on the command line.

if [[ $# -ne 0 && $# -ne 2 ]]; then
    echo
    echo "ERROR: Exactly two arguments must be specified."
    echo
    exit 1
fi
if [[ $# -eq 2 ]]; then
    device0Name_="/${devDest}/$1"
    device1Name_="/${devDest}/$2"
fi

# Save device names to be used later
echo "$device0Name_ $device1Name_" > ${infoFile}

###############################################################################

echo "Creating virtual serial port pair..."
echo "Serial Device #1: $device0Name_"
echo "Serial Device #2: $device1Name_"
nohup sudo socat -d -d pty,raw,echo=0,link=$device0Name_ pty,raw,echo=0,link=$device1Name_ &
socatPid=$!

sleep 0.25
echo "Process #$socatPid created."

###############################################################################
# Must wait for both devices to exist before changing their properties.
while [[ ! -c $device0Name_ || ! -c $device1Name_ ]]; do
  sleep 0.1
done

sudo chmod a+rw $device0Name_
sudo chmod a+rw $device1Name_

###############################################################################
