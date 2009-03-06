/***********************************************************************
EmineoClient - Client object to implement the Emineo 3D video tele-
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
#include <stdexcept>
#include <Misc/ThrowStdErr.h>
#include <Misc/StandardValueCoders.h>
#include <Misc/ConfigurationFile.h>
#include <Geometry/GeometryValueCoders.h>
#include <GL/gl.h>
#include <GL/GLTransformationWrappers.h>
#include <Vrui/Vrui.h>

#include <Collaboration/CollaborationPipe.h>

#include <FacadeGroup.h>
#include <ImageTriangleSoupRenderer.h>
#include <FusedRenderer.h>
#include <PlainRenderer.h>
#include <WorldPointSoupRenderer.h>
#include <WorldTriangleSoupRenderer.h>

#define EMINEO_USE_ITS_FUSED_RENDERER  0
#define EMINEO_USE_ITS_PLAIN_RENDERER  0
#define EMINEO_USE_WPS_PLAIN_RENDERER  0
#define EMINEO_USE_WTS_FUSED_RENDERER  0
#define EMINEO_USE_WTS_PLAIN_RENDERER  1

#include <Collaboration/EmineoClient.h>

namespace Collaboration {

/************************************************
Methods of class EmineoClient::RemoteClientState:
************************************************/

EmineoClient::RemoteClientState::RemoteClientState(const std::string& sGatewayHostname,int sGatewayPort,Comm::MulticastPipe* mcPipe)
	:gatewayHostname(sGatewayHostname),gatewayPort(sGatewayPort),
	 hasSource(!gatewayHostname.empty()&&gatewayPort>0),
	 facades(0),renderer(0)
	{
	if(hasSource)
		{
		/* Start the Emineo initialization thread: */
		std::cout<<"EmineoClient: Connecting to 3D video gateway "<<gatewayHostname<<", port "<<gatewayPort<<std::endl;
		
		initializeEmineoThread.start(this,&EmineoClient::RemoteClientState::initializeEmineoThreadMethod);
		}
	}

EmineoClient::RemoteClientState::~RemoteClientState(void)
	{
	if(hasSource)
		{
		/* Wait for the initialization thread to terminate: */
		initializeEmineoThread.join();
		}
	
	/* Delete the Emineo objects: */
	delete renderer;
	delete facades;
	}

void* EmineoClient::RemoteClientState::initializeEmineoThreadMethod(void)
	{
	emineo::FacadeGroup* newFacades=0;
	emineo::Renderer* newRenderer=0;
	try
		{
		/* Create a facade group by connecting to the given gateway (output is disabled for now): */
		std::cout<<"EmineoClient: Creating facade group"<<std::endl;
		newFacades=new emineo::FacadeGroup(gatewayHostname.c_str(),gatewayPort,0);
		
		/* Create the renderer: */
		std::cout<<"EmineoClient: Creating renderer"<<std::endl;
		#if EMINEO_USE_ITS_FUSED_RENDERER
		newRenderer=new emineo::FusedRenderer<emineo::ImageTriangleSoup>;
		#endif
		#if EMINEO_USE_ITS_PLAIN_RENDERER
		newRenderer=new emineo::PlainRenderer<emineo::ImageTriangleSoup>;
		#endif
		#if EMINEO_USE_WPS_PLAIN_RENDERER
		newRenderer=new emineo::PlainRenderer<emineo::WorldPointSoup>;
		#endif
		#if EMINEO_USE_WTS_FUSED_RENDERER
		newRenderer=new emineo::FusedRenderer<emineo::WorldTriangleSoup>;
		#endif
		#if EMINEO_USE_WTS_PLAIN_RENDERER
		newRenderer=new emineo::PlainRenderer<emineo::WorldTriangleSoup>;
		#endif
		
		std::cout<<"EmineoClient: Binding renderer to facade group"<<std::endl;
		newRenderer->bindFacadeGroup(newFacades);
		}
	catch(std::runtime_error err)
		{
		std::cerr<<"EmineoClient::RemoteClientState::initializeEmineoThreadMethod: Caught exception "<<err.what()<<std::endl;
		
		/* Delete everything: */
		delete newRenderer;
		delete newFacades;
		}
	catch(...)
		{
		std::cerr<<"EmineoClient::RemoteClientState::initializeEmineoThreadMethod: Caught spurious exception"<<std::endl;
		
		/* Delete everything: */
		delete newRenderer;
		delete newFacades;
		}
	
	/* Set the Emineo objects in the client's state so they can be used: */
	facades=newFacades;
	renderer=newRenderer;
	
	if(renderer!=0)
		std::cout<<"EmineoClient: Successfully created Emineo renderer connected to "<<gatewayHostname<<", port "<<gatewayPort<<std::endl;
	
	/* Exit: */
	return 0;
	}

