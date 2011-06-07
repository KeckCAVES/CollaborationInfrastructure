/***********************************************************************
CollaborationServer - Class to support collaboration between
applications in spatially distributed (immersive) visualization
environments.
Copyright (c) 2007-2009 Oliver Kreylos

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

#include <Collaboration/CollaborationServer.h>

#include <iostream>
#include <algorithm>
#include <Misc/ThrowStdErr.h>
#include <Misc/StandardValueCoders.h>
#include <Misc/CompoundValueCoders.h>

namespace Collaboration {

/***************************************************
Methods of class CollaborationServer::Configuration:
***************************************************/

CollaborationServer::Configuration::Configuration(void)
	:configFile(COLLABORATION_CONFIGFILENAME),
	 cfg(configFile.getSection("/CollaborationServer"))
	{
	}

void CollaborationServer::Configuration::setListenPortId(int newListenPortId)
	{
	cfg.storeValue<int>("./listenPortId",newListenPortId);
	}

double CollaborationServer::Configuration::getTickTime(void)
	{
	return cfg.retrieveValue<double>("./tickTime",0.02);
	}

/******************************************************
Methods of class CollaborationServer::ClientConnection:
******************************************************/

CollaborationServer::ClientConnection::ClientConnection(unsigned int sClientID,const Comm::TCPSocket& socket)
	:clientID(sClientID),pipe(&socket),
	 clientHostname(socket.getPeerHostname(false)),
	 clientPortId(socket.getPeerPortId())
	{
	}

CollaborationServer::ClientConnection::~ClientConnection(void)
	{
	/* Delete the client states of all protocol plug-ins: */
	for(ClientProtocolList::iterator pIt=protocols.begin();pIt!=protocols.end();++pIt)
		delete pIt->protocolClientState;
	}

bool CollaborationServer::ClientConnection::negotiateProtocols(CollaborationServer& server)
	{
	bool result=true;
	
	/* Read the list of protocol plug-ins registered on the client, and match them against the server's list: */
	unsigned int numProtocols=pipe.read<unsigned int>();
	for(unsigned int i=0;i<numProtocols&&result;++i)
		{
		/* Read the protocol name: */
		std::string protocolName=pipe.read<std::string>();
		
		/* Read the length of the protocol-specific message payload: */
		size_t protocolMessageLength=pipe.read<unsigned int>();
		
		/* Ask the server to load the protocol: */
		#ifdef VERBOSE
		std::cout<<"CollaborationServer: Loading protocol "<<protocolName<<"..."<<std::flush;
		#endif
		std::pair<ProtocolServer*,int> ps=server.loadProtocol(protocolName);
		if(ps.first!=0)
			{
			/* Let the protocol plug-in process the message payload: */
			ProtocolClientState* pcs=ps.first->receiveConnectRequest(protocolMessageLength,pipe);
			
			#ifdef VERBOSE
			if(pcs!=0)
				std::cout<<" done"<<std::endl;
			else
				std::cout<<" rejected by protocol engine"<<std::endl;
			#else
			if(pcs==0)
				std::cerr<<"CollaborationServer: Protocol "<<protocolName<<" rejected by protocol engine"<<std::endl;
			#endif
			
			/* Add the protocol to the client state object's list: */
			protocols.push_back(ProtocolListEntry(ps.second,i,ps.first,pcs));
			
			/* Bail out if the protocol plug-in returned a null pointer: */
			result=pcs!=0;
			}
		else
			{
			#ifdef VERBOSE
			std::cout<<" rejected due to missing plug-in"<<std::endl;
			#else
			std::cerr<<"CollaborationServer: Protocol "<<protocolName<<" rejected due to missing plug-in"<<std::endl;
			#endif
			
			/* Skip the protocol message payload: */
			unsigned char skipBuffer[256];
			while(protocolMessageLength>0)
				{
				size_t skipSize=protocolMessageLength;
				if(skipSize>256)
					skipSize=256;
				pipe.read<unsigned char>(skipBuffer,skipSize);
				protocolMessageLength-=skipSize;
				}
			}
		}
	
	return result;
	}

