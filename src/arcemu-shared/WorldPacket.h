/*
 * ArcEmu MMORPG Server
 * Copyright (C) 2008-2012 <http://www.ArcEmu.org/>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#ifndef WOWSERVER_WORLDPACKET_H
#define WOWSERVER_WORLDPACKET_H

#include "Common.h"
#include "ByteBuffer.h"
#include "StackBuffer.h"

class SERVER_DECL WorldPacket : public ByteBuffer
{
	public:
		__inline WorldPacket() : ByteBuffer(), m_opcode(0) { }
		__inline WorldPacket(uint32 opcode, size_t res) : ByteBuffer(res), m_opcode(opcode) {}
		__inline WorldPacket(size_t res) : ByteBuffer(res), m_opcode(0) { }
		__inline WorldPacket(const WorldPacket & packet) : ByteBuffer(packet), m_opcode(packet.m_opcode) {}

		//! Clear packet and set opcode all in one mighty blow
		__inline void Initialize(uint32 opcode)
		{
			clear();
			m_opcode = opcode;
		}

		__inline uint32 GetOpcode() const { return m_opcode; }
		__inline void SetOpcode(uint32 opcode) { m_opcode = opcode; }

	protected:
		uint32 m_opcode;
};

#define sStackWorldPacket(name, opcode, size) uint8 local_stackbuffer[ size ]; StackWorldPacket name(opcode, local_stackbuffer, size);

template<uint32 Size>
class SERVER_DECL StackWorldPacket : public StackBuffer<Size>
{
		uint32 m_opcode;
	public:
		__inline StackWorldPacket(uint32 opcode) : StackBuffer<Size>(), m_opcode(opcode) { }

		//! Clear packet and set opcode all in one mighty blow
		__inline void Initialize(uint32 opcode)
		{
			StackBuffer<Size>::Clear();
			m_opcode = opcode;
		}

		uint32 GetOpcode() { return m_opcode; }
		__inline void SetOpcode(uint32 opcode) { m_opcode = opcode; }
};

#endif
