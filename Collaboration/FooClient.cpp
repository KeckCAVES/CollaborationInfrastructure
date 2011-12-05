/***********************************************************************
FooClient - Dummy protocol plug-in class to stress-test the plug-in
mechanism.
Copyright (c) 2009-2011 Oliver Kreylos

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

#include <Collaboration/FooClient.h>

#if DUMP_PROTOCOL
#include <iostream>
#endif
#include <Misc/ThrowStdErr.h>
#include <Comm/NetPipe.h>

namespace Collaboration {

/*********************************************
Methods of class FooClient::RemoteClientState:
*********************************************/

FooClient::RemoteClientState::RemoteClientState(void)
	{
	#if DUMP_PROTOCOL
	std::cout<<"FooClient::RemoteClientState::RemoteClientState"<<std::endl;
	#endif
	}

FooClient::RemoteClientState::~RemoteClientState(void)
	{
	#if DUMP_PROTOCOL
	std::cout<<"FooClient::RemoteClientState::~RemoteClientState"<<std::endl;
	#endif
	}

/**************************
Methods of class FooClient:
**************************/

FooClient::FooClient(void)
	{
	#if DUMP_PROTOCOL
	std::cout<<"FooClient::FooClient"<<std::endl;
	#endif
	}

FooClient::~FooClient(void)
	{
	#if DUMP_PROTOCOL
	std::cout<<"FooClient::~FooClient"<<std::endl;
	#endif
	}

const char* FooClient::getName(void) const
	{
	#if DUMP_PROTOCOL
	std::cout<<"FooClient::getName"<<std::endl;
	#endif
	
	return "Foo";
	}

unsigned int FooClient::getNumMessages(void) const
	{
	#if DUMP_PROTOCOL
	std::cout<<"FooClient::getNumMessages"<<std::endl;
	#endif
	
	return 1;
	}

void FooClient::initialize(CollaborationClient* sClient,Misc::ConfigurationFileSection& configFileSection)
	{
	/* Call base class method: */
	ProtocolClient::initialize(sClient,configFileSection);
	
	#if DUMP_PROTOCOL
	std::cout<<"FooClient::initialize"<<std::endl;
	#endif
	}

void FooClient::sendConnectRequest(Comm::NetPipe& pipe)
	{
	#if DUMP_PROTOCOL
	std::cout<<"FooClient::sendConnectRequest"<<std::endl;
	#endif
	
	/* Need to send fixed-length random crap: */
	unsigned int messageSize=Math::randUniformCO(0,64)+32;
	pipe.write<Card>(messageSize+sizeof(Card)+sizeof(Card)); // First time is for base collaboration server!
	pipe.write<Card>(messageSize); // This one is for Foo server!
	unsigned int sumTotal=0;
	for(unsigned int i=0;i<messageSize;++i)
		{
		Byte value=Byte(Math::randUniformCO(0,256));
		pipe.write<Byte>(value);
		sumTotal+=value;
		}
	pipe.write<Card>(sumTotal);
	#if DUMP_PROTOCOL
	std::cout<<"Sent "<<messageSize<<" bytes with checksum "<<sumTotal<<std::endl;
	#endif
	}

void FooClient::receiveConnectReply(Comm::NetPipe& pipe)
	{
	#if DUMP_PROTOCOL
	std::cout<<"FooClient::receiveConnectReply"<<std::endl;
	#endif
	
	receiveRandomCrap(pipe);
	}

void FooClient::receiveConnectReject(Comm::NetPipe& pipe)
	{
	#if DUMP_PROTOCOL
	std::cout<<"FooClient::receiveConnectReject"<<std::endl;
	#endif
	
	receiveRandomCrap(pipe);
	}

void FooClient::sendDisconnectRequest(Comm::NetPipe& pipe)
	{
	#if DUMP_PROTOCOL
	std::cout<<"FooClient::sendDisconnectRequest"<<std::endl;
	#endif
	
	sendRandomCrap(pipe);
	}

void FooClient::receiveDisconnectReply(Comm::NetPipe& pipe)
	{
	#if DUMP_PROTOCOL
	std::cout<<"FooClient::receiveDisconnectReply"<<std::endl;
	#endif
	
	receiveRandomCrap(pipe);
	}

ProtocolClient::RemoteClientState* FooClient::receiveClientConnect(Comm::NetPipe& pipe)
	{
	#if DUMP_PROTOCOL
	std::cout<<"FooClient::receiveClientConnect"<<std::endl;
	#endif
	
	receiveRandomCrap(pipe);
	
	return new RemoteClientState;
	}

bool FooClient::receiveServerUpdate(Comm::NetPipe& pipe)
	{
	#if DUMP_PROTOCOL
	std::cout<<"FooClient::receiveServerUpdate"<<std::endl;
	#endif
	
	receiveRandomCrap(pipe);
	
	return false;
	}