/*****************************
Methods of class EmineoClient:
*****************************/

EmineoClient::EmineoClient(void)
	:gatewayHostname(""),gatewayPort(-1),hasSource(false)
	{
	/* Open the Emineo configuration file: */
	try
		{
		Misc::ConfigurationFile cfgFile(EMINEO_CONFIG_FILE);
		Misc::ConfigurationFileSection cfg=cfgFile.getSection("/");
		
		/* Read all relevant settings: */
		gatewayHostname=cfg.retrieveValue<std::string>("gatewayHostname",gatewayHostname);
		gatewayPort=cfg.retrieveValue<int>("./gatewayPort",gatewayPort);
		hasSource=gatewayHostname!=""&&gatewayPort>0;
		cameraTransformation=cfg.retrieveValue<OGTransform>("cameraTransformation",cameraTransformation);
		}
	catch(...)
		{
		/* Ignore errors silently... */
		}
	
	if(hasSource)
		std::cout<<"EmineoClient::EmineoClient: Offering 3D video on gateway "<<gatewayHostname<<", port "<<gatewayPort<<std::endl;
	}

EmineoClient::~EmineoClient(void)
	{
	}

const char* EmineoClient::getName(void) const
	{
	return protocolName;
	}

void EmineoClient::sendConnectRequest(CollaborationPipe& pipe)
	{
	/* Send the length of the following message: */
	unsigned int messageLength=sizeof(unsigned int)+gatewayHostname.length()+sizeof(int);
	pipe.write<unsigned int>(messageLength);
	
	/* Write the gateway hostname and port: */
	pipe.write<std::string>(gatewayHostname);
	pipe.write<int>(gatewayPort);
	}

void EmineoClient::sendClientUpdate(CollaborationPipe& pipe)
	{
	if(hasSource)
		{
		/* Send the current transformation from camera space into navigational space: */
		OGTransform cam=OGTransform(Vrui::getInverseNavigationTransformation());
		cam*=cameraTransformation;
		sendOGTransform(cam,pipe);
		}
	}

EmineoClient::RemoteClientState* EmineoClient::receiveClientConnect(CollaborationPipe& pipe)
	{
	/* Read the remote client's state: */
	std::string remoteGatewayHostname=pipe.read<std::string>();
	int remoteGatewayPort=pipe.read<int>();
	
	std::cout<<"EmineoClient::receiveClientConnect: Whoa, the new client has Emineo capability!"<<std::endl;
	
	/* Create a new remote client state object, connecting to the remote client's 3D video gateway: */
	return new RemoteClientState(remoteGatewayHostname,remoteGatewayPort,Vrui::openPipe());
	}

void EmineoClient::receiveServerUpdate(ProtocolClient::RemoteClientState* rcs,CollaborationPipe& pipe)
	{
	/* Get a handle on the Emineo state object: */
	RemoteClientState* myRcs=dynamic_cast<RemoteClientState*>(rcs);
	if(myRcs==0)
		Misc::throwStdErr("EmineoClient::receiveServerUpdate: Remote client state object has mismatching type");
	
	if(myRcs->hasSource)
		{
		/* Receive the remote client's current camera transformation: */
		myRcs->cameraTransform.postNewValue(receiveOGTransform(pipe));
		}
	}

void EmineoClient::frame(ProtocolClient::RemoteClientState* rcs)
	{
	RemoteClientState* myRcs=dynamic_cast<RemoteClientState*>(rcs);
	if(myRcs==0)
		Misc::throwStdErr("EmineoClient::frame: Mismatching remote client state object type");
	
	if(myRcs->hasSource&&myRcs->renderer!=0)
		{
		/* Lock the remote client's most recent camera transform: */
		myRcs->cameraTransform.lockNewValue();
		
		/* Update the renderer: */
		myRcs->renderer->update();
		}
	}

void EmineoClient::display(const ProtocolClient::RemoteClientState* rcs,GLContextData& contextData) const
	{
	const RemoteClientState* myRcs=dynamic_cast<const RemoteClientState*>(rcs);
	if(myRcs==0)
		Misc::throwStdErr("EmineoClient::display: Mismatching remote client state object type");
	
	if(myRcs->hasSource&&myRcs->renderer!=0)
		{
		/* Render the client's 3D video stream: */
		glPushMatrix();
		glMultMatrix(myRcs->cameraTransform.getLockedValue());
		
		/* Draw the current 3D video frame: */
		myRcs->renderer->display(contextData);
		
		glPopMatrix();
		}
	}

}
