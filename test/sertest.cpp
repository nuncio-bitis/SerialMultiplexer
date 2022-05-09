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
 * testA.cpp
 *
 * Created on: 9 May 2022
 * Author: Jim Parziale
 */
// ****************************************************************************

#include <iostream>
#include <string>

#include <chrono>
#include <thread>  // std::thread

#include <string.h>
#include <libgen.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdarg.h>
#include <signal.h>

#include "SerialPort.h"

// ****************************************************************************

const int cMaxBufSize  = 4096;

static std::string processName;

static std::string instanceName;

// Termination conditions
static bool terminateProcess = false;
static int  count = -1;

// Path name of serial port
static std::string serialPortPath;

static SerialPort sPort;

// ****************************************************************************

static void help(void)
{
    std::cerr << std::endl;
    std::cerr << "Usage: " << processName << " [-c] [-h] <instanceName> <serialPort>\n"
              << "   -c : Message count; terminate after this many data exchanges\n"
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
            count = atoi(optarg);
            break;

        case 'h':
        default:
            help();
            return EXIT_SUCCESS;
        }
    }

    if (optind >= argc)
    {
        std::cerr << std::endl;
        std::cerr << "ERROR: Must specify instance name and serial port" << std::endl;
        help();
        return EXIT_FAILURE;
    }

    // Get program's instance name
    instanceName = argv[optind++];

    // Get physical port device path
    serialPortPath = argv[optind];

    // ----------------------------------------------------
    // Open the serial port. Exit if something goes wrong.

    sPort.SetDevice(serialPortPath);
    sPort.SetTimeout(-1);

    if (0 != sPort.Open())
    {
        std::cerr << "ERROR: Could not open serial port" << serialPortPath << ". Cannot continue." << std::endl;
        return EXIT_FAILURE;
    }
    std::cout << "Port opened: " << serialPortPath << std::endl;

    // ----------------------------------------------------
    // Perform testing...

    std::cout << std::endl;
    std::cout << instanceName << ": Starting test..." << std::endl;
    std::cout << "------------------------------------------------------------" << std::endl;

    // Test can be terminated if user types Ctrl-C (or max count reached)
    int i = 0;
    int ret;
    char response[cMaxBufSize];
    while (!terminateProcess && ((count < 0) || (i++ < count)))
    {
        (void)sPort.Write(instanceName.c_str(), instanceName.size());

        memset(response, 0, sizeof(response));
        ret = sPort.Read(response, cMaxBufSize, 100);
        if (ret >= 0)
        {
            std::cout << timestamp() << instanceName << " : " << response << std::endl;
        }

        std::this_thread::sleep_for (std::chrono::seconds(1));
    }  // end while (!terminateProcess)

    // ----------------------------------------------------

    sPort.Close();

    std::cout << std::endl;

    return EXIT_SUCCESS;
} // end main

// ****************************************************************************
