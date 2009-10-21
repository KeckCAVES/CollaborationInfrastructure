/***********************************************************************
ProtocolServer - Abstract base class for the server-side components of
collaboration protocols that can be added to the base protocol
implemented by CollaborationClient and CollaborationServer, to simplify
creating complex higher-level protocols.
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
	:messageIdBase(0)
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

ProtocolServer::ClientState* ProtocolServer::receiveConnectRequest(unsigned int protocolMessageLength,CollaborationPipe& pipe)
	{
	/* Return a dummy object: */
	return new ClientState;
	}

void ProtocolServer::sendConnectReply(ProtocolServer::ClientState* cs,CollaborationPipe& pipe)
	{
	}

void ProtocolServer::sendConnectReject(ProtocolServer::ClientState* cs,CollaborationPipe& pipe)
	{
	}

void ProtocolServer::receiveDisconnectRequest(ProtocolServer::ClientState* cs,CollaborationPipe& pipe)
	{
	}

void ProtocolServer::sendDisconnectReply(ProtocolServer::ClientState* cs,CollaborationPipe& pipe)
	{
	}

void ProtocolServer::receiveClientUpdate(ProtocolServer::ClientState* cs,CollaborationPipe& pipe)
	{
	}

void ProtocolServer::sendClientConnect(ProtocolServer::ClientState* sourceCs,ProtocolServer::ClientState* destCs,CollaborationPipe& pipe)
	{
	}

void ProtocolServer::sendClientDisconnect(ProtocolServer::ClientState* sourceCs,ProtocolServer::ClientState* destCs,CollaborationPipe& pipe)
	{
	}

void ProtocolServer::sendServerUpdate(ProtocolServer::ClientState* destCs,CollaborationPipe& pipe)
	{
	}

void ProtocolServer::sendServerUpdate(ProtocolServer::ClientState* sourceCs,ProtocolServer::ClientState* destCs,CollaborationPipe& pipe)
	{
	}

bool ProtocolServer::handleMessage(ProtocolServer::ClientState* cs,unsigned int messageId,CollaborationPipe& pipe)
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

void ProtocolServer::beforeServerUpdate(ProtocolServer::ClientState* cs)
	{
	}

void ProtocolServer::beforeServerUpdate(ProtocolServer::ClientState* destCs,CollaborationPipe& pipe)
	{
	}

void ProtocolServer::afterServerUpdate(ProtocolServer::ClientState* cs)
	{
	}

}
