/**
SerialPortLinux.h File Description

Author: Neil Johnson (neil.johnson@ssci.com)
Date: 11/13/2015

Purpose:  This is the header file for a class that opens a serial port connection in Linux.
    It provides the ability to specify a path to the device handle, a baud rate, and other serial port
    settings and open a connection to the device.

Date                      Modified By                               Reason
11/13/15              neil.johnson@ssci.com                  Created this file

Distribution Statement B: Distribution authorized to U.S. Government agencies only due to
the inclusion of proprietary information and to prevent Premature Dissemination of potentially
critical technological Information. Other requests for this document shall be referred to
DARPA Public Release Center (PRC) at PRC@darpa.mil.

SBIR DATA RIGHTS
Contract No.:  HR0011-15-C-0111
Contractor Name:  Scientific Systems Co., Inc.
Contractor Address:  500 W. Cummings Park, Suite 3000, Woburn, MA 01801
Expiration of SBIR Data Rights Period:  07/29/22
The Government's rights to use, modify, reproduce, release, perform, display or disclose
technical data or computer software marked with this legend are restricted during the period
shown as provided in paragraph (b)(4) of the Rights in Noncommercial Technical Data and Computer
Software-Small Business Innovative Research (SBIR) Program clause contained in the above
identified contract.  No restrictions apply after the expiration date shown above.  Any reproduction
of technical data, computer software or portions thereof marked with this legend must also reproduce
the markings.

Proprietary Information: This source code contains information proprietary and confidential to Scientific
Systems Company, Inc. (SSCI).

Copyright 2015-2017 Scientific Systems Company, Inc. All Rights Reserved. Use or disclosure of all
information and/or data above are subject to the restrictions listed above.

*/

#ifndef SERIALPORTLINUX_H
#define SERIALPORTLINUX_H

#include <string>
#include <termios.h>
#include <fcntl.h>
#include <unistd.h>

enum serialParity {
    PARITY_NONE = 0,
    PARITY_ODD,
    PARITY_EVEN
};

class SerialPortLinux
{
    public:
        SerialPortLinux();
        virtual ~SerialPortLinux();

        void SetPort(std::string devicePath) { m_portStr = devicePath; }

        //! The baudrate should be a B115200 type of number...
        void SetBaudRate(int baudrate) { m_baudrate = baudrate; }
        void SetParity(serialParity par) { m_parity = par; }

        int Open(std::string devicePath, int baudrate, serialParity par);

        int Close();

        void FlushInput();

        int sendBytes(unsigned char* buffer, int len);
        int readBytes(unsigned char* buffer, int len);

    protected:
    private:
        std::string m_portStr;
        int m_baudrate;
        serialParity m_parity;
        int m_fd;	// file descriptor for device handle
};

#endif // SERIALPORTLINUX_H
