/***********************************************************************
GrapheinServer - Server object to implement the Graphein shared
annotation protocol.
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

#include <Collaboration/GrapheinServer.h>

#include <iostream>
#include <Misc/ThrowStdErr.h>

#include <Collaboration/CollaborationPipe.h>

namespace Collaboration {

/********************************************
Methods of class GrapheinServer::ClientState:
********************************************/

GrapheinServer::ClientState::ClientState(void)
	:curves(101)
	{
	}

GrapheinServer::ClientState::~ClientState(void)
	{
	/* Delete all curves in the hash table: */
	for(CurveHasher::Iterator cIt=curves.begin();!cIt.isFinished();++cIt)
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

ProtocolServer::ClientState* GrapheinServer::receiveConnectRequest(unsigned int protocolMessageLength,CollaborationPipe& pipe)
	{
	/* Receive the client's protocol version: */
	unsigned int clientProtocolVersion=pipe.read<unsigned int>();
	
	/* Check for the correct version number: */
	if(clientProtocolVersion==protocolVersion)
		return new ClientState;
	else
		return 0;
	}

void GrapheinServer::receiveClientUpdate(ProtocolServer::ClientState* cs,CollaborationPipe& pipe)
	{
	/* Get a handle on the Graphein state object: */
	ClientState* myCs=dynamic_cast<ClientState*>(cs);
	if(myCs==0)
		Misc::throwStdErr("GrapheinServer::receiveClientUpdate: Client state object has mismatching type");
	
	/* Receive a list of curve action messages from the client: */
	CollaborationPipe::MessageIdType message;
	while((message=pipe.readMessage())!=UPDATE_END)
		switch(message)
			{
			case ADD_CURVE:
				{
				/* Read the new curve's ID: */
				unsigned int newCurveId=pipe.read<unsigned int>();
				
				/* Create a new curve object and add it to the client state's set: */
				Curve* newCurve=new Curve;
				myCs->curves.setEntry(CurveHasher::Entry(newCurveId,newCurve));
				
				/* Receive the new curve from the pipe: */
				readCurve(pipe,*newCurve);
				
				/* Enqueue a curve action: */
				myCs->actions.push_back(CurveAction(ADD_CURVE,myCs->curves.findEntry(newCurveId)));
				
				break;
				}
			
			case APPEND_POINT:
				{
				/* Read the affected curve's ID: */
				unsigned int curveId=pipe.read<unsigned int>();
				
				/* Read the new vertex position: */
				Point newVertex;
				pipe.read<Scalar>(newVertex.getComponents(),3);
				
				/* Enqueue a curve action: */
				myCs->actions.push_back(CurveAction(APPEND_POINT,myCs->curves.findEntry(curveId),newVertex));
				
				break;
				}
			
			case DELETE_CURVE:
				{
				/* Read the affected curve's ID: */
				unsigned int curveId=pipe.read<unsigned int>();
				
				/* Enqueue a curve action: */
				myCs->actions.push_back(CurveAction(DELETE_CURVE,myCs->curves.findEntry(curveId)));
				
				break;
				}
			
			case DELETE_ALL_CURVES:
				{
				/* Enqueue a curve action: */
				myCs->actions.push_back(CurveAction(DELETE_ALL_CURVES));
				
				break;
				}
			
			default:
				Misc::throwStdErr("GrapheinServer::receiveClientUpdate: received unknown locator action message %u",message);
			}
	}

void GrapheinServer::sendClientConnect(ProtocolServer::ClientState* sourceCs,ProtocolServer::ClientState* destCs,CollaborationPipe& pipe)
	{
	/* Get handles on the Graphein state objects: */
	ClientState* mySourceCs=dynamic_cast<ClientState*>(sourceCs);
	ClientState* myDestCs=dynamic_cast<ClientState*>(destCs);
	if(mySourceCs==0||myDestCs==0)
		Misc::throwStdErr("GrapheinServer::sendClientConnect: Client state object has mismatching type");
	
	/* Send all curves currently owned by the source client to the destination client: */
	unsigned int numCurves=mySourceCs->curves.getNumEntries();
	pipe.write<unsigned int>(numCurves);
	for(CurveHasher::Iterator cIt=mySourceCs->curves.begin();!cIt.isFinished();++cIt)
		{
		/* Send the curve's ID: */
		pipe.write<unsigned int>(cIt->getSource());
		
		/* Send the curve itself: */
		writeCurve(pipe,*(cIt->getDest()));
		}
	}

void GrapheinServer::sendServerUpdate(ProtocolServer::ClientState* sourceCs,ProtocolServer::ClientState* destCs,CollaborationPipe& pipe)
	{
	/* Get handles on the Graphein state objects: */
	ClientState* mySourceCs=dynamic_cast<ClientState*>(sourceCs);
	ClientState* myDestCs=dynamic_cast<ClientState*>(destCs);
	if(mySourceCs==0||myDestCs==0)
		Misc::throwStdErr("GrapheinServer::sendServerUpdate: Client state object has mismatching type");
	
	/* Send the source client's curve action list to the destination client: */
	for(CurveActionList::const_iterator aIt=mySourceCs->actions.begin();aIt!=mySourceCs->actions.end();++aIt)
		{
		switch(aIt->action)
			{
			case ADD_CURVE:
				/* Send a creation message: */
				pipe.writeMessage(ADD_CURVE);
				
				/* Send the new curve's ID: */
				pipe.write<unsigned int>(aIt->curveIt->getSource());
				
				/* Send the new curve: */
				writeCurve(pipe,*(aIt->curveIt->getDest()));
				
				break;
			
			case APPEND_POINT:
				/* Send an append message: */
				pipe.writeMessage(APPEND_POINT);
				
				/* Send the affected curve's ID: */
				pipe.write<unsigned int>(aIt->curveIt->getSource());
				
				/* Send the new vertex: */
				pipe.write<Scalar>(aIt->newVertex.getComponents(),3);
				
				break;
			
			case DELETE_CURVE:
				/* Send a delete message: */
				pipe.writeMessage(DELETE_CURVE);
				
				/* Send the affected curve's ID: */
				pipe.write<unsigned int>(aIt->curveIt->getSource());
				
				break;
			
			case DELETE_ALL_CURVES:
				/* Send a delete message: */
				pipe.writeMessage(DELETE_ALL_CURVES);
				
				break;
			
			default:
				; // Just to make g++ happy
			}
		}
	
	/* Terminate the action list: */
	pipe.writeMessage(UPDATE_END);
	}

void GrapheinServer::afterServerUpdate(ProtocolServer::ClientState* cs)
	{
	/* Get a handle on the Graphein state object: */
	ClientState* myCs=dynamic_cast<ClientState*>(cs);
	if(myCs==0)
		Misc::throwStdErr("GrapheinServer::afterServerUpdate: Mismatching client state object type");
	
	/* Apply all actions to the client's curve set: */
	for(CurveActionList::const_iterator aIt=myCs->actions.begin();aIt!=myCs->actions.end();++aIt)
		switch(aIt->action)
			{
			case APPEND_POINT:
				/* Append a new vertex to the curve: */
				aIt->curveIt->getDest()->vertices.push_back(aIt->newVertex);
				
				break;
			
			case DELETE_CURVE:
				/* Delete the curve: */
				delete aIt->curveIt->getDest();
				myCs->curves.removeEntry(aIt->curveIt);
				
				break;
			
			case DELETE_ALL_CURVES:
				{
				/* Delete all curves: */
				for(CurveHasher::Iterator cIt=myCs->curves.begin();!cIt.isFinished();++cIt)
					delete cIt->getDest();
				
				myCs->curves.clear();
				
				break;
				}
			}
	
	/* Clear the action list: */
	myCs->actions.clear();
	}

}
