/***********************************************************************
CollaborationPipe - Class defining the communication protocol between a
collaboration client and a collaboration server.
Copyright (c) 2007-2010 Oliver Kreylos

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

#include <Collaboration/CollaborationPipe.h>

#include <Geometry/Rotation.h>
#include <Vrui/InputDevice.h>
#include <Vrui/Viewer.h>
#include <Vrui/Vrui.h>

namespace Collaboration {

/***********************************************
Methods of class CollaborationPipe::ClientState:
***********************************************/

CollaborationPipe::ClientState::ClientState(void)
	:numViewers(0),viewerStates(0)
	 #ifdef COLLABORATION_SHARE_DEVICES
	 ,numInputDevices(0),inputDeviceStates(0)
	 #endif
	{
	}

CollaborationPipe::ClientState::~ClientState(void)
	{
	delete[] viewerStates;
	#ifdef COLLABORATION_SHARE_DEVICES
	delete[] inputDeviceStates;
	#endif
	}

#ifdef COLLABORATION_SHARE_DEVICES
CollaborationPipe::ClientState& CollaborationPipe::ClientState::resize(unsigned int newNumViewers,unsigned int newNumInputDevices)
#else
CollaborationPipe::ClientState& CollaborationPipe::ClientState::resize(unsigned int newNumViewers)
#endif
	{
	if(newNumViewers!=numViewers)
		{
		/* Re-allocate the viewer states array: */
		delete[] viewerStates;
		numViewers=newNumViewers;
		viewerStates=numViewers>0?new OGTransform[numViewers]:0;
		}
	#ifdef COLLABORATION_SHARE_DEVICES
	if(newNumInputDevices!=numInputDevices)
		{
		/* Re-allocate the input device states array: */
		delete[] inputDeviceStates;
		numInputDevices=newNumInputDevices;
		inputDeviceStates=numInputDevices>0?new OGTransform[numInputDevices]:0;
		}
	#endif
	
	return *this;
	}

CollaborationPipe::ClientState& CollaborationPipe::ClientState::updateFromVrui(void)
	{
	/* Get the inverse navigation transformation: */
	const Vrui::NavTransform& invNav=Vrui::getInverseNavigationTransformation();
	
	/* Get the definition of the physical environment: */
	center=Point(invNav.transform(Vrui::getDisplayCenter()));
	forward=Vector(invNav.transform(Vrui::getForwardDirection()));
	up=Vector(invNav.transform(Vrui::getUpDirection()));
	size=Scalar(invNav.getScaling()*Vrui::getDisplaySize());
	inchScale=Scalar(invNav.getScaling()*Vrui::getInchFactor());
	
	#ifdef COLLABORATION_SHARE_DEVICES
	/* Get the positions/orientations of all viewers and input devices: */
	resize(Vrui::getNumViewers(),Vrui::getNumInputDevices());
	for(unsigned int i=0;i<numViewers;++i)
		viewerStates[i]=OGTransform(invNav*Vrui::NavTrackerState(Vrui::getViewer(i)->getHeadTransformation()));
	for(unsigned int i=0;i<numInputDevices;++i)
		inputDeviceStates[i]=OGTransform(invNav*Vrui::NavTrackerState(Vrui::getInputDevice(i)->getTransformation()));
	#else
	/* Get the positions/orientations of all viewers: */
	resize(Vrui::getNumViewers());
	for(unsigned int i=0;i<numViewers;++i)
		viewerStates[i]=OGTransform(invNav*Vrui::NavTrackerState(Vrui::getViewer(i)->getHeadTransformation()));
	#endif
	
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

void CollaborationPipe::writeTrackerState(const CollaborationPipe::OGTransform& trackerState)
	{
	write<Scalar>(trackerState.getTranslation().getComponents(),3);
	write<Scalar>(trackerState.getRotation().getQuaternion(),4);
	write<Scalar>(trackerState.getScaling());
	}

CollaborationPipe::OGTransform CollaborationPipe::readTrackerState(void)
	{
	Vector translation;
	read<Scalar>(translation.getComponents(),3);
	Scalar rotationQuaternion[4];
	read<Scalar>(rotationQuaternion,4);
	Scalar scaling=read<Scalar>();
	return OGTransform(translation,OGTransform::Rotation::fromQuaternion(rotationQuaternion),scaling);
	}

void CollaborationPipe::writeClientState(const CollaborationPipe::ClientState& clientState)
	{
	/* Write the client's physical environment definition: */
	write<Scalar>(clientState.center.getComponents(),3);
	write<Scalar>(clientState.forward.getComponents(),3);
	write<Scalar>(clientState.up.getComponents(),3);
	write<Scalar>(clientState.size);
	write<Scalar>(clientState.inchScale);
	
	/* Write the client's viewer and input device states: */
	write<unsigned int>(clientState.numViewers);
	#ifdef COLLABORATION_SHARE_DEVICES
	write<unsigned int>(clientState.numInputDevices);
	#endif
	for(unsigned int i=0;i<clientState.numViewers;++i)
		writeTrackerState(clientState.viewerStates[i]);
	#ifdef COLLABORATION_SHARE_DEVICES
	for(unsigned int i=0;i<clientState.numInputDevices;++i)
		writeTrackerState(clientState.inputDeviceStates[i]);
	#endif
	}

CollaborationPipe::ClientState& CollaborationPipe::readClientState(CollaborationPipe::ClientState& clientState)
	{
	/* Read the client's physical environment definition: */
	read<Scalar>(clientState.center.getComponents(),3);
	read<Scalar>(clientState.forward.getComponents(),3);
	read<Scalar>(clientState.up.getComponents(),3);
	read<Scalar>(clientState.size);
	read<Scalar>(clientState.inchScale);
	
	/* Read the client's viewer and input device states: */
	unsigned int newNumViewers=read<unsigned int>();
	#ifdef COLLABORATION_SHARE_DEVICES
	unsigned int newNumInputDevices=read<unsigned int>();
	clientState.resize(newNumViewers,newNumInputDevices);
	#else
	clientState.resize(newNumViewers);
	#endif
	for(unsigned int i=0;i<clientState.numViewers;++i)
		clientState.viewerStates[i]=readTrackerState();
	#ifdef COLLABORATION_SHARE_DEVICES
	for(unsigned int i=0;i<clientState.numInputDevices;++i)
		clientState.inputDeviceStates[i]=readTrackerState();
	#endif
	
	return clientState;
	}

}
