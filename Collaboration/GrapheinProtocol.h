/***********************************************************************
GrapheinProtocol - Class defining the communication protocol between a
Graphein server and a Graphein client.
Copyright (c) 2009-2011 Oliver Kreylos

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

#ifndef COLLABORATION_GRAPHEINPROTOCOL_INCLUDED
#define COLLABORATION_GRAPHEINPROTOCOL_INCLUDED

#include <string>
#include <vector>
#include <Misc/HashTable.h>
#include <GL/gl.h>
#include <GL/GLColor.h>
#include <Collaboration/Protocol.h>

namespace Collaboration {

class GrapheinProtocol:public Protocol
	{
	/* Embedded classes: */
	public:
	enum MessageId // Enumerated type for Graphein protocol messages
		{
		ADD_CURVE=0,
		APPEND_POINT,
		DELETE_CURVE,
		DELETE_ALL_CURVES,
		UPDATE_END,
		MESSAGES_END
		};
	
	struct Curve // Structure to represent single-stroke curves
		{
		/* Embedded classes: */
		public:
		typedef GLColor<GLubyte,3> Color; // Type for colors
		
		/* Elements: */
		GLfloat lineWidth; // The curve's cosmetic line width
		Color color; // The curve's color
		std::vector<Point> vertices; // The curve's vertices
		
		/* Methods: */
		void read(IO::File& source); // Reads a curve from the given source
		void write(IO::File& sink) const; // Writes a curve to the given sink
		};
	
	typedef Misc::HashTable<unsigned int,Curve*> CurveMap; // Hash table to map curve IDs to curve objects
	
	/* Elements: */
	static const char* protocolName; // Network name of Graphein protocol
	static const unsigned int protocolVersion; // Specific version of protocol implementation
	};

}

#endif
