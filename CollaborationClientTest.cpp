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
#include <Misc/SelfDestructPointer.h>
#include <GLMotif/PopupMenu.h>
#include <GLMotif/Menu.h>
#include <GLMotif/ToggleButton.h>
#include <Vrui/Vrui.h>
#include <Vrui/Application.h>

#include <Collaboration/CollaborationClient.h>

class CollaborationClientTest:public Vrui::Application
	{
	/* Elements: */
	private:
	Collaboration::CollaborationClient* collaborationClient; // Pointer to the collaboration client object
	GLMotif::PopupMenu* mainMenu; // The program's main menu
	
	/* Private methods: */
	GLMotif::PopupMenu* createMainMenu(void); // Creates the program's main menu
	
	/* Constructors and destructors: */
	public:
	CollaborationClientTest(int& argc,char**& argv,char**& appDefaults);
	virtual ~CollaborationClientTest(void);
	
	/* Methods: */
	virtual void frame(void);
	virtual void display(GLContextData& contextData) const;
	virtual void sound(ALContextData& contextData) const;
	void showClientDialogCallback(GLMotif::ToggleButton::ValueChangedCallbackData* cbData);
	};

GLMotif::PopupMenu* CollaborationClientTest::createMainMenu(void)
	{
	/* Create a popup shell to hold the main menu: */
	GLMotif::PopupMenu* mainMenuPopup=new GLMotif::PopupMenu("MainMenuPopup",Vrui::getWidgetManager());
	mainMenuPopup->setTitle("Collaboration Client Test");
	
	/* Create the main menu itself: */
	GLMotif::Menu* mainMenu=new GLMotif::Menu("MainMenu",mainMenuPopup,false);
	
	/* Create a button: */
	GLMotif::ToggleButton* showClientDialogToggle=new GLMotif::ToggleButton("ShowClientDialogToggle",mainMenu,"Show Client Dialog");
	showClientDialogToggle->setToggle(false);
	showClientDialogToggle->getValueChangedCallbacks().add(this,&CollaborationClientTest::showClientDialogCallback);
	
	/* Finish building the main menu: */
	mainMenu->manageChild();
	
	return mainMenuPopup;
	}

CollaborationClientTest::CollaborationClientTest(int& argc,char**& argv,char**& appDefaults)
	:Vrui::Application(argc,argv,appDefaults),
	 collaborationClient(0),mainMenu(0)
	{
	/* Create a configuration object: */
	Misc::SelfDestructPointer<Collaboration::CollaborationClient::Configuration> cfg(new Collaboration::CollaborationClient::Configuration);
	
	/* Parse the command line: */
	for(int i=1;i<argc;++i)
		{
		if(argv[i][0]=='-')
			{
			if(strcasecmp(argv[i]+1,"server")==0)
				{
				++i;
				
				/* Split the server name into host name and port ID: */
				char* colonPtr=0;
				for(char* sPtr=argv[i];*sPtr!='\0';++sPtr)
					if(*sPtr==':')
						colonPtr=sPtr;
				if(colonPtr!=0)
					cfg->setServer(std::string(argv[i],colonPtr),atoi(colonPtr+1));
				else
					{
					/* Use the default port: */
					cfg->setServer(argv[i],26000);
					}
				}
			else if(strcasecmp(argv[i]+1,"name")==0)
				{
				++i;
				cfg->setClientName(argv[i]);
				}
			}
		}
	
	try
		{
		/* Create the collaboration client: */
		collaborationClient=new Collaboration::CollaborationClient(cfg.getTarget());
		cfg.releaseTarget();
		
		/* Initiate the collaboration protocol: */
		collaborationClient->connect();
		}
	catch(std::runtime_error err)
		{
		/* Print error message and carry on: */
		std::cerr<<"Unable to connect to collaboration server due to "<<err.what()<<std::endl;
		}
	
	/* Create the main menu: */
	mainMenu=createMainMenu();
	Vrui::setMainMenu(mainMenu);
	
	/* Initialize the navigation transformation: */
	Vrui::setNavigationTransformation(Vrui::Point(0.0,0.0,0.0),Vrui::Scalar(1.5),Vrui::Vector(0.0,0.0,1.0));
	}

CollaborationClientTest::~CollaborationClientTest(void)
	{
	/* Delete the user interface: */
	delete mainMenu;
	
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

void CollaborationClientTest::sound(ALContextData& contextData) const
	{
	/* Call the collaboration client's sound method: */
	collaborationClient->sound(contextData);
	}

void CollaborationClientTest::showClientDialogCallback(GLMotif::ToggleButton::ValueChangedCallbackData* cbData)
	{
	if(cbData->set)
		collaborationClient->showDialog();
	else
		collaborationClient->hideDialog();
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
