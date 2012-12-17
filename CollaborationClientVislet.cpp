/***********************************************************************
CollaborationClientVislet - Vislet class to embed a collaboration client
into an otherwise unaware Vrui application.
Copyright (c) 2007-2010 Oliver Kreylos

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

#include <CollaborationClientVislet.h>

#include <string.h>
#include <stdlib.h>
#include <stdexcept>
#include <iostream>
#include <GL/gl.h>
#include <GL/GLTransformationWrappers.h>
#include <GLMotif/WidgetManager.h>
#include <GLMotif/PopupWindow.h>
#include <AL/ALContextData.h>
#include <Vrui/Vrui.h>
#include <Vrui/DisplayState.h>
#include <Vrui/VisletManager.h>

#include <Collaboration/CollaborationClient.h>

namespace Vrui {

namespace Vislets {

/*******************************************
Methods of class CollaborationClientFactory:
*******************************************/

CollaborationClientFactory::CollaborationClientFactory(Vrui::VisletManager& visletManager)
	:VisletFactory("CollaborationClient",visletManager)
	{
	#if 0
	/* Insert class into class hierarchy: */
	VisletFactory* visletFactory=visletManager.loadClass("Vislet");
	visletFactory->addChildClass(this);
	addParentClass(visletFactory);
	#endif
	
	/* Set tool class' factory pointer: */
	CollaborationClient::factory=this;
	}

CollaborationClientFactory::~CollaborationClientFactory(void)
	{
	/* Reset tool class' factory pointer: */
	CollaborationClient::factory=0;
	}

Vrui::Vislet* CollaborationClientFactory::createVislet(int numArguments,const char* const arguments[]) const
	{
	return new CollaborationClient(numArguments,arguments);
	}

void CollaborationClientFactory::destroyVislet(Vrui::Vislet* vislet) const
	{
	delete vislet;
	}

extern "C" void resolveCollaborationClientFactoryDependencies(Plugins::FactoryManager<Vrui::VisletFactory>& manager)
	{
	#if 0
	/* Load base classes: */
	manager.loadClass("Vislet");
	#endif
	}

extern "C" Vrui::VisletFactory* createCollaborationClientFactory(Plugins::FactoryManager<Vrui::VisletFactory>& manager)
	{
	/* Get pointer to vislet manager: */
	Vrui::VisletManager* visletManager=static_cast<Vrui::VisletManager*>(&manager);
	
	/* Create factory object and insert it into class hierarchy: */
	CollaborationClientFactory* collaborationClientFactory=new CollaborationClientFactory(*visletManager);
	
	/* Return factory object: */
	return collaborationClientFactory;
	}

extern "C" void destroyCollaborationClientFactory(Vrui::VisletFactory* factory)
	{
	delete factory;
	}

/********************************************
Static elements of class CollaborationClient:
********************************************/

CollaborationClientFactory* CollaborationClient::factory=0;

/************************************
Methods of class CollaborationClient:
************************************/

CollaborationClient::CollaborationClient(int numArguments,const char* const arguments[])
	:collaborationClient(0),firstFrame(true)
	{
	/* Create a configuration object: */
	Collaboration::CollaborationClient::Configuration* cfg=new Collaboration::CollaborationClient::Configuration;
	
	/* Parse the command line: */
	for(int i=0;i<numArguments;++i)
		{
		if(arguments[i][0]=='-')
			{
			if(strcasecmp(arguments[i]+1,"server")==0)
				{
				++i;
				if(i<numArguments)
					{
					/* Split the server name into host name and port ID: */
					const char* colonPtr=0;
					for(const char* sPtr=arguments[i];*sPtr!='\0';++sPtr)
						if(*sPtr==':')
							colonPtr=sPtr;
					if(colonPtr!=0)
						cfg->setServer(std::string(arguments[i],colonPtr),atoi(colonPtr+1));
					else
						{
						/* Use the default port: */
						cfg->setServer(arguments[i],26000);
						}
					}
				else
					std::cerr<<"CollaborationClient: Ignoring dangling -server argument"<<std::endl;
				}
			else if(strcasecmp(arguments[i]+1,"name")==0)
				{
				++i;
				if(i<numArguments)
					cfg->setClientName(arguments[i]);
				else
					std::cerr<<"CollaborationClient: Ignoring dangling -name argument"<<std::endl;
				}
			}
		}
	
	try
		{
		/* Create the collaboration client: */
		collaborationClient=new Collaboration::CollaborationClient(cfg);
		
		/* Initiate the collaboration protocol: */
		collaborationClient->connect();
		}
	catch(std::runtime_error err)
		{
		std::cerr<<"CollaborationClient: Disabling collaboration due to exception "<<err.what()<<std::endl;
		delete collaborationClient;
		collaborationClient=0;
		}
	}

CollaborationClient::~CollaborationClient(void)
	{
	/* Delete the collaboration client: */
	delete collaborationClient;
	}

Vrui::VisletFactory* CollaborationClient::getFactory(void) const
	{
	return factory;
	}

void CollaborationClient::disable(void)
	{
	if(collaborationClient!=0)
		{
		Vislet::disable();
		
		/* Hide the collaboration client's dialog: */
		collaborationClient->hideDialog();
		}
	}

void CollaborationClient::enable(void)
	{
	if(collaborationClient!=0)
		{
		Vislet::enable();
		
		/* Show the collaboration client's dialog: */
		collaborationClient->showDialog();
		}
	}

void CollaborationClient::frame(void)
	{
	/* Call the collaboration client's frame method: */
	collaborationClient->frame();
	}

void CollaborationClient::display(GLContextData& contextData) const
	{
	/* Go to navigational coordinates: */
	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	glLoadIdentity();
	glMultMatrix(getDisplayState(contextData).modelviewNavigational);
	
	/* Call the collaboration client's display method: */
	collaborationClient->display(contextData);
	
	/* Return to physical coordinates: */
	glMatrixMode(GL_MODELVIEW);
	glPopMatrix();
	}

void CollaborationClient::sound(ALContextData& contextData) const
	{
	/* Go to navigational coordinates: */
	contextData.pushMatrix();
	contextData.multMatrix(Vrui::getNavigationTransformation());
	
	/* Call the collaboration client's sound method: */
	collaborationClient->sound(contextData);
	
	/* Return to physical coordinates: */
	contextData.popMatrix();
	}

}

}
