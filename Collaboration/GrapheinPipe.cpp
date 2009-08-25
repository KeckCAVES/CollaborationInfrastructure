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

#include <Collaboration/GrapheinPipe.h>

namespace Collaboration {

/*************************************
Static elements of class GrapheinPipe:
*************************************/

const char* GrapheinPipe::protocolName="Graphein";
const unsigned int GrapheinPipe::protocolVersion=1*65536+0; // Version 1.0

/*****************************
Methods of class GrapheinPipe:
*****************************/

void GrapheinPipe::writeCurve(CollaborationPipe& pipe,const GrapheinPipe::Curve& curve)
	{
	/* Write the curve's line width and color: */
	pipe.write<GLfloat>(curve.lineWidth);
	pipe.write<GLubyte>(curve.color.getRgba(),3);
	
	/* Write the curve's vertex array: */
	unsigned int numVertices=curve.vertices.size();
	pipe.write(numVertices);
	for(std::vector<Point>::const_iterator vIt=curve.vertices.begin();vIt!=curve.vertices.end();++vIt)
		pipe.write<Scalar>(vIt->getComponents(),3);
	}

GrapheinPipe::Curve& GrapheinPipe::readCurve(CollaborationPipe& pipe,GrapheinPipe::Curve& curve)
	{
	/* Read the curve's line width and color: */
	curve.lineWidth=pipe.read<GLfloat>();
	pipe.read<GLubyte>(curve.color.getRgba(),3);
	
	/* Read the curve's vertex array: */
	curve.vertices.clear();
	unsigned int numVertices=pipe.read<unsigned int>();
	curve.vertices.reserve(numVertices);
	for(unsigned int i=0;i<numVertices;++i)
		{
		Point v;
		pipe.read<Scalar>(v.getComponents(),3);
		curve.vertices.push_back(v);
		}
	
	return curve;
	}

}
