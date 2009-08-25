/***********************************************************************
EmineoTest - Vrui application to render "raw" Emineo video streams.
Copyright (c) 2009 Oliver Kreylos
***********************************************************************/

#include <stdlib.h>
#include <string>
#include <stdexcept>
#include <iostream>
#include <Vrui/Application.h>

#include <FacadeGroup.h>
#include <ImageTriangleSoupRenderer.h>
#include <FusedRenderer.h>
#include <PlainRenderer.h>
#include <WorldPointSoupRenderer.h>
#include <WorldTriangleSoupRenderer.h>

class EmineoTest:public Vrui::Application
	{
	/* Elements: */
	private:
	
	/* Elements from actual Emineo 3D video rendering engine: */
	emineo::FacadeGroup* facades; // A collection of 3D camera images provided by the remote client's 3D video gateway
	emineo::Renderer* renderer; // A renderer for the facade group
	
	/* Constructors and destructors: */
	public:
	EmineoTest(int& argc,char**& argv,char**& appDefaults);
	virtual ~EmineoTest(void);
	
	/* Methods from Vrui::Application: */
	virtual void frame(void);
	virtual void display(GLContextData& contextData) const;
	};

/***************************
Methods of class EmineoTest:
***************************/

EmineoTest::EmineoTest(int& argc,char**& argv,char**& appDefaults)
	:Vrui::Application(argc,argv,appDefaults),
	 facades(0),renderer(0)
	{
	/* Parse the command line: */
	std::string gatewayHostname=argv[1];
	int gatewayPort=atoi(argv[2]);
	int rendererType=atoi(argv[3]);
	
	/* Create a facade group by connecting to the given gateway (output is disabled for now): */
	std::cout<<"EmineoClient: Creating facade group"<<std::endl;
	facades=new emineo::FacadeGroup(gatewayHostname.c_str(),gatewayPort,0);
	
	/* Create the renderer: */
	std::cout<<"EmineoClient: Creating renderer"<<std::endl;
	switch(rendererType)
		{
		case 0:
			// renderer=new emineo::FusedRenderer<emineo::ImageTriangleSoup>;
			break;
		
		case 1:
			renderer=new emineo::PlainRenderer<emineo::ImageTriangleSoup>;
			break;
		
		case 2:
			renderer=new emineo::PlainRenderer<emineo::WorldPointSoup>;
			break;
		
		case 3:
			// renderer=new emineo::FusedRenderer<emineo::WorldTriangleSoup>;
			break;
		
		case 4:
			renderer=new emineo::PlainRenderer<emineo::WorldTriangleSoup>;
			break;
		}
	
	std::cout<<"EmineoClient: Binding renderer to facade group"<<std::endl;
	renderer->bindFacadeGroup(facades);
	}

EmineoTest::~EmineoTest(void)
	{
	delete renderer;
	delete facades;
	}

void EmineoTest::frame(void)
	{
	/* Update the renderer: */
	renderer->update();
	}

void EmineoTest::display(GLContextData& contextData) const
	{
	/* Draw the current 3D video frame: */
	renderer->display(contextData);
	}

int main(int argc,char* argv[])
	{
	try
		{
		char** appDefaults=0;
		EmineoTest app(argc,argv,appDefaults);
		app.run();
		}
	catch(std::runtime_error err)
		{
		std::cerr<<"Terminating due to exception "<<err.what()<<std::endl;
		return 1;
		}
	
	return 0;
	}