void CollaborationServer::ClientConnection::sendClientConnectProtocols(ClientConnection* dest,CollaborationPipe& pipe)
	{
	/* Count the number of protocol plug-ins supported by both clients: */
	unsigned int numSharedProtocols=0;
	const ClientProtocolList& cpl1=protocols;
	const ClientProtocolList& cpl2=dest->protocols;
	unsigned int i1=0;
	unsigned int i2=0;
	while(i1<cpl1.size()&&i2<cpl2.size())
		{
		if(cpl1[i1].index<cpl2[i2].index)
			++i1; // Protocol in first list is not shared
		else if(cpl1[i1].index>cpl2[i2].index)
			++i2; // Protocol in second list is not shared
		else
			{
			++numSharedProtocols;
			++i1;
			++i2;
			}
		}
	
	/* Write the number of shared protocols: */
	pipe.write<unsigned int>(numSharedProtocols);
	
	/* Now send the actual protocol messages: */
	i1=0;
	i2=0;
	while(i1<cpl1.size()&&i2<cpl2.size())
		{
		if(cpl1[i1].index<cpl2[i2].index)
			++i1; // Protocol in first list is not shared
		else if(cpl1[i1].index>cpl2[i2].index)
			++i2; // Protocol in second list is not shared
		else
			{
			/* Write the destination client's protocol index: */
			pipe.write<unsigned int>(i2);
			
			/* Let the protocol send its data: */
			cpl1[i1].protocol->sendClientConnect(cpl1[i1].protocolClientState,cpl2[i2].protocolClientState,pipe);
			
			++i1;
			++i2;
			}
		}
	}

/************************************
Methods of class CollaborationServer:
************************************/

void* CollaborationServer::listenThreadMethod(void)
	{
	/* Enable immediate cancellation of this thread: */
	Threads::Thread::setCancelState(Threads::Thread::CANCEL_ENABLE);
	Threads::Thread::setCancelType(Threads::Thread::CANCEL_ASYNCHRONOUS);
	
	while(true)
		{
		/* Wait for the next incoming connection: */
		#ifdef VERBOSE
		std::cout<<"CollaborationServer: Waiting for client connection"<<std::endl<<std::flush;
		#endif
		Comm::TCPSocket clientSocket=listenSocket.accept();
		
		/**************************************************************************
		Connect the new client by creating a new client connection state structure:
		**************************************************************************/
		
		try
			{
			/* Create a new client connection state structure: */
			ClientConnection* newClientConnection=new ClientConnection(nextClientID,clientSocket);
			++nextClientID;
			
			#ifdef VERBOSE
			std::cout<<"CollaborationServer: Connecting new client from host "<<newClientConnection->clientHostname<<", port "<<newClientConnection->clientPortId<<std::endl<<std::flush;
			#endif
			
			/* Start a communication thread for the new client: */
			newClientConnection->communicationThread.start(this,&CollaborationServer::clientCommunicationThreadMethod,newClientConnection);
			}
		catch(std::runtime_error err)
			{
			std::cerr<<"CollaborationServer: Cancelled connecting new client due to exception "<<err.what()<<std::endl<<std::flush;
			}
		catch(...)
			{
			std::cerr<<"CollaborationServer: Cancelled connecting new client due to spurious exception"<<std::endl<<std::flush;
			}
		}
	
	return 0;
	}