bool FooClient::receiveServerUpdate(ProtocolClient::RemoteClientState* rcs,Comm::NetPipe& pipe)
	{
	#if DUMP_PROTOCOL
	std::cout<<"FooClient::receiveServerUpdate"<<std::endl;
	#endif
	
	RemoteClientState* myRcs=dynamic_cast<RemoteClientState*>(rcs);
	if(myRcs==0)
		Misc::throwStdErr("FooClient::receiveServerUpdate: Mismatching remote client state object type");
	
	receiveRandomCrap(pipe);
	
	return false;
	}

void FooClient::sendClientUpdate(Comm::NetPipe& pipe)
	{
	#if DUMP_PROTOCOL
	std::cout<<"FooClient::sendClientUpdate"<<std::endl;
	#endif
	
	sendRandomCrap(pipe);
	}

void FooClient::rejectedByServer(void)
	{
	#if DUMP_PROTOCOL
	std::cout<<"FooClient::rejectedByServer"<<std::endl;
	#endif
	}

void FooClient::connectClient(ProtocolClient::RemoteClientState* rcs)
	{
	#if DUMP_PROTOCOL
	std::cout<<"FooClient::connectClient"<<std::endl;
	#endif
	
	RemoteClientState* myRcs=dynamic_cast<RemoteClientState*>(rcs);
	if(myRcs==0)
		Misc::throwStdErr("FooClient::connectClient: Mismatching remote client state object type");
	}

void FooClient::disconnectClient(ProtocolClient::RemoteClientState* rcs)
	{
	#if DUMP_PROTOCOL
	std::cout<<"FooClient::disconnectClient"<<std::endl;
	#endif
	
	RemoteClientState* myRcs=dynamic_cast<RemoteClientState*>(rcs);
	if(myRcs==0)
		Misc::throwStdErr("FooClient::disconnectClient: Mismatching remote client state object type");
	}

void FooClient::frame(void)
	{
	#if DUMP_PROTOCOL
	std::cout<<"FooClient::frame"<<std::endl;
	#endif
	}

void FooClient::frame(ProtocolClient::RemoteClientState* rcs)
	{
	#if DUMP_PROTOCOL
	std::cout<<"FooClient::frame"<<std::endl;
	#endif
	
	RemoteClientState* myRcs=dynamic_cast<RemoteClientState*>(rcs);
	if(myRcs==0)
		Misc::throwStdErr("FooClient::frame: Mismatching remote client state object type");
	}

void FooClient::glRenderAction(GLContextData& contextData) const
	{
	#if DUMP_PROTOCOL
	std::cout<<"FooClient::glRenderAction"<<std::endl;
	#endif
	}

void FooClient::glRenderAction(const ProtocolClient::RemoteClientState* rcs,GLContextData& contextData) const
	{
	#if DUMP_PROTOCOL
	std::cout<<"FooClient::glRenderAction"<<std::endl;
	#endif
	
	const RemoteClientState* myRcs=dynamic_cast<const RemoteClientState*>(rcs);
	if(myRcs==0)
		Misc::throwStdErr("FooClient::glRenderAction: Mismatching remote client state object type");
	}

void FooClient::alRenderAction(ALContextData& contextData) const
	{
	#if DUMP_PROTOCOL
	std::cout<<"FooClient::alRenderAction"<<std::endl;
	#endif
	}

void FooClient::alRenderAction(const ProtocolClient::RemoteClientState* rcs,ALContextData& contextData) const
	{
	#if DUMP_PROTOCOL
	std::cout<<"FooClient::alRenderAction"<<std::endl;
	#endif
	
	const RemoteClientState* myRcs=dynamic_cast<const RemoteClientState*>(rcs);
	if(myRcs==0)
		Misc::throwStdErr("FooClient::alRenderAction: Mismatching remote client state object type");
	}

void FooClient::beforeClientUpdate(Comm::NetPipe& pipe)
	{
	#if DUMP_PROTOCOL
	std::cout<<"FooClient::beforeClientUpdate"<<std::endl;
	#endif
	}

bool FooClient::handleMessage(unsigned int messageId,Comm::NetPipe& pipe)
	{
	#if DUMP_PROTOCOL
	std::cout<<"FooClient::handleMessage"<<std::endl;
	#endif
	
	receiveRandomCrap(pipe);
	
	return true;
	}

}

/****************
DSO entry points:
****************/

extern "C" {

Collaboration::ProtocolClient* createObject(Collaboration::ProtocolClientLoader& objectLoader)
	{
	return new Collaboration::FooClient;
	}

void destroyObject(Collaboration::ProtocolClient* object)
	{
	delete object;
	}

}
