/**
ChrImuPacketParser.cpp File Description

Author: Neil Johnson (neil.johnson@ssci.com)
Date: 11/13/2015

Purpose:  This class opens a serial port connection to the CH Robotics UM7 IMU and parses packets
    received from the IMU.

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

 #include "ChrImuPacketParser.h"
 #include "SerialPortLinux.h"
// #include <NVXIO/Utility.hpp>
 //#include "common_include.h"
// #include "flaUtilities.h"
 #include <string.h>
 #include <stdexcept>

 #include "flaUtilities.h"

#define SHOW_PRINTFD 1
//#define printfd(msg, lev, fmt, ...) printf(fmt, __VA_ARGS__)

SimpleFifo::SimpleFifo(void)
{
	FifoIn = 0;
	FifoOut = 0;
}

int SimpleFifo::Put(unsigned char n)
{
	if (FifoIn == (( FifoOut - 1 + FIFO_SIZE) % FIFO_SIZE))
	{
		printf("Serial FIFO error: FIFO is Full!\n");
		return -1;	//! FIFO is full!
	}

	Fifo[FifoIn] = n;
	FifoIn = (FifoIn + 1) % FIFO_SIZE;
	return 0;
}

double last_fifo_error_time = 0.0;

int SimpleFifo::Get(unsigned char *old)
{
	if (FifoIn == FifoOut) {
        if (ms_time() - last_fifo_error_time > 1000.0) {
            printf("Serial FIFO is empty\n");
            last_fifo_error_time = ms_time();
		}
		return -1;
	}

	*old = Fifo[FifoOut];

	FifoOut = (FifoOut + 1) % FIFO_SIZE;
	return 0;
}

int SimpleFifo::getNumBytes(void)
{
	if (FifoIn == FifoOut)
		return 0;
	else if (FifoIn > FifoOut)
		return FifoIn-FifoOut;
	else
		return (FifoIn + FIFO_SIZE) - FifoOut;
}

int SimpleFifo::Peek(int peekIndex, unsigned char *val)
{
	if (peekIndex >= getNumBytes()) {
		printf("Trying to peek beyond the available size! peek: %d, size: %d\n", peekIndex, getNumBytes());
		return -1;
	}

	int index = (FifoOut + peekIndex) % FIFO_SIZE;
	*val = Fifo[index];
	return 0;
}

chrImuPacketParser::chrImuPacketParser(std::string devicePath, int baudrate)
    :   imuSampleCount(0)
    ,   loggingEnabled(false)
    //,    imuLogFile(NULL)
{
    serialReadTime_ms = ms_time();
	m_portIsOpen = false;

    if (baudrate != 921600) {
        throw std::runtime_error("Invalid baud rate specified for IMU!  Only 921600 supported");
    }

	if (m_serialPort.Open(devicePath.c_str(), B921600, PARITY_NONE) >= 0) {
		m_portIsOpen = true;
	} else {
		printf("Error opening IMU serial port!\n");
	}

	//! Init attitude mutex so that calls to get the attitude are threadsafe:
    pthread_mutex_init( &m_attitudeMutex, NULL );
}

chrImuPacketParser::~chrImuPacketParser()
{
	if (m_portIsOpen) {
		m_serialPort.Close();
		m_portIsOpen = false;
	}

/*    if (imuLogFile != NULL) {
        fprintf(imuLogFile, "];\n");
        fclose(imuLogFile);
        imuLogFile = nullptr;
    }*/
}

#define MIN_BYTE_PROCESS_SIZE 100
#define MIN_PACKET_SIZE	7