void* CollaborationServer::clientCommunicationThreadMethod(CollaborationServer::ClientConnection* client)
	{
	/* Enable immediate cancellation of this thread: */
	Threads::Thread::setCancelState(Threads::Thread::CANCEL_ENABLE);
	Threads::Thread::setCancelType(Threads::Thread::CANCEL_ASYNCHRONOUS);
	
	enum State // Possible states of the client communication state machine
		{
		START,CONNECTED,FINISH
		};
	
	CollaborationPipe& pipe=client->pipe;
	unsigned int clientID=client->clientID;
	
	/* Run the client communication state machine until the client disconnects or there is a communication error: */
	bool clientAdded=false; // Flag to remember whether this client was ever "officially" connected
	try
		{
		State state=START;
		while(state!=FINISH)
			{
			/* Wait for the next message: */
			CollaborationPipe::MessageIdType message=pipe.readMessage();
			
			/* Process the message based on the communication state: */
			switch(state)
				{
				case START:
					
					/*************************************************************
					Handle message exchanges related to connection initiation:
					*************************************************************/
					
					switch(message)
						{
						case CollaborationPipe::CONNECT_REQUEST:
							{
							/* Read the client's display name: */
							client->name=pipe.read<std::string>();
							
							bool connectionOk=true;
							
							/* Negotiate protocol plug-ins with the new client: */
							connectionOk=connectionOk&&client->negotiateProtocols(*this);
							
							/* Sort the new client's negotiated protocol list in order of ascending main list index to facilitate quick intersection tests: */
							std::sort(client->protocols.begin(),client->protocols.end(),ClientConnection::ProtocolListEntry::comp);
							
							/* Process higher-level protocols: */
							bool higherLevelsSawRequest=connectionOk;
							connectionOk=connectionOk&&receiveConnectRequest(clientID,pipe);
							
							/* Reply appropriately to the connect request: */
							if(connectionOk)
								{
								/* Send connect reply message: */
								{
								Threads::Mutex::Lock pipeLock(pipe.getMutex());
								pipe.writeMessage(CollaborationPipe::CONNECT_REPLY);
								
								/* Write the number of negotiated protocols: */
								pipe.write<unsigned int>(client->protocols.size());
								
								/* Let all negotiated protocols insert their message payloads: */
								for(ClientConnection::ClientProtocolList::const_iterator cpIt=client->protocols.begin();cpIt!=client->protocols.end();++cpIt)
									{
									/* Write the client's index of the protocol: */
									pipe.write<unsigned int>(cpIt->clientIndex);
									
									/* Write the protocol's message ID base: */
									pipe.write<unsigned int>(cpIt->protocol->messageIdBase);
									
									/* Write the protocol's message payload: */
									cpIt->protocol->sendConnectReply(cpIt->protocolClientState,pipe);
									}
								
								/* Process higher-level protocols: */
								sendConnectReply(clientID,pipe);
								
								/* Send client connect messages for all clients that are already connected: */
								{
								Threads::Mutex::Lock clientListLock(clientListMutex);
								for(ClientList::const_iterator clIt=clientList.begin();clIt!=clientList.end();++clIt)
									{
									{
									Threads::Mutex::Lock clientLock((*clIt)->mutex);
									
									/* Send a client connect message: */
									pipe.writeMessage(CollaborationPipe::CLIENT_CONNECT);
									pipe.write<unsigned int>((*clIt)->clientID);
									pipe.write<std::string>((*clIt)->name);
									
									/* Send the intersection of protocol plug-ins negotiated with both clients to the client: */
									(*clIt)->sendClientConnectProtocols(client,pipe);
									
									/* Process higher-level protocols: */
									sendClientConnect((*clIt)->clientID,clientID,pipe);
									}
									}
								
								/* Add client action to list: */
								clientAdded=true;
								actionList.push_back(ClientListAction(ClientListAction::ADD_CLIENT,clientID,client));
								}
								
								pipe.flush();
								}
								
								#ifdef VERBOSE
								std::cout<<"CollaborationServer: Connected client from host "<<client->clientHostname<<", port "<<client->clientPortId<<" as "<<client->name<<std::endl<<std::flush;
								#endif
								
								state=CONNECTED;
								}
							else
								{
								{
								Threads::Mutex::Lock pipeLock(pipe.getMutex());
								
								/* Reject the connection request: */
								pipe.writeMessage(CollaborationPipe::CONNECT_REJECT);
								
								/* Write the number of negotiated protocols (including the one that might have failed): */
								pipe.write<unsigned int>(client->protocols.size());
								
								/* Inform all protocol plug-ins that have seen the CONNECT_REQUEST message: */
								for(ClientConnection::ClientProtocolList::const_iterator cpIt=client->protocols.begin();cpIt!=client->protocols.end();++cpIt)
									{
									/* Write the client's index of the protocol: */
									pipe.write<unsigned int>(cpIt->clientIndex);
									
									cpIt->protocol->sendConnectReject(cpIt->protocolClientState,pipe);
									}
								
								if(higherLevelsSawRequest)
									{
									/* Process higher-level protocols: */
									sendConnectReject(clientID,pipe);
									}
								
								pipe.flush();
								}
								
								state=FINISH;
								}
							break;
							}
						
						default:
							/* Bail out: */
							Misc::throwStdErr("Protocol error during connection initialization");
						}
					break;
				
				case CONNECTED:
					
					/*************************************************************
					Handle message exchanges while the client is connected:
					*************************************************************/
					
					switch(message)
						{
						case CollaborationPipe::CLIENT_UPDATE:
							{
							{
							/* Lock client state: */
							Threads::Mutex::Lock clientLock(client->mutex);
							
							/* Read the transient client state: */
							pipe.readClientState(client->state);
							
							/* Let protocol plug-ins read their own client update messages: */
							for(ClientConnection::ClientProtocolList::iterator cplIt=client->protocols.begin();cplIt!=client->protocols.end();++cplIt)
								cplIt->protocol->receiveClientUpdate(cplIt->protocolClientState,pipe);
							
							/* Process higher-level protocols: */
							receiveClientUpdate(clientID,pipe);
							}
							
							break;
							}
						
						case CollaborationPipe::DISCONNECT_REQUEST:
							{
							{
							/* Lock client state: */
							Threads::Mutex::Lock clientLock(client->mutex);
							
							/* Let protocol plug-ins read their own disconnect request messages: */
							for(ClientConnection::ClientProtocolList::iterator cplIt=client->protocols.begin();cplIt!=client->protocols.end();++cplIt)
								cplIt->protocol->receiveDisconnectRequest(cplIt->protocolClientState,pipe);
							
							/* Process higher-level protocols: */
							receiveDisconnectRequest(clientID,pipe);
							
							{
							Threads::Mutex::Lock pipeLock(pipe.getMutex());
							
							/* Send a disconnect reply: */
							pipe.writeMessage(CollaborationPipe::DISCONNECT_REPLY);
							
							/* Let protocol plug-ins insert their own disconnect reply messages: */
							for(ClientConnection::ClientProtocolList::iterator cplIt=client->protocols.begin();cplIt!=client->protocols.end();++cplIt)
								cplIt->protocol->sendDisconnectReply(cplIt->protocolClientState,pipe);
							
							/* Process higher-level protocols: */
							sendDisconnectReply(clientID,pipe);
							
							pipe.flush();
							}
							}
							
							/* Go to finish state: */
							state=FINISH;
							break;
							}
						
						default:
							{
							{
							Threads::Mutex::Lock clientLock(client->mutex);
							
							/* Find the protocol that registered itself for this message ID: */
							if(message<messageTable.size())
								{
								/* Find the protocol's client state object: */
								ProtocolServer* protocol=messageTable[message];
								ProtocolClientState* pcs=0;
								for(ClientConnection::ClientProtocolList::iterator pclIt=client->protocols.begin();pclIt!=client->protocols.end();++pclIt)
									if(pclIt->protocol==protocol)
										pcs=pclIt->protocolClientState;
								
								/* Call on the protocol plug-in to handle the message: */
								if(protocol==0||pcs==0||!protocol->handleMessage(pcs,message-protocol->messageIdBase,pipe))
									{
									/* Bail out: */
									Misc::throwStdErr("Protocol error, received message %d",int(message));
									}
								}
							else
								{
								/* Check for higher-level protocol messages: */
								if(!handleMessage(clientID,pipe,message))
									{
									/* Bail out: */
									Misc::throwStdErr("Protocol error, received message %d",int(message));
									}
								}
							}
							}
						}
					break;
				
				default:
					/* Just to make g++ happy... */
					;
				}
			}
		}
	catch(std::runtime_error err)
		{
		/* Print error message to stderr and disconnect the client: */
		std::cerr<<"CollaborationServer: Terminating client connection due to exception "<<err.what()<<std::endl<<std::flush;
		}
	catch(...)
		{
		/* Print error message to stderr and disconnect the client: */
		std::cerr<<"CollaborationServer: Terminating client connection due to spurious exception"<<std::endl<<std::flush;
		}
	
	/******************************************************************************************
	Disconnect the client by removing it from the list and deleting the client state structure:
	******************************************************************************************/
	
	#ifdef VERBOSE
	std::cout<<"CollaborationServer: Disconnecting client from host "<<client->clientHostname<<", port "<<client->clientPortId<<std::endl<<std::flush;
	#endif
	
	/* Delete the client state structure directly, or defer to main thread: */
	if(clientAdded)
		{
		/* Lock the client list: */
		Threads::Mutex::Lock clientListLock(clientListMutex);
		
		/* Check if the request to add the client is still in the action list: */
		ActionList::iterator alIt;
		for(alIt=actionList.begin();alIt!=actionList.end();++alIt)
			if(alIt->clientID==clientID&&alIt->action==ClientListAction::ADD_CLIENT)
				break;
		if(alIt!=actionList.end())
			{
			/* Remove the request to add the client from the action list: */
			actionList.erase(alIt);
			
			/* Delete the client connection state structure immediately (closing the TCP socket): */
			delete client;
			
			/* Process higher-level protocols: */
			disconnectClient(clientID);
			}
		else
			{
			/* Add the client removal action to the list: */
			actionList.push_back(ClientListAction(ClientListAction::REMOVE_CLIENT,clientID,client));
			}
		}
	else
		{
		/* Delete the client connection state structure immediately (closing the TCP socket): */
		delete client;
		
		/* Process higher-level protocols: */
		disconnectClient(clientID);
		}
	
	/* Terminate: */
	return 0;
	}

