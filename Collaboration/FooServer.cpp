/***********************************************************************
FooServer - Dummy protocol plug-in class to stress-test the plug-in
mechanism.
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

#include <Collaboration/FooCrapSender.h>

#include <Collaboration/FooServer.h>

#define DUMP_PROTOCOL 0

namespace Collaboration {

/***************************************
Methods of class FooServer::ClientState:
***************************************/

FooServer::ClientState::ClientState(void)
	{
	#if DUMP_PROTOCOL
	std::cout<<"FooServer::ClientState::ClientState"<<std::endl;
	#endif
	}

FooServer::ClientState::~ClientState(void)
	{
	#if DUMP_PROTOCOL
	std::cout<<"FooServer::ClientState::~ClientState"<<std::endl;
	#endif
	}

/**************************
Methods of class FooServer:
**************************/

FooServer::FooServer(void)
	{
	#if DUMP_PROTOCOL
	std::cout<<"FooServer::FooServer"<<std::endl;
	#endif
	}

FooServer::~FooServer(void)
	{
	#if DUMP_PROTOCOL
	std::cout<<"FooServer::~FooServer"<<std::endl;
	#endif
	}

const char* FooServer::getName(void) const
	{
	#if DUMP_PROTOCOL
	std::cout<<"FooServer::getName"<<std::endl;
	#endif
	
	return "Foo";
	}

unsigned int FooServer::getNumMessages(void) const
	{
	#if DUMP_PROTOCOL
	std::cout<<"FooServer::getNumMessages"<<std::endl;
	#endif
	
	return 1; // We have a special CRAP message!
	}

ProtocolServer::ClientState* FooServer::receiveConnectRequest(unsigned int protocolMessageLength,CollaborationPipe& pipe)
	{
	#if DUMP_PROTOCOL
	std::cout<<"FooServer::receiveConnectRequest"<<std::endl;
	#endif
	
	receiveRandomCrap(pipe);
	
	return new ClientState();
	}

void FooServer::sendConnectReply(ProtocolServer::ClientState* cs,CollaborationPipe& pipe)
	{
	#if DUMP_PROTOCOL
	std::cout<<"FooServer::sendConnectReply"<<std::endl;
	#endif
	
	ClientState* myCs=dynamic_cast<ClientState*>(cs);
	if(myCs==0)
		Misc::throwStdErr("FooServer::sendConnectReply: Mismatching client state object type");
	
	sendRandomCrap(pipe);
	}

void FooServer::sendConnectReject(ProtocolServer::ClientState* cs,CollaborationPipe& pipe)
	{
	#if DUMP_PROTOCOL
	std::cout<<"FooServer::sendConnectReject"<<std::endl;
	#endif
	
	ClientState* myCs=dynamic_cast<ClientState*>(cs);
	if(myCs==0)
		Misc::throwStdErr("FooServer::sendConnectReject: Mismatching client state object type");
	
	sendRandomCrap(pipe);
	}

void FooServer::receiveDisconnectRequest(ProtocolServer::ClientState* cs,CollaborationPipe& pipe)
	{
	#if DUMP_PROTOCOL
	std::cout<<"FooServer::receiveDisconnectRequest"<<std::endl;
	#endif
	
	ClientState* myCs=dynamic_cast<ClientState*>(cs);
	if(myCs==0)
		Misc::throwStdErr("FooServer::receiveDisconnectRequest: Mismatching client state object type");
	
	receiveRandomCrap(pipe);
	}

void FooServer::sendDisconnectReply(ProtocolServer::ClientState* cs,CollaborationPipe& pipe)
	{
	#if DUMP_PROTOCOL
	std::cout<<"FooServer::sendDisconnectReply"<<std::endl;
	#endif
	
	ClientState* myCs=dynamic_cast<ClientState*>(cs);
	if(myCs==0)
		Misc::throwStdErr("FooServer::sendDisconnectReply: Mismatching client state object type");
	
	sendRandomCrap(pipe);
	}

void FooServer::receiveClientUpdate(ProtocolServer::ClientState* cs,CollaborationPipe& pipe)
	{
	#if DUMP_PROTOCOL
	std::cout<<"FooServer::receiveClientUpdate"<<std::endl;
	#endif
	
	ClientState* myCs=dynamic_cast<ClientState*>(cs);
	if(myCs==0)
		Misc::throwStdErr("FooServer::receiveClientUpdate: Mismatching client state object type");
	
	receiveRandomCrap(pipe);
	}