int chrImuPacketParser::ProcessBytes(void)
{
	if (!m_portIsOpen) {
		printf("Trying to process bytes, but port isn't open!\n");
		return -1;
	}
	//printf("%s %d\n", __FILE__, __LINE__);

	int numBytesRead=0;
    serialReadTime_ms = ms_time();

	//! Try to read from serial buffer:
	if ( (numBytesRead = m_serialPort.readBytes(readBuf, MAX_READ_COUNT)) > 0) {
		//printf("Read %d bytes from serial port!\n", numBytesRead);

		if (numBytesRead > MAX_READ_COUNT-2) {
            //! If we get in here, we waited too long to service the serial port
            //! interrupt, so we should flush the input buffer and get out.  We don't want old stale data.
            //printfd(DM_IMU, 0, "We waited too long to parse the IMU serial port.  Flushing!\n");
            //m_serialPort.FlushInput();
            return -CHR_PACKET_ERROR_FLUSH_INPUT;
		}
		for (int i=0; i<numBytesRead; i++)
		{
			rxFifo.Put(readBuf[i]);
		}
		//printf("Put %d bytes into rxFifo\n", numBytesRead);
	}

	//! If there is data in the FIFO, look for the start bytes:
	int numBytes = rxFifo.getNumBytes();
	//printf("Bytes in: %d, Bytes out: %d, In-out = %d, count = %d\n", totalIn, totalOut, totalIn-totalOut, numBytes);
	static unsigned char temp[3] = {0};
	unsigned char possible_packet[FIFO_ELEMENTS];
	int numPacketsFound = 0;
	int packetStartIndices[200];

	if (numBytes >= MIN_BYTE_PROCESS_SIZE) {
		//printf("%s %d\n", __FILE__, __LINE__);
		//printf("Got some bytes: \n");
		for (int i=0; i<numBytes-2; i++) {
			rxFifo.Peek(i, &temp[0]);
			rxFifo.Peek(i+1, &temp[1]);
			rxFifo.Peek(i+2, &temp[2]);
			if ( (temp[0] == 's') && (temp[1] == 'n') && (temp[2] == 'p')) {
				//! Keep track of the indices where packet start bytes were found:
				packetStartIndices[numPacketsFound++] = i;
			}
		}
		//printf("Found %d possible packets in this batch\n", numPacketsFound);
		//printf("First packet start index: %d\n", packetStartIndices[0]);

		//! For each start index, peek at the corresponding bytes, and put them
		//! into a buffer and hand off for processing packets.
		if (numPacketsFound > 1) {
			//! Get rid of any bytes in front of the first packet index:
			int numBytesRemoved = 0;
			for (int i=0; i<packetStartIndices[0]; i++)
			{
				//printfd(DM_IMU, 1, "Removing bytes from front of array before first index\n");
				rxFifo.Get(&temp[0]);
				numBytesRemoved++;
			}

			//if (numBytesRemoved > 0) printf("After removing, there are %d bytes left in the FIFO\n", rxFifo.getNumBytes());

			//! Subtract off the number of bytes removed from the indices in the packetStartIndices:
			for (int i=0; i<numPacketsFound; i++)
			{
				packetStartIndices[i] -= numBytesRemoved;
				numBytes -= numBytesRemoved;
			}

			for (int i=0; i<numPacketsFound; i++)
			{
				//! Make sure there are enough bytes to form a valid packet.  If not, come back next time:
				//! If this is the last of the discovered packets, then the packet length will be the total number of bytes available
				//! minus the last packetStartIndex.  Otherwise, it's just the difference between successive packet start indices.
				int packet_bytes = 0;
				int end_index = 0;
                if ( i==(numPacketsFound-1) ) {
                    packet_bytes = numBytes - packetStartIndices[i];
                    end_index = numBytes;
                } else {
                    //int remaining_bytes = numBytes - packetStartIndices[i];
                    packet_bytes = packetStartIndices[i+1] - packetStartIndices[i];
                    end_index = packetStartIndices[i+1];
                }
				if (packet_bytes >= MIN_PACKET_SIZE)
				{
					//printf("Grabbing a packet from %d to %d\n", packetStartIndices[i], end_index);

					int count=0;
					for (int j=packetStartIndices[i]; j<end_index; j++, count++)
					{
						rxFifo.Get(&possible_packet[count]);
					}
					//! Process the packet now!
					ProcessPacket(possible_packet, packet_bytes);
				}
			}
		}
		//! After processing packets, how much data is left in the FIFO?
		//if (rxFifo.getNumBytes() > 0) printf("  After processing, there are %d bytes left in the FIFO\n", rxFifo.getNumBytes() );
	}
	return CHR_PACKET_SUCCESS;
}

/*
	//! Print out the whole packet:
	printf("Health Packet: \n  ");
	for (int i=0; i<m_Packet.getPacketLength(); i++) {
		printf("0x%02X ", pkt[i]);
		if (i%10 == 9 ) {
			printf("\n  ");
		}
	}
	printf("\n");
*/

