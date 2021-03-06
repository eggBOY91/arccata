/*
 * ArcEmu MMORPG Server
 * Copyright (C) 2005-2007 Ascent Team <http://www.ascentemu.com/>
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

#include "StdAfx.h"
#include "Unit.h"
using namespace std;

Object::Object() : m_position(0, 0, 0, 0), m_spawnLocation(0, 0, 0, 0)
{
    m_mapId = MAPID_NOT_IN_WORLD;
	m_zoneId = 0;

	m_uint32Values = 0;
	m_objectUpdated = false;

	m_currentSpell = NULL;
	m_valuesCount = 0;

	//official Values
	m_walkSpeed = 2.5f;
	m_runSpeed = 7.0f;
	m_base_runSpeed = m_runSpeed;
	m_base_walkSpeed = m_walkSpeed;

	m_flySpeed = 7.0f;
	m_backFlySpeed = 4.5f;

	m_backWalkSpeed = 4.5f;	// this should really be named m_backRunSpeed
	m_swimSpeed = 4.722222f;
	m_backSwimSpeed = 2.5f;
	m_turnRate = M_PI_FLOAT;

	m_phase = 1; //Set the default phase: 00000000 00000000 00000000 00000001

	m_mapMgr = 0;
	m_mapCell_x = m_mapCell_y = uint32(-1);

	m_faction = NULL;
	m_factionDBC = NULL;

	m_instanceId = INSTANCEID_NOT_IN_WORLD;
	Active = false;
	m_inQueue = false;
	m_extensions = NULL;
	m_loadedFromDB = false;

	m_faction = dbcFactionTemplate.LookupRow(0);
	m_factionDBC = dbcFaction.LookupRow(0);

	m_objectsInRange.clear();
	m_inRangePlayers.clear();
	m_oppFactsInRange.clear();
	m_sameFactsInRange.clear();

	Active = false;
}

Object::~Object()
{
	if(!IsItem())
		ARCEMU_ASSERT(!m_inQueue);

	ARCEMU_ASSERT(!IsInWorld());

	// for linux
	m_instanceId = INSTANCEID_NOT_IN_WORLD;

	if(m_extensions != NULL)
		delete m_extensions;

	if(m_currentSpell)
	{
		m_currentSpell->cancel();
		m_currentSpell = NULL;
	}

	//avoid leaving traces in eventmanager. Have to work on the speed. Not all objects ever had events so list iteration can be skipped
	sEventMgr.RemoveEvents(this);
}


void Object::_Create(uint32 mapid, float x, float y, float z, float ang)
{
	m_mapId = mapid;
	m_position.ChangeCoords(x, y, z, ang);
	m_spawnLocation.ChangeCoords(x, y, z, ang);
	m_lastMapUpdatePosition.ChangeCoords(x, y, z, ang);
}

//!!! This method needs improvements + cleanup

uint32 Object::BuildCreateUpdateBlockForPlayer(ByteBuffer* data, Player* target)
{
	uint16 flags = 0;
	uint32 flags2 = 0;

	uint8 updatetype = 1; // UPDATETYPE_CREATE_OBJECT

	//uint8 temp_objectTypeId = m_objectTypeId;

	switch(m_objectTypeId)
	{
	case TYPEID_CORPSE:
	case TYPEID_DYNAMICOBJECT:
		flags = UPDATEFLAG_HAS_POSITION; // UPDATEFLAG_HAS_STATIONARY_POSITION
		updatetype = 2;
		break;

	case TYPEID_GAMEOBJECT:
		flags = UPDATEFLAG_HAS_POSITION | UPDATEFLAG_ROTATION;
		//flags = 0x0040 | 0x0200; // UPDATEFLAG_HAS_STATIONARY_POSITION | UPDATEFLAG_ROTATION
		updatetype = 2;
		break;

	case TYPEID_UNIT:
	case TYPEID_PLAYER:
		flags = UPDATEFLAG_LIVING;
		updatetype = 2;
		break;
	}


	/*if(IsGameObject())
	{
//		switch( GetByte(GAMEOBJECT_BYTES_1,GAMEOBJECT_BYTES_TYPEID) )
		switch(m_uint32Values[GAMEOBJECT_BYTES_1])
		{
			/*case GAMEOBJECT_TYPE_MO_TRANSPORT:
				{
					if(GetTypeFromGUID() != HIGHGUID_TYPE_TRANSPORTER)
						return 0;   // bad transporter
					else
						flags = 0x0352;
				}
				break;

			case GAMEOBJECT_TYPE_TRANSPORT: // 1
				{
					flags |= 0x0002 | 0x0040 | 0x0200; // UPDATEFLAG_TRANSPORT | UPDATEFLAG_HAS_STATIONARY_POSITION | UPDATEFLAG_ROTATION
				}
				break;

			case GAMEOBJECT_TYPE_TRAP:
			case GAMEOBJECT_TYPE_DUEL_ARBITER:
			case GAMEOBJECT_TYPE_FLAGSTAND:
			case GAMEOBJECT_TYPE_FLAGDROP:
				updatetype = 2; // UPDATETYPE_CREATE_OBJECT2
				break;

			default:
				break;
		}
		//The above 3 checks FAIL to identify transports, thus their flags remain 0x58, and this is BAAAAAAD! Later they don't get position x,y,z,o updates, so they appear randomly by a client-calculated path, they always face north, etc... By: VLack
		//if(flags != 0x0352 && IsGameObject() && TO< GameObject* >(this)->GetInfo()->Type == GAMEOBJECT_TYPE_TRANSPORT && !(TO< GameObject* >(this)->GetOverrides() & GAMEOBJECT_OVERRIDE_PARENTROT))
			//flags = 0x0352;
	}*/

	 /*if(GetTypeFromGUID() == HIGHGUID_TYPE_VEHICLE)
        {
			flags |= UPDATEFLAG_VEHICLE;
			updatetype = 2; // UPDATETYPE_CREATE_OBJECT_SELF
	     }*/

	 
	if(target == this)
	{
		// player creating self
		flags |= UPDATEFLAG_SELF;  // UPDATEFLAG_SELF
		updatetype = 2; // UPDATEFLAG_CREATE_OBJECT_SELF
	}

	if(IsUnit())
	{
		if(TO_UNIT(this)->GetTargetGUID())
			flags |= UPDATEFLAG_HAS_TARGET; // UPDATEFLAG_HAS_ATTACKING_TARGET
	}

	// build our actual update
	*data << updatetype;

	// we shouldn't be here, under any circumstances, unless we have a wowguid..
	ASSERT(m_wowGuid.GetNewGuidLen());
	*data << m_wowGuid;
	
	*data << m_objectTypeId;

	_BuildMovementUpdate(data, flags, flags2, target);

	// we have dirty data, or are creating for ourself.
	UpdateMask updateMask;
	updateMask.SetCount( m_valuesCount );
	_SetCreateBits( &updateMask, target );

	// this will cache automatically if needed
	_BuildValuesUpdate( data, &updateMask, target );

	// update count: 1 ;)
	return 1;

	// end here

	// any other case
	/*switch(m_objectTypeId)
	{
			// items + containers: 0x8
		case TYPEID_ITEM:
		case TYPEID_CONTAINER:
			flags = 0x10;
			break;

			// player/unit: 0x68 (except self)
		case TYPEID_UNIT:
			flags = 0x70;
			break;

		case TYPEID_PLAYER:
			flags = 0x70;
			break;

			// gameobject/dynamicobject
		case TYPEID_GAMEOBJECT:
			flags = 0x0350;
			if(TO< GameObject* >(this)->GetDisplayId() == 3831) flags = 0x0252; //Deeprun Tram proper flags as of 3.2.0.
			break;

		case TYPEID_DYNAMICOBJECT:
			flags = 0x0150;
			break;

		case TYPEID_CORPSE:
			flags = 0x0150;
			break;
			// anyone else can get fucked and die!
	}

	if(target == this)
	{
		// player creating self
		flags |= 0x01;
		updatetype = UPDATETYPE_CREATE_YOURSELF;
	}

	

	// gameobject stuff
	if(IsGameObject())
	{
//		switch( GetByte(GAMEOBJECT_BYTES_1,GAMEOBJECT_BYTES_TYPEID) )
		switch(m_uint32Values[GAMEOBJECT_BYTES_1])
		{
			case GAMEOBJECT_TYPE_MO_TRANSPORT:
				{
					if(GetTypeFromGUID() != HIGHGUID_TYPE_TRANSPORTER)
						return 0;   // bad transporter
					else
						flags = 0x0352;
				}
				break;

			case GAMEOBJECT_TYPE_TRANSPORT:
				{
					/* deeprun tram, etc 
					flags = 0x252;
				}
				break;

			case GAMEOBJECT_TYPE_DUEL_ARBITER:
				{
					// duel flags have to stay as updatetype 3, otherwise
					// it won't animate
					updatetype = UPDATETYPE_CREATE_YOURSELF;
				}
				break;
		}
		//The above 3 checks FAIL to identify transports, thus their flags remain 0x58, and this is BAAAAAAD! Later they don't get position x,y,z,o updates, so they appear randomly by a client-calculated path, they always face north, etc... By: VLack
		if(flags != 0x0352 && IsGameObject() && TO< GameObject* >(this)->GetInfo()->Type == GAMEOBJECT_TYPE_TRANSPORT && !(TO< GameObject* >(this)->GetOverrides() & GAMEOBJECT_OVERRIDE_PARENTROT))
			flags = 0x0352;
	}

	if( IsVehicle() )
		flags |= UPDATEFLAG_VEHICLE;

	// build our actual update
	*data << updatetype;

	// we shouldn't be here, under any circumstances, unless we have a wowguid..
	ARCEMU_ASSERT(m_wowGuid.GetNewGuidLen() > 0);
	*data << m_wowGuid;

	*data << m_objectTypeId;

	_BuildMovementUpdate(data, flags, flags2, target);

	// we have dirty data, or are creating for ourself.
	UpdateMask updateMask;
	updateMask.SetCount(m_valuesCount);
	_SetCreateBits(&updateMask, target);

	if(IsGameObject() && (TO< GameObject* >(this)->GetOverrides() & GAMEOBJECT_OVERRIDE_PARENTROT))
	{
		updateMask.SetBit(GAMEOBJECT_PARENTROTATION_02);
		updateMask.SetBit(GAMEOBJECT_PARENTROTATION_03);
	}

	// this will cache automatically if needed
	_BuildValuesUpdate(data, &updateMask, target);

	// update count: 1 ;)
	return 1;*/
}


//That is dirty fix it actually creates update of 1 field with
//the given value ignoring existing changes in fields and so on
//useful if we want update this field for certain players
//NOTE: it does not change fields. This is also very fast method
WorldPacket* Object::BuildFieldUpdatePacket(uint32 index, uint32 value)
{
	// uint64 guidfields = GetGUID();
	// uint8 guidmask = 0;
	WorldPacket* packet = new WorldPacket(1500);
	packet->SetOpcode(SMSG_UPDATE_OBJECT);

	*packet << (uint32)1;//number of update/create blocks
//	*packet << (uint8)0;//unknown //VLack: removed for 3.1

	*packet << (uint8) UPDATETYPE_VALUES;		// update type == update
	*packet << GetNewGUID();

	uint32 mBlocks = index / 32 + 1;
	*packet << (uint8)mBlocks;

	for(uint32 dword_n = mBlocks - 1; dword_n; dword_n--)
		*packet << (uint32)0;

	*packet << (((uint32)(1)) << (index % 32));
	*packet << value;

	return packet;
}

