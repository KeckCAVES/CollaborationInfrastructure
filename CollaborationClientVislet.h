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

#ifndef COLLABORATION_COLLABORATIONCLIENTVISLET_INCLUDED
#define COLLABORATION_COLLABORATIONCLIENTVISLET_INCLUDED

#include <Vrui/Vislet.h>

namespace Vrui {
class VisletManager;
}
namespace Collaboration {
class CollaborationClient;
}

namespace Vrui {

namespace Vislets {

class CollaborationClientFactory:public VisletFactory
	{
	friend class CollaborationClient;
	
	/* Constructors and destructors: */
	public:
	CollaborationClientFactory(Vrui::VisletManager& visletManager);
	virtual ~CollaborationClientFactory(void);
	
	/* Methods: */
	virtual Vrui::Vislet* createVislet(int numVisletArguments,const char* const visletArguments[]) const;
	virtual void destroyVislet(Vrui::Vislet* vislet) const;
	};

class CollaborationClient:public Vislet
	{
	friend class CollaborationClientFactory;
	
	/* Elements: */
	private:
	static CollaborationClientFactory* factory; // Pointer to the factory object for this class
	
	Collaboration::CollaborationClient* collaborationClient; // Pointer to the collaboration client object
	bool firstFrame; // Flag to indicate the first frame in the vislet's lifetime
	
	/* Constructors and destructors: */
	public:
	CollaborationClient(int numArguments,const char* const arguments[]); // Creates a collaboration client vislet from the given argument list
	virtual ~CollaborationClient(void); // Disconnects from the collaboration server and destroys the vislet
	
	/* Methods: */
	virtual Vrui::VisletFactory* getFactory(void) const;
	virtual void disable(void);
	virtual void enable(void);
	virtual void frame(void);
	virtual void display(GLContextData& contextData) const;
	virtual void sound(ALContextData& contextData) const;
	};

}

}

#endif