int chrImuPacketParser::ProcessPacket(const unsigned char *pkt, int len)
{
	if (!m_portIsOpen) {
		return -1;
	}
	//! Make a new packet...need to define what is in a packet...is it just a struct?

	if (m_Packet.setPacketData(pkt, len) < 0) {
		//printfd(DM_ERROR_MSG, 3, "Packet failed setting data! (byte length: %d, should be %d)\n", len, m_Packet.getPacketLength());
		return -1;
	}

	int16_t temp_uint16_1;
	int16_t temp_uint16_2;
	uint32_t tempUint32_1;
	uint32_t tempUint32_2;
	uint8_t batchLen;
	bool gotAttitude = false;

	static int attPktsRxed = 0;

	//! If this is a batch, we need to process all the bytes:
	if (m_Packet.hasData()) {
		if (m_Packet.isBatch()) {
			batchLen = m_Packet.getBatchLength();
		} else {
			batchLen = 1;
		}
	} else {
		batchLen = 0;
	}

	if (batchLen > 0) {
		//! Get the first address in the batch:
		uint8_t regIndex = m_Packet.getAddress();
		int regIndexCounter = 0; 	//! This is how many of the registers in the batch that we have processed
		uint8_t batchBytes[4];



		while (regIndexCounter < batchLen) {
			//! The data for this register is found in batchBytes after the following is called:
			if (m_Packet.getBatchBytes(regIndexCounter, batchBytes) < 0) {
				//printfd(DM_ERROR_MSG, 1, "Error getting bytes from packet!\n");
				continue;
			}

			switch(regIndex)
			{
				case CHR_DREG_HEALTH: {
					pthread_mutex_lock(&m_attitudeMutex);
                        memcpy(&tempUint32_1, batchBytes, 4);
                        m_latestAttitudeSample.health = ntohl(tempUint32_1);
                        if (m_latestAttitudeSample.health != 0x00000001) {
                            printf("Health warning! 0x%0X\n", m_latestAttitudeSample.health);
                        }
					pthread_mutex_unlock(&m_attitudeMutex);
				}
					break;
				case CHR_DREG_EULER_PHI_THETA:
				    pthread_mutex_lock(&m_attitudeMutex);
                        m_latestAttitudeSample.tegra_timestamp_ms = serialReadTime_ms;
                        attPktsRxed++;
                        //! Copy out the bytes from the packet:
                        m_latestAttitudeSample.phi_raw = (int16_t)((batchBytes[0] << 8) & 0xFF00) | batchBytes[1];
                        m_latestAttitudeSample.theta_raw = (int16_t)((batchBytes[2] << 8) & 0xFF00) | batchBytes[3];
                        //! TODO: Stuff the data into a structure
                        m_latestAttitudeSample.theta_deg = ((float)m_latestAttitudeSample.theta_raw)/91.02222;
                        m_latestAttitudeSample.phi_deg = ((float)m_latestAttitudeSample.phi_raw)/91.02222;
					pthread_mutex_unlock(&m_attitudeMutex);
					break;
				case CHR_DREG_EULER_PSI:
				    pthread_mutex_lock(&m_attitudeMutex);
                        m_latestAttitudeSample.psi_raw = (int16_t)((batchBytes[0] << 8) & 0xFF00) | batchBytes[1];
                        m_latestAttitudeSample.psi_deg = ((float)m_latestAttitudeSample.psi_raw)/91.02222;
					pthread_mutex_unlock(&m_attitudeMutex);
					break;
				case CHR_DREG_EULER_PHI_THETA_DOT:
					memcpy(&temp_uint16_1, &batchBytes[0], 2);	//!< pitch rate (theta dot)
					memcpy(&temp_uint16_2, &batchBytes[2], 2); 	//!< roll rate (phi dot)
					//printf("  Roll/Pitch Rates: %f %f\n", ((float)temp_uint16_2)/16.0, ((float)temp_uint16_1)/16.0);
					break;
				case CHR_DREG_EULER_PSI_DOT:
					break;
				case CHR_DREG_EULER_TIME:
				    pthread_mutex_lock(&m_attitudeMutex);
                        memcpy(&tempUint32_1, batchBytes, 4);
                        tempUint32_2 = ntohl(tempUint32_1);
                        memcpy(&m_latestAttitudeSample.imu_time, &tempUint32_2, 4);
                    pthread_mutex_unlock(&m_attitudeMutex);
				    gotAttitude = true;
					break;
				case CHR_MREG_GET_FIRMWARE_VERSION:
				{
					//! Got the firmware revision:
					/*char firmware_rev[5];
					firmware_rev[0] = batchBytes[0];
					firmware_rev[1] = batchBytes[1];
					firmware_rev[2] = batchBytes[2];
					firmware_rev[3] = batchBytes[3];
					firmware_rev[4] = '\0';*/
					//printf("Firmware Revision: %s\n", firmware_rev);
				}
					break;
				default:
					//printf("Got a packet of address %d, length: %d, packet length: %d\n", m_Packet.getAddress(), len, m_Packet.getPacketLength());
					break;
			}
			regIndex++;
			regIndexCounter++;

            if (gotAttitude) {
                /*if (loggingEnabled) {
                    pthread_mutex_lock(&m_attitudeMutex);
                        //! Write the current data sample to the log file.
                        fprintf(imuLogFile, "[%lu, %lf, %f, %f, %f %f %u];\n",
                            imuSampleCount, m_latestAttitudeSample.tegra_timestamp_ms, m_latestAttitudeSample.imu_time,
                            m_latestAttitudeSample.phi_deg, m_latestAttitudeSample.theta_deg, m_latestAttitudeSample.psi_deg, m_latestAttitudeSample.health);
                    pthread_mutex_unlock(&m_attitudeMutex);
                }*/
                imuSampleCount++;
                gotAttitude = false;
            }
		}
	}
	//! The packet expires as soon as this function returns...and the next packet gets copied in.

	return CHR_PACKET_SUCCESS;
}