void Object::BuildFieldUpdatePacket(Player* Target, uint32 Index, uint32 Value)
{
	ByteBuffer buf(500);
	buf << uint8(UPDATETYPE_VALUES);
	buf << GetNewGUID();

	uint32 mBlocks = Index / 32 + 1;
	buf << (uint8)mBlocks;

	for(uint32 dword_n = mBlocks - 1; dword_n; dword_n--)
		buf << (uint32)0;

	buf << (((uint32)(1)) << (Index % 32));
	buf << Value;

	Target->PushUpdateData(&buf, 1);
}

void Object::BuildFieldUpdatePacket(ByteBuffer* buf, uint32 Index, uint32 Value)
{
	*buf << uint8(UPDATETYPE_VALUES);
	*buf << GetNewGUID();

	uint32 mBlocks = Index / 32 + 1;
	*buf << (uint8)mBlocks;

	for(uint32 dword_n = mBlocks - 1; dword_n; dword_n--)
		*buf << (uint32)0;

	*buf << (((uint32)(1)) << (Index % 32));
	*buf << Value;
}

uint32 Object::BuildValuesUpdateBlockForPlayer(ByteBuffer* data, Player* target)
{
	UpdateMask updateMask;
	updateMask.SetCount(m_valuesCount);
	_SetUpdateBits(&updateMask, target);
	for(uint32 x = 0; x < m_valuesCount; ++x)
	{
		if(updateMask.GetBit(x))
		{
			*data << (uint8) UPDATETYPE_VALUES;		// update type == update
			ARCEMU_ASSERT(m_wowGuid.GetNewGuidLen() > 0);
			*data << m_wowGuid;

			_BuildValuesUpdate(data, &updateMask, target);
			return 1;
		}
	}

	return 0;
}

uint32 Object::BuildValuesUpdateBlockForPlayer(ByteBuffer* buf, UpdateMask* mask)
{
	// returns: update count
	// update type == update
	*buf << (uint8) UPDATETYPE_VALUES;

	ARCEMU_ASSERT(m_wowGuid.GetNewGuidLen() > 0);
	*buf << m_wowGuid;

	_BuildValuesUpdate(buf, mask, 0);

	// 1 update.
	return 1;
}

///////////////////////////////////////////////////////////////
/// Build the Movement Data portion of the update packet
/// Fills the data with this object's movement/speed info
/// TODO: rewrite this stuff, document unknown fields and flags
uint32 TimeStamp();

//!!! This method needs improvements

