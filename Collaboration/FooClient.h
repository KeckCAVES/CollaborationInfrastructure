/***********************************************************************
FooClient - Dummy protocol plug-in class to stress-test the plug-in
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

#ifndef FOOCLIENT_INCLUDED
#define FOOCLIENT_INCLUDED

#include <Collaboration/ProtocolClient.h>

namespace Collaboration {

class FooClient:public ProtocolClient
	{
	/* Embedded classes: */
	protected:
	class RemoteClientState:public ProtocolClient::RemoteClientState // Class representing client-side state of a remote client
		{
		/* Constructors and destructors: */
		public:
		RemoteClientState(void);
		virtual ~RemoteClientState(void);
		};
	
	/* Constructors and destructors: */
	public:
	FooClient(void);
	virtual ~FooClient(void);
	
	/* Methods from ProtocolClient: */
	virtual const char* getName(void) const;
	virtual unsigned int getNumMessages(void) const;
	virtual void initialize(CollaborationClient& collaborationClient,Misc::ConfigurationFileSection& configFileSection);
	virtual void sendConnectRequest(CollaborationPipe& pipe);
	virtual void receiveConnectReply(CollaborationPipe& pipe);
	virtual void receiveConnectReject(CollaborationPipe& pipe);
	virtual void sendDisconnectRequest(CollaborationPipe& pipe);
	virtual void receiveDisconnectReply(CollaborationPipe& pipe);
	virtual void sendClientUpdate(CollaborationPipe& pipe);
	virtual ProtocolClient::RemoteClientState* receiveClientConnect(CollaborationPipe& pipe);
	virtual void receiveClientDisconnect(ProtocolClient::RemoteClientState* rcs,CollaborationPipe& pipe);
	virtual void receiveServerUpdate(CollaborationPipe& pipe);
	virtual void receiveServerUpdate(ProtocolClient::RemoteClientState* rcs,CollaborationPipe& pipe);
	virtual void rejectedByServer(void);
	virtual bool handleMessage(unsigned int messageId,CollaborationPipe& pipe);
	virtual void beforeClientUpdate(CollaborationPipe& pipe);
	virtual void connectClient(ProtocolClient::RemoteClientState* rcs);
	virtual void disconnectClient(ProtocolClient::RemoteClientState* rcs);
	virtual void frame(void);
	virtual void frame(ProtocolClient::RemoteClientState* rcs);
	virtual void glRenderAction(GLContextData& contextData) const;
	virtual void glRenderAction(const ProtocolClient::RemoteClientState* rcs,GLContextData& contextData) const;
	virtual void alRenderAction(ALContextData& contextData) const;
	virtual void alRenderAction(const ProtocolClient::RemoteClientState* rcs,ALContextData& contextData) const;
	};

}

#endif
