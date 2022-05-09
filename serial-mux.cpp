/*
 * This file is part of the SerialMultiplexer distribution
 *   (https://github.com/nuncio-bitis/SerialMultiplexer
 * Copyright (c) 2022 Jim Parziale
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */
/*
 * serial-mux.cpp
 *
 * Created on: 8 May 2022
 * Author: Jim Parziale
 */
// ****************************************************************************

#include <iostream>
#include <string>
#include <map>

#include <chrono>
#include <thread>             // std::thread
#include <mutex>              // std::mutex, std::unique_lock
#include <condition_variable> // std::condition_variable

#include <string.h>
#include <libgen.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdarg.h>
#include <signal.h>
#include <termios.h>

#include "serial-mux.h"

/******************************************************************************
 * Example command line:
 *   serial-mux -c10:/tmp/ptyA -c20:/tmp/ptyB /dev/ttyp0
 * Here we are creating two virtual ports that go through
 * physical port /dev/ttyp0:
 * - Port 10 will be in /tmp/ptyA
 * - Port 20 will be in /tmp/ptyB
 *
 * Data packets over the physical serial port will have the
 * following format:
 *   ChannelId : 1 byte (0..255)
 *   NumBytes  : 2 bytes (max cMaxDataSize bytes)
 *   Data...   : NumBytes bytes (0..cMaxDataSize-1)
 *
 * @NOTE that this utility is not intended for streaming data. RS-232 is
 * already too slow for that anyway. This utility facilitates a command
 * and messaging interface between two systems with limited serial ports
 * and do not have access to a network.
 *
 *****************************************************************************/

const int cMaxNameSize = 256;
const int cMaxBufSize  = 1024;
const int cMaxDataSize = 1000;  // max number of data bytes per packet
const int cMaxChannelId = 255;

static std::string processName;

// Termination condition
static bool terminateProcess = false;

// Path name of physical serial port
static std::string serialPortPath;
// File descriptor of physical serial port
static int phys_tty;

//******************************************************************************

static int  openPysicalPort(std::string devPath);
static int  configurePort(int handle);
static int  startPty(struct ptyChan &pty);
static void shutdownVPorts(void);
static void readThread(void);
static void writeThread(void);

//******************************************************************************
// Define list of virtual tty channels

struct ptyChan
{
    // User-specified channel ID
    uint8_t channelId;
    // Pseudo-tty file descriptor
    int pty;
    // User-specified pseudo-tty port path (blank if none specified)
    std::string linkPath;
    // Pseudo-device name assigned by the OS
    std::string ptyName;
};

// [key, value] = [channelId, ptyChan]
static std::map<uint8_t, ptyChan> sVirtualPorts;

// ****************************************************************************

static void help(void)
{
    std::cerr << std::endl;
    std::cerr << processName << " v" << SERIAL_MUX_VERSION_MAJOR << "." << SERIAL_MUX_VERSION_MINOR << std::endl;
    std::cerr << "Usage: " << processName
              << " -c id[:link] [-c id[:link]...] <serialPort>\n"
              << "   -c : Set up channel with ID and optional symlink (full path)\n"
              << "   -h : This help text\n"
              << std::endl
              << std::endl;
}

//******************************************************************************

void exceptionHandler(int signo)
{
    std::cout << std::endl;
    std::cout << "Caught signal " << signo << std::endl;

    terminateProcess = true;
}

// ****************************************************************************

// ISO 8601 style time stamp (w/o time zone)
std::string timestamp()
{
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);

    time_t curtime;
    time(&curtime);
    struct tm *local_time = localtime(&curtime);

    char buffer[64];
    int len;

    len = snprintf(buffer, sizeof(buffer),
                   "%04d-%02d-%02dT%02d:%02d:%02d ", //.%09ld ",
                   1900 + local_time->tm_year,
                   1 + local_time->tm_mon,
                   local_time->tm_mday,
                   local_time->tm_hour,
                   local_time->tm_min,
                   local_time->tm_sec);
                   //ts.tv_nsec);

    return std::string(buffer);
}

// ****************************************************************************

