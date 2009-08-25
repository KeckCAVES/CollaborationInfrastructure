/***********************************************************************
GrapheinPipe - Common interface between a Graphein server and a Graphein
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

#ifndef GRAPHEINPIPE_INCLUDED
#define GRAPHEINPIPE_INCLUDED

#include <vector>
#include <Misc/HashTable.h>
#include <GL/gl.h>
#include <GL/GLColor.h>
#include <Collaboration/CollaborationPipe.h>

namespace Collaboration {

struct GrapheinPipe
	{
	/* Embedded classes: */
	protected:
	enum MessageId // Enumerated type for Graphein protocol messages
		{
		ADD_CURVE=0,
		APPEND_POINT,
		DELETE_CURVE,
		DELETE_ALL_CURVES,
		UPDATE_END,
		MESSAGES_END
		};
	
	typedef CollaborationPipe::Scalar Scalar; // Scalar type for geometry data
	typedef CollaborationPipe::Point Point; // Point type
	
	struct Curve // Structure to represent single-stroke curves
		{
		/* Embedded classes: */
		public:
		typedef GLColor<GLubyte,3> Color; // Type for colors
		
		/* Elements: */
		GLfloat lineWidth; // The curve's cosmetic line width
		Color color; // The curve's color
		std::vector<Point> vertices; // The curve's vertices
		};
	
	typedef Misc::HashTable<unsigned int,Curve*> CurveHasher; // Hash table to map curve ID to curve objects
	
	struct CurveAction // Structure to retain changes to a client's state between update packets
		{
		/* Elements: */
		public:
		CollaborationPipe::MessageIdType action; // Kind of action; taken from protocol message IDs
		CurveHasher::Iterator curveIt; // Iterator to the affected curve in the curve hash table
		Point newVertex; // New curve vertex for APPEND_POINT actions
		
		/* Constructors and destructors: */
		CurveAction(CollaborationPipe::MessageIdType sAction)
			:action(sAction)
			{
			}
		CurveAction(CollaborationPipe::MessageIdType sAction,const CurveHasher::Iterator& sCurveIt)
			:action(sAction),curveIt(sCurveIt)
			{
			}
		CurveAction(CollaborationPipe::MessageIdType sAction,const CurveHasher::Iterator& sCurveIt,const Point& sNewVertex)
			:action(sAction),curveIt(sCurveIt),newVertex(sNewVertex)
			{
			}
		};
	
	typedef std::vector<CurveAction> CurveActionList; // Type for lists of curve actions
	
	/* Elements: */
	static const char* protocolName; // Network name of Graphein protocol
	static const unsigned int protocolVersion; // Specific version of protocol implementation
	
	/* Methods: */
	void writeCurve(CollaborationPipe& pipe,const Curve& curve); // Sends a curve to the pipe
	Curve& readCurve(CollaborationPipe& pipe,Curve& curve); // Receives a curve from the pipe
	};

}

#endif
