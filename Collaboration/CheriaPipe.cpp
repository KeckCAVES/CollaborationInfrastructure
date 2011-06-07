/***********************************************************************
CheriaPipe - Common interface between a Cheria server and a Cheria
client.
Copyright (c) 2010 Oliver Kreylos

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

#include <Collaboration/CheriaPipe.h>

#include <Misc/ReadBuffer.h>
#include <Misc/WriteBuffer.h>
#include <Misc/StringMarshaller.h>
#include <Geometry/OrthogonalTransformation.h>
#include <Vrui/InputDevice.h>

namespace Collaboration {

/**********************************************
Methods of class CheriaPipe::ServerDeviceState:
**********************************************/

CheriaPipe::ServerDeviceState::ServerDeviceState(CollaborationPipe& pipe)
	:newDevice(true),
	 buttonStates(0),
	 valuatorStates(0)
	{
	/* Read the track type: */
	trackType=pipe.read<int>();
	
	/* Read the number of buttons and allocate the state array: */
	numButtons=pipe.read<int>();
	buttonStates=new unsigned char[(numButtons+7)/8];
	
	/* Read the number of valuators and allocate the state array: */
	numValuators=pipe.read<int>();
	valuatorStates=new Scalar[numValuators];
	}

CheriaPipe::ServerDeviceState::~ServerDeviceState(void)
	{
	delete[] buttonStates;
	delete[] valuatorStates;
	}

template <class PipeParam>
void CheriaPipe::ServerDeviceState::writeLayout(PipeParam& pipe) const
	{
	pipe.template write<int>(trackType);
	pipe.template write<int>(numButtons);
	pipe.template write<int>(numValuators);
	}

void CheriaPipe::ServerDeviceState::read(CollaborationPipe& pipe)
	{
	/* Read the device's position and orientation: */
	Vector translation;
	pipe.read<Scalar>(translation.getComponents(),3);
	Scalar quaternion[4];
	pipe.read<Scalar>(quaternion,4);
	Scalar scaling=pipe.read<Scalar>();
	transform=OGTransform(translation,OGTransform::Rotation::fromQuaternion(quaternion),scaling);
	
	/* Read the device's ray direction: */
	pipe.read<Scalar>(rayDirection.getComponents(),3);
	
	/* Read the device's button states: */
	pipe.read<unsigned char>(buttonStates,(numButtons+7)/8);
	
	/* Read the device's valuator states: */
	pipe.read<Scalar>(valuatorStates,numValuators);
	}

template <class PipeParam>
void CheriaPipe::ServerDeviceState::write(PipeParam& pipe) const
	{
	/* Write the device's position and orientation: */
	pipe.template write<Scalar>(transform.getTranslation().getComponents(),3);
	pipe.template write<Scalar>(transform.getRotation().getQuaternion(),4);
	pipe.template write<Scalar>(transform.getScaling());
	
	/* Write the device's ray direction: */
	pipe.template write<Scalar>(rayDirection.getComponents(),3);
	
	/* Write the device's button states: */
	pipe.template write<unsigned char>(buttonStates,(numButtons+7)/8);
	
	/* Write the device's valuator states: */
	pipe.template write<Scalar>(valuatorStates,numValuators);
	}

/********************************************
Methods of class CheriaPipe::ServerToolState:
********************************************/

CheriaPipe::ServerToolState::ServerToolState(CollaborationPipe& pipe)
	:newTool(true),
	 className(0),
	 numButtonSlots(0),buttonSlots(0),
	 numValuatorSlots(0),valuatorSlots(0)
	{
	/* Read the tool's class name: */
	className=Misc::readCString(pipe);
	
	/* Read the tool's button slots: */
	numButtonSlots=pipe.read<int>();
	buttonSlots=new Slot[numButtonSlots];
	for(int buttonSlotIndex=0;buttonSlotIndex<numButtonSlots;++buttonSlotIndex)
		{
		buttonSlots[buttonSlotIndex].deviceId=pipe.read<unsigned int>();
		buttonSlots[buttonSlotIndex].index=pipe.read<int>();
		}
	
	/* Read the tool's valuator slots: */
	numValuatorSlots=pipe.read<int>();
	valuatorSlots=new Slot[numValuatorSlots];
	for(int valuatorSlotIndex=0;valuatorSlotIndex<numValuatorSlots;++valuatorSlotIndex)
		{
		valuatorSlots[valuatorSlotIndex].deviceId=pipe.read<unsigned int>();
		valuatorSlots[valuatorSlotIndex].index=pipe.read<int>();
		}
	}

CheriaPipe::ServerToolState::~ServerToolState(void)
	{
	delete[] className;
	delete[] buttonSlots;
	delete[] valuatorSlots;
	}

template <class PipeParam>
void CheriaPipe::ServerToolState::writeLayout(PipeParam& pipe) const
	{
	Misc::writeCString(className,pipe);
	
	/* Write the tool's button slots: */
	pipe.template write<int>(numButtonSlots);
	for(int buttonSlotIndex=0;buttonSlotIndex<numButtonSlots;++buttonSlotIndex)
		{
		pipe.template write<unsigned int>(buttonSlots[buttonSlotIndex].deviceId);
		pipe.template write<int>(buttonSlots[buttonSlotIndex].index);
		}
	
	/* Write the tool's valuator slots: */
	pipe.template write<int>(numValuatorSlots);
	for(int valuatorSlotIndex=0;valuatorSlotIndex<numValuatorSlots;++valuatorSlotIndex)
		{
		pipe.template write<unsigned int>(valuatorSlots[valuatorSlotIndex].deviceId);
		pipe.template write<int>(valuatorSlots[valuatorSlotIndex].index);
		}
	}

/***********************************
Static elements of class CheriaPipe:
***********************************/

const char* CheriaPipe::protocolName="Cheria"; // How inventive
const unsigned int CheriaPipe::protocolVersion=1*65536+0; // Version 1.0

/****************************************************
Force instantiation of all standard template methods:
****************************************************/

template void CheriaPipe::ServerDeviceState::writeLayout<CollaborationPipe>(CollaborationPipe&) const;
template void CheriaPipe::ServerDeviceState::writeLayout<Misc::WriteBuffer>(Misc::WriteBuffer&) const;
template void CheriaPipe::ServerDeviceState::write<CollaborationPipe>(CollaborationPipe&) const;
template void CheriaPipe::ServerDeviceState::write<Misc::WriteBuffer>(Misc::WriteBuffer&) const;
template void CheriaPipe::ServerToolState::writeLayout<CollaborationPipe>(CollaborationPipe&) const;
template void CheriaPipe::ServerToolState::writeLayout<Misc::WriteBuffer>(Misc::WriteBuffer&) const;

}
