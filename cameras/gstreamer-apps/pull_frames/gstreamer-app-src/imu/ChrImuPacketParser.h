/**
ChrImuPacketParser.h File Description

Author: Neil Johnson (neil.johnson@ssci.com)
Date: 11/13/2015

Purpose:  This is the header file for a class that opens a serial port connection to the
CH Robotics IMU and parses packets received from the IMU.

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

#ifndef CHRIMUPACKETPARSER_H
#define CHRIMUPACKETPARSER_H

#include <stdint.h>
#include <stdio.h>
#include <string>
#include "SerialPortLinux.h"
#include "ChrPacket.h"
//#include "fprintf_queue.h"
//#include "common_include.h"
//#include "NVXIO/FrameSource.hpp"

#include <arpa/inet.h>	//!< Used for byte swapping bigendian to little

#if 0
#define IMU_USE_PROTHREAD 1
#include "prothread.h"
#else
#define IMU_USE_PROTHREAD 0
#include <pthread.h>
#endif

#define MAX_READ_COUNT 2048

/**
    This structure holds all of the data we get from the IMU including raw and processed angles and timestamps.
 */
struct attitude_imu {
    double tegra_timestamp_ms;
    float imu_time;
	int16_t phi_raw;
	int16_t theta_raw;
	int16_t psi_raw;
	float phi_deg;
	float theta_deg;
	float psi_deg;
	uint32_t health;

	attitude_imu() :
        tegra_timestamp_ms(0.0),
        imu_time(0.0),
        phi_raw(0),
        theta_raw(0),
        psi_raw(0),
        phi_deg(0.0),
        theta_deg(0.0),
        psi_deg(0.0),
        health(0)
	{

	}
};

//! enumerate the registers of the UM7 that we might care about:
enum ChrRegister {
	//! CREG = Config register (I believe, that's how they do it in the data sheet)
	CHR_CREG_SETTINGS = 0,
	CHR_CREG_COM_RATES1 = 1,
	CHR_CREG_COM_RATES2 = 2,
	CHR_CREG_COM_RATES3 = 3,
	CHR_CREG_COM_RATES4 = 4,
	CHR_CREG_COM_RATES5 = 5,
	CHR_CREG_COM_RATES6 = 6,
	CHR_CREG_COM_RATES7 = 7,
	CHR_CREG_MISC_SETTINGS = 8,
	CHR_CREG_HOME_NORTH = 9,
	CHR_CREG_HOME_EAST = 10,
	CHR_CREG_HOME_UP = 11,
	CHR_CREG_GYRO_TRIM_X = 12,
	CHR_CREG_GYRO_TRIM_Y = 13,
	CHR_CREG_GYRO_TRIM_Z = 14,
	CHR_CREG_MAG_CAL1_1 = 15,
	CHR_CREG_MAG_CAL1_2 = 16,
	CHR_CREG_MAG_CAL1_3 = 17,
	CHR_CREG_MAG_CAL2_1 = 18,
	CHR_CREG_MAG_CAL2_2 = 19,
	CHR_CREG_MAG_CAL2_3 = 20,
	CHR_CREG_MAG_CAL3_1 = 21,
	CHR_CREG_MAG_CAL3_2 = 22,
	CHR_CREG_MAG_CAL3_3 = 23,
	CHR_CREG_MAG_BIAS_X = 24,
	CHR_CREG_MAG_BIAS_Y = 25,
	CHR_CREG_MAG_BIAS_Z = 26,
	//! DREG = Data Register (I believe, that's how they do it in the data sheet)
	CHR_DREG_HEALTH = 85,
	CHR_DREG_GYRO_RAW_XY = 86,
	CHR_DREG_GYRO_RAW_Z = 87,
	CHR_DREG_GYRO_TIME = 88,
	CHR_DREG_ACCEL_RAW_XY = 89,
	CHR_DREG_ACCEL_RAW_Z = 90,
	CHR_DREG_ACCEL_TIME = 91,
	CHR_DREG_MAG_RAW_XY = 92,
	CHR_DREG_MAG_RAW_Z = 93,
	CHR_DREG_MAG_RAW_TIME = 94,
	CHR_DREG_TEMPERATURE = 95,
	CHR_DREG_TEMPERATURE_TIME = 96,
	CHR_DREG_GYRO_PROC_X = 97,
	CHR_DREG_GYRO_PROC_Y = 98,
	CHR_DREG_GYRO_PROC_Z = 99,
	CHR_DREG_GYRO_PROC_TIME = 100,
	CHR_DREG_ACCEL_PROC_X = 101,
	CHR_DREG_ACCEL_PROC_Y = 102,
	CHR_DREG_ACCEL_PROC_Z = 103,
	CHR_DREG_ACCEL_PROC_TIME = 104,
	CHR_DREG_MAG_PROC_X = 105,
	CHR_DREG_MAG_PROC_Y = 106,
	CHR_DREG_MAG_PROC_Z = 107,
	CHR_DREG_MAG_PROC_TIME = 108,
	CHR_DREG_QUAT_AB = 109,
	CHR_DREG_QUAT_CD = 110,
	CHR_DREG_QUAT_TIME = 111,
	CHR_DREG_EULER_PHI_THETA = 112,
	CHR_DREG_EULER_PSI = 113,
	CHR_DREG_EULER_PHI_THETA_DOT = 114,
	CHR_DREG_EULER_PSI_DOT = 115,
	CHR_DREG_EULER_TIME = 116,
	CHR_DREG_POSITION_NORTH = 117,
	CHR_DREG_POSITION_EAST = 118,
	CHR_DREG_POSITION_UP = 119,
	CHR_DREG_POSITION_TIME = 120,
	CHR_DREG_VELOCITY_NORTH = 121,
	CHR_DREG_VELOCITY_EAST = 122,
	CHR_DREG_VELOCITY_UP = 123,
	CHR_DREG_VELOCITY_TIME = 124,
	CHR_DREG_GPS_LATITUDE = 125,
	CHR_DREG_GPS_LONGITUDE = 126,
	CHR_DREG_GPS_ALTITUDE = 127,
	CHR_DREG_GPS_COURSE = 128,
	CHR_DREG_GPS_SPEED = 129,
	CHR_DREG_GPS_TIME = 130,
	CHR_DREG_GPS_SAT_1_2 = 131,
	CHR_DREG_GPS_SAT_3_4 = 132,
	CHR_DREG_GPS_SAT_5_6 = 133,
	CHR_DREG_GPS_SAT_7_8 = 134,
	CHR_DREG_GPS_SAT_9_10 = 135,
	CHR_DREG_GPS_SAT_11_12 = 136,
	//! MISC REGS:
	CHR_MREG_GET_FIRMWARE_VERSION = 170,
	CHR_MREG_FLAST_COMMIT = 171,
	CHR_MREG_RESET_TO_FACTORY = 172,
	CHR_MREG_ZERO_GYROS = 173,
	CHR_MREG_SET_HOME_POSITION = 174,
	CHR_MREG_RESERVED175 = 175,
	CHR_MREG_SET_MAG_REFERENCE = 176,
	CHR_MREG_RESERVED177 = 177,
	CHR_MREG_RESERVED178 = 178,
	CHR_MREG_RESET_EKF = 179
};

