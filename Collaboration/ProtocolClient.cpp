/***********************************************************************
ProtocolClient - Abstract base class for the client-side components of
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

#include <Collaboration/ProtocolClient.h>

#include <Comm/NetPipe.h>
#include <Collaboration/Protocol.h>

namespace Collaboration {

/**************************************************
Methods of class ProtocolClient::RemoteClientState:
**************************************************/

ProtocolClient::RemoteClientState::RemoteClientState(void)
	{
	}

ProtocolClient::RemoteClientState::~RemoteClientState(void)
	{
	}

/*******************************
Methods of class ProtocolClient:
*******************************/

ProtocolClient::ProtocolClient(void)
	:client(0),messageIdBase(0)
	{
	}

ProtocolClient::~ProtocolClient(void)
	{
	}

unsigned int ProtocolClient::getNumMessages(void) const
	{
	/* Default is not to have protocol messages: */
	return 0;
	}

void ProtocolClient::initialize(CollaborationClient* sClient,Misc::ConfigurationFileSection& configFileSection)
	{
	/* Remember the client object: */
	client=sClient;
	}

bool ProtocolClient::haveSettingsDialog(void) const
	{
	/* Default is not to have user interface elements: */
	return false;
	}

void ProtocolClient::buildSettingsDialog(GLMotif::RowColumn* settingsDialog)
	{
	/* Default behavior is to do nothing */
	}

void ProtocolClient::sendConnectRequest(Comm::NetPipe& pipe)
	{
	/* Must send a zero to indicate an empty protocol message: */
	pipe.write<Protocol::Card>(0);
	}

void ProtocolClient::receiveConnectReply(Comm::NetPipe& pipe)
	{
	}

void ProtocolClient::receiveConnectReject(Comm::NetPipe& pipe)
	{
	}

void ProtocolClient::sendDisconnectRequest(Comm::NetPipe& pipe)
	{
	}

void ProtocolClient::receiveDisconnectReply(Comm::NetPipe& pipe)
	{
	}

void ProtocolClient::sendClientUpdate(Comm::NetPipe& pipe)
	{
	}

ProtocolClient::RemoteClientState* ProtocolClient::receiveClientConnect(Comm::NetPipe& pipe)
	{
	/* Return a dummy object: */
	return new RemoteClientState;
	}

bool ProtocolClient::receiveServerUpdate(Comm::NetPipe& pipe)
	{
	return false;
	}

bool ProtocolClient::receiveServerUpdate(RemoteClientState* rcs,Comm::NetPipe& pipe)
	{
	return false;
	}

void ProtocolClient::rejectedByServer(void)
	{
	}

void ProtocolClient::connectClient(ProtocolClient::RemoteClientState* rcs)
	{
	}

void ProtocolClient::disconnectClient(ProtocolClient::RemoteClientState* rcs)
	{
	}

void ProtocolClient::frame(void)
	{
	}

void ProtocolClient::frame(ProtocolClient::RemoteClientState* rcs)
	{
	}

void ProtocolClient::glRenderAction(GLContextData& contextData) const
	{
	}

void ProtocolClient::glRenderAction(const ProtocolClient::RemoteClientState* rcs,GLContextData& contextData) const
	{
	}

void ProtocolClient::alRenderAction(ALContextData& contextData) const
	{
	}

void ProtocolClient::alRenderAction(const ProtocolClient::RemoteClientState* rcs,ALContextData& contextData) const
	{
	}

void ProtocolClient::beforeClientUpdate(Comm::NetPipe& pipe)
	{
	}

bool ProtocolClient::handleMessage(unsigned int messageId,Comm::NetPipe& pipe)
	{
	/* Default is to reject all messages: */
	return false;
	}

}