int main(int argc, char *argv[])
{
    processName = basename((char *) argv[0]);

    // Set up handlers
    signal(SIGINT,  exceptionHandler); // signal #2

    // ----------------------------------------------------
    // Process command line options

    int opt;
    while ((opt = getopt(argc, argv, "c:h")) != -1)
    {
        switch (opt)
        {
        case 'c':
        {
            struct ptyChan chan;
            chan.pty = -1; // will init later in startPty

            int n = atoi(optarg);
            if ((n > cMaxChannelId) || (n < 0))
            {
                std::cerr << std::endl;
                std::cerr << "ERROR: Channel ID must be 0-" << cMaxChannelId << std::endl;
                std::cerr << std::endl;
                return EXIT_FAILURE;
            }
            chan.channelId = (uint8_t)n;

            // Check if a link name was specified
            char *colon = strchr(optarg, ':');
            if (colon)
            {
                chan.linkPath = (colon+1);
            }
            // otherwise the link path is left empty.

            // Create map entry in place.
            sVirtualPorts.emplace(std::make_pair(n, chan));
        }
        break;

        case 'h':
        default:
            help();
            return EXIT_SUCCESS;
        }
    }

    if (sVirtualPorts.size() < 1)
    {
        std::cerr << std::endl;
        std::cerr << "ERROR: Must specify at least one channel" << std::endl;
        help();
        return EXIT_FAILURE;
    }

    if (optind >= argc)
    {
        std::cerr << std::endl;
        std::cerr << "ERROR: Must specify serial port" << std::endl;
        help();
        return EXIT_FAILURE;
    }
    // Get physical port device path
    serialPortPath = argv[optind];

    // ----------------------------------------------------
    // Open physical port.

    if (openPysicalPort(serialPortPath) != 0)
    {
        std::cerr << std::endl;
        std::cerr << "ERROR: Could not open physical serial port" << std::endl;
        std::cerr << std::endl;
        return EXIT_FAILURE;
    }

    std::cout << std::endl;
    std::cout << processName << ": Physical port has been opened; " << serialPortPath << std::endl;
    std::cout << std::endl;

    // ----------------------------------------------------
    // Create virtual ports

    for (auto& [cid, chan] : sVirtualPorts)
    {
        if (startPty(chan) != 0)
        {
            std::cerr << std::endl;
            std::cerr << "ERROR: Can't open PTY " << std::to_string(cid) << std::endl;
            std::cerr << std::endl;
            return EXIT_FAILURE;
        }
        else
        {
            printf("Connected %3d : %s (%s)\n",
                    chan.channelId, chan.ptyName.c_str(), chan.linkPath.c_str());
        }
    }

    // ----------------------------------------------------
    // Start the worker threads

    std::thread readThd(readThread);
    std::thread writeThd(writeThread);
    std::this_thread::sleep_for(std::chrono::milliseconds(250)); // wait for threads to be running

    // ----------------------------------------------------

    std::cout << std::endl;
    std::cout << timestamp() << "Welcome to " << processName << std::endl;
    std::cout << std::endl;

    // The read and write threads do all the work.
    // The main thread doesn't have to do anything.
    while (!terminateProcess)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // ----------------------------------------------------

    // Wait for the worker threads to exit gracefully before closing ports.
    readThd.join();
    writeThd.join();

    // Close all virtual ports and delete their sym links.
    shutdownVPorts();

    std::cout << std::endl;
    std::cout << timestamp() << "END Process: " << processName << std::endl;
    std::cout << std::endl;

    return EXIT_SUCCESS;
} // end main

//******************************************************************************

int openPysicalPort(std::string devPath)
{
    phys_tty = open(devPath.c_str(), O_RDWR | O_NOCTTY | O_SYNC | O_NONBLOCK);
    if (phys_tty < 0)
    {
        perror(devPath.c_str());
        return -1;
    }

    tcflush(phys_tty, TCIOFLUSH);

    if (configurePort(phys_tty)!= 0)
    {
        perror("Physical TTY configure.");
        return -1;
    }

    return 0;
}

// ****************************************************************************

int configurePort(int handle)
{
    struct termios info;
    tcgetattr(handle, &info);

    cfmakeraw(&info);
    info.c_cflag &= ~CRTSCTS;
    info.c_cflag |= (CLOCAL | CREAD);
    info.c_cflag &= ~CSIZE;
    info.c_oflag &= ~OPOST;
    info.c_cc[VTIME] = 0;
    info.c_cc[VMIN] = 0;

    return tcsetattr(handle, TCSANOW, &info); // -1 on error, 0 on success
}

// ****************************************************************************

