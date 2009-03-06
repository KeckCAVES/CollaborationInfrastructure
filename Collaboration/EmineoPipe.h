/***********************************************************************
EmineoPipe - Common interface between an Emineo server and an Emineo
client.
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

#ifndef EMINEOPIPE_INCLUDED
#define EMINEOPIPE_INCLUDED

#include <Geometry/OrthogonalTransformation.h>

/* Forward declarations: */
namespace Collaboration {
class CollaborationPipe;
}

namespace Collaboration {

struct EmineoPipe
	{
	/* Embedded classes: */
	protected:
	typedef Geometry::OrthogonalTransformation<float,3> OGTransform; // Class for 3D video transformations
	
	/* Elements: */
	static const char* protocolName; // Network name of Emineo protocol
	static const unsigned int numProtocolMessages; // Number of Emineo-specific protocol messages
	
	/* Methods: */
	static OGTransform receiveOGTransform(CollaborationPipe& pipe); // Receives a camera transformation from a pipe
	static void sendOGTransform(const OGTransform& transform,CollaborationPipe& pipe); // Sends a camera transformation to a pipe
	};

}

#endif