CollaborationServer::CollaborationServer(CollaborationServer::Configuration* sConfiguration)
	:configuration(sConfiguration!=0?sConfiguration:new Configuration),
	 protocolLoader(configuration->cfg.retrieveString("./pluginDsoNameTemplate",COLLABORATION_PLUGINDSONAMETEMPLATE)),
	 listenSocket(configuration->cfg.retrieveValue<int>("./listenPortId",-1),0),
	 nextClientID(0)
	{
	typedef std::vector<std::string> StringList;
	
	/* Get additional search paths from configuration file section and add them to the object loader: */
	StringList pluginSearchPaths=configuration->cfg.retrieveValue<StringList>("./pluginSearchPaths",StringList());
	for(StringList::const_iterator tspIt=pluginSearchPaths.begin();tspIt!=pluginSearchPaths.end();++tspIt)
		{
		/* Add the path: */
		protocolLoader.getDsoLocator().addPath(*tspIt);
		}
	
	/* Initialize the protocol message table to have invalid entries for the collaboration pipe's own messages: */
	for(unsigned int i=0;i<CollaborationPipe::MESSAGES_END;++i)
		messageTable.push_back(0);
	
	/* Start connection initiating thread: */
	listenThread.start(this,&CollaborationServer::listenThreadMethod);
	}

