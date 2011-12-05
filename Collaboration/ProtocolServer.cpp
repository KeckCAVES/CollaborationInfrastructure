/***********************************************************************
ProtocolServer - Abstract base class for the server-side components of
collaboration protocols that can be added to the base protocol
implemented by CollaborationClient and CollaborationServer, to simplify
creating complex higher-level protocols.
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

#include <Collaboration/ProtocolServer.h>

namespace Collaboration {

/********************************************
Methods of class ProtocolServer::ClientState:
********************************************/

ProtocolServer::ClientState::ClientState(void)
	{
	}

ProtocolServer::ClientState::~ClientState(void)
	{
	}

/*******************************
Methods of class ProtocolServer:
*******************************/

ProtocolServer::ProtocolServer(void)
	:server(0),messageIdBase(0)
	{
	}

ProtocolServer::~ProtocolServer(void)
	{
	}

unsigned int ProtocolServer::getNumMessages(void) const
	{
	/* Default is not to have protocol messages: */
	return 0;
	}

void ProtocolServer::initialize(CollaborationServer* sServer,Misc::ConfigurationFileSection& configFileSection)
	{
	/* Remember the server object: */
	server=sServer;
	}

ProtocolServer::ClientState* ProtocolServer::receiveConnectRequest(unsigned int protocolMessageLength,Comm::NetPipe& pipe)
	{
	/* Reject the connection: */
	return 0;
	}

void ProtocolServer::sendConnectReply(ProtocolServer::ClientState* cs,Comm::NetPipe& pipe)
	{
	}

void ProtocolServer::sendConnectReject(ProtocolServer::ClientState* cs,Comm::NetPipe& pipe)
	{
	}

void ProtocolServer::receiveDisconnectRequest(ProtocolServer::ClientState* cs,Comm::NetPipe& pipe)
	{
	}

void ProtocolServer::sendDisconnectReply(ProtocolServer::ClientState* cs,Comm::NetPipe& pipe)
	{
	}

void ProtocolServer::receiveClientUpdate(ProtocolServer::ClientState* cs,Comm::NetPipe& pipe)
	{
	}

void ProtocolServer::sendClientConnect(ProtocolServer::ClientState* sourceCs,ProtocolServer::ClientState* destCs,Comm::NetPipe& pipe)
	{
	}

void ProtocolServer::sendServerUpdate(ProtocolServer::ClientState* destCs,Comm::NetPipe& pipe)
	{
	}

void ProtocolServer::sendServerUpdate(ProtocolServer::ClientState* sourceCs,ProtocolServer::ClientState* destCs,Comm::NetPipe& pipe)
	{
	}

bool ProtocolServer::handleMessage(ProtocolServer::ClientState* cs,unsigned int messageId,Comm::NetPipe& pipe)
	{
	/* Default is to reject all messages: */
	return false;
	}

void ProtocolServer::connectClient(ProtocolServer::ClientState* cs)
	{
	}

void ProtocolServer::disconnectClient(ProtocolServer::ClientState* cs)
	{
	}

void ProtocolServer::beforeServerUpdate(void)
	{
	}

void ProtocolServer::beforeServerUpdate(ProtocolServer::ClientState* cs)
	{
	}

void ProtocolServer::beforeServerUpdate(ProtocolServer::ClientState* destCs,Comm::NetPipe& pipe)
	{
	}

void ProtocolServer::afterServerUpdate(ProtocolServer::ClientState* cs)
	{
	}

void ProtocolServer::afterServerUpdate(void)
	{
	}

}
