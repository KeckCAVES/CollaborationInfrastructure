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

#include <Collaboration/GrapheinProtocol.h>

#include <IO/File.h>

namespace Collaboration {

/****************************************
Methods of class GrapheinProtocol::Curve:
****************************************/

void GrapheinProtocol::Curve::read(IO::File& source)
	{
	/* Read the curve's cosmetic line width: */
	lineWidth=GLfloat(source.read<Misc::Float32>());
	
	/* Read the curve's color: */
	Misc::UInt8 rgb[3];
	source.read(rgb,3);
	color=Color(rgb);
	
	/* Read the curve's vertex array: */
	vertices.clear();
	unsigned int numVertices=source.read<Card>();
	vertices.reserve(numVertices);
	for(unsigned int i=0;i<numVertices;++i)
		vertices.push_back(GrapheinProtocol::read<Point>(source));
	}

void GrapheinProtocol::Curve::write(IO::File& sink) const
	{
	/* Write the curve's cosmetic line width: */
	sink.write<Misc::Float32>(Misc::Float32(lineWidth));
	
	/* Write the curve's color: */
	for(int i=0;i<3;++i)
		sink.write<Misc::UInt8>(color.getRgba()[i]);
	
	/* Write the curve's vertex array: */
	sink.write<Card>(Card(vertices.size()));
	for(std::vector<Point>::const_iterator vIt=vertices.begin();vIt!=vertices.end();++vIt)
		GrapheinProtocol::write<Point>(*vIt,sink);
	}

/*****************************************
Static elements of class GrapheinProtocol:
*****************************************/

const char* GrapheinProtocol::protocolName="Graphein";
const unsigned int GrapheinProtocol::protocolVersion=(2U<<16)+0U; // Version 2.0

}