CollaborationServer::~CollaborationServer(void)
	{
	#ifdef VERBOSE
	std::cout<<"CollaborationServer: Shutting down server"<<std::endl<<std::flush;
	#endif
	
	{
	/* Lock client list: */
	Threads::Mutex::Lock clientListLock(clientListMutex);
	
	/* Stop connection initiating thread: */
	listenThread.cancel();
	listenThread.join();
	
	/* Disconnect all clients: */
	for(ClientList::iterator clIt=clientList.begin();clIt!=clientList.end();++clIt)
		{
		/* Stop client communication thread: */
		(*clIt)->communicationThread.cancel();
		(*clIt)->communicationThread.join();
		
		/* Delete client connection state structure (closing TCP socket): */
		delete *clIt;
		}
	}
	
	/* Delete all protocol plug-ins: */
	for(ProtocolList::iterator pIt=protocols.begin();pIt!=protocols.end();++pIt)
		{
		/* Only delete the protocol plug-in if it is not managed by the protocol loader: */
		if(!protocolLoader.isManaged(*pIt))
			delete *pIt;
		}
	
	/* Delete the configuration object: */
	delete configuration;
	}

void CollaborationServer::registerProtocol(ProtocolServer* newProtocol)
	{
	/* Simply add the protocol to the list; already-connected clients won't be able to use it: */
	Threads::Mutex::Lock protocolListLock(protocolListMutex);
	protocols.push_back(newProtocol);
	
	/* Register message IDs for the new protocol: */
	newProtocol->messageIdBase=messageTable.size();
	unsigned int numMessages=newProtocol->getNumMessages();
	for(unsigned int i=0;i<numMessages;++i)
		messageTable.push_back(newProtocol);
	}