void Object::_BuildMovementUpdate(ByteBuffer* data, uint16 flags, uint32 flags2, Player* target)
{
	// do we need this? yep, we do. haven't implemented it yet
	//!!! implement spline flags
    //!!! implement transport time

	/*ByteBuffer* splinebuf = (m_objectTypeId == TYPEID_UNIT) ? target->GetAndRemoveSplinePacket(GetGUID()) : 0;

	uint32 unkLoopCounter = 0;

	if(splinebuf != NULL)
	{
		flags2 |= 0x00000001;	   // move forward
		if(GetTypeId() == TYPEID_UNIT)
		{
			if(TO_UNIT(this)->GetAIInterface()->HasWalkMode(WALKMODE_WALK))
				flags2 |= 0x00000100; // move walk
		}
	}
	*/
	uint16 moveflags2 = 0; // mostly seem to be used by vehicles to control what kind of movement is allowed
	/*if( IsVehicle() ){
		Unit *u = static_cast< Unit* >( this );
		if( u->GetVehicleComponent() != NULL )
			moveflags2 |= u->GetVehicleComponent()->GetMoveFlags2();

		if( IsCreature() ){
			if( static_cast< Unit* >( this )->HasAuraWithName( SPELL_AURA_ENABLE_FLIGHT ) )
				flags2 |= ( 0x20000000 | 0x02000000 ); // 1 = MOVEFLAG_DISABLE_COLLISION | 2 = MOVEFLAG_AIR_SWIMMING == MOVEFLAG_FLYING
		}

	}*/

		bool hasTransport = transporter_info.guid;
        bool isSplineEnabled = false; // spline not supported
        //bool hasPitch = (flags2 & (0x00100000) || (moveflags2 & 0x0010));
		bool hasPitch = (flags2 & (MOVEFLAG_SWIMMING | MOVEFLAG_AIR_SWIMMING)) || (moveflags2 & MOVEFLAG2_ALLOW_PITCHING); // (hasPitch == swimming) flags2 & (MOVEFLAG_SWIMMING | MOVEFLAG_AIR_SWIMMING)) || (moveflags2 & MOVEFLAG2_ALLOW_PITCHING)
        bool haveFallData = flags2 & MOVEFLAG2_INTERPOLATED_TURNING;
        bool hasFallDirection = flags & MOVEFLAG_FALLING;
        bool hasElevation = false; //flags & 0x02000000; // MOVEFLAG_SPLINE_ELEVATION, I think flags(1); 2: spline not supported/enabled
        bool hasOrientation = GetTypeId() != TYPEID_ITEM && GetTypeId() != TYPEID_CONTAINER;

	data->writeBit(0);
    data->writeBit(0);
    data->writeBit(flags & UPDATEFLAG_ROTATION); // UPDATEFLAG_HAS_GO_ROTATION
    data->writeBit(flags & UPDATEFLAG_HAS_ANIMKITS); // UPDATEFLAG_HAS_ANIMKITS                                  
    data->writeBit(flags & UPDATEFLAG_HAS_TARGET); // UPDATEFLAG_HAS_ATTACKING_TARGET
    data->writeBit(flags & UPDATEFLAG_SELF); // UPDATEFLAG_HAS_SELF
    data->writeBit(flags & UPDATEFLAG_VEHICLE); // UPDATEFLAG_HAS_VEHICLE
    data->writeBit(flags & UPDATEFLAG_LIVING); // UPDATEFLAG_HAS_LIVING
    data->writeBits(0, 24);                                              
    data->writeBit(0);
    data->writeBit(flags & UPDATEFLAG_POSITION); // UPDATEFLAG_HAS_GO_POSITION
    data->writeBit(flags & UPDATEFLAG_HAS_POSITION); // UPDATEFLAG_HAS_STATIONARY_POSITION
    data->writeBit(flags & UPDATEFLAG_UNK5); // UPDATEFLAG_TRANSPORT_ARR
    data->writeBit(0);
    data->writeBit(flags & UPDATEFLAG_TRANSPORT); // UPDATEFLAG_TRANSPORT

	Player* pThis = NULL;
	MovementInfo* moveinfo = NULL;
	if(IsPlayer())
	{
		pThis = TO< Player* >(this);
		if(pThis->GetSession())
			moveinfo = pThis->GetSession()->GetMovementInfo();
		if(target == this)
		{
			// Updating our last speeds.
			pThis->UpdateLastSpeeds();
		}
	}
	Creature* uThis = NULL;
	if(IsCreature())
		uThis = TO< Creature* >(this);

	if(flags & UPDATEFLAG_LIVING)  // UPDATEFLAG_LIVING
	{
		if(pThis && pThis->transporter_info.guid != 0)
			flags2 |= MOVEFLAG_TRANSPORT; // MOVEFLAG_TRANSPORT
		else if(uThis != NULL && transporter_info.guid != 0 && uThis->transporter_info.guid != 0)
			flags2 |= MOVEFLAG_TRANSPORT; // MOVEFLAG_TRANSPORT

		if( ( pThis != NULL ) && pThis->isRooted() )
			flags2 |= MOVEFLAG_ROOTED; // MOVEFLAG_ROOTED
		else if( ( uThis != NULL ) && uThis->isRooted() )
			flags2 |= MOVEFLAG_ROOTED; // MOVEFLAG_ROOTED

		// flags      - update flags
		// flags2     -  movement flags
		// moveflags2 - movement flags for vehicles

		if(uThis != NULL)
		{
			if(uThis->GetAIInterface()->IsFlying())
				flags2 |= MOVEFLAG_AIR_SWIMMING; // MOVEFLAG_FLYING
			if(uThis->GetAIInterface()->onGameobject)
				flags2 |= MOVEFLAG_ROOTED; // MOVEFLAG_ROOTED
			if(uThis->GetProto()->extra_a9_flags)
			{
// wtf
//do not send shit we can't honor
//#define UNKNOWN_FLAGS2 ( 0x00002000 | 0x04000000 | 0x08000000 )
//#define UNKNOWN_FLAGS2 ( 0x0200 | 0x00002000 | 0x00001000 | 0x00200000 | 0x04000000 | 0x08000000 )
				//uint32 inherit = uThis->GetProto()->extra_a9_flags & (~UNKNOWN_FLAGS2);
				//flags2 |= inherit;
			}
		}


		data->writeBit(!flags2); // movement flags
		data->writeBit(hasOrientation); // hasO or !hasO?

		// get the player's guid into a uint8 array (8 * 8 = 64)
		uint8 plrGuid[8];
		*(uint64*)plrGuid = GetGUID();

		data->writeBit(plrGuid[7]);
		data->writeBit(plrGuid[3]);
		data->writeBit(plrGuid[2]);

		if (flags2)
			data->writeBits(flags2, 30);

		data->writeBit(0); 
        data->writeBit(!hasPitch);
        data->writeBit(isSplineEnabled);
        data->writeBit(haveFallData);
        data->writeBit(!hasElevation);
        data->writeBit(plrGuid[5]);
        //data->writeBit(transporter_info.guid); // hasTransport | do we need pThis->?
		data->writeBit(flags2 & MOVEFLAG_TRANSPORT); // MOVEFLAG_TRANSPORT
        data->writeBit(0);  // true or false?

		//if (hasTransport)
		if(flags2 & MOVEFLAG_TRANSPORT) // MOVEFLAG_TRANSPORT
		//if(transporter_info.guid)
		{
            uint8 transGuid[8];
			*(uint64*)transGuid = transporter_info.guid;

			data->writeBit(transGuid[1]);
            data->writeBit(0);                                  // Has transport time 2 = false
            data->writeBit(transGuid[4]);
            data->writeBit(transGuid[0]);
            data->writeBit(transGuid[6]);
            data->writeBit(0);                                  // Has transport time 3 = false
            data->writeBit(transGuid[7]);
            data->writeBit(transGuid[5]);
            data->writeBit(transGuid[3]);
            data->writeBit(transGuid[2]);
		}

		data->writeBit(plrGuid[4]);

		if (isSplineEnabled) // false
		{
			//data->append(*splinebuf);
			//delete splinebuf;
		}

		data->writeBit(plrGuid[6]);

		if (haveFallData)
			data->writeBit(hasFallDirection);

		data->writeBit(plrGuid[0]);
        data->writeBit(plrGuid[1]);
        data->writeBit(0);       // unknown 4.3.3
        data->writeBit(!moveflags2); // moveflags2 are movement flags for vehicles
        if (moveflags2)
            data->writeBits(moveflags2, 12);

		} // flags & UPDATEFLAG_LIVING __END__

		if (flags & UPDATEFLAG_POSITION) // UPDATEFLAG_HAS_GO_POSITION
		{
			uint8 transGuid[8];
			*(uint64*)transGuid = transporter_info.guid;

			data->writeBit(transGuid[5]);
			data->writeBit(0); // Has GO transport time 3 - false
			data->writeBit(transGuid[0]);
		    data->writeBit(transGuid[3]);
		    data->writeBit(transGuid[6]);
			data->writeBit(transGuid[1]);
			data->writeBit(transGuid[4]);
			data->writeBit(transGuid[2]);
			data->writeBit(0); // Has GO transport time 2 - false
			data->writeBit(transGuid[7]);
		}

		if (flags & UPDATEFLAG_HAS_TARGET) // UPDATEFLAG_HAS_ATTACKING_TARGET / UPDATEFLAG_HAS_TARGET
		{
            if (IsUnit())
			{
			uint8 victimGuid[8];
			*(uint64*)victimGuid = TO_UNIT(this)->GetTargetGUID();

			data->writeBit(victimGuid[2]);
			data->writeBit(victimGuid[7]);
			data->writeBit(victimGuid[0]);
			data->writeBit(victimGuid[4]);
            data->writeBit(victimGuid[5]);
            data->writeBit(victimGuid[6]);
            data->writeBit(victimGuid[1]);
			data->writeBit(victimGuid[3]);
			}
			else
				//data->writeBit(uint64(0));
				data->writeBits(0, 8); // i think so
		}

		if (flags & UPDATEFLAG_HAS_ANIMKITS) // UPDATEFLAG_HAS_ANIMKITS
		{
			data->writeBit(1); // hasAnimKit0 - false
			data->writeBit(1); // hasAnimKit1 - false
			data->writeBit(1); // hasAnimKit2 - false
		}

		data->flushBits();

		 // Data
    //for (uint32 i = 0; i < unkLoopCounter; ++i)
        //*data << uint32(0);

		if (flags & UPDATEFLAG_LIVING) // UPDATEFLAG_HAS_LIVING
		{
		uint8 plrGuid[8];
		*(uint64*)plrGuid = GetGUID();

		data->WriteByteSeq(plrGuid[4]);

		*data << m_backWalkSpeed;

		if (haveFallData)
		{
			if (hasFallDirection) // is this alright?
			{
				*data << (float)0;
				*data << (float)0;
				*data << (float)1.0;
			}
			
			*data << (uint32)0; // fall time - uint32 or float
			*data << (float)0; // velocity
		}

		*data << m_backSwimSpeed;

		// not sure about these two
		//if (hasElevation)
			//*data << (float)0;

		/*if (isSplineEnabled)
			*data << (uint32)0;*/

		*data << m_position.z;
		data->WriteByteSeq(plrGuid[5]); // byte seq?

		//if (hasTransport)
		if(flags2 & MOVEFLAG_TRANSPORT) // MOVEFLAG_TRANSPORT
		//if(transporter_info.guid)
		{
			uint8 transGuid[8];
			*(uint64*)transGuid = transporter_info.guid;

			data->WriteByteSeq(transGuid[5]);
            data->WriteByteSeq(transGuid[7]);
            *data << (uint32)0; // transport time
			*data << transporter_info.o;
			//*data << (uint32)0;
			// if (hasTransportTime2) *data << (uint32)0;
			*data << transporter_info.y;
			*data << transporter_info.x;
			data->WriteByteSeq(transGuid[3]);
			*data << transporter_info.z;
			data->WriteByteSeq(transGuid[0]);
			//*data << (uint32)0;
			// if (hasTransportTime3) *data << (uint32)0;
			*data << transporter_info.seat;
			data->WriteByteSeq(transGuid[1]);
            data->WriteByteSeq(transGuid[6]);
            data->WriteByteSeq(transGuid[2]);
            data->WriteByteSeq(transGuid[4]);
		}

		*data << m_position.x;
		*data << float(7); // pitch rate - is this right?
		data->WriteByteSeq(plrGuid[3]);
		data->WriteByteSeq(plrGuid[0]);
		*data << m_swimSpeed;
		*data << m_position.y;
		data->WriteByteSeq(plrGuid[7]);
        data->WriteByteSeq(plrGuid[1]);
        data->WriteByteSeq(plrGuid[2]);
        
		if(m_walkSpeed == 0)
			*data << 8.0f;
		else
			*data << m_walkSpeed;	// walk speed

		*data << uint32(getMSTime());
		*data << m_turnRate;
		data->WriteByteSeq(plrGuid[6]);
		
		if(m_flySpeed == 0)
			*data << 8.0f;
		else
			*data << m_flySpeed;	// fly speed

		if (!hasOrientation) // do we need this check here?
			*data << m_position.o;

			if(m_runSpeed == 0)
			*data << 8.0f;
		else
			*data << m_runSpeed;	// run speed

		if(hasPitch) // this ok? // and this check here?
			*data << moveinfo->pitch;

		*data << m_backFlySpeed;

    } // has_living {2} _END_

		if( flags & UPDATEFLAG_VEHICLE ) // UPDATEFLAG_HAS_VEHICLE
		{
		uint32 vehicleid = 0;

		if( IsCreature() )
			vehicleid = TO< Creature* >( this )->GetProto()->vehicleid;
		else
		if( IsPlayer() )
			vehicleid = TO< Player* >( this )->mountvehicleid;

		*data << float( GetOrientation() );
		*data << uint32( vehicleid );
	}

		if (flags & UPDATEFLAG_POSITION) // UPDATEFLAG_GO_TRANSPORT_POSITION - is this the correct flag?
		{
			uint8 transGuid[8];
			*(uint64*)transGuid = transporter_info.guid;

			data->WriteByteSeq(transGuid[0]);
			data->WriteByteSeq(transGuid[5]);
			// if has transport time 3
			//*data << uint32(0);
			//*data << uint32(0);
			data->WriteByteSeq(transGuid[3]);
			*data << float(transporter_info.x);
			data->WriteByteSeq(transGuid[4]);
			data->WriteByteSeq(transGuid[6]);
			data->WriteByteSeq(transGuid[1]);
			*data << (uint32)0; // transporter time.. do we have this?
			*data << float(transporter_info.y);
			data->WriteByteSeq(transGuid[2]);
			data->WriteByteSeq(transGuid[7]);
			*data << float(transporter_info.z);
			*data << int8(transporter_info.seat);
		    *data << float(transporter_info.o);
			// if has transport time 2
			//*data << uint32(0);
			//*data << uint32(0);
		}

		if(flags & UPDATEFLAG_ROTATION) // UPDATEFLAG_ROTATION
	    {
		if(IsGameObject())
			*data << TO< GameObject* >(this)->m_rotation;
	    }

		if (flags & UPDATEFLAG_UNK5) // UPDATEFLAG_UNK5
		{
	    *data << float(0.0f);
        *data << float(0.0f);
        *data << float(0.0f);
        *data << float(0.0f);
        *data << uint8(0);
        *data << float(0.0f);
        *data << float(0.0f);
        *data << float(0.0f);
        *data << float(0.0f);
        *data << float(0.0f);
        *data << float(0.0f);
        *data << float(0.0f);
        *data << float(0.0f);
        *data << float(0.0f);
        *data << float(0.0f);
        *data << float(0.0f);
        *data << float(0.0f);
		}

		if(flags & UPDATEFLAG_HAS_POSITION) // UPDATEFLAG_HAS_STATIONARY_POSITION
		{
			*data << (float)m_position.o;
			*data << (float)m_position.x;
			*data << (float)m_position.y;
			*data << (float)m_position.z;
		}

		if(flags & UPDATEFLAG_HAS_TARGET) // UPDATEFLAG_HAS_TARGET
	{
		if(IsUnit())
		{
			uint8 victimGuid[8];
		*(uint64*)victimGuid = TO_UNIT(this)->GetTargetGUID();

		data->WriteByteSeq(victimGuid[4]);
        data->WriteByteSeq(victimGuid[0]);
        data->WriteByteSeq(victimGuid[3]);
        data->WriteByteSeq(victimGuid[5]);
        data->WriteByteSeq(victimGuid[7]);
        data->WriteByteSeq(victimGuid[6]);
        data->WriteByteSeq(victimGuid[2]);
        data->WriteByteSeq(victimGuid[1]);
		}
		else
			for (int i = 0; i < 8; i++)
				*data << uint8(0);
		}

		if(flags & UPDATEFLAG_TRANSPORT) // UPDATEFLAG_TRANSPORT
		{
			*data << uint32(getMSTime()); // I don't think it's MSTime here
		}
}

