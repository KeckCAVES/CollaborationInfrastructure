/***********************************************************************
Main program for a dedicated VR collaboration server.
Copyright (c) 2007-2011 Oliver Kreylos

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
#include <signal.h>
#include <iostream>
#include <Misc/SelfDestructPointer.h>
#include <Misc/Time.h>

#include <Collaboration/CollaborationServer.h>

volatile bool runServerLoop=true;

void termSignalHandler(int)
	{
	runServerLoop=false;
	}

int main(int argc,char* argv[])
	{
	try
		{
		/* Create a new configuration object: */
		Misc::SelfDestructPointer<Collaboration::CollaborationServer::Configuration> cfg(new Collaboration::CollaborationServer::Configuration);
		
		/* Parse the command line: */
		Misc::Time tickTime(cfg->getTickTime()); // Server update time interval in seconds
		for(int i=1;i<argc;++i)
			{
			if(argv[i][0]=='-')
				{
				if(strcasecmp(argv[i]+1,"port")==0)
					{
					++i;
					if(i<argc)
						{
						/* Override the listen port ID in the configuration file section: */
						cfg->setListenPortId(atoi(argv[i]));
						}
					else
						std::cerr<<"CollaborationServerMain: ignored dangling -port option"<<std::endl;
					}
				else if(strcasecmp(argv[i]+1,"tick")==0)
					{
					++i;
					if(i<argc)
						tickTime=Misc::Time(atof(argv[i]));
					else
						std::cerr<<"CollaborationServerMain: ignored dangling -tick option"<<std::endl;
					}
				}
			}
		
		/* Ignore SIGPIPE and leave handling of pipe errors to TCP sockets: */
		struct sigaction sigPipeAction;
		sigPipeAction.sa_handler=SIG_IGN;
		sigemptyset(&sigPipeAction.sa_mask);
		sigPipeAction.sa_flags=0x0;
		sigaction(SIGPIPE,&sigPipeAction,0);
		
		/* Create the collaboration server object: */
		Collaboration::CollaborationServer server(cfg.getTarget());
		cfg.releaseTarget();
		std::cout<<"CollaborationServerMain: Started server on port "<<server.getListenPortId()<<std::endl;
		
		/* Reroute SIG_INT signals to cleanly shut down multiplexer: */
		struct sigaction sigIntAction;
		memset(&sigIntAction,0,sizeof(struct sigaction));
		sigIntAction.sa_handler=termSignalHandler;
		if(sigaction(SIGINT,&sigIntAction,0)!=0)
			std::cerr<<"CollaborationServerMain: Cannot intercept SIG_INT signals. Server won't shut down cleanly."<<std::endl;
		
		/* Run the server loop at the specified time interval: */
		Misc::Time nextTick=Misc::Time::now();
		int i=0;
		while(runServerLoop)
			{
			/* Sleep for the tick time: */
			nextTick+=tickTime;
			Misc::Time sleepTime=nextTick-Misc::Time::now();
			if(sleepTime.tv_sec>=0)
				Misc::sleep(sleepTime);
			
			/* Update the server state: */
			server.update();
			std::cout<<'\r'<<char('0'+i%10)<<std::flush;
			++i;
			}
		}
	catch(std::runtime_error err)
		{
		std::cerr<<"Caught exception "<<err.what()<<std::endl;
		return 1;
		}
	
	return 0;
	}
