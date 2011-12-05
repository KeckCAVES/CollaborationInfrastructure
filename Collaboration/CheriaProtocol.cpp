/***********************************************************************
CheriaProtocol - Class defining the communication protocol between a
Cheria server and a Cheria client.
Copyright (c) 2010-2011 Oliver Kreylos

This file is part of the Vrui remote collaboration infrastructure.

The Vrui remote collaboration infrastructure is free software; you can
redistribute it and/or modify it under the terms of the GNU General
Public License as published by the Free Software Foundation; either
version 2 of the License, or (at your option) any later version.

The Vrui remote collaboration infrastructure is distributed in the hope
that it will be useful, but WITHOUT ANY WARRANTY; without even the
implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with the Vrui remote collaboration infrastructure; if not, write to the
Free Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
02111-1307 USA
***********************************************************************/

#include <Collaboration/CheriaProtocol.h>

#include <IO/File.h>

namespace Collaboration {

/********************************************
Methods of class CheriaProtocol::DeviceState:
********************************************/

CheriaProtocol::DeviceState::DeviceState(int sTrackType,unsigned int sNumButtons,unsigned int sNumValuators)
	:trackType(sTrackType),
	 numButtons(sNumButtons),
	 numValuators(sNumValuators),
	 updateMask(NO_CHANGE),
	 rayDirection(0,1,0),
	 transform(ONTransform::identity),
	 buttonStates(numButtons>0?new Byte[(numButtons+7)/8]:0),
	 valuatorStates(numValuators>0?new Scalar[numValuators]:0)
	{
	/* Initialize button states: */
	for(unsigned int i=0;i<(numButtons+7)/8;++i)
		buttonStates[i]=0x0U;
	
	/* Initialize the valuator states: */
	for(unsigned int i=0;i<numValuators;++i)
		valuatorStates[i]=Scalar(0);
	}

CheriaProtocol::DeviceState::DeviceState(IO::File& source)
	:trackType(source.read<Misc::SInt32>()),
	 numButtons(source.read<Card>()),
	 numValuators(source.read<Card>()),
	 updateMask(NO_CHANGE),
	 rayDirection(0,1,0),
	 transform(ONTransform::identity),
	 buttonStates(numButtons>0?new Byte[(numButtons+7)/8]:0),
	 valuatorStates(numValuators>0?new Scalar[numValuators]:0)
	{
	/* Initialize button states: */
	for(unsigned int i=0;i<(numButtons+7)/8;++i)
		buttonStates[i]=0x0U;
	
	/* Initialize the valuator states: */
	for(unsigned int i=0;i<numValuators;++i)
		valuatorStates[i]=Scalar(0);
	}

CheriaProtocol::DeviceState::~DeviceState(void)
	{
	delete[] buttonStates;
	delete[] valuatorStates;
	}

void CheriaProtocol::DeviceState::skipLayout(IO::File& source)
	{
	source.skip<Misc::SInt32>(1);
	source.skip<Card>(2);
	}

void CheriaProtocol::DeviceState::writeLayout(IO::File& sink) const
	{
	sink.write<Misc::SInt32>(trackType);
	sink.write<Card>(numButtons);
	sink.write<Card>(numValuators);
	}

void CheriaProtocol::DeviceState::read(IO::File& source)
	{
	/* Read the update mask: */
	unsigned int newUpdateMask=source.read<Byte>();
	
	/* Read the device's ray direction: */
	if(newUpdateMask&RAYDIRECTION)
		CheriaProtocol::read(rayDirection,source);
	
	/* Read the device's position and orientation: */
	if(newUpdateMask&TRANSFORM)
		CheriaProtocol::read(transform,source);
	
	/* Read the device's button states: */
	if(newUpdateMask&BUTTON)
		source.read(buttonStates,(numButtons+7)/8);
	
	/* Read the device's valuator states: */
	if(newUpdateMask&VALUATOR)
		source.read(valuatorStates,numValuators);
	
	/* Update the cumulative update mask: */
	updateMask|=newUpdateMask;
	}

void CheriaProtocol::DeviceState::write(unsigned int writeUpdateMask,IO::File& sink) const
	{
	/* Write the update mask: */
	sink.write<Byte>(writeUpdateMask);
	
	/* Write the device's ray direction: */
	if(writeUpdateMask&RAYDIRECTION)
		CheriaProtocol::write(rayDirection,sink);
	
	/* Write the device's position and orientation: */
	if(writeUpdateMask&TRANSFORM)
		CheriaProtocol::write(transform,sink);
	
	/* Write the device's button states: */
	if(writeUpdateMask&BUTTON)
		sink.write(buttonStates,(numButtons+7)/8);
	
	/* Write the device's valuator states: */
	if(writeUpdateMask&VALUATOR)
		sink.write(valuatorStates,numValuators);
	}

/******************************************
Methods of class CheriaProtocol::ToolState:
******************************************/

CheriaProtocol::ToolState::ToolState(const char* sClassName,unsigned int sNumButtonSlots,unsigned int sNumValuatorSlots)
	:className(sClassName),
	 numButtonSlots(sNumButtonSlots),buttonSlots(numButtonSlots>0?new Slot[numButtonSlots]:0),
	 numValuatorSlots(sNumValuatorSlots),valuatorSlots(numValuatorSlots>0?new Slot[numValuatorSlots]:0)
	{
	}	

CheriaProtocol::ToolState::ToolState(IO::File& source)
	:buttonSlots(0),
	 valuatorSlots(0)
	{
	/* Read the tool's class name: */
	CheriaProtocol::read(className,source);
	
	/* Read the tool's button slots: */
	numButtonSlots=source.read<Card>();
	buttonSlots=new Slot[numButtonSlots];
	for(unsigned int buttonSlotIndex=0;buttonSlotIndex<numButtonSlots;++buttonSlotIndex)
		{
		buttonSlots[buttonSlotIndex].deviceId=source.read<Card>();
		buttonSlots[buttonSlotIndex].index=source.read<Card>();
		}
	
	/* Read the tool's valuator slots: */
	numValuatorSlots=source.read<Card>();
	valuatorSlots=new Slot[numValuatorSlots];
	for(unsigned int valuatorSlotIndex=0;valuatorSlotIndex<numValuatorSlots;++valuatorSlotIndex)
		{
		valuatorSlots[valuatorSlotIndex].deviceId=source.read<Card>();
		valuatorSlots[valuatorSlotIndex].index=source.read<Card>();
		}
	}

CheriaProtocol::ToolState::~ToolState(void)
	{
	delete[] buttonSlots;
	delete[] valuatorSlots;
	}

void CheriaProtocol::ToolState::skip(IO::File& source)
	{
	/* Skip the tool's class name: */
	CheriaProtocol::read<std::string>(source);
	
	/* Skip the tool's button slots: */
	unsigned int numButtonSlots=source.read<Card>();
	source.skip<Card>(numButtonSlots*2);
	
	/* Skip the tool's valuator slots: */
	unsigned int numValuatorSlots=source.read<Card>();
	source.skip<Card>(numValuatorSlots*2);
	}

void CheriaProtocol::ToolState::write(IO::File& sink) const
	{
	/* Write the tool's class name: */
	CheriaProtocol::write(className,sink);
	
	/* Write the tool's button slots: */
	sink.write<Card>(numButtonSlots);
	for(unsigned int buttonSlotIndex=0;buttonSlotIndex<numButtonSlots;++buttonSlotIndex)
		{
		sink.write<Card>(buttonSlots[buttonSlotIndex].deviceId);
		sink.write<Card>(buttonSlots[buttonSlotIndex].index);
		}
	
	/* Write the tool's valuator slots: */
	sink.write<Card>(numValuatorSlots);
	for(unsigned int valuatorSlotIndex=0;valuatorSlotIndex<numValuatorSlots;++valuatorSlotIndex)
		{
		sink.write<Card>(valuatorSlots[valuatorSlotIndex].deviceId);
		sink.write<Card>(valuatorSlots[valuatorSlotIndex].index);
		}
	}

/***************************************
Static elements of class CheriaProtocol:
***************************************/

const char* CheriaProtocol::protocolName="Cheria"; // How inventive
const unsigned int CheriaProtocol::protocolVersion=(2U<<16)+0U; // Version 2.0

}