//=======================================================================================
//  Creates an update block with the values of this object as
//  determined by the updateMask.
//=======================================================================================
void Object::_BuildValuesUpdate(ByteBuffer* data, UpdateMask* updateMask, Player* target)
{
	bool activate_quest_object = false;
	bool reset = false;
	uint32 oldflags = 0;

	if(updateMask->GetBit(OBJECT_FIELD_GUID) && target)	   // We're creating.
	{
		if(IsCreature())
		{
			Creature* pThis = TO< Creature* >(this);
			if(pThis->IsTagged() && (pThis->loot.gold || pThis->loot.items.size()))
			{
				// Let's see if we're the tagger or not.
				oldflags = m_uint32Values[UNIT_DYNAMIC_FLAGS];
				uint32 Flags = m_uint32Values[UNIT_DYNAMIC_FLAGS];
				uint32 oldFlags = 0;

				if(pThis->GetTaggerGUID() == target->GetGUID())
				{
					// Our target is our tagger.
					oldFlags = U_DYN_FLAG_TAGGED_BY_OTHER;

					if(Flags & U_DYN_FLAG_TAGGED_BY_OTHER)
						Flags &= ~oldFlags;

					if(!(Flags & U_DYN_FLAG_LOOTABLE) && pThis->HasLootForPlayer(target))
						Flags |= U_DYN_FLAG_LOOTABLE;
				}
				else
				{
					// Target is not the tagger.
					oldFlags = U_DYN_FLAG_LOOTABLE;

					if(!(Flags & U_DYN_FLAG_TAGGED_BY_OTHER))
						Flags |= U_DYN_FLAG_TAGGED_BY_OTHER;

					if(Flags & U_DYN_FLAG_LOOTABLE)
						Flags &= ~oldFlags;
				}

				m_uint32Values[UNIT_DYNAMIC_FLAGS] = Flags;

				updateMask->SetBit(UNIT_DYNAMIC_FLAGS);

				reset = true;
			}
		}

		if(target && IsGameObject())
		{
			GameObject* go = TO_GAMEOBJECT(this);
			QuestLogEntry* qle;
			GameObjectInfo* info;
			if(go->HasQuests())
			{
				std::list<QuestRelation*>::iterator itr;
				for(itr = go->QuestsBegin(); itr != go->QuestsEnd(); ++itr)
				{
					QuestRelation* qr = (*itr);
					if(qr != NULL)
					{
						Quest* qst = qr->qst;
						if(qst != NULL)
						{
							if((qr->type & QUESTGIVER_QUEST_START && !target->HasQuest(qst->id))
							        ||	(qr->type & QUESTGIVER_QUEST_END && target->HasQuest(qst->id))
							  )
							{
								activate_quest_object = true;
								break;
							}
						}
					}
				}
			}
			else
			{
				info = go->GetInfo();
				if(info && (info->goMap.size() || info->itemMap.size()))
				{
					for(GameObjectGOMap::iterator itr = go->GetInfo()->goMap.begin(); itr != go->GetInfo()->goMap.end(); ++itr)
					{
						qle = target->GetQuestLogForEntry(itr->first->id);
						if(qle != NULL)
						{
							if(qle->GetQuest()->count_required_mob == 0)
								continue;
							for(uint32 i = 0; i < 4; ++i)
							{
								if(qle->GetQuest()->required_mob[i] == static_cast<int32>(go->GetEntry()) && qle->GetMobCount(i) < qle->GetQuest()->required_mobcount[i])
								{
									activate_quest_object = true;
									break;
								}
							}
							if(activate_quest_object)
								break;
						}
					}

					if(!activate_quest_object)
					{
						for(GameObjectItemMap::iterator itr = go->GetInfo()->itemMap.begin();
						        itr != go->GetInfo()->itemMap.end();
						        ++itr)
						{
							for(std::map<uint32, uint32>::iterator it2 = itr->second.begin();
							        it2 != itr->second.end();
							        ++it2)
							{
								if((qle = target->GetQuestLogForEntry(itr->first->id)) != 0)
								{
									if(target->GetItemInterface()->GetItemCount(it2->first) < it2->second)
									{
										activate_quest_object = true;
										break;
									}
								}
							}
							if(activate_quest_object)
								break;
						}
					}
				}
			}
		}
	}


	if(activate_quest_object)
	{
		oldflags = m_uint32Values[GAMEOBJECT_DYNAMIC];
		if(!updateMask->GetBit(GAMEOBJECT_DYNAMIC))
			updateMask->SetBit(GAMEOBJECT_DYNAMIC);
		m_uint32Values[GAMEOBJECT_DYNAMIC] = 1 | 8; // 8 to show sparkles
		reset = true;
	}

	ARCEMU_ASSERT(updateMask && updateMask->GetCount()  == m_valuesCount);
	uint32 bc;
	uint32 values_count;
	if(m_valuesCount > (2 * 0x20))    //if number of blocks > 2->  unit and player+item container
	{
		bc = updateMask->GetUpdateBlockCount();
		values_count = min<uint32>(bc * 32, m_valuesCount);

	}
	else
	{
		bc = updateMask->GetBlockCount();
		values_count = m_valuesCount;
	}

	*data << (uint8)bc;
	data->append(updateMask->GetMask(), bc * 4);

	for(uint32 index = 0; index < values_count; index ++)
	{
		if(updateMask->GetBit(index))
		{
			*data << m_uint32Values[ index ];
		}
	}

	if(reset)
	{
		switch(GetTypeId())
		{
			case TYPEID_UNIT:
				m_uint32Values[UNIT_DYNAMIC_FLAGS] = oldflags;
				break;
			case TYPEID_GAMEOBJECT:
				m_uint32Values[GAMEOBJECT_DYNAMIC] = oldflags;
				break;
		}
	}
}

void Object::BuildHeartBeatMsg(WorldPacket* data) const
{
	data->Initialize(MSG_MOVE_HEARTBEAT);

	*data << GetGUID();

	*data << uint32(0); // flags
//	*data << uint32(0); // mysterious value #1
	*data << getMSTime();
	*data << m_position;
	*data << m_position.o;
}

bool Object::SetPosition(const LocationVector & v, bool allowPorting /* = false */)
{
	bool updateMap = false, result = true;

	if(m_position.x != v.x || m_position.y != v.y)
		updateMap = true;

	m_position = const_cast<LocationVector &>(v);

	if(!allowPorting && v.z < -500)
	{
		m_position.z = 500;
		LOG_ERROR("setPosition: fell through map; height ported");

		result = false;
	}

	if(IsInWorld() && updateMap)
	{
		m_mapMgr->ChangeObjectLocation(this);
	}

	return result;
}

bool Object::SetPosition(float newX, float newY, float newZ, float newOrientation, bool allowPorting)
{
	bool updateMap = false, result = true;

	ARCEMU_ASSERT(!isnan(newX) && !isnan(newY) && !isnan(newOrientation));

	//It's a good idea to push through EVERY transport position change, no matter how small they are. By: VLack aka. VLsoft
	if(IsGameObject() && TO< GameObject* >(this)->GetInfo()->Type == GAMEOBJECT_TYPE_TRANSPORT)
		updateMap = true;

	//if (m_position.x != newX || m_position.y != newY)
	//updateMap = true;
	if(m_lastMapUpdatePosition.Distance2DSq(newX, newY) > 4.0f)		/* 2.0f */
		updateMap = true;

	m_position.ChangeCoords(newX, newY, newZ, newOrientation);

	if(!allowPorting && newZ < -500)
	{
		m_position.z = 500;
		LOG_ERROR("setPosition: fell through map; height ported");

		result = false;
	}

	if(IsInWorld() && updateMap)
	{
		m_lastMapUpdatePosition.ChangeCoords(newX, newY, newZ, newOrientation);
		m_mapMgr->ChangeObjectLocation(this);

		if(IsPlayer() && TO< Player* >(this)->GetGroup() && TO< Player* >(this)->m_last_group_position.Distance2DSq(m_position) > 25.0f)       // distance of 5.0
		{
			TO< Player* >(this)->GetGroup()->HandlePartialChange(PARTY_UPDATE_FLAG_POSITION, TO< Player* >(this));
		}
	}

	if( IsUnit() ){
		Unit *u = static_cast< Unit* >( this );
		if( u->GetVehicleComponent() != NULL )
			u->GetVehicleComponent()->MovePassengers( newX, newY, newZ, newOrientation );
	}

	return result;
}

////////////////////////////////////////////////////////////////////////////
/// Fill the object's Update Values from a space delimitated list of values.
void Object::LoadValues(const char* data)
{
	// thread-safe ;) strtok is not.
	std::string ndata = data;
	std::string::size_type last_pos = 0, pos = 0;
	uint32 index = 0;
	uint32 val;
	do
	{
		// prevent overflow
		if(index >= m_valuesCount)
		{
			break;
		}
		pos = ndata.find(" ", last_pos);
		val = atol(ndata.substr(last_pos, (pos - last_pos)).c_str());
		if(m_uint32Values[index] == 0)
			m_uint32Values[index] = val;
		last_pos = pos + 1;
		++index;
	}
	while(pos != std::string::npos);
}

void Object::_SetUpdateBits(UpdateMask* updateMask, Player* target) const
{
	*updateMask = m_updateMask;
}


void Object::_SetCreateBits(UpdateMask* updateMask, Player* target) const
{

	for(uint32 i = 0; i < m_valuesCount; ++i)
		if(m_uint32Values[i] != 0)
			updateMask->SetBit(i);
}

void Object::AddToWorld()
{
	MapMgr* mapMgr = sInstanceMgr.GetInstance(this);
	if(mapMgr == NULL)
	{
		LOG_ERROR("AddToWorld() failed for Object with GUID " I64FMT " MapId %u InstanceId %u", GetGUID(), GetMapId(), GetInstanceID());
		return;
	}

	if(IsPlayer())
	{
		Player* plr = TO< Player* >(this);
		if(mapMgr->pInstance != NULL && !plr->HasFlag(PLAYER_FLAGS, PLAYER_FLAG_GM))
		{
			// Player limit?
			if(mapMgr->GetMapInfo()->playerlimit && mapMgr->GetPlayerCount() >= mapMgr->GetMapInfo()->playerlimit)
				return;
			Group* group = plr->GetGroup();
			// Player in group?
			if(group == NULL && mapMgr->pInstance->m_creatorGuid == 0)
				return;
			// If set: Owns player the instance?
			if(mapMgr->pInstance->m_creatorGuid != 0 && mapMgr->pInstance->m_creatorGuid != plr->GetLowGUID())
				return;

			if(group != NULL)
			{
				// Is instance empty or owns our group the instance?
				if(mapMgr->pInstance->m_creatorGroup != 0 && mapMgr->pInstance->m_creatorGroup != group->GetID())
				{
					// Player not in group or another group is already playing this instance.
					sChatHandler.SystemMessageToPlr(plr, "Another group is already inside this instance of the dungeon.");
					if(plr->GetSession()->GetPermissionCount() > 0)
						sChatHandler.BlueSystemMessageToPlr(plr, "Enable your GameMaster flag to ignore this rule.");
					return;
				}
				else if(mapMgr->pInstance->m_creatorGroup == 0)
					// Players group now "owns" the instance.
					mapMgr->pInstance->m_creatorGroup = group->GetID();
			}
		}
	}

	m_mapMgr = mapMgr;
	m_inQueue = true;

	// correct incorrect instance id's
	m_instanceId = m_mapMgr->GetInstanceID();
	m_mapId = m_mapMgr->GetMapId();
	mapMgr->AddObject(this);
}

void Object::AddToWorld(MapMgr* pMapMgr)
{
	if(!pMapMgr || (pMapMgr->GetMapInfo()->playerlimit && this->IsPlayer() && pMapMgr->GetPlayerCount() >= pMapMgr->GetMapInfo()->playerlimit))
		return; //instance add failed

	m_mapMgr = pMapMgr;
	m_inQueue = true;

	pMapMgr->AddObject(this);

	// correct incorrect instance id's
	m_instanceId = pMapMgr->GetInstanceID();
	m_mapId = m_mapMgr->GetMapId();
}

//Unlike addtoworld it pushes it directly ignoring add pool
//this can only be called from the thread of mapmgr!!!
void Object::PushToWorld(MapMgr* mgr)
{
	ARCEMU_ASSERT(t_currentMapContext.get() == mgr);

	if(mgr == NULL)
	{
		LOG_ERROR("Invalid push to world of Object " I64FMT, GetGUID());
		return; //instance add failed
	}

	m_mapId = mgr->GetMapId();
	//note: there's no need to set the InstanceId before calling PushToWorld() because it's already set here.
	m_instanceId = mgr->GetInstanceID();

	if(IsPlayer())
	{
		TO_PLAYER(this)->m_cache->SetInt32Value(CACHE_MAPID, m_mapId);
		TO_PLAYER(this)->m_cache->SetInt32Value(CACHE_INSTANCEID, m_instanceId);
	}

	m_mapMgr = mgr;
	OnPrePushToWorld();

	mgr->PushObject(this);

	// correct incorrect instance id's
	m_inQueue = false;

	event_Relocate();

	// call virtual function to handle stuff.. :P
	OnPushToWorld();
}

//! Remove object from world
void Object::RemoveFromWorld(bool free_guid)
{
	ARCEMU_ASSERT(m_mapMgr != NULL);

	OnPreRemoveFromWorld();

	MapMgr* m = m_mapMgr;
	m_mapMgr = NULL;

	m->RemoveObject(this, free_guid);

	OnRemoveFromWorld();

	std::set<Spell*>::iterator itr, itr2;
	Spell* sp;
	for(itr = m_pendingSpells.begin(); itr != m_pendingSpells.end();)
	{
		itr2 = itr++;
		sp = *itr2;
		//if the spell being deleted is the same being casted, Spell::cancel will take care of deleting the spell
		//if it's not the same removing us from world. Otherwise finish() will delete the spell once all SpellEffects are handled.
		if(sp == m_currentSpell)
			sp->cancel();
		else
			delete sp;
	}
	//shouldnt need to clear, spell destructor will erase
	//m_pendingSpells.clear();

	m_instanceId = INSTANCEID_NOT_IN_WORLD;
	m_mapId = MAPID_NOT_IN_WORLD;
	//m_inQueue is set to true when AddToWorld() is called. AddToWorld() queues the Object to be pushed, but if it's not pushed and RemoveFromWorld()
	//is called, m_inQueue will still be true even if the Object is no more inworld, nor queued.
	m_inQueue = false;

	// update our event holder
	event_Relocate();
}