std::pair<ProtocolServer*,int> CollaborationServer::loadProtocol(std::string protocolName)
	{
	Threads::Mutex::Lock protocolListLock(protocolListMutex);
	
	std::pair<ProtocolServer*,int> result=std::pair<ProtocolServer*,int>(0,-1);
	
	/* Check if a protocol plug-in of the given name already exists: */
	int numProtocols=int(protocols.size());
	for(int index=0;index<numProtocols;++index)
		if(protocols[index]->getName()==protocolName)
			{
			result.first=protocols[index];
			result.second=index;
			break;
			}
	
	if(result.first==0)
		{
		/* Try loading a protocol plug-in dynamically: */
		try
			{
			#ifdef VERBOSE
			std::cout<<"Loading protocol plug-in "<<protocolName<<"Server"<<std::endl;
			#endif
			result.first=protocolLoader.createObject((protocolName+"Server").c_str());
			result.second=numProtocols;
			
			/* Append the new protocol plug-in to the list: */
			protocols.push_back(result.first);
			
			/* Register message IDs for the new protocol: */
			result.first->messageIdBase=messageTable.size();
			unsigned int numMessages=result.first->getNumMessages();
			for(unsigned int i=0;i<numMessages;++i)
				messageTable.push_back(result.first);
			#ifdef VERBOSE
			std::cout<<"Protocol "<<protocolName<<" is assigned message IDs "<<result.first->messageIdBase<<" to "<<result.first->messageIdBase+numMessages-1<<std::endl;
			#endif
			}
		catch(std::runtime_error err)
			{
			/* Print an error message and carry on: */
			std::cerr<<"CollaborationServer::loadProtocol: Caught exception "<<err.what()<<" while loading protocol "<<protocolName<<std::endl;
			}
		}
	
	return result;
	}