void FooServer::sendClientConnect(ProtocolServer::ClientState* sourceCs,ProtocolServer::ClientState* destCs,CollaborationPipe& pipe)
	{
	#if DUMP_PROTOCOL
	std::cout<<"FooServer::sendClientConnect"<<std::endl;
	#endif
	
	ClientState* mySourceCs=dynamic_cast<ClientState*>(sourceCs);
	ClientState* myDestCs=dynamic_cast<ClientState*>(destCs);
	if(mySourceCs==0||myDestCs==0)
		Misc::throwStdErr("FooServer::sendClientConnect: Mismatching client state object type");
	
	sendRandomCrap(pipe);
	}

void FooServer::sendClientDisconnect(ProtocolServer::ClientState* sourceCs,ProtocolServer::ClientState* destCs,CollaborationPipe& pipe)
	{
	#if DUMP_PROTOCOL
	std::cout<<"FooServer::sendClientDisconnect"<<std::endl;
	#endif
	
	ClientState* mySourceCs=dynamic_cast<ClientState*>(sourceCs);
	ClientState* myDestCs=dynamic_cast<ClientState*>(destCs);
	if(mySourceCs==0||myDestCs==0)
		Misc::throwStdErr("FooServer::sendClientDisconnect: Mismatching client state object type");
	
	sendRandomCrap(pipe);
	}

void FooServer::sendServerUpdate(ProtocolServer::ClientState* destCs,CollaborationPipe& pipe)
	{
	#if DUMP_PROTOCOL
	std::cout<<"FooServer::sendServerUpdate"<<std::endl;
	#endif
	
	ClientState* myDestCs=dynamic_cast<ClientState*>(destCs);
	if(myDestCs==0)
		Misc::throwStdErr("FooServer::sendServerUpdate: Mismatching client state object type");
	
	sendRandomCrap(pipe);
	}

void FooServer::sendServerUpdate(ProtocolServer::ClientState* sourceCs,ProtocolServer::ClientState* destCs,CollaborationPipe& pipe)
	{
	#if DUMP_PROTOCOL
	std::cout<<"FooServer::sendServerUpdate"<<std::endl;
	#endif
	
	ClientState* mySourceCs=dynamic_cast<ClientState*>(sourceCs);
	ClientState* myDestCs=dynamic_cast<ClientState*>(destCs);
	if(mySourceCs==0||myDestCs==0)
		Misc::throwStdErr("FooServer::sendServerUpdate: Mismatching client state object type");
	
	sendRandomCrap(pipe);
	}

bool FooServer::handleMessage(ProtocolServer::ClientState* cs,unsigned int messageId,CollaborationPipe& pipe)
	{
	#if DUMP_PROTOCOL
	std::cout<<"FooServer::handleMessage"<<std::endl;
	#endif
	
	ClientState* myCs=dynamic_cast<ClientState*>(cs);
	if(myCs==0)
		Misc::throwStdErr("FooServer::handleMessage: Mismatching client state object type");
	
	receiveRandomCrap(pipe);
	
	return true;
	}

void FooServer::connectClient(ProtocolServer::ClientState* cs)
	{
	#if DUMP_PROTOCOL
	std::cout<<"FooServer::connectClient"<<std::endl;
	#endif
	
	ClientState* myCs=dynamic_cast<ClientState*>(cs);
	if(myCs==0)
		Misc::throwStdErr("FooServer::connectClient: Mismatching client state object type");
	}

void FooServer::disconnectClient(ProtocolServer::ClientState* cs)
	{
	#if DUMP_PROTOCOL
	std::cout<<"FooServer::disconnectClient"<<std::endl;
	#endif
	
	ClientState* myCs=dynamic_cast<ClientState*>(cs);
	if(myCs==0)
		Misc::throwStdErr("FooServer::disconnectClient: Mismatching client state object type");
	}

void FooServer::beforeServerUpdate(ProtocolServer::ClientState* destCs,CollaborationPipe& pipe)
	{
	#if DUMP_PROTOCOL
	std::cout<<"FooServer::beforeServerUpdate"<<std::endl;
	#endif
	
	ClientState* myDestCs=dynamic_cast<ClientState*>(destCs);
	if(myDestCs==0)
		Misc::throwStdErr("FooServer::beforeServerUpdate: Mismatching client state object type");
	
	/* Need to wrap our crap into an actual message packet: */
	pipe.writeMessage(getMessageIdBase());
	sendRandomCrap(pipe);
	}

void FooServer::afterServerUpdate(ProtocolServer::ClientState* cs)
	{
	#if DUMP_PROTOCOL
	std::cout<<"FooServer::afterServerUpdate"<<std::endl;
	#endif
	
	ClientState* myCs=dynamic_cast<ClientState*>(cs);
	if(myCs==0)
		Misc::throwStdErr("FooServer::afterServerUpdate: Mismatching client state object type");
	}

}