//! Set uint32 property
void Object::SetUInt32Value(const uint32 index, const uint32 value)
{
	ARCEMU_ASSERT(index < m_valuesCount);
	//! Save updating when val isn't changing.
	if(m_uint32Values[index] == value)
		return;

	m_uint32Values[ index ] = value;

	if(IsInWorld())
	{
		m_updateMask.SetBit(index);

		if(!m_objectUpdated)
		{
			m_mapMgr->ObjectUpdated(this);
			m_objectUpdated = true;
		}
	}

	//! Group update handling
	if(IsPlayer())
	{
		TO_PLAYER(this)->HandleUpdateFieldChanged(index);
		if(IsInWorld())
		{
			Group* pGroup = TO< Player* >(this)->GetGroup();
			if(pGroup != NULL)
				pGroup->HandleUpdateFieldChange(index, TO< Player* >(this));
		}

		switch(index)
		{
			case UNIT_FIELD_POWER1:
			case UNIT_FIELD_POWER2:
			case UNIT_FIELD_POWER4:
			case UNIT_FIELD_POWER5:
				TO< Unit* >(this)->SendPowerUpdate(true);
				break;
			default:
				break;
		}
	}
	else if(IsCreature())
	{
		switch(index)
		{
			case UNIT_FIELD_POWER1:
			case UNIT_FIELD_POWER2:
			case UNIT_FIELD_POWER3:
			case UNIT_FIELD_POWER4:
			case UNIT_FIELD_POWER5:
				TO_CREATURE(this)->SendPowerUpdate(false);
				break;
			default:
				break;
		}
	}
}

uint32 Object::GetModPUInt32Value(const uint32 index, const int32 value)
{
	ARCEMU_ASSERT(index < m_valuesCount);
	int32 basevalue = (int32)m_uint32Values[ index ];
	return ((basevalue * value) / 100);
}

void Object::ModUnsigned32Value(uint32 index, int32 mod)
{
	ARCEMU_ASSERT(index < m_valuesCount);
	if(mod == 0)
		return;

	m_uint32Values[ index ] += mod;
	if((int32)m_uint32Values[index] < 0)
		m_uint32Values[index] = 0;

	if(IsInWorld())
	{
		m_updateMask.SetBit(index);

		if(!m_objectUpdated)
		{
			m_mapMgr->ObjectUpdated(this);
			m_objectUpdated = true;
		}
	}

	if(IsPlayer())
	{
		switch(index)
		{
			case UNIT_FIELD_POWER1:
			case UNIT_FIELD_POWER2:
			case UNIT_FIELD_POWER4:
			case UNIT_FIELD_POWER5:
				TO< Unit* >(this)->SendPowerUpdate(true);
				break;
			default:
				break;
		}
	}
	else if(IsCreature())
	{
		switch(index)
		{
			case UNIT_FIELD_POWER1:
			case UNIT_FIELD_POWER2:
			case UNIT_FIELD_POWER3:
			case UNIT_FIELD_POWER4:
			case UNIT_FIELD_POWER5:
				TO_CREATURE(this)->SendPowerUpdate(false);
				break;
			default:
				break;
		}
	}
}

void Object::ModSignedInt32Value(uint32 index, int32 value)
{
	ARCEMU_ASSERT(index < m_valuesCount);
	if(value == 0)
		return;

	m_uint32Values[ index ] += value;
	if(IsInWorld())
	{
		m_updateMask.SetBit(index);

		if(!m_objectUpdated)
		{
			m_mapMgr->ObjectUpdated(this);
			m_objectUpdated = true;
		}
	}
}

void Object::ModFloatValue(const uint32 index, const float value)
{
	ARCEMU_ASSERT(index < m_valuesCount);
	m_floatValues[ index ] += value;

	if(IsInWorld())
	{
		m_updateMask.SetBit(index);

		if(!m_objectUpdated)
		{
			m_mapMgr->ObjectUpdated(this);
			m_objectUpdated = true;
		}
	}
}
void Object::ModFloatValueByPCT(const uint32 index, int32 byPct)
{
	ARCEMU_ASSERT(index < m_valuesCount);
	if(byPct > 0)
		m_floatValues[ index ] *= 1.0f + byPct / 100.0f;
	else
		m_floatValues[ index ] /= 1.0f - byPct / 100.0f;


	if(IsInWorld())
	{
		m_updateMask.SetBit(index);

		if(!m_objectUpdated)
		{
			m_mapMgr->ObjectUpdated(this);
			m_objectUpdated = true;
		}
	}
}

//! Set uint64 property
void Object::SetUInt64Value(const uint32 index, const uint64 value)
{
	ARCEMU_ASSERT(index + 1 < m_valuesCount);

	uint64* p = reinterpret_cast< uint64* >(&m_uint32Values[ index ]);

	if(*p == value)
		return;
	else
		*p = value;

	if(IsInWorld())
	{
		m_updateMask.SetBit(index);
		m_updateMask.SetBit(index + 1);

		if(!m_objectUpdated)
		{
			m_mapMgr->ObjectUpdated(this);
			m_objectUpdated = true;
		}
	}
}

//! Set float property
void Object::SetFloatValue(const uint32 index, const float value)
{
	ARCEMU_ASSERT(index < m_valuesCount);
	if(m_floatValues[index] == value)
		return;

	m_floatValues[ index ] = value;

	if(IsInWorld())
	{
		m_updateMask.SetBit(index);

		if(!m_objectUpdated)
		{
			m_mapMgr->ObjectUpdated(this);
			m_objectUpdated = true;
		}
	}
}


void Object::SetFlag(const uint32 index, uint32 newFlag)
{
	SetUInt32Value(index, GetUInt32Value(index) | newFlag);
}


void Object::RemoveFlag(const uint32 index, uint32 oldFlag)
{
	SetUInt32Value(index, GetUInt32Value(index) & ~oldFlag);
}

////////////////////////////////////////////////////////////

float Object::CalcDistance(Object* Ob)
{
	ARCEMU_ASSERT(Ob != NULL);
	return CalcDistance(this->GetPositionX(), this->GetPositionY(), this->GetPositionZ(), Ob->GetPositionX(), Ob->GetPositionY(), Ob->GetPositionZ());
}
float Object::CalcDistance(float ObX, float ObY, float ObZ)
{
	return CalcDistance(this->GetPositionX(), this->GetPositionY(), this->GetPositionZ(), ObX, ObY, ObZ);
}
float Object::CalcDistance(Object* Oa, Object* Ob)
{
	ARCEMU_ASSERT(Oa != NULL);
	ARCEMU_ASSERT(Ob != NULL);
	return CalcDistance(Oa->GetPositionX(), Oa->GetPositionY(), Oa->GetPositionZ(), Ob->GetPositionX(), Ob->GetPositionY(), Ob->GetPositionZ());
}
float Object::CalcDistance(Object* Oa, float ObX, float ObY, float ObZ)
{
	ARCEMU_ASSERT(Oa != NULL);
	return CalcDistance(Oa->GetPositionX(), Oa->GetPositionY(), Oa->GetPositionZ(), ObX, ObY, ObZ);
}

float Object::CalcDistance(float OaX, float OaY, float OaZ, float ObX, float ObY, float ObZ)
{
	float xdest = OaX - ObX;
	float ydest = OaY - ObY;
	float zdest = OaZ - ObZ;
	return sqrtf(zdest * zdest + ydest * ydest + xdest * xdest);
}

bool Object::IsWithinDistInMap(Object* obj, const float dist2compare) const
{
	ARCEMU_ASSERT(obj != NULL);
	float xdest = this->GetPositionX() - obj->GetPositionX();
	float ydest = this->GetPositionY() - obj->GetPositionY();
	float zdest = this->GetPositionZ() - obj->GetPositionZ();
	return sqrtf(zdest * zdest + ydest * ydest + xdest * xdest) <= dist2compare;
}

bool Object::IsWithinLOSInMap(Object* obj)
{
	ARCEMU_ASSERT(obj != NULL);
	if(!IsInMap(obj)) return false;
	LocationVector location;
	location = obj->GetPosition();
	return IsWithinLOS(location);
}

bool Object::IsWithinLOS(LocationVector location)
{
	LocationVector location2;
	location2 = GetPosition();

	if(sWorld.Collision)
	{
		return CollideInterface.CheckLOS(GetMapId(), location2.x, location2.y, location2.z + 2.0f, location.x, location.y, location.z + 2.0f);
	}
	else
	{
		return true;
	}
}


float Object::calcAngle(float Position1X, float Position1Y, float Position2X, float Position2Y)
{
	float dx = Position2X - Position1X;
	float dy = Position2Y - Position1Y;
	double angle = 0.0f;

	// Calculate angle
	if(dx == 0.0)
	{
		if(dy == 0.0)
			angle = 0.0;
		else if(dy > 0.0)
			angle = M_PI * 0.5 /* / 2 */;
		else
			angle = M_PI * 3.0 * 0.5/* / 2 */;
	}
	else if(dy == 0.0)
	{
		if(dx > 0.0)
			angle = 0.0;
		else
			angle = M_PI;
	}
	else
	{
		if(dx < 0.0)
			angle = atanf(dy / dx) + M_PI;
		else if(dy < 0.0)
			angle = atanf(dy / dx) + (2 * M_PI);
		else
			angle = atanf(dy / dx);
	}

	// Convert to degrees
	angle = angle * float(180 / M_PI);

	// Return
	return float(angle);
}

float Object::calcRadAngle(float Position1X, float Position1Y, float Position2X, float Position2Y)
{
	double dx = double(Position2X - Position1X);
	double dy = double(Position2Y - Position1Y);
	double angle = 0.0;

	// Calculate angle
	if(dx == 0.0)
	{
		if(dy == 0.0)
			angle = 0.0;
		else if(dy > 0.0)
			angle = M_PI * 0.5/*/ 2.0*/;
		else
			angle = M_PI * 3.0 * 0.5/*/ 2.0*/;
	}
	else if(dy == 0.0)
	{
		if(dx > 0.0)
			angle = 0.0;
		else
			angle = M_PI;
	}
	else
	{
		if(dx < 0.0)
			angle = atan(dy / dx) + M_PI;
		else if(dy < 0.0)
			angle = atan(dy / dx) + (2 * M_PI);
		else
			angle = atan(dy / dx);
	}

	// Return
	return float(angle);
}

float Object::getEasyAngle(float angle)
{
	while(angle < 0)
	{
		angle = angle + 360;
	}
	while(angle >= 360)
	{
		angle = angle - 360;
	}
	return angle;
}