void CollaborationServer::update(void)
	{
	{
	/* Lock protocol list: */
	Threads::Mutex::Lock protocolListLock(protocolListMutex);
	
	/* Process plug-in protocols: */
	for(ProtocolList::iterator plIt=protocols.begin();plIt!=protocols.end();++plIt)
		(*plIt)->beforeServerUpdate();
	
	{
	/* Lock client list: */
	Threads::Mutex::Lock clientListLock(clientListMutex);
	
	/* Process all actions from the client action list: */
	for(ActionList::const_iterator alIt=actionList.begin();alIt!=actionList.end();++alIt)
		{
		switch(alIt->action)
			{
			case ClientListAction::ADD_CLIENT:
				{
				/* Add the client state to the list: */
				clientList.push_back(alIt->client);
				
				/* Process plug-in protocols: */
				{
				Threads::Mutex::Lock clientLock(alIt->client->mutex);
				for(ClientConnection::ClientProtocolList::iterator cplIt=alIt->client->protocols.begin();cplIt!=alIt->client->protocols.end();++cplIt)
					cplIt->protocol->connectClient(cplIt->protocolClientState);
				}
				
				/* Process higher-level protocols: */
				connectClient(alIt->clientID);
				break;
				}
			
			case ClientListAction::REMOVE_CLIENT:
				{
				/* Find the client connection state structure in the list: */
				ClientList::iterator clIt;
				for(clIt=clientList.begin();clIt!=clientList.end()&&(*clIt)->clientID!=alIt->clientID;++clIt)
					;
				if(clIt!=clientList.end())
					{
					/* Process plug-in protocols: */
					{
					Threads::Mutex::Lock clientLock(alIt->client->mutex);
					for(ClientConnection::ClientProtocolList::iterator cplIt=(*clIt)->protocols.begin();cplIt!=(*clIt)->protocols.end();++cplIt)
						cplIt->protocol->disconnectClient(cplIt->protocolClientState);
					}
					
					/* Delete client connection state structure (closing TCP socket): */
					delete *clIt;
					
					/* Remove the client from the list: */
					clientList.erase(clIt);
					
					/* Process higher-level protocols: */
					disconnectClient(alIt->clientID);
					}
				break;
				}
			}
		}
	
	/* Lock the connection states of all clients: */
	for(ClientList::iterator clIt=clientList.begin();clIt!=clientList.end();++clIt)
		{
		ClientConnection* client=*clIt;
		
		/* Lock the client state: */
		client->mutex.lock();
		
		/* Process plug-in protocols for the client: */
		for(ClientConnection::ClientProtocolList::iterator cplIt=client->protocols.begin();cplIt!=client->protocols.end();++cplIt)
			cplIt->protocol->beforeServerUpdate(cplIt->protocolClientState);
		}
	
	/* Create a temporary action list to cleanly disconnect all clients that bomb out during the update step: */
	std::vector<ClientConnection*> deadClientList;
	
	/* Send state updates to all connected clients: */
	for(ClientList::iterator clIt=clientList.begin();clIt!=clientList.end();++clIt)
		{
		ClientConnection* destClient=*clIt;
		CollaborationPipe& pipe=destClient->pipe;
		
		try
			{
			{
			Threads::Mutex::Lock pipeLock(pipe.getMutex());
			
			/* Check the client state action list for any actions relevant for this client: */
			for(ActionList::const_iterator alIt=actionList.begin();alIt!=actionList.end();++alIt)
				if(alIt->clientID!=destClient->clientID)
					{
					switch(alIt->action)
						{
						case ClientListAction::ADD_CLIENT:
							{
							/* Find the added client's state: */
							ClientConnection* newClient=0;
							for(ClientList::iterator cl2It=clientList.begin();cl2It!=clientList.end();++cl2It)
								if((*cl2It)->clientID==alIt->clientID)
									{
									newClient=*cl2It;
									break;
									}
							
							if(newClient!=0)
								{
								/* Send a client connect message: */
								pipe.writeMessage(CollaborationPipe::CLIENT_CONNECT);
								pipe.write<unsigned int>(newClient->clientID);
								pipe.write<std::string>(newClient->name);
								
								/* Send the intersection of protocol plug-ins negotiated with both clients to the client: */
								newClient->sendClientConnectProtocols(destClient,pipe);
								
								/* Process higher-level protocols: */
								sendClientConnect(newClient->clientID,destClient->clientID,pipe);
								}
							break;
							}
						
						case ClientListAction::REMOVE_CLIENT:
							{
							/* Send a client disconnect message: */
							pipe.writeMessage(CollaborationPipe::CLIENT_DISCONNECT);
							pipe.write<unsigned int>(alIt->clientID);
							
							break;
							}
						}
					}
			
			/* Process plug-in protocols for the client: */
			for(ClientConnection::ClientProtocolList::iterator cplIt=destClient->protocols.begin();cplIt!=destClient->protocols.end();++cplIt)
				cplIt->protocol->beforeServerUpdate(cplIt->protocolClientState,pipe);
			
			/* Process higher-level protocols: */
			beforeServerUpdate(destClient->clientID,pipe);
			
			/* Send the server update packet header: */
			pipe.writeMessage(CollaborationPipe::SERVER_UPDATE);
			pipe.write<unsigned int>(clientList.size()-1);
			
			/* Process plug-in protocols for the client: */
			for(ClientConnection::ClientProtocolList::iterator cplIt=destClient->protocols.begin();cplIt!=destClient->protocols.end();++cplIt)
				cplIt->protocol->sendServerUpdate(cplIt->protocolClientState,pipe);
			
			/* Process higher-level protocols: */
			sendServerUpdate(destClient->clientID,pipe);
			
			/* Send the states of all other clients: */
			for(ClientList::iterator cl2It=clientList.begin();cl2It!=clientList.end();++cl2It)
				if(cl2It!=clIt)
					{
					ClientConnection* sourceClient=*cl2It;
					
					/* Send the server update packet: */
					pipe.write<unsigned int>(sourceClient->clientID);
					pipe.writeClientState(sourceClient->state);
					
					/* Process plug-in protocols shared by the two clients: */
					ClientConnection::ClientProtocolList::iterator cpl1It=sourceClient->protocols.begin();
					ClientConnection::ClientProtocolList::iterator cpl2It=destClient->protocols.begin();
					while(cpl1It!=sourceClient->protocols.end()&&cpl2It!=destClient->protocols.end())
						{
						if(cpl1It->index<cpl2It->index)
							++cpl1It;
						else if(cpl1It->index>cpl2It->index)
							++cpl2It;
						else
							{
							/* Send the shared protocol's payload: */
							cpl1It->protocol->sendServerUpdate(cpl1It->protocolClientState,cpl2It->protocolClientState,pipe);
							++cpl1It;
							++cpl2It;
							}
						}
					
					/* Process higher-level protocols: */
					sendServerUpdate(sourceClient->clientID,destClient->clientID,pipe);
					}
			
			/* Finish the message: */
			pipe.flush();
			}
			}
		catch(std::runtime_error err)
			{
			/* Forcibly disconnect clients that cause pipe errors during a state update: */
			std::cerr<<"CollaborationServer: Terminating client connection due to exception "<<err.what()<<std::endl;
			
			#ifdef VERBOSE
			std::cout<<"CollaborationServer: Disconnecting client from host "<<destClient->clientHostname<<", port "<<destClient->clientPortId<<std::endl<<std::flush;
			#endif
			
			/* Stop client communication thread: */
			destClient->communicationThread.cancel();
			destClient->communicationThread.join();
			
			/* Properly disconnect the client on the next update: */
			deadClientList.push_back(destClient);
			}
		catch(...)
			{
			/* Forcibly disconnect clients that cause pipe errors during a state update: */
			std::cerr<<"CollaborationServer: Terminating client connection due to spurious exception"<<std::endl;
			
			#ifdef VERBOSE
			std::cout<<"CollaborationServer: Disconnecting client from host "<<destClient->clientHostname<<", port "<<destClient->clientPortId<<std::endl<<std::flush;
			#endif
			
			/* Stop client communication thread: */
			destClient->communicationThread.cancel();
			destClient->communicationThread.join();
			
			/* Properly disconnect the client on the next update: */
			deadClientList.push_back(destClient);
			}
		}
	
	/* Process plug-in protocols: */
	for(ClientList::iterator clIt=clientList.begin();clIt!=clientList.end();++clIt)
		{
		ClientConnection* client=*clIt;
		
		/* Process plug-in protocols for the client: */
		for(ClientConnection::ClientProtocolList::iterator cplIt=client->protocols.begin();cplIt!=client->protocols.end();++cplIt)
			cplIt->protocol->afterServerUpdate(cplIt->protocolClientState);
		
		/* Unlock the client state: */
		client->mutex.unlock();
		}
	
	/* Clear the client state list action list: */
	actionList.clear();
	
	/* Mark all dead clients for removal on the next update: */
	for(std::vector<ClientConnection*>::const_iterator dclIt=deadClientList.begin();dclIt!=deadClientList.end();++dclIt)
		{
		/* Add the client removal action to the list: */
		actionList.push_back(ClientListAction(ClientListAction::REMOVE_CLIENT,(*dclIt)->clientID,*dclIt));
		}
	}
	
	/* Process plug-in protocols: */
	for(ProtocolList::iterator plIt=protocols.begin();plIt!=protocols.end();++plIt)
		(*plIt)->afterServerUpdate();
	}
	}

