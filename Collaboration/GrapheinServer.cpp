/***********************************************************************
GrapheinServer - Server object to implement the Graphein shared
annotation protocol.
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

#include <Collaboration/GrapheinServer.h>

#include <Misc/ThrowStdErr.h>
#include <Comm/NetPipe.h>

namespace Collaboration {

/********************************************
Methods of class GrapheinServer::ClientState:
********************************************/

GrapheinServer::ClientState::ClientState(void)
	:curves(17)
	{
	}

GrapheinServer::ClientState::~ClientState(void)
	{
	/* Delete all curves in the curve map: */
	for(CurveMap::Iterator cIt=curves.begin();!cIt.isFinished();++cIt)
		delete cIt->getDest();
	}

/*******************************
Methods of class GrapheinServer:
*******************************/

GrapheinServer::GrapheinServer(void)
	{
	}

GrapheinServer::~GrapheinServer(void)
	{
	}

const char* GrapheinServer::getName(void) const
	{
	return protocolName;
	}

unsigned int GrapheinServer::getNumMessages(void) const
	{
	return MESSAGES_END;
	}

ProtocolServer::ClientState* GrapheinServer::receiveConnectRequest(unsigned int protocolMessageLength,Comm::NetPipe& pipe)
	{
	/* Check the protocol message length: */
	if(protocolMessageLength!=sizeof(Card))
		{
		/* Fatal error; stop communicating with client entirely: */
		Misc::throwStdErr("GrapheinServer::receiveConnectRequest: Protocol error; received %u bytes instead of %u",(unsigned int)protocolMessageLength,(unsigned int)sizeof(Card));
		}
	
	/* Receive the client's protocol version: */
	unsigned int clientProtocolVersion=pipe.read<Card>();
	
	/* Check for the correct version number: */
	if(clientProtocolVersion==protocolVersion)
		{
		/* Create the new client state object and set its message buffer's endianness: */
		ClientState* result=new ClientState;
		result->messageBuffer.setSwapOnWrite(pipe.mustSwapOnWrite());
		
		return result;
		}
	else
		return 0;
	}

void GrapheinServer::receiveClientUpdate(ProtocolServer::ClientState* cs,Comm::NetPipe& pipe)
	{
	/* Get a handle on the Graphein state object: */
	ClientState* myCs=dynamic_cast<ClientState*>(cs);
	if(myCs==0)
		Misc::throwStdErr("GrapheinServer::receiveClientUpdate: Client state object has mismatching type");
	
	/* Receive a list of curve action messages from the client: */
	MessageIdType message;
	while((message=readMessage(pipe))!=UPDATE_END)
		switch(message)
			{
			case ADD_CURVE:
				{
				/* Read the new curve's ID: */
				unsigned int newCurveId=pipe.read<Card>();
				
				/* Create a new curve object and add it to the client's curve map: */
				Curve* newCurve=new Curve;
				myCs->curves.setEntry(CurveMap::Entry(newCurveId,newCurve));
				
				/* Read the new curve's state from the pipe: */
				newCurve->read(pipe);
				
				/* Append a curve creation message to the client's outgoing buffer: */
				writeMessage(ADD_CURVE,myCs->messageBuffer);
				myCs->messageBuffer.write<Card>(newCurveId);
				newCurve->write(myCs->messageBuffer);
				
				break;
				}
			
			case APPEND_POINT:
				{
				/* Read the affected curve's ID: */
				unsigned int curveId=pipe.read<Card>();
				
				/* Read the new vertex position: */
				Point newVertex=read<Point>(pipe);
				
				/* Append the new vertex to the curve: */
				Curve* curve=myCs->curves.getEntry(curveId).getDest();
				unsigned int vertexIndex=curve->vertices.size();
				curve->vertices.push_back(newVertex);
				
				/* Append a vertex addition message to the client's outgoing buffer: */
				writeMessage(APPEND_POINT,myCs->messageBuffer);
				myCs->messageBuffer.write<Card>(curveId);
				myCs->messageBuffer.write<Card>(vertexIndex);
				write(newVertex,myCs->messageBuffer);
				
				break;
				}
			
			case DELETE_CURVE:
				{
				/* Read the affected curve's ID: */
				unsigned int curveId=pipe.read<Card>();
				
				/* Erase the curve from the client's curve map: */
				CurveMap::Iterator cIt=myCs->curves.findEntry(curveId);
				if(!cIt.isFinished())
					{
					/* Delete the curve: */
					delete cIt->getDest();
					myCs->curves.removeEntry(cIt);
					}
				
				/* Append a curve destruction message to the client's outgoing buffer: */
				writeMessage(DELETE_CURVE,myCs->messageBuffer);
				myCs->messageBuffer.write<Card>(curveId);
				
				break;
				}
			
			case DELETE_ALL_CURVES:
				{
				/* Delete all curves in the client's curve map: */
				for(CurveMap::Iterator cIt=myCs->curves.begin();!cIt.isFinished();++cIt)
					delete cIt->getDest();
				myCs->curves.clear();
				
				/* Append a curve set destruction message to the client's outgoing buffer: */
				writeMessage(DELETE_ALL_CURVES,myCs->messageBuffer);
				
				break;
				}
			
			default:
				Misc::throwStdErr("GrapheinServer::receiveClientUpdate: received unknown message %u",message);
			}
	}