bool Object::inArc(float Position1X, float Position1Y, float FOV, float Orientation, float Position2X, float Position2Y)
{
	float angle = calcAngle(Position1X, Position1Y, Position2X, Position2Y);
	float lborder = getEasyAngle((Orientation - (FOV * 0.5f/*/2*/)));
	float rborder = getEasyAngle((Orientation + (FOV * 0.5f/*/2*/)));
	//LOG_DEBUG("Orientation: %f Angle: %f LeftBorder: %f RightBorder %f",Orientation,angle,lborder,rborder);
	if(((angle >= lborder) && (angle <= rborder)) || ((lborder > rborder) && ((angle < rborder) || (angle > lborder))))
	{
		return true;
	}
	else
	{
		return false;
	}
}

bool Object::isInFront(Object* target)
{
	// check if we facing something ( is the object within a 180 degree slice of our positive y axis )

	double x = target->GetPositionX() - m_position.x;
	double y = target->GetPositionY() - m_position.y;

	double angle = atan2(y, x);
	angle = (angle >= 0.0) ? angle : 2.0 * M_PI + angle;
	angle -= m_position.o;

	while(angle > M_PI)
		angle -= 2.0 * M_PI;

	while(angle < -M_PI)
		angle += 2.0 * M_PI;

	// replace M_PI in the two lines below to reduce or increase angle

	double left = -1.0 * (M_PI / 2.0);
	double right = (M_PI / 2.0);

	return((angle >= left) && (angle <= right));
}

bool Object::isInBack(Object* target)
{
	// check if we are behind something ( is the object within a 180 degree slice of our negative y axis )

	double x = m_position.x - target->GetPositionX();
	double y = m_position.y - target->GetPositionY();

	double angle = atan2(y, x);
	angle = (angle >= 0.0) ? angle : 2.0 * M_PI + angle;

	// if we are a creature and have a UNIT_FIELD_TARGET then we are always facing them
	if(IsCreature() && TO_CREATURE(this)->GetTargetGUID() != 0)
	{
		Unit* pTarget = TO_CREATURE(this)->GetAIInterface()->getNextTarget();
		if(pTarget != NULL)
			angle -= double(Object::calcRadAngle(target->m_position.x, target->m_position.y, pTarget->m_position.x, pTarget->m_position.y));
		else
			angle -= target->GetOrientation();
	}
	else
		angle -= target->GetOrientation();

	while(angle > M_PI)
		angle -= 2.0 * M_PI;

	while(angle < -M_PI)
		angle += 2.0 * M_PI;

	// replace M_H_PI in the two lines below to reduce or increase angle

	double left = -1.0 * (M_H_PI / 2.0);
	double right = (M_H_PI / 2.0);

	return((angle <= left) && (angle >= right));
}
bool Object::isInArc(Object* target , float angle) // angle in degrees
{
	return inArc(GetPositionX() , GetPositionY() , angle , GetOrientation() , target->GetPositionX() , target->GetPositionY());
}

bool Object::HasInArc(float degrees, Object* target)
{
	return isInArc(target, degrees);
}

bool Object::isInRange(Object* target, float range)
{
	if(!this->IsInWorld() || !target) return false;
	float dist = CalcDistance(target);
	return(dist <= range);
}

void Object::_setFaction()
{
	FactionTemplateDBC* factT = NULL;

	if(IsUnit())
	{
		factT = dbcFactionTemplate.LookupEntryForced(TO_UNIT(this)->GetFaction());
		if(!factT)
			LOG_ERROR("Unit does not have a valid faction. It will make him act stupid in world. Don't blame us, blame yourself for not checking :P, faction %u set to entry %u", TO_UNIT(this)->GetFaction(), GetEntry());
	}
	else if(IsGameObject())
	{
		factT = dbcFactionTemplate.LookupEntryForced(TO< GameObject* >(this)->GetFaction());
		if(!factT)
			LOG_ERROR("Game Object does not have a valid faction. It will make him act stupid in world. Don't blame us, blame yourself for not checking :P, faction %u set to entry %u", TO< GameObject* >(this)->GetFaction(), GetEntry());
	}

	if(!factT)
	{
		factT = dbcFactionTemplate.LookupRow(0);
		//this is causing a lot of crashes cause people have shitty dbs
//		return;
	}
	m_faction = factT;
	m_factionDBC = dbcFaction.LookupEntryForced(factT->Faction);
	if(!m_factionDBC)
		m_factionDBC = dbcFaction.LookupRow(0);
}

void Object::UpdateOppFactionSet()
{
	m_oppFactsInRange.clear();

	for(std::set< Object* >::iterator itr = m_objectsInRange.begin(); itr != m_objectsInRange.end(); ++itr)
	{
		Object* i = *itr;

		if(i->IsUnit() || i->IsGameObject())
		{
			if(isHostile(this, i))
			{
				if(!i->IsInRangeOppFactSet(this))
					i->m_oppFactsInRange.insert(this);
				if(!IsInRangeOppFactSet(i))
					m_oppFactsInRange.insert(i);

			}
			else
			{
				if(i->IsInRangeOppFactSet(this))
					i->m_oppFactsInRange.erase(this);
				if(IsInRangeOppFactSet(i))
					m_oppFactsInRange.erase(i);
			}
		}
	}
}

void Object::UpdateSameFactionSet()
{
	m_sameFactsInRange.clear();


	for(std::set< Object* >::iterator itr = m_objectsInRange.begin(); itr != m_objectsInRange.end(); ++itr)
	{
		Object* i = *itr;

		if(i->IsUnit() || i->IsGameObject())
		{
			if(isFriendly(this, i))
			{
				if(!i->IsInRangeSameFactSet(this))
					i->m_sameFactsInRange.insert(this);

				if(!IsInRangeOppFactSet(i))
					m_sameFactsInRange.insert(i);

			}
			else
			{
				if(i->IsInRangeSameFactSet(this))
					i->m_sameFactsInRange.erase(this);

				if(IsInRangeSameFactSet(i))
					m_sameFactsInRange.erase(i);
			}
		}
	}
}

void Object::EventSetUInt32Value(uint32 index, uint32 value)
{
	SetUInt32Value(index, value);
}

void Object::DealDamage(Unit* pVictim, uint32 damage, uint32 targetEvent, uint32 unitEvent, uint32 spellId, bool no_remove_auras)
{
}

void Object::SpellNonMeleeDamageLog(Unit* pVictim, uint32 spellID, uint32 damage, bool allowProc, bool static_damage, bool no_remove_auras)
{
//==========================================================================================
//==============================Unacceptable Cases Processing===============================
//==========================================================================================
	if(pVictim == NULL || !pVictim->isAlive())
		return;

	SpellEntry* spellInfo = dbcSpell.LookupEntryForced(spellID);
	if(spellInfo == NULL)
		return;

	if(this->IsPlayer() && ! TO< Player* >(this)->canCast(spellInfo))
		return;
//==========================================================================================
//==============================Variables Initialization====================================
//==========================================================================================
	float res = static_cast< float >(damage);
	bool critical = false;

	uint32 aproc = PROC_ON_ANY_HOSTILE_ACTION; /*| PROC_ON_SPELL_HIT;*/
	uint32 vproc = PROC_ON_ANY_HOSTILE_ACTION | PROC_ON_ANY_DAMAGE_VICTIM; /*| PROC_ON_SPELL_HIT_VICTIM;*/

	//A school damage is not necessarily magic
	switch(spellInfo->Spell_Dmg_Type)
	{
		case SPELL_DMG_TYPE_RANGED:
			{
				aproc |= PROC_ON_RANGED_ATTACK;
				vproc |= PROC_ON_RANGED_ATTACK_VICTIM;
			}
			break;

		case SPELL_DMG_TYPE_MELEE:
			{
				aproc |= PROC_ON_MELEE_ATTACK;
				vproc |= PROC_ON_MELEE_ATTACK_VICTIM;
			}
			break;

		case SPELL_DMG_TYPE_MAGIC:
			{
				aproc |= PROC_ON_SPELL_HIT;
				vproc |= PROC_ON_SPELL_HIT_VICTIM;
			}
			break;
	}

//==========================================================================================
//==============================+Spell Damage Bonus Calculations============================
//==========================================================================================
//------------------------------by stats----------------------------------------------------
	if(IsUnit() && !static_damage)
	{
		Unit* caster = TO< Unit* >(this);

		caster->RemoveAurasByInterruptFlag(AURA_INTERRUPT_ON_START_ATTACK);

		res += static_cast< float >( caster->GetSpellDmgBonus(pVictim, spellInfo, damage, false) );

		if(res < 0.0f)
			res = 0.0f;
	}
//==========================================================================================
//==============================Post +SpellDamage Bonus Modifications=======================
//==========================================================================================
	if( res > 0.0f && !(spellInfo->AttributesExB & ATTRIBUTESEXB_CANT_CRIT) )
	{
		critical = this->IsCriticalDamageForSpell(pVictim, spellInfo);

//==========================================================================================
//==============================Spell Critical Hit==========================================
//==========================================================================================
		if(critical)
		{
			res = this->GetCriticalDamageBonusForSpell(pVictim, spellInfo, res);

			switch(spellInfo->Spell_Dmg_Type)
			{
				case SPELL_DMG_TYPE_RANGED:
					{
						aproc |= PROC_ON_RANGED_CRIT_ATTACK;
						vproc |= PROC_ON_RANGED_CRIT_ATTACK_VICTIM;
					}
					break;

				case SPELL_DMG_TYPE_MELEE:
					{
						aproc |= PROC_ON_CRIT_ATTACK;
						vproc |= PROC_ON_CRIT_HIT_VICTIM;
					}
					break;

				case SPELL_DMG_TYPE_MAGIC:
					{
						aproc |= PROC_ON_SPELL_CRIT_HIT;
						vproc |= PROC_ON_SPELL_CRIT_HIT_VICTIM;
					}
					break;
			}
		}
	}

//==========================================================================================
//==============================Post Roll Calculations======================================
//==========================================================================================

//------------------------------damage reduction--------------------------------------------
	if( this->IsUnit() )
		res += TO< Unit* >(this)->CalcSpellDamageReduction(pVictim, spellInfo, res);
//------------------------------absorption--------------------------------------------------
	uint32 ress = static_cast< uint32 >(res);
	uint32 abs_dmg = pVictim->AbsorbDamage(spellInfo->School, &ress);
	uint32 ms_abs_dmg = pVictim->ManaShieldAbsorb(ress);
	if(ms_abs_dmg)
	{
		if(ms_abs_dmg > ress)
			ress = 0;
		else
			ress -= ms_abs_dmg;

		abs_dmg += ms_abs_dmg;
	}

	if(abs_dmg)
		vproc |= PROC_ON_ABSORB;

	// Incanter's Absorption
	if( pVictim->IsPlayer() && pVictim->HasAurasWithNameHash(SPELL_HASH_INCANTER_S_ABSORPTION) )
	{
		float pctmod = 0.0f;
		Player* pl = TO< Player* >(pVictim);
		if(pl->HasAura(44394))
			pctmod = 0.05f;
		else if(pl->HasAura(44395))
			pctmod = 0.10f;
		else if(pl->HasAura(44396))
			pctmod = 0.15f;

		uint32 hp = static_cast< uint32 >( 0.05f * pl->GetUInt32Value(UNIT_FIELD_MAXHEALTH) );
		uint32 spellpower = static_cast< uint32 >( pctmod * pl->GetPosDamageDoneMod(SCHOOL_NORMAL) );

		if(spellpower > hp)
			spellpower = hp;

		SpellEntry* entry = dbcSpell.LookupEntryForced(44413);
		if(!entry)
			return;

		Spell* sp = sSpellFactoryMgr.NewSpell(pl, entry, true, NULL);
		sp->GetProto()->eff[0].EffectBasePoints = spellpower;
		SpellCastTargets targets;
		targets.m_unitTarget = pl->GetGUID();
		sp->prepare(&targets);
	}

	if(ress < 0) ress = 0;

	res = static_cast< float >(ress);
	dealdamage dmg;
	dmg.school_type = spellInfo->School;
	dmg.full_damage = ress;
	dmg.resisted_damage = 0;

	if(res <= 0)
		dmg.resisted_damage = dmg.full_damage;

	//------------------------------resistance reducing-----------------------------------------
	if(res > 0 && this->IsUnit())
	{
		TO< Unit* >(this)->CalculateResistanceReduction(pVictim, &dmg, spellInfo, 0);
		if((int32)dmg.resisted_damage > dmg.full_damage)
			res = 0;
		else
			res = static_cast< float >(dmg.full_damage - dmg.resisted_damage);
	}
	//------------------------------special states----------------------------------------------
	if(pVictim->IsPlayer() && TO< Player* >(pVictim)->GodModeCheat == true)
	{
		res = static_cast< float >(dmg.full_damage);
		dmg.resisted_damage = dmg.full_damage;
	}

	// Paladin: Blessing of Sacrifice, and Warlock: Soul Link
	if(pVictim->m_damageSplitTarget)
	{
		res = (float)pVictim->DoDamageSplitTarget((uint32)res, spellInfo->School, false);
	}

//==========================================================================================
//==============================Data Sending ProcHandling===================================
//==========================================================================================
	SendSpellNonMeleeDamageLog(this, pVictim, spellID, static_cast< int32 >(res), static_cast< uint8 >(spellInfo->School), abs_dmg, dmg.resisted_damage, false, 0, critical, IsPlayer());
	DealDamage(pVictim, static_cast< int32 >(res), 2, 0, spellID);

	if(IsUnit())
	{
		int32 dmg2 = static_cast< int32 >(res);

		pVictim->HandleProc(vproc, TO< Unit* >(this), spellInfo, !allowProc, dmg2, abs_dmg);
		pVictim->m_procCounter = 0;
		TO< Unit* >(this)->HandleProc(aproc, pVictim, spellInfo, !allowProc, dmg2, abs_dmg);
		TO< Unit* >(this)->m_procCounter = 0;
	}
	if(this->IsPlayer())
	{
		TO< Player* >(this)->m_casted_amount[spellInfo->School] = (uint32)res;
	}

	if(!(dmg.full_damage == 0 && abs_dmg))
	{
		//Only pushback the victim current spell if it's not fully absorbed
		if(pVictim->GetCurrentSpell())
			pVictim->GetCurrentSpell()->AddTime(spellInfo->School);
	}

//==========================================================================================
//==============================Post Damage Processing======================================
//==========================================================================================
	if((int32)dmg.resisted_damage == dmg.full_damage && !abs_dmg)
	{
		//Magic Absorption
		if(pVictim->IsPlayer())
		{
			if(TO< Player* >(pVictim)->m_RegenManaOnSpellResist)
			{
				Player* pl = TO< Player* >(pVictim);

				uint32 maxmana = pl->GetMaxPower(POWER_TYPE_MANA);
				uint32 amount = static_cast< uint32 >(maxmana * pl->m_RegenManaOnSpellResist);

				pVictim->Energize(pVictim, 29442, amount, POWER_TYPE_MANA);
			}
			// we still stay in combat dude
			TO< Player* >(pVictim)->CombatStatusHandler_ResetPvPTimeout();
		}
		if(IsPlayer())
			TO< Player* >(this)->CombatStatusHandler_ResetPvPTimeout();
	}
	if(spellInfo->School == SCHOOL_SHADOW)
	{
		if(pVictim->isAlive() && this->IsUnit())
		{
			//Shadow Word:Death
			if(spellID == 32379 || spellID == 32996 || spellID == 48157 || spellID == 48158)
			{
				uint32 damage2 = static_cast< uint32 >(res + abs_dmg);
				uint32 absorbed = TO< Unit* >(this)->AbsorbDamage(spellInfo->School, &damage2);
				DealDamage(TO< Unit* >(this), damage2, 2, 0, spellID);
				SendSpellNonMeleeDamageLog(this, this, spellID, damage2, static_cast< uint8 >(spellInfo->School), absorbed, 0, false, 0, false, IsPlayer());
			}
		}
	}
}

