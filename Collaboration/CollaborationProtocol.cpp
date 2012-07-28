/***********************************************************************
CollaborationProtocol - Class defining the communication protocol
between a collaboration client and a collaboration server.
Copyright (c) 2007-2011 Oliver Kreylos

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

#include <Collaboration/CollaborationProtocol.h>

#include <IO/File.h>

namespace Collaboration {

/***************************************************
Methods of class CollaborationProtocol::ClientState:
***************************************************/

CollaborationProtocol::ClientState::ClientState(void)
	:updateMask(NO_CHANGE),
	 inchFactor(1),
	 displayCenter(Point::origin),
	 displaySize(1),
	 forward(0,1,0),up(0,0,1),
	 floorPlane(Vector(0,0,1),0),
	 numViewers(0),viewerStates(0)
	{
	}

CollaborationProtocol::ClientState& CollaborationProtocol::ClientState::operator=(const CollaborationProtocol::ClientState& source)
	{
	if(&source!=this)
		{
		/* Copy the physical environment state: */
		updateMask=source.updateMask;
		inchFactor=source.inchFactor;
		displayCenter=source.displayCenter;
		displaySize=source.displaySize;
		forward=source.forward;
		up=source.up;
		floorPlane=source.floorPlane;
		
		/* Copy the client name: */
		clientName=source.clientName;
		
		/* Copy the array of viewer states: */
		if(numViewers!=source.numViewers)
			{
			delete[] viewerStates;
			numViewers=source.numViewers;
			viewerStates=numViewers>0?new ONTransform[numViewers]:0;
			}
		for(unsigned int i=0;i<numViewers;++i)
			viewerStates[i]=source.viewerStates[i];
		
		/* Copy the navigation transformation: */
		navTransform=source.navTransform;
		}
	return *this;
	}

CollaborationProtocol::ClientState::~ClientState(void)
	{
	delete[] viewerStates;
	}

bool CollaborationProtocol::ClientState::resize(unsigned int newNumViewers)
	{
	if(newNumViewers!=numViewers)
		{
		/* Re-allocate the viewer states array: */
		delete[] viewerStates;
		numViewers=newNumViewers;
		viewerStates=numViewers>0?new ONTransform[numViewers]:0;
		updateMask|=NUM_VIEWERS;
		return true;
		}
	
	return false;
	}

/**************************************
Methods of class CollaborationProtocol:
**************************************/

void CollaborationProtocol::readClientState(CollaborationProtocol::ClientState& clientState,IO::File& source)
	{
	/* Read this update's update mask: */
	unsigned int newUpdateMask=source.read<Byte>();
	
	if(newUpdateMask&ClientState::ENVIRONMENT)
		{
		/* Read the client's physical environment definition: */
		read(clientState.inchFactor,source);
		read(clientState.displayCenter,source);
		read(clientState.displaySize,source);
		read(clientState.forward,source);
		read(clientState.up,source);
		read(clientState.floorPlane,source);
		}
	
	if(newUpdateMask&ClientState::CLIENTNAME)
		{
		/* Read the client's display name: */
		read(clientState.clientName,source);
		}
	
	if(newUpdateMask&ClientState::NUM_VIEWERS)
		{
		/* Read the new number of viewers and resize the state array: */
		unsigned int newNumViewers=source.read<Card>();
		clientState.resize(newNumViewers);
		}
	
	if(newUpdateMask&ClientState::VIEWER)
		{
		/* Read the client's viewer states: */
		for(unsigned int i=0;i<clientState.numViewers;++i)
			read(clientState.viewerStates[i],source);
		}
	
	if(newUpdateMask&&ClientState::NAVTRANSFORM)
		{
		/* Read the navigation transformation: */
		read(clientState.navTransform,source);
		}
	
	/* Update the client state's update mask: */
	clientState.updateMask|=newUpdateMask;
	}

void CollaborationProtocol::writeClientState(unsigned int updateMask,const CollaborationProtocol::ClientState& clientState,IO::File& sink)
	{
	/* Write the update mask: */
	sink.write<Byte>(updateMask);
	
	if(updateMask&ClientState::ENVIRONMENT)
		{
		/* Write the client's physical environment definition: */
		write(clientState.inchFactor,sink);
		write(clientState.displayCenter,sink);
		write(clientState.displaySize,sink);
		write(clientState.forward,sink);
		write(clientState.up,sink);
		write(clientState.floorPlane,sink);
		}
	
	if(updateMask&ClientState::CLIENTNAME)
		{
		/* Write the client's display name: */
		write(clientState.clientName,sink);
		}
	
	if(updateMask&ClientState::NUM_VIEWERS)
		{
		/* Write the new number of viewers: */
		sink.write<Card>(clientState.numViewers);
		}
	
	if(updateMask&ClientState::VIEWER)
		{
		/* Write the client's viewer states: */
		for(unsigned int i=0;i<clientState.numViewers;++i)
			write(clientState.viewerStates[i],sink);
		}
	
	if(updateMask&&ClientState::NAVTRANSFORM)
		{
		/* Write the navigation transformation: */
		write(clientState.navTransform,sink);
		}
	}

}