int startPty(struct ptyChan &chan)
{
    int rv = 0;

    // allocate pty
    chan.pty = posix_openpt(O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (chan.pty == -1)
    {
        return -1;
    }

    grantpt(chan.pty);
    unlockpt(chan.pty);

/* @XXX *
    if (configurePort(chan.pty) != 0)
    {
        std::cerr << "@WARNING: Could not configure PTY for channel " << std::to_string(chan.channelId) << std::endl;
    }
* @XXX */

    char ptyName[cMaxNameSize];
    ptsname_r(chan.pty, ptyName, cMaxNameSize);
    chan.ptyName = ptyName;

    if (chan.linkPath.size() > 0)
    {
        // Make sure the link path doesn't exist.
        unlink(chan.linkPath.c_str());
        if ((rv = symlink(ptyName, chan.linkPath.c_str())))
        {
            perror(chan.linkPath.c_str());
        }
    }

    return rv;
}

// ****************************************************************************

void shutdownVPorts(void)
{
    for (auto& [cid, chan] : sVirtualPorts)
    {
        printf("Disconnecting %3d : %s (%s)\n",
                chan.channelId, chan.ptyName.c_str(), chan.linkPath.c_str());

        close(chan.pty);

        if (chan.linkPath.size() != 0)
        {
            unlink(chan.linkPath.c_str());
        }
    }
}

// ****************************************************************************

// Thread that reads from the physical port and dispatches the data to
// the appropriate virtual port.
void readThread(void)
{
    std::cout << "START: readThread()" << std::endl; // @DEBUG

    while (!terminateProcess)
    {
        int n;
        uint8_t  tmp;
        uint8_t  cid;
        uint16_t nbytes;

        // First byte is the channel ID
        if ((n = read(phys_tty, &cid, 1)) != 1)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(1)); // @DEBUG
            continue;
        }
        std::cout << "readThread cid=" << cid << std::endl; // @DEBUG

        // The next 2 bytes are the number of bytes to follow.
        // MSB first.
        n = read(phys_tty, &tmp, 1);
        nbytes = (tmp & 0xFF) << 8;
        n = read(phys_tty, &tmp, 1);
        nbytes += (tmp & 0xFF);
        std::cout << "readThread nbytes=" << std::to_string(nbytes) << std::endl; // @DEBUG

        struct ptyChan *chan = &sVirtualPorts[cid];

        // Read nbytes from phys_tty and write them to chan->pty
        while (!terminateProcess && nbytes)
        {
            n = read(phys_tty, &tmp, 1);
            std::cout << "readThread read " << std::to_string(n) << " bytes" << std::endl; // @DEBUG
            write(chan->pty, &tmp, 1);
            std::cout << "readThread wrote " << std::to_string(n) << " bytes" << std::endl; // @DEBUG
            nbytes--;
        }
    } // end while()
    std::cout << "END: readThread()" << std::endl; // @DEBUG
}

// ****************************************************************************

// Thread that reads from the virtual ports and writes to the physical port.
void writeThread(void)
{
    std::cout << "START: writeThread()" << std::endl; // @DEBUG

    while (!terminateProcess)
    {
        int tmp;
        uint8_t  c;
        uint16_t nbytes;
        // Holds data from a channel
        uint8_t buf[cMaxDataSize];

        for (auto& [cid, chan] : sVirtualPorts)
        {
            // Try to read as much data as possible from pty.
            tmp = read(chan.pty, buf, cMaxDataSize);
            if (tmp < 1)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(1)); // @DEBUG
                // No data read - check next pty
                continue;
            }
            nbytes = (uint16_t)tmp;

            // Send channel ID, nbytes MSB, nbytes LSB, then data bytes.
            tmp = write(phys_tty, &cid, 1);
            c = (nbytes >> 8) & 0xFF;
            tmp = write(phys_tty, &c, 1);
            c = nbytes & 0xFF;
            tmp = write(phys_tty, &c, 1);

            tmp = write(phys_tty, buf, nbytes);
            if (tmp != nbytes)
            {
                std::cerr << "ERROR: Write failed. Tried to send " << std::to_string(nbytes)
                          << " bytes, but write() returned " << std::to_string(tmp) << std::endl;
            }

            // Check termination condition
            if (terminateProcess)
            {
                break;
            }
        } // end for(cid, chan)
    } // end while()
    std::cout << "END: writeThread()" << std::endl; // @DEBUG
}

// ****************************************************************************