//*****************************************************************************************
//* SpellLog packets just to keep the code cleaner and better to read
//*****************************************************************************************

void Object::SendSpellLog(Object* Caster, Object* Target, uint32 Ability, uint8 SpellLogType)
{
	if(Caster == NULL || Target == NULL || Ability == 0)
		return;


	WorldPacket data(SMSG_SPELLLOGMISS, 26);

	data << uint32(Ability);					// spellid
	data << Caster->GetGUID();					// caster / player
	data << uint8(1);							// unknown but I think they are const
	data << uint32(1);						// unknown but I think they are const
	data << Target->GetGUID();					// target
	data << uint8(SpellLogType);				// spelllogtype

	Caster->SendMessageToSet(&data, true);
}


void Object::SendSpellNonMeleeDamageLog(Object* Caster, Object* Target, uint32 SpellID, uint32 Damage, uint8 School, uint32 AbsorbedDamage, uint32 ResistedDamage, bool PhysicalDamage, uint32 BlockedDamage, bool CriticalHit, bool bToset)
{
	if(!Caster || !Target || !SpellID)
		return;

	uint32 Overkill = 0;

	if(Damage > Target->GetUInt32Value(UNIT_FIELD_HEALTH))
		Overkill = Damage - Target->GetUInt32Value(UNIT_FIELD_HEALTH);

	WorldPacket data(SMSG_SPELLNONMELEEDAMAGELOG, 48);

	data << Target->GetNewGUID();
	data << Caster->GetNewGUID();
	data << uint32(SpellID);                      // SpellID / AbilityID
	data << uint32(Damage);                       // All Damage
	data << uint32(Overkill);					// Overkill
	data << uint8(g_spellSchoolConversionTable[School]);     // School
	data << uint32(AbsorbedDamage);               // Absorbed Damage
	data << uint32(ResistedDamage);               // Resisted Damage
	data << uint8(PhysicalDamage);        // Physical Damage (true/false)
	data << uint8(0);                     // unknown or it binds with Physical Damage
	data << uint32(BlockedDamage);		       // Physical Damage (true/false)

	// unknown const
	if(CriticalHit)
		data << uint8(7);
	else
		data << uint8(5);

	data << uint32(0);

	Caster->SendMessageToSet(&data, bToset);
}

void Object::SendAttackerStateUpdate(Object* Caster, Object* Target, dealdamage* Dmg, uint32 Damage, uint32 Abs, uint32 BlockedDamage, uint32 HitStatus, uint32 VState)
{
	if(!Caster || !Target || !Dmg)
		return;

	WorldPacket data(SMSG_ATTACKERSTATEUPDATE, 70);

	uint32 Overkill = 0;

	if(Damage > Target->GetUInt32Value(UNIT_FIELD_MAXHEALTH))
		Overkill = Damage - Target->GetUInt32Value(UNIT_FIELD_HEALTH);

	data << uint32(HitStatus);
	data << Caster->GetNewGUID();
	data << Target->GetNewGUID();

	data << uint32(Damage);						// Realdamage
	data << uint32(Overkill);					// Overkill
	data << uint8(1);					// Damage type counter / swing type

	data << uint32(g_spellSchoolConversionTable[Dmg->school_type]);				    // Damage school
	data << float(Dmg->full_damage);	// Damage float
	data << uint32(Dmg->full_damage);	// Damage amount

	if(HitStatus & HITSTATUS_ABSORBED)
	{
		data << uint32(Abs);				// Damage absorbed
	}

	if(HitStatus & HITSTATUS_RESIST)
	{
		data << uint32(Dmg->resisted_damage);	// Damage resisted
	}

	data << uint8(VState);
	data << uint32(0);				// can be 0,1000 or -1
	data << uint32(0);

	if(HitStatus & HITSTATUS_BLOCK)
	{
		data << uint32(BlockedDamage);		// Damage amount blocked
	}


	if(HitStatus & HITSTATUS_UNK2)
	{
		data << uint32(0);				// unknown
	}

	if(HitStatus & HITSTATUS_UNK)
	{
		data << uint32(0);
		data << float(0);
		data << float(0);
		data << float(0);
		data << float(0);
		data << float(0);
		data << float(0);
		data << float(0);
		data << float(0);

		data << float(0);   // Found in loop
		data << float(0);	// Found in loop
		data << uint32(0);
	}

	SendMessageToSet(&data, Caster->IsPlayer());
}

int32 Object::event_GetInstanceID()
{
	// return -1 for non-inworld.. so we get our shit moved to the right thread
	//default value is -1, if it's something else then we are/will be soon InWorld.
	return m_instanceId;
}

void Object::EventSpellDamage(uint64 Victim, uint32 SpellID, uint32 Damage)
{
	if(!IsInWorld())
		return;

	Unit* pUnit = GetMapMgr()->GetUnit(Victim);
	if(pUnit == NULL)
		return;

	SpellNonMeleeDamageLog(pUnit, SpellID, Damage, true);
}

//! Object has an active state
bool Object::CanActivate()
{
	switch(m_objectTypeId)
	{
		case TYPEID_UNIT:
			{
				if(!IsPet())
					return true;
			}
			break;

		case TYPEID_GAMEOBJECT:
			{
				if(TO_GAMEOBJECT(this)->HasAI() && TO_GAMEOBJECT(this)->GetType() != GAMEOBJECT_TYPE_TRAP)
					return true;
			}
			break;
	}

	return false;
}

void Object::Activate(MapMgr* mgr)
{
	switch(m_objectTypeId)
	{
		case TYPEID_UNIT:
			mgr->activeCreatures.insert(TO_CREATURE(this));
			break;

		case TYPEID_GAMEOBJECT:
			mgr->activeGameObjects.insert(TO_GAMEOBJECT(this));
			break;
	}
	// Objects are active so set to true.
	Active = true;
}

void Object::Deactivate(MapMgr* mgr)
{
	if(mgr == NULL)
		return;

	switch(m_objectTypeId)
	{
		case TYPEID_UNIT:
			// check iterator
			if(mgr->creature_iterator != mgr->activeCreatures.end() && (*mgr->creature_iterator)->GetGUID() == GetGUID())
				++mgr->creature_iterator;
			mgr->activeCreatures.erase(TO_CREATURE(this));
			break;

		case TYPEID_GAMEOBJECT:
			mgr->activeGameObjects.erase(TO_GAMEOBJECT(this));
			break;
	}
	Active = false;
}

