/***********************************************************************
CollaborationPipe - Class defining the communication protocol between a
collaboration client and a collaboration server.
Copyright (c) 2007-2009 Oliver Kreylos

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

#include <Geometry/Rotation.h>
#include <Geometry/OrthogonalTransformation.h>
#include <Vrui/InputDevice.h>
#include <Vrui/Viewer.h>
#include <Vrui/Vrui.h>

#include <Collaboration/CollaborationPipe.h>

namespace Collaboration {

/***********************************************
Methods of class CollaborationPipe::ClientState:
***********************************************/

CollaborationPipe::ClientState::~ClientState(void)
	{
	delete[] viewerStates;
	delete[] inputDeviceStates;
	}

CollaborationPipe::ClientState& CollaborationPipe::ClientState::resize(unsigned int newNumViewers,unsigned int newNumInputDevices)
	{
	if(newNumViewers!=numViewers)
		{
		/* Re-allocate the viewer states array: */
		delete[] viewerStates;
		numViewers=newNumViewers;
		viewerStates=numViewers>0?new Vrui::NavTrackerState[numViewers]:0;
		}
	if(newNumInputDevices!=numInputDevices)
		{
		/* Re-allocate the input device states array: */
		delete[] inputDeviceStates;
		numInputDevices=newNumInputDevices;
		inputDeviceStates=numInputDevices>0?new Vrui::NavTrackerState[numInputDevices]:0;
		}
	
	return *this;
	}

CollaborationPipe::ClientState& CollaborationPipe::ClientState::updateFromVrui(void)
	{
	/* Get the inverse navigation transformation: */
	const Vrui::NavTransform& invNav=Vrui::getInverseNavigationTransformation();
	
	/* Get the definition of the physical environment: */
	center=invNav.transform(Vrui::getDisplayCenter());
	forward=invNav.transform(Vrui::getForwardDirection());
	up=invNav.transform(Vrui::getUpDirection());
	size=invNav.getScaling()*Vrui::getDisplaySize();
	
	/* Get the positions/orientations of all viewers and input devices: */
	resize(Vrui::getNumViewers(),Vrui::getNumInputDevices());
	for(unsigned int i=0;i<numViewers;++i)
		viewerStates[i]=invNav*Vrui::NavTrackerState(Vrui::getViewer(i)->getHeadTransformation());
	for(unsigned int i=0;i<numInputDevices;++i)
		inputDeviceStates[i]=invNav*Vrui::NavTrackerState(Vrui::getInputDevice(i)->getTransformation());
	
	return *this;
	}

/**********************************
Methods of class CollaborationPipe:
**********************************/

CollaborationPipe::CollaborationPipe(const char* hostName,int portID,Comm::MulticastPipe* sPipe)
	:Comm::ClusterPipe(hostName,portID,sPipe)
	{
	}

CollaborationPipe::CollaborationPipe(const Comm::TCPSocket* sSocket,Comm::MulticastPipe* sPipe)
	:Comm::ClusterPipe(sSocket,sPipe)
	{
	}

void CollaborationPipe::writeTrackerState(const Vrui::NavTrackerState& trackerState)
	{
	write<Vrui::Scalar>(trackerState.getTranslation().getComponents(),3);
	write<Vrui::Scalar>(trackerState.getRotation().getQuaternion(),4);
	write<Vrui::Scalar>(trackerState.getScaling());
	}

Vrui::NavTrackerState CollaborationPipe::readTrackerState(void)
	{
	Vrui::Vector translation;
	read<Vrui::Scalar>(translation.getComponents(),3);
	Vrui::Scalar rotationQuaternion[4];
	read<Vrui::Scalar>(rotationQuaternion,4);
	Vrui::Scalar scaling=read<Vrui::Scalar>();
	return Vrui::NavTrackerState(translation,Vrui::Rotation::fromQuaternion(rotationQuaternion),scaling);
	}

void CollaborationPipe::writeClientState(const CollaborationPipe::ClientState& clientState)
	{
	/* Write the client's physical environment definition: */
	write<Vrui::Scalar>(clientState.center.getComponents(),3);
	write<Vrui::Scalar>(clientState.forward.getComponents(),3);
	write<Vrui::Scalar>(clientState.up.getComponents(),3);
	write<Vrui::Scalar>(clientState.size);
	
	/* Write the client's viewer and input device states: */
	write<unsigned int>(clientState.numViewers);
	write<unsigned int>(clientState.numInputDevices);
	for(unsigned int i=0;i<clientState.numViewers;++i)
		writeTrackerState(clientState.viewerStates[i]);
	for(unsigned int i=0;i<clientState.numInputDevices;++i)
		writeTrackerState(clientState.inputDeviceStates[i]);
	}

CollaborationPipe::ClientState& CollaborationPipe::readClientState(CollaborationPipe::ClientState& clientState)
	{
	/* Read the client's physical environment definition: */
	read<Vrui::Scalar>(clientState.center.getComponents(),3);
	read<Vrui::Scalar>(clientState.forward.getComponents(),3);
	read<Vrui::Scalar>(clientState.up.getComponents(),3);
	read<Vrui::Scalar>(clientState.size);
	
	/* Read the client's viewer and input device states: */
	unsigned int newNumViewers=read<unsigned int>();
	unsigned int newNumInputDevices=read<unsigned int>();
	clientState.resize(newNumViewers,newNumInputDevices);
	for(unsigned int i=0;i<clientState.numViewers;++i)
		clientState.viewerStates[i]=readTrackerState();
	for(unsigned int i=0;i<clientState.numInputDevices;++i)
		clientState.inputDeviceStates[i]=readTrackerState();
	
	return clientState;
	}

}
