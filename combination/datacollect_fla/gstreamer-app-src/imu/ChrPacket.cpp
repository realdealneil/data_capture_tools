/**
ChrPacket.cpp File Description

Author: Neil Johnson (neil.johnson@ssci.com)
Date: 11/13/2015

Purpose:  This is the implementation file for a class that represents a single CH Robotics UM7
    IMU Packet.

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
#include "ChrPacket.h"

//! You can instantiate a new packet without specifying anything else.
ChrPacket::ChrPacket(void)
{
	m_isHiddenAddress = false;
	m_batchLength = 0;
	m_packetLength = 0;
	m_payloadLength = 0;
	m_address = 0;
	m_packetDescriptor = 0;

	m_checksum = 0;
	m_payload = NULL;
}

int ChrPacket::setPacketData(const uint8_t *pkt, int len)
{
	m_packetDescriptor = pkt[3];
	m_address = pkt[4];

	/**
        Check the R/W bit in the packet descriptor.  If it is set, this packet does not contain data:
        (the packet is either reporting that the last write operation was successful, or it is reporting that
        a command finished)
        If the R/W bit is cleared, and the batch bit is cleared, then the packet is 11 bytes long.
        Make sure that the buffer contains enough data to hold a complete packet.
	*/

	if (len < getPacketLength()) {
		//printf("Insufficient bytes for complete packet!  Rejecting packet\n");
		return -CHR_PACKET_ERROR_INSUFFICIENT_BYTES;
	}

	//! This function in turn calls getBatchLength, so after this point,
	//! you can use m_batchLength and m_payloadLength within this class
	//! and get valid information
	computePayloadLength();

	m_payload = &pkt[5];

	if (m_payloadLength > getPacketLength()) {
		printf("  Payload size is %d, total size: %d\n", m_payloadLength, getPacketLength());
		return -CHR_PACKET_ERROR_PAYLOAD_SIZE;
	}

	//! Check the checksum:
	computeChecksum();

	uint16_t packetCheck;
	packetCheck = pkt[5+m_payloadLength] << 8;
	packetCheck |= pkt[6+m_payloadLength];

	if (packetCheck != m_checksum) {
		printf("Checksum Failed: Computed Checksum: %d, packet checksum: %d\n", m_checksum, packetCheck);
		return -CHR_PACKET_ERROR_FAILED_CHECKSUM;
	}

	//! At this point, everything passed checks, so we have succeeded:
	return CHR_PACKET_SUCCESS;
}

ChrPacket::~ChrPacket(void)
{

}

int ChrPacket::getBatchBytes(uint8_t batchIndex, uint8_t bytes[4])
{
	//! Grab the Nth set of bytes from the payload:
	if (m_batchLength == 0) {
		printf("Error: Trying to get a batch from a batchless packet!\n");
		return -CHR_PACKET_ERROR_BATCH_RETRIEVAL;
	}

	if (batchIndex > m_batchLength-1) {
		printf("Error: Going past the edge of the number of batches available!\n");
		return -CHR_PACKET_ERROR_BATCH_RETRIEVAL;
	}

	memcpy(bytes, &m_payload[4*batchIndex], 4);
	return CHR_PACKET_SUCCESS;
}

uint16_t ChrPacket::computeChecksum(void)
{
	uint16_t check = 0;

	check += (uint8_t)'s';
	check += (uint8_t)'n';
	check += (uint8_t)'p';

	check += m_packetDescriptor;
	check += m_address;
	for (int i=0; i<m_payloadLength; i++) {
		check += m_payload[i];
	}

	m_checksum = check;

	return m_checksum;
}
