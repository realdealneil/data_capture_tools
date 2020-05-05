/**
SerialPortLinux.cpp File Description

Author: Neil Johnson (neil.johnson@ssci.com)
Date: 11/13/2015

Purpose:  This is the implementation file for a class that opens a serial port connection in Linux.
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


#include <stdio.h>
#include <string.h>
#include "SerialPortLinux.h"
#include <unistd.h>

SerialPortLinux::SerialPortLinux()
{
    m_fd = 0;
}

SerialPortLinux::~SerialPortLinux()
{
    //dtor
}

int SerialPortLinux::sendBytes(unsigned char* buffer, int len)
{
	return write(m_fd, buffer, len);
}

int SerialPortLinux::readBytes(unsigned char* buffer, int len)
{
	return read(m_fd, buffer, len);
}

int SerialPortLinux::Open(std::string devicePath, int baudrate, serialParity par)
{
	printf(" ****** Open serial port now!\n");
	
	//printf(" ******* User: %s\n", getUserName());
	
	struct termios terminalSettings;
	// http://linux.die.net/man/2/open

	m_fd = open(devicePath.c_str(), O_RDWR | O_NOCTTY | O_NDELAY | O_FSYNC);
	if (m_fd < 0) {
		printf("Failed to open serial port %s\n", devicePath.c_str());
		return m_fd;
	}

	// clear terminal attributes
	memset(&terminalSettings, 0, sizeof(struct termios));

	//setReceiveCallback();

	//! Set control flags:
	terminalSettings.c_cflag = baudrate | CS8 | CLOCAL | CREAD;

	//! Set Input Flags:
	terminalSettings.c_iflag = IGNPAR; // ONLCR? - do we want that?

	//! Set Output Flags:
	// Don't set any flags in c_oflag for now.  Leave alone.

	//! Set up non-canonical mode:
	terminalSettings.c_cc[VTIME] = 0;
	terminalSettings.c_cc[VMIN] = 1;

	//! Set the settings!
	tcsetattr(m_fd, TCSANOW, &terminalSettings);

	//! flush out input and output buffers:
	tcflush(m_fd, TCOFLUSH);
	tcflush(m_fd, TCIFLUSH);

	printf("Done opening serial port! device = %d Yeah\n", m_fd);
	return 0;
}

void SerialPortLinux::FlushInput()
{
    tcflush(m_fd, TCIFLUSH);
}

int SerialPortLinux::Close()
{
	close(m_fd);
	m_fd = 0;
	return 0;
}