bool CollaborationServer::receiveConnectRequest(unsigned int clientID,CollaborationPipe& pipe)
	{
	/* Default behavior is to accept all connections: */
	return true;
	}

void CollaborationServer::sendConnectReply(unsigned int clientID,CollaborationPipe& pipe)
	{
	}

void CollaborationServer::sendConnectReject(unsigned int clientID,CollaborationPipe& pipe)
	{
	}

void CollaborationServer::receiveDisconnectRequest(unsigned int clientID,CollaborationPipe& pipe)
	{
	}

void CollaborationServer::sendDisconnectReply(unsigned int clientID,CollaborationPipe& pipe)
	{
	}

void CollaborationServer::receiveClientUpdate(unsigned int clientID,CollaborationPipe& pipe)
	{
	}

void CollaborationServer::sendClientConnect(unsigned int sourceClientID,unsigned int destClientID,CollaborationPipe& pipe)
	{
	}

void CollaborationServer::sendServerUpdate(unsigned int destClientID,CollaborationPipe& pipe)
	{
	}

void CollaborationServer::sendServerUpdate(unsigned int sourceClientID,unsigned int destClientID,CollaborationPipe& pipe)
	{
	}

bool CollaborationServer::handleMessage(unsigned int clientID,CollaborationPipe& pipe,CollaborationPipe::MessageIdType messageId)
	{
	/* Default behavior is to reject all unknown messages: */
	return false;
	}

void CollaborationServer::connectClient(unsigned int clientID)
	{
	}

void CollaborationServer::disconnectClient(unsigned int clientID)
	{
	}

void CollaborationServer::beforeServerUpdate(unsigned int clientID,CollaborationPipe& pipe)
	{
	}

}
