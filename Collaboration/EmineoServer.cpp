/***********************************************************************
EmineoServer - Server object to implement the Emineo 3D video tele-
immersion protocol.
Copyright (c) 2009 Oliver Kreylos

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

#include <iostream>
#include <Misc/ThrowStdErr.h>

#include <Collaboration/CollaborationPipe.h>

#include <Collaboration/EmineoServer.h>

namespace Collaboration {

/******************************************
Methods of class EmineoServer::ClientState:
******************************************/

EmineoServer::ClientState::ClientState(const std::string& sGatewayHostname,int sGatewayPort)
	:gatewayHostname(sGatewayHostname),gatewayPort(sGatewayPort),
	 hasSource(gatewayHostname!=""&&gatewayPort>0)
	{
	if(hasSource)
		std::cout<<"EmineoServer::ClientState::ClientState: Connected Emineo client with gateway "<<gatewayHostname<<", port "<<gatewayPort<<std::endl;
	else
		std::cout<<"EmineoServer::ClientState::ClientState: Connected passive Emineo client"<<std::endl;
	}

EmineoServer::ClientState::~ClientState(void)
	{
	}

/*****************************
Methods of class EmineoServer:
*****************************/

EmineoServer::EmineoServer(void)
	{
	}

EmineoServer::~EmineoServer(void)
	{
	}

const char* EmineoServer::getName(void) const
	{
	return protocolName;
	}

ProtocolServer::ClientState* EmineoServer::receiveConnectRequest(unsigned int protocolMessageLength,CollaborationPipe& pipe)
	{
	/* Read the gateway hostname / port pair: */
	std::string gatewayHostname=pipe.read<std::string>();
	int gatewayPort=pipe.read<int>();
	
	/* Check for correctness: */
	if(protocolMessageLength!=sizeof(unsigned int)+gatewayHostname.length()+sizeof(int))
		{
		/* Must be a protocol error; return failure: */
		return 0;
		}
	
	/* Create and return a client state object: */
	return new ClientState(gatewayHostname,gatewayPort);
	}

void EmineoServer::receiveClientUpdate(ProtocolServer::ClientState* cs,CollaborationPipe& pipe)
	{
	/* Get a handle on the Emineo state object: */
	ClientState* myCs=dynamic_cast<ClientState*>(cs);
	if(myCs==0)
		Misc::throwStdErr("EmineoServer::receiveClientUpdate: Client state object has mismatching type");
	
	if(myCs->hasSource)
		{
		/* Read the client's new camera transformation: */
		myCs->cameraTransform=receiveOGTransform(pipe);
		}
	}

void EmineoServer::sendClientConnect(ProtocolServer::ClientState* sourceCs,ProtocolServer::ClientState* destCs,CollaborationPipe& pipe)
	{
	/* Get a handle on the Emineo state object: */
	ClientState* mySourceCs=dynamic_cast<ClientState*>(sourceCs);
	if(mySourceCs==0)
		Misc::throwStdErr("EmineoServer::sendClientConnect: Client state object has mismatching type");
	
	/* Send the client's gateway hostname and port: */
	pipe.write<std::string>(mySourceCs->gatewayHostname);
	pipe.write<int>(mySourceCs->gatewayPort);
	}

void EmineoServer::sendServerUpdate(ProtocolServer::ClientState* sourceCs,ProtocolServer::ClientState* destCs,CollaborationPipe& pipe)
	{
	/* Get a handle on the Emineo state object: */
	ClientState* mySourceCs=dynamic_cast<ClientState*>(sourceCs);
	if(mySourceCs==0)
		Misc::throwStdErr("EmineoServer::sendServerUpdate: Client state object has mismatching type");
	
	if(mySourceCs->hasSource)
		{
		/* Send the source client's current camera transformation: */
		sendOGTransform(mySourceCs->cameraTransform,pipe);
		}
	}

}