//! Simple FIFO: (from Stack Overflow)
#define FIFO_ELEMENTS 8192
#define FIFO_SIZE (FIFO_ELEMENTS + 1)

class SimpleFifo
{
public:
	SimpleFifo(void);
	virtual ~SimpleFifo(void) {}
	int Put(unsigned char n);
	int Get(unsigned char *old);
	int getNumBytes(void);
	int Peek(int peekIndex, unsigned char *val);

private:
	int Fifo[FIFO_SIZE];
	int FifoIn, FifoOut;

	//int totalIn;
	//int totalOut;
};

class chrImuPacketParser
{
public:
	chrImuPacketParser(std::string devicePath, int baudrate=921600);
	virtual ~chrImuPacketParser(void);

	//! Get status of whether the port opened:
	bool portIsOpen(void) { return m_portIsOpen; }

	void setupLogging(const std::string logPath);

	//! Sending functions:
	void sendZeroGyroPacket(void);
	void getFirmwareRevision(void);
	void sendResetEkfPacket(void);

	int ProcessBytes(void);

    void getLastAttitudeSample(attitude_imu& att) {
        pthread_mutex_lock(&m_attitudeMutex);
        att = m_latestAttitudeSample;
        pthread_mutex_unlock(&m_attitudeMutex);
    }

private:
	int ProcessPacket(const unsigned char *pkt, int len);
	ChrPacket m_Packet;

	SimpleFifo rxFifo;
	SerialPortLinux m_serialPort;
	bool m_portIsOpen;
	unsigned char readBuf[MAX_READ_COUNT];
	unsigned long imuSampleCount;
	bool loggingEnabled;
	//fprintf_queue_t * imuLogFile;

	double serialReadTime_ms;

	attitude_imu m_latestAttitudeSample;
#if IMU_USE_PROTHREAD
    prothread_mutex_t m_attitudeMutex;
#else
	pthread_mutex_t m_attitudeMutex;
#endif
};



#endif //CHRIMUPACKETPARSER_H