void GrapheinServer::sendClientConnect(ProtocolServer::ClientState* sourceCs,ProtocolServer::ClientState* destCs,Comm::NetPipe& pipe)
	{
	/* Get handles on the Graphein state objects: */
	ClientState* mySourceCs=dynamic_cast<ClientState*>(sourceCs);
	ClientState* myDestCs=dynamic_cast<ClientState*>(destCs);
	if(mySourceCs==0||myDestCs==0)
		Misc::throwStdErr("GrapheinServer::sendClientConnect: Client state object has mismatching type");
	
	/* Send all curves currently owned by the source client to the destination client: */
	unsigned int numCurves=mySourceCs->curves.getNumEntries();
	pipe.write<Card>(numCurves);
	for(CurveMap::Iterator cIt=mySourceCs->curves.begin();!cIt.isFinished();++cIt)
		{
		/* Send the curve's ID: */
		pipe.write<Card>(cIt->getSource());
		
		/* Send the curve itself: */
		cIt->getDest()->write(pipe);
		}
	}

void GrapheinServer::sendServerUpdate(ProtocolServer::ClientState* sourceCs,ProtocolServer::ClientState* destCs,Comm::NetPipe& pipe)
	{
	/* Get handles on the Graphein state objects: */
	ClientState* mySourceCs=dynamic_cast<ClientState*>(sourceCs);
	ClientState* myDestCs=dynamic_cast<ClientState*>(destCs);
	if(mySourceCs==0||myDestCs==0)
		Misc::throwStdErr("GrapheinServer::sendServerUpdate: Client state object has mismatching type");
	
	/*********************************************************************
	Send the source client's accumulated state tracking messages to the
	destination client:
	*********************************************************************/
	
	/* Send the total size of the message first: */
	pipe.write<Card>(mySourceCs->messageBuffer.getDataSize());
	
	/* Write the message itself: */
	mySourceCs->messageBuffer.writeToSink(pipe);
	}

void GrapheinServer::afterServerUpdate(ProtocolServer::ClientState* cs)
	{
	/* Get a handle on the Graphein state object: */
	ClientState* myCs=dynamic_cast<ClientState*>(cs);
	if(myCs==0)
		Misc::throwStdErr("GrapheinServer::afterServerUpdate: Mismatching client state object type");
	
	/* Clear the client's message buffer: */
	myCs->messageBuffer.clear();
	}

}

/****************
DSO entry points:
****************/

extern "C" {

Collaboration::ProtocolServer* createObject(Collaboration::ProtocolServerLoader& objectLoader)
	{
	return new Collaboration::GrapheinServer;
	}

void destroyObject(Collaboration::ProtocolServer* object)
	{
	delete object;
	}

}
