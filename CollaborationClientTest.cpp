/***********************************************************************
CollaborationClientTest - Simple Vrui application to test the
collaboration between spatially distributed VR environments using the
collaboration client class.
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

#include <string.h>
#include <stdlib.h>
#include <iostream>
#include <Vrui/Vrui.h>
#include <Vrui/Application.h>

#include <Collaboration/CollaborationClient.h>

#include <Collaboration/FooClient.h>
#ifdef COLLABORATIONCLIENT_USE_EMINEO
#include <Collaboration/EmineoClient.h>
#endif

class CollaborationClientTest:public Vrui::Application
	{
	/* Elements: */
	private:
	Collaboration::CollaborationClient* collaborationClient; // Pointer to the collaboration client object
	
	/* Constructors and destructors: */
	public:
	CollaborationClientTest(int& argc,char**& argv,char**& appDefaults);
	virtual ~CollaborationClientTest(void);
	
	/* Methods: */
	virtual void frame(void);
	virtual void display(GLContextData& contextData) const;
	};

CollaborationClientTest::CollaborationClientTest(int& argc,char**& argv,char**& appDefaults)
	:Vrui::Application(argc,argv,appDefaults),
	 collaborationClient(0)
	{
	/* Parse the command line: */
	const char* serverHostname=0;
	int serverPortId=0;
	const char* clientName=getenv("HOSTNAME");
	if(clientName==0)
		clientName=getenv("HOST");
	if(clientName==0)
		clientName="Anonymous Coward";
	bool fixGlyphScaling=true;
	bool renderRemoteEnvironments=true;
	for(int i=1;i<argc;++i)
		{
		if(argv[i][0]=='-')
			{
			if(strcasecmp(argv[i]+1,"fix")==0)
				fixGlyphScaling=true;
			else if(strcasecmp(argv[i]+1,"nofix")==0)
				fixGlyphScaling=false;
			else if(strcasecmp(argv[i]+1,"remote")==0)
				renderRemoteEnvironments=true;
			else if(strcasecmp(argv[i]+1,"noremote")==0)
				renderRemoteEnvironments=false;
			else if(strcasecmp(argv[i]+1,"foo")==0)
				{
				/* Register a Foo client: */
				if(collaborationClient!=0)
					collaborationClient->registerProtocol(new Collaboration::FooClient);
				else
					std::cerr<<"Ignoring -foo flag; need server hostname / port first!"<<std::endl;
				}
			else if(strcasecmp(argv[i]+1,"emineo")==0)
				{
				#ifdef COLLABORATIONCLIENT_USE_EMINEO
				/* Register an Emineo client: */
				if(collaborationClient!=0)
					collaborationClient->registerProtocol(new Collaboration::EmineoClient);
				#else
				std::cerr<<"Ignoring -emineo flag; support for 3D video was disabled at build"<<std::endl;
				#endif
				}
			}
		else if(serverHostname==0)
			serverHostname=argv[i];
		else if(serverPortId==0)
			{
			/* Read the server port: */
			serverPortId=atoi(argv[i]);
			
			/* Create the collaboration client: */
			collaborationClient=new Collaboration::CollaborationClient(serverHostname,serverPortId);
			collaborationClient->setFixGlyphScaling(fixGlyphScaling);
			collaborationClient->setRenderRemoteEnvironments(renderRemoteEnvironments);
			}
		else
			clientName=argv[i];
		}
	
	/* Initiate the collaboration protocol: */
	collaborationClient->connect(clientName);
	
	/* Initialize the navigation transformation: */
	Vrui::setNavigationTransformation(Vrui::Point(0.0,0.0,0.0),Vrui::Scalar(1.5),Vrui::Vector(0.0,0.0,1.0));
	}

CollaborationClientTest::~CollaborationClientTest(void)
	{
	/* Delete the collaboration client: */
	delete collaborationClient;
	}

void CollaborationClientTest::frame(void)
	{
	/* Call the collaboration client's frame method: */
	collaborationClient->frame();
	}

void CollaborationClientTest::display(GLContextData& contextData) const
	{
	/* Draw something: */
	glPushAttrib(GL_ENABLE_BIT|GL_LINE_BIT);
	glDisable(GL_LIGHTING);
	glLineWidth(1.0f);
	
	glColor3f(1.0f,1.0f,1.0f);
	glBegin(GL_LINE_STRIP);
	glVertex3d(-1.0,-1.0,-1.0);
	glVertex3d( 1.0,-1.0,-1.0);
	glVertex3d( 1.0, 1.0,-1.0);
	glVertex3d(-1.0, 1.0,-1.0);
	glVertex3d(-1.0,-1.0,-1.0);
	glVertex3d(-1.0,-1.0, 1.0);
	glVertex3d( 1.0,-1.0, 1.0);
	glVertex3d( 1.0, 1.0, 1.0);
	glVertex3d(-1.0, 1.0, 1.0);
	glVertex3d(-1.0,-1.0, 1.0);
	glEnd();
	glBegin(GL_LINES);
	glVertex3d( 1.0,-1.0,-1.0);
	glVertex3d( 1.0,-1.0, 1.0);
	glVertex3d( 1.0, 1.0,-1.0);
	glVertex3d( 1.0, 1.0, 1.0);
	glVertex3d(-1.0, 1.0,-1.0);
	glVertex3d(-1.0, 1.0, 1.0);
	glEnd();
	
	glPopAttrib();
	
	/* Call the collaboration client's display method: */
	collaborationClient->display(contextData);
	}

int main(int argc,char* argv[])
	{
	try
		{
		char** appDefaults=0;
		CollaborationClientTest cst(argc,argv,appDefaults);
		cst.run();
		}
	catch(std::runtime_error err)
		{
		std::cerr<<"Caught exception "<<err.what()<<std::endl;
		return 1;
		}
	
	return 0;
	}
