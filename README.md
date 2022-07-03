# Serial Multiplexer
Utility to create multiple virtual serial ports that all go over a single physical serial port.  

Users of the virtual ports don't have to know anything about the underlying
architecture. All the channel-handling is done by the utility.  
This same utility should be run on the remote side to implement the remote
side of the virtual ports. Make sure the same channels are specified on both sides.

Virtual ports are specified on the command line as channel:devicePath pairs.  
Example:

```
$ serial-mux -c10:/tmp/ptyA -c20:/tmp/ptyB /dev/ttyp0
```

Here we are creating two virtual ports that go through physical port /dev/ttyp0:
* Port 10 will be attached to virtual port /tmp/ptyA
* Port 20 will be attached to virtual port /tmp/ptyB

The remote machine could start the utility as:

```
$ serial-mux -c10:/tmp/virtualA -c20:/tmp/virtualB /dev/ttyAMA0
```

In this way data is transferred across both computers to connect channel 10 on one
side to channel 10 on the other, and similarly for channel 20.

Data packets over the physical serial port will have the following format:
*  Channel Id : 1 byte (0..255)
*  NumBytes   : 2 bytes (max cMaxDataSize bytes)
*  Data...    : NumBytes bytes (0..cMaxDataSize-1)

NOTE that this utility is not intended for streaming high-speed data.  
RS-232 is already too slow for that anyway.  
This utility facilitates a command and messaging interface between two systems with limited serial ports  
and do not have access to a network.

---
