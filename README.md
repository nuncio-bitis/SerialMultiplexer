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

---

# I'm Using GitHub Under Protest

This project is currently hosted on GitHub.  This is not ideal; GitHub is a proprietary,  
trade-secret system that is not Free and Open Souce Software (FOSS).  
I am deeply concerned about using a proprietary system like GitHub to develop my FOSS project.  
I urge you to read about the
[Give up GitHub](https://GiveUpGitHub.org) campaign from
[the Software Freedom Conservancy](https://sfconservancy.org)  
to understand some of the reasons why GitHub is not a good place to host FOSS projects.

Any use of this project's code by GitHub Copilot, past or present, is done without my permission.  
I do not consent to GitHub's use of this project's code in Copilot.

I plan to move this project to another hosting site (TBD) and will leave a link to it here in this README file.

---

![Logo of the GiveUpGitHub campaign](https://sfconservancy.org/img/GiveUpGitHub.png)