void Object::SetByte(uint32 index, uint32 index1, uint8 value)
{
	ARCEMU_ASSERT(index < m_valuesCount);
	// save updating when val isn't changing.

	uint8* v = &((uint8*)m_uint32Values)[index * 4 + index1];

	if(*v == value)
		return;

	*v = value;

	if(IsInWorld())
	{
		m_updateMask.SetBit(index);

		if(!m_objectUpdated)
		{
			m_mapMgr->ObjectUpdated(this);
			m_objectUpdated = true;
		}
	}

}

void Object::SetByteFlag(uint16 index, uint8 offset, uint8 newFlag)
{
	ARCEMU_ASSERT(index < m_valuesCount);
	ARCEMU_ASSERT(offset < 4);

	offset <<= 3;

	if(!(uint8(m_uint32Values[ index ] >> offset) & newFlag))
	{
		m_uint32Values[ index ] |= uint32(uint32(newFlag) << offset);

		if(IsInWorld())
		{
			m_updateMask.SetBit(index);

			if(!m_objectUpdated)
			{
				m_mapMgr->ObjectUpdated(this);
				m_objectUpdated = true;
			}
		}
	}
}

void Object::RemoveByteFlag(uint16 index, uint8 offset, uint8 oldFlag)
{
	ARCEMU_ASSERT(index < m_valuesCount);
	ARCEMU_ASSERT(offset < 4);

	offset <<= 3;

	if(uint8(m_uint32Values[ index ] >> offset) & oldFlag)
	{
		m_uint32Values[ index ] &= ~uint32(uint32(oldFlag) << offset);

		if(IsInWorld())
		{
			m_updateMask.SetBit(index);

			if(!m_objectUpdated)
			{
				m_mapMgr->ObjectUpdated(this);
				m_objectUpdated = true;
			}
		}
	}
}

void Object::SetZoneId(uint32 newZone)
{
	m_zoneId = newZone;

	if(IsPlayer())
	{
		TO_PLAYER(this)->m_cache->SetUInt32Value(CACHE_PLAYER_ZONEID, newZone);
		if(TO_PLAYER(this)->GetGroup() != NULL)
			TO_PLAYER(this)->GetGroup()->HandlePartialChange(PARTY_UPDATE_FLAG_ZONEID, TO_PLAYER(this));
	}
}

void Object::PlaySoundToSet(uint32 sound_entry)
{
	WorldPacket data(SMSG_PLAY_SOUND, 4);
	data << sound_entry;
	data << uint64(GetGUID());

	SendMessageToSet(&data, true);
}

void Object::_SetExtension(const string & name, void* ptr)
{
	if(m_extensions == NULL)
		m_extensions = new ExtensionSet;

	m_extensions->insert(make_pair(name, ptr));
}

bool Object::IsInBg()
{
	MapInfo* pMapinfo = WorldMapInfoStorage.LookupEntry(GetMapId());

	if(pMapinfo != NULL)
	{
		return (pMapinfo->type == INSTANCE_BATTLEGROUND);
	}

	return false;
}

uint32 Object::GetTeam()
{

	switch(m_factionDBC->ID)
	{
			// Human
		case 1:
			// Dwarf
		case 3:
			// Nightelf
		case 4:
			// Gnome
		case 8:
			// Draenei
		case 927:
			return TEAM_ALLIANCE;

			// Orc
		case 2:
			// Undead
		case 5:
			// Tauren
		case 6:
			// Troll
		case 9:
			// Bloodelf
		case 914:
			return TEAM_HORDE;
	}

	return static_cast< uint32 >(-1);
}

//Manipulates the phase value, see "enum PHASECOMMANDS" in Object.h for a longer explanation!
void Object::Phase(uint8 command, uint32 newphase)
{
	switch(command)
	{
		case PHASE_SET:
			m_phase = newphase;
			break;
		case PHASE_ADD:
			m_phase |= newphase;
			break;
		case PHASE_DEL:
			m_phase &= ~newphase;
			break;
		case PHASE_RESET:
			m_phase = 1;
			break;
		default:
			ARCEMU_ASSERT(false);
	}

	return;
}

void Object::AddInRangeObject(Object* pObj)
{

	ARCEMU_ASSERT(pObj != NULL);

	if(pObj == this)
		LOG_ERROR("We are in range of ourselves!");

	if(pObj->IsPlayer())
		m_inRangePlayers.insert(pObj);

	m_objectsInRange.insert(pObj);
}

void Object::OutPacketToSet(uint32 Opcode, uint16 Len, const void* Data, bool self)
{
	if(!IsInWorld())
		return;

	// We are on Object level, which means we can't send it to ourselves so we only send to Players inrange
	for(std::set< Object* >::iterator itr = m_inRangePlayers.begin(); itr != m_inRangePlayers.end(); ++itr)
	{
		Object* o = *itr;

		o->OutPacket(Opcode, Len, Data);
	}
}

void Object::SendMessageToSet(WorldPacket* data, bool bToSelf, bool myteam_only)
{
	if(! IsInWorld())
		return;

	uint32 myphase = GetPhase();
	for(std::set< Object* >::iterator itr = m_inRangePlayers.begin(); itr != m_inRangePlayers.end(); ++itr)
	{
		Object* o = *itr;
		if((o->GetPhase() & myphase) != 0)
			o->SendPacket(data);
	}
}

void Object::RemoveInRangeObject(Object* pObj)
{
	ARCEMU_ASSERT(pObj != NULL);

	if(pObj->IsPlayer())
	{
		ARCEMU_ASSERT(m_inRangePlayers.erase(pObj) == 1);
	}

	ARCEMU_ASSERT(m_objectsInRange.erase(pObj) == 1);

	OnRemoveInRangeObject(pObj);
}

void Object::RemoveSelfFromInrangeSets()
{
	std::set< Object* >::iterator itr;

	for(itr = m_objectsInRange.begin(); itr != m_objectsInRange.end(); ++itr)
	{
		Object* o = *itr;

		ARCEMU_ASSERT(o != NULL);

		o->RemoveInRangeObject(this);

	}

}


void Object::OnRemoveInRangeObject(Object* pObj)
{
	/* This method will remain empty for now, don't remove it!
	-dfighter
	*/
}

Object* Object::GetMapMgrObject(const uint64 & guid)
{
	if(!IsInWorld())
		return NULL;

	return GetMapMgr()->_GetObject(guid);
}

Pet* Object::GetMapMgrPet(const uint64 & guid)
{
	if(!IsInWorld())
		return NULL;

	return GetMapMgr()->GetPet(GET_LOWGUID_PART(guid));
}

Unit* Object::GetMapMgrUnit(const uint64 & guid)
{
	if(!IsInWorld())
		return NULL;

	return GetMapMgr()->GetUnit(guid);
}

Player* Object::GetMapMgrPlayer(const uint64 & guid)
{
	if(!IsInWorld())
		return NULL;

	return GetMapMgr()->GetPlayer(GET_LOWGUID_PART(guid));
}

Creature* Object::GetMapMgrCreature(const uint64 & guid)
{
	if(!IsInWorld())
		return NULL;

	return GetMapMgr()->GetCreature(GET_LOWGUID_PART(guid));
}

GameObject* Object::GetMapMgrGameObject(const uint64 & guid)
{
	if(!IsInWorld())
		return NULL;

	return GetMapMgr()->GetGameObject(GET_LOWGUID_PART(guid));
}

DynamicObject* Object::GetMapMgrDynamicObject(const uint64 & guid)
{
	if(!IsInWorld())
		return NULL;

	return GetMapMgr()->GetDynamicObject(GET_LOWGUID_PART(guid));
}

Object* Object::GetPlayerOwner()
{
	return NULL;
}

MapCell* Object::GetMapCell() const
{
	ARCEMU_ASSERT(m_mapMgr != NULL);
	return m_mapMgr->GetCell(m_mapCell_x, m_mapCell_y);
}

void Object::SetMapCell(MapCell* cell)
{
	if(cell == NULL)
	{
		//mapcell coordinates are uint16, so using uint32(-1) will always make GetMapCell() return NULL.
		m_mapCell_x = m_mapCell_y = uint32(-1);
	}
	else
	{
		m_mapCell_x = cell->GetPositionX();
		m_mapCell_y = cell->GetPositionY();
	}
}

void Object::SendAIReaction(uint32 reaction)
{
	WorldPacket data(SMSG_AI_REACTION, 12);
	data << uint64(GetGUID());
	data << uint32(reaction);
	SendMessageToSet(&data, false);
}

void Object::SendDestroyObject()
{
	WorldPacket data(SMSG_DESTROY_OBJECT, 9);
	data << uint64(GetGUID());
	data << uint8(0);
	SendMessageToSet(&data, false);
}

bool Object::GetPoint(float angle, float rad, float & outx, float & outy, float & outz, bool sloppypath)
{
	if(!IsInWorld())
		return false;
	outx = GetPositionX() + rad * cos(angle);
	outy = GetPositionY() + rad * sin(angle);
	outz = GetMapMgr()->GetLandHeight(outx, outy, GetPositionZ() + 2);
	float waterz;
	uint32 watertype;
	GetMapMgr()->GetLiquidInfo(outx, outy, GetPositionZ() + 2, waterz, watertype);
	outz = max(waterz, outz);

	NavMeshData* nav = CollideInterface.GetNavMesh(GetMapId());

	if(nav != NULL)
	{
		//if we can path there, go for it
		if(!IsUnit() || !sloppypath || !TO_UNIT(this)->GetAIInterface()->CanCreatePath(outx, outy, outz))
		{
			//raycast nav mesh to see if this place is valid
			float start[3] = { GetPositionY(), GetPositionZ() + 0.5f, GetPositionX() };
			float end[3] = { outy, outz + 0.5f, outx };
			float extents[3] = { 3, 5, 3 };
			dtQueryFilter filter;
			filter.setIncludeFlags(NAV_GROUND | NAV_WATER | NAV_SLIME | NAV_MAGMA);

			dtPolyRef startref;
			nav->query->findNearestPoly(start, extents, &filter, &startref, NULL);

			float point, pointNormal;
			float result[3];
			int numvisited;
			dtPolyRef visited[MAX_PATH_LENGTH];

			dtStatus rayresult = nav->query->raycast(startref, start, end, &filter, &point, &pointNormal, visited, &numvisited, MAX_PATH_LENGTH);

			if (point <= 1.0f)
			{
				if (numvisited == 0 || rayresult == DT_FAILURE)
				{
					result[0] = start[0];
					result[1] = start[1];
					result[2] = start[2];
				}
				else
				{
					result[0] = start[0] + ((end[0] - start[0]) * point);
					result[1] = start[1] + ((end[1] - start[1]) * point);
					result[2] = start[2] + ((end[2] - start[2]) * point);
					nav->query->getPolyHeight(visited[numvisited - 1], result, &result[1]);
				}

				//copy end back to function floats
				outy = result[0];
				outz = result[1];
				outx = result[2];
			}
		}
	}
	else //test against vmap if mmap isn't available
	{
		float testx, testy, testz;

		if(CollideInterface.GetFirstPoint(GetMapId(), GetPositionX(), GetPositionY(), GetPositionZ() + 2, outx, outy, outz + 2, testx, testy, testz, -0.5f))
		{
			//hit something
			outx = testx;
			outy = testy;
			outz = testz;
		}

		outz = GetMapMgr()->GetLandHeight(outx, outy, outz + 2);
	}

	return true;
}