void chrImuPacketParser::setupLogging(const std::string logPath)
{
    loggingEnabled = true;

    //! Set up a log for IMU data:
/*    char imuLogFilename[2048];
    printf("imu log save path: %s\n", logPath.c_str());
    snprintf(imuLogFilename, 2048, "%s/imu.m", logPath.c_str());
    imuLogFile = fprintf_queue_t::fopen(imuLogFilename, "w+");
    if (imuLogFile == NULL)
    {
        printf("Error creating imu log file\n");
        //assert( false );
        return;
    }
    fprintf(imuLogFile, "imuHeader = {'Index', 'tegra_time_ms', 'imu_time', 'euler_phi_deg', 'euler_theta_deg', 'euler_psi_deg', 'health_bitmask'};\n");
    fprintf(imuLogFile, "imuData = [...\n");
*/

}

void chrImuPacketParser::sendZeroGyroPacket()
{
	if (!m_portIsOpen) {
		return;
	}
	uint8_t tx_data[20];
	tx_data[0] = 's';
	tx_data[1] = 'n';
	tx_data[2] = 'p';
	tx_data[3] = 0x00;										//!< Packet Type byte
	tx_data[4] = (uint8_t)CHR_MREG_ZERO_GYROS; 				//!< Address
	uint16_t check= 0;
	for (int i=0; i<5; i++)
	{
		check += tx_data[i];
	}

	tx_data[5] = 0x01; //(uint8_t)((check >> 8) & 0xFF);
	tx_data[6] = 0xFE; //(uint8_t)(check & 0xFF);

	/*printf("zero gyro checksum = 0x%0X (%d), high byte: 0x%0x, low byte: 0x%0x\n",
		check, check, tx_data[5], tx_data[6]);
		*/

	m_serialPort.sendBytes(tx_data, 7);
}

void chrImuPacketParser::getFirmwareRevision()
{
	if (!m_portIsOpen) {
		return;
	}

	uint8_t tx_data[20];
	tx_data[0] = 's';
	tx_data[1] = 'n';
	tx_data[2] = 'p';
	tx_data[3] = 0x00;										//!< Packet Type byte
	tx_data[4] = (uint8_t)CHR_MREG_GET_FIRMWARE_VERSION; 	//!< Address
	uint16_t check= 0;
	for (int i=0; i<5; i++)
	{
		check += tx_data[i];
	}
	tx_data[5] = 0x01; //(uint8_t)((check >> 8) & 0xFF);
	tx_data[6] = 0xFB; //(uint8_t)(check & 0xFF);

	/*printf("checksum = 0x%0X (%d), high byte: 0x%0x, low byte: 0x%0x\n",
		check, check, tx_data[5], tx_data[6]);
		*/

	m_serialPort.sendBytes(tx_data, 7);
}

void chrImuPacketParser::sendResetEkfPacket()
{
	if (!m_portIsOpen) {
		return;
	}

	uint8_t tx_data[20];
	tx_data[0] = 's';
	tx_data[1] = 'n';
	tx_data[2] = 'p';
	tx_data[3] = 0x00;										//!< Packet Type byte
	tx_data[4] = (uint8_t)CHR_MREG_RESET_EKF; 				//!< Address
	uint16_t check= 0;
	for (int i=0; i<5; i++)
	{
		check += tx_data[i];
	}

	tx_data[5] = 0x02; //(uint8_t)((check >> 8) & 0xFF);
	tx_data[6] = 0x04; //(uint8_t)(check & 0xFF);

	/*printf("reset ekf checksum = 0x%0X (%d), high byte: 0x%0x, low byte: 0x%0x\n",
		check, check, tx_data[5], tx_data[6]);
		*/


	m_serialPort.sendBytes(tx_data, 7);
}

