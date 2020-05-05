/**
ChrPacket.h File Description

Author: Neil Johnson (neil.johnson@ssci.com)
Date: 11/13/2015

Purpose:  This is the header file for a class represents a single CH Robotics UM7 IMU Packet.

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

#ifndef CHR_PACKET_H
#define CHR_PACKET_H

#include <stdint.h>
#include <vector>

#define PKT_COMMAND_FAILED	(0x01)
#define PKT_IS_HIDDEN_ADDR	(0x02)
#define PKT_IS_BATCH		(0x40)
#define PKT_HAS_DATA		(0x80)

/**
    When you fill a packet using setPacketData, it will return either success (0)
    or a negative error from the list below:
 */
enum ChrPacketReturn {
    CHR_PACKET_SUCCESS=0,                       //!< Packet has correct size and passed checksum
    CHR_PACKET_ERROR_INSUFFICIENT_BYTES=1,      //!< We did not receive enough bytes for this packet, and we received the next start bytes.
    CHR_PACKET_ERROR_PAYLOAD_SIZE=2,            //!< The payload indicates a size that is too big for this packet to be valid
    CHR_PACKET_ERROR_FAILED_CHECKSUM=3,         //!< The checksum computed does not match the checksum in the packet indicating serial errors
    CHR_PACKET_ERROR_BATCH_RETRIEVAL=4,         //!< An error occurred when trying to index into the register batch bytes
    CHR_PACKET_ERROR_FLUSH_INPUT,               //!< We needed to flush the input buffer.  This isn't a critical error
};

class ChrPacket
{
public:

	//! You can instantiate a new packet without specifying anything else.  (Useful for sending outgoing packets)
	ChrPacket(void);

	int setPacketData(const uint8_t *packet_bytes, int len);

	virtual ~ChrPacket(void);

	uint16_t computeChecksum(void);

	//! Setters and Getters:
	uint8_t computePayloadLength(void) {
		if (hasData()) {
			if (isBatch()) {
				m_payloadLength = 4*getBatchLength();
				return m_payloadLength;
			} else {
				m_batchLength = 1;
				m_payloadLength = 4;
				return m_payloadLength;
			}
		}
		m_batchLength = 0;
		m_payloadLength = 0;
		return m_payloadLength;
	}

	uint8_t getPayloadLength(void) { return m_payloadLength; }

	uint8_t getAddress(void) { return m_address; }
	void setAddress(uint8_t addr) { m_address = addr; }

	uint8_t getPacketDescriptor(void) { return m_packetDescriptor; }
	void setPacketDescriptor(uint8_t desc) { m_packetDescriptor = desc; }

	uint8_t getBatchLength(void) {
		m_batchLength = (m_packetDescriptor >> 2) & 0x0F;
		return m_batchLength;
	}

	void setBatchLength(uint8_t len) {
		len &= 0x0F;
		m_packetDescriptor &= ~(0x0F << 2);
		m_packetDescriptor |= (len << 2);
		m_batchLength = len;
	}

	uint8_t getPacketLength(void) {
		computePayloadLength();
		return getPayloadLength()+7;
	}

	uint16_t getChecksumFromPacket(void) { return m_checksum; }
	void setChecksumFromPacket(uint16_t check) { m_checksum = check; }

	bool isBatch(void) { return m_packetDescriptor & PKT_IS_BATCH; }
	void setIsBatch(bool isBatch) {
		if (isBatch) {
			m_packetDescriptor |= PKT_IS_BATCH;
		} else {
			m_packetDescriptor &= ~PKT_IS_BATCH;
		}
	}

	bool hasData(void) { return m_packetDescriptor & PKT_HAS_DATA; }
	void setHasData(bool has_data) {
		if (has_data) {
			m_packetDescriptor |= PKT_HAS_DATA;
		} else {
			m_packetDescriptor &= ~PKT_HAS_DATA;
		}
	}

	bool isHiddenAddress(void) { return m_isHiddenAddress; }
	void setHiddenAddress(bool hidden) {
		if (hidden) {
			m_packetDescriptor |= PKT_IS_HIDDEN_ADDR;
		} else {
			m_packetDescriptor &= ~PKT_IS_HIDDEN_ADDR;
		}
	}

	uint8_t getCommandFailed(void) { return m_packetDescriptor & PKT_COMMAND_FAILED; }
	void setCommandFailed(uint8_t failed) {
		failed &= PKT_COMMAND_FAILED;
		m_packetDescriptor &= ~PKT_COMMAND_FAILED;
		m_packetDescriptor |= failed;
	}

	int getBatchBytes(uint8_t batchIndex, uint8_t bytes[4]);

private:
	bool m_isHiddenAddress;
	uint8_t m_batchLength;
	uint8_t m_packetLength;

	uint8_t m_address;
	uint8_t m_packetDescriptor;

	uint16_t m_checksum;
	const uint8_t* m_payload; //! Points to the beginning of the payload data
	uint8_t m_payloadLength;
};

#endif //CHR_PACKET_H
