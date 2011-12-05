/***********************************************************************
GrapheinClient - Client object to implement the Graphein shared
annotation protocol.
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

#include <Collaboration/GrapheinClient.h>

#include <iostream>
#include <Misc/ThrowStdErr.h>
#include <IO/FixedMemoryFile.h>
#include <Comm/NetPipe.h>
#include <GL/gl.h>
#include <GL/GLColorTemplates.h>
#include <GL/GLGeometryWrappers.h>
#include <GL/GLTransformationWrappers.h>
#include <GLMotif/StyleSheet.h>
#include <GLMotif/WidgetManager.h>
#include <GLMotif/PopupWindow.h>
#include <GLMotif/RowColumn.h>
#include <GLMotif/Blind.h>
#include <GLMotif/Label.h>
#include <GLMotif/TextField.h>
#include <Vrui/Vrui.h>
#include <Vrui/InputDevice.h>

namespace Collaboration {

/**************************************************
Methods of class GrapheinClient::RemoteClientState:
**************************************************/

GrapheinClient::RemoteClientState::RemoteClientState(void)
	:curves(17)
	{
	}

GrapheinClient::RemoteClientState::~RemoteClientState(void)
	{
	/* Delete all curves in the hash table: */
	for(CurveMap::Iterator cIt=curves.begin();!cIt.isFinished();++cIt)
		delete cIt->getDest();
	
	/* Delete any leftover message buffers: */
	{
	Threads::Mutex::Lock messageBufferLock(messageBufferMutex);
	for(std::vector<IncomingMessage*>::iterator mIt=messages.begin();mIt!=messages.end();++mIt)
		delete *mIt;
	}
	}

void GrapheinClient::RemoteClientState::processMessages(void)
	{
	Threads::Mutex::Lock messageBufferLock(messageBufferMutex);
	
	/* Handle all state tracking messages: */
	for(std::vector<IncomingMessage*>::iterator mIt=messages.begin();mIt!=messages.end();++mIt)
		{
		/* Process all messages in this buffer: */
		IO::File& msg=**mIt;
		while(!msg.eof())
			{
			/* Read the next message: */
			MessageIdType message=GrapheinProtocol::readMessage(msg);
			switch(message)
				{
				case ADD_CURVE:
					{
					unsigned int newCurveId=msg.read<Card>();
					
					/* Check if a curve of the given ID already exists: */
					if(curves.isEntry(newCurveId))
						{
						/* Skip the curve definition: */
						Curve dummy;
						dummy.read(msg);
						}
					else
						{
						/* Create a new curve and add it to the curve map: */
						Curve* newCurve=new Curve;
						curves.setEntry(CurveMap::Entry(newCurveId,newCurve));
						newCurve->read(msg);
						}
					
					break;
					}
				
				case APPEND_POINT:
					{
					/* Read the curve ID and index of the new vertex: */
					unsigned int curveId=msg.read<Card>();
					unsigned int vertexIndex=msg.read<Card>();
					
					/* Read the vertex: */
					Point newVertex=GrapheinProtocol::read<Point>(msg);
					
					/* Get a handle on the curve: */
					CurveMap::Iterator cIt=curves.findEntry(curveId);
					if(!cIt.isFinished())
						{
						/* Check that the curve doesn't already contains the vertex: */
						if(vertexIndex>=cIt->getDest()->vertices.size())
							{
							/* Append the vertex: */
							cIt->getDest()->vertices.push_back(newVertex);
							}
						}
					break;
					}
				
				case DELETE_CURVE:
					{
					/* Read the curve ID: */
					unsigned int curveId=msg.read<Card>();
					
					/* Get a handle on the curve: */
					CurveMap::Iterator cIt=curves.findEntry(curveId);
					if(!cIt.isFinished())
						{
						/* Delete the curve: */
						delete cIt->getDest();
						curves.removeEntry(cIt);
						}
					
					break;
					}
				
				case DELETE_ALL_CURVES:
					{
					/* Delete all curves: */
					for(CurveMap::Iterator cIt=curves.begin();!cIt.isFinished();++cIt)
						delete cIt->getDest();
					curves.clear();
					break;
					}
				}
			}
		
		/* Destroy the just-read message buffer: */
		delete *mIt;
		}
	
	/* Clear the message buffer list: */
	messages.clear();
	}

void GrapheinClient::RemoteClientState::glRenderAction(GLContextData& contextData) const
	{
	glPushAttrib(GL_ENABLE_BIT|GL_LINE_BIT);
	glDisable(GL_LIGHTING);
	
	/* Render all curves: */
	for(CurveMap::ConstIterator cIt=curves.begin();!cIt.isFinished();++cIt)
		{
		glLineWidth(cIt->getDest()->lineWidth);
		glColor(cIt->getDest()->color);
		glBegin(GL_LINE_STRIP);
		for(std::vector<Point>::const_iterator vIt=cIt->getDest()->vertices.begin();vIt!=cIt->getDest()->vertices.end();++vIt)
			glVertex(*vIt);
		glEnd();
		}
	
	glPopAttrib();
	}

/*****************************************************
Static elements of class GrapheinClient::GrapheinTool:
*****************************************************/

GrapheinClient::GrapheinToolFactory* GrapheinClient::GrapheinTool::factory=0;
const GrapheinClient::Curve::Color GrapheinClient::GrapheinTool::curveColors[8]=
	{
	GrapheinClient::Curve::Color(0,0,0),GrapheinClient::Curve::Color(255,0,0),
	GrapheinClient::Curve::Color(255,255,0),GrapheinClient::Curve::Color(0,255,0),
	GrapheinClient::Curve::Color(0,255,255),GrapheinClient::Curve::Color(0,0,255),
	GrapheinClient::Curve::Color(255,0,255),GrapheinClient::Curve::Color(255,255,255)
	};

/*********************************************
Methods of class GrapheinClient::GrapheinTool:
*********************************************/

GrapheinClient::GrapheinTool::GrapheinTool(const Vrui::ToolFactory* factory,const Vrui::ToolInputAssignment& inputAssignment)
	:Vrui::UtilityTool(factory,inputAssignment),
	 client(0),
	 controlDialogPopup(0),lineWidthValue(0),colorBox(0),
	 newLineWidth(3.0f),newColor(255,0,0),
	 active(false)
	{
	/* Get the style sheet: */
	const GLMotif::StyleSheet* ss=Vrui::getWidgetManager()->getStyleSheet();
	
	/* Build the tool control dialog: */
	controlDialogPopup=new GLMotif::PopupWindow("GrapheinToolControlDialog",Vrui::getWidgetManager(),"Shared Curve Editor Settings");
	controlDialogPopup->setResizableFlags(false,false);
	
	GLMotif::RowColumn* controlDialog=new GLMotif::RowColumn("ControlDialog",controlDialogPopup,false);
	controlDialog->setNumMinorWidgets(2);
	
	/* Create a slider to set the line width: */
	new GLMotif::Label("LineWidthLabel",controlDialog,"Line Width");
	
	GLMotif::RowColumn* lineWidthBox=new GLMotif::RowColumn("LineWidthBox",controlDialog,false);
	lineWidthBox->setOrientation(GLMotif::RowColumn::HORIZONTAL);
	
	lineWidthValue=new GLMotif::TextField("LineWidthValue",lineWidthBox,4);
	lineWidthValue->setFloatFormat(GLMotif::TextField::FIXED);
	lineWidthValue->setPrecision(1);
	lineWidthValue->setValue(newLineWidth);
	
	GLMotif::Slider* lineWidthSlider=new GLMotif::Slider("LineWidthSlider",lineWidthBox,GLMotif::Slider::HORIZONTAL,ss->fontHeight*10.0f);
	lineWidthSlider->setValueRange(0.5,11.0,0.5);
	lineWidthSlider->setValue(newLineWidth);
	lineWidthSlider->getValueChangedCallbacks().add(this,&GrapheinTool::lineWidthSliderCallback);
	
	lineWidthBox->manageChild();
	
	/* Create a radio box to set the line color: */
	new GLMotif::Label("ColorLabel",controlDialog,"Color");
	
	colorBox=new GLMotif::RowColumn("ColorBox",controlDialog,false);
	colorBox->setOrientation(GLMotif::RowColumn::HORIZONTAL);
	colorBox->setPacking(GLMotif::RowColumn::PACK_GRID);
	colorBox->setAlignment(GLMotif::Alignment::LEFT);
	
	/* Add the color buttons: */
	for(int i=0;i<8;++i)
		{
		char colorButtonName[16];
		snprintf(colorButtonName,sizeof(colorButtonName),"ColorButton%d",i);
		GLMotif::NewButton* colorButton=new GLMotif::NewButton(colorButtonName,colorBox,GLMotif::Vector(ss->fontHeight,ss->fontHeight,0.0f));
		colorButton->setBackgroundColor(GLMotif::Color(curveColors[i]));
		colorButton->getSelectCallbacks().add(this,&GrapheinTool::colorButtonSelectCallback);
		}
	
	colorBox->manageChild();
	
	/* Add a button to clear all curves: */
	new GLMotif::Blind("Blind1",controlDialog);
	
	GLMotif::NewButton* deleteCurvesButton=new GLMotif::NewButton("DeleteCurvesButton",controlDialog,"Delete All Curves");
	deleteCurvesButton->getSelectCallbacks().add(this,&GrapheinTool::deleteCurvesCallback);
	
	controlDialog->manageChild();
	
	/* Pop up the control dialog: */
	Vrui::popupPrimaryWidget(controlDialogPopup);
	}

GrapheinClient::GrapheinTool::~GrapheinTool(void)
	{
	delete controlDialogPopup;
	}

void GrapheinClient::GrapheinTool::buttonCallback(int,Vrui::InputDevice::ButtonCallbackData* cbData)
	{
	if(client==0)
		return;
	
	/* Check if the button has just been pressed: */
	if(cbData->newButtonState)
		{
		/* Activate the tool: */
		active=true;
		
		/* Start a new curve: */
		currentCurveId=client->nextLocalCurveId;
		++client->nextLocalCurveId;
		Curve* newCurve=new Curve;
		newCurve->lineWidth=newLineWidth;
		newCurve->color=newColor;
		const Vrui::NavTransform& invNav=Vrui::getInverseNavigationTransformation();
		lastPoint=invNav.transform(getButtonDevicePosition(0));
		newCurve->vertices.push_back(lastPoint);
		client->localCurves.setEntry(CurveMap::Entry(currentCurveId,newCurve));
		
		/* Send a curve creation message: */
		{
		Threads::Mutex::Lock messageLock(client->messageMutex);
		writeMessage(ADD_CURVE,client->message);
		client->message.write<Card>(currentCurveId);
		newCurve->write(client->message);
		}
		}
	else
		{
		/* Retrieve the current curve: */
		CurveMap::Iterator cIt=client->localCurves.findEntry(currentCurveId);
		if(!cIt.isFinished())
			{
			if(currentPoint!=lastPoint)
				{
				/* Add the final dragging point to the curve: */
				cIt->getDest()->vertices.push_back(currentPoint);
				
				/* Send a vertex appending message: */
				{
				Threads::Mutex::Lock messageLock(client->messageMutex);
				writeMessage(APPEND_POINT,client->message);
				client->message.write<Card>(currentCurveId);
				write(currentPoint,client->message);
				}
				}
			}
		
		/* Deactivate the tool: */
		active=false;
		}
	}

void GrapheinClient::GrapheinTool::frame(void)
	{
	if(client==0)
		return;
	
	if(active)
		{
		/* Update the current dragging point: */
		const Vrui::NavTransform& invNav=Vrui::getInverseNavigationTransformation();
		currentPoint=invNav.transform(getButtonDevicePosition(0));
		
		/* Check if the dragging point is far enough away from the most recent curve vertex: */
		if(Geometry::sqrDist(currentPoint,lastPoint)>=Math::sqr(Vrui::getUiSize()*invNav.getScaling()))
			{
			CurveMap::Iterator cIt=client->localCurves.findEntry(currentCurveId);
			if(!cIt.isFinished())
				{
				/* Add the dragging point to the curve: */
				cIt->getDest()->vertices.push_back(currentPoint);
				
				/* Send a vertex appending message: */
				{
				Threads::Mutex::Lock messageLock(client->messageMutex);
				writeMessage(APPEND_POINT,client->message);
				client->message.write<Card>(currentCurveId);
				write(currentPoint,client->message);
				}
				}
			
			/* Remember the last added point: */
			lastPoint=currentPoint;
			}
		}
	}

void GrapheinClient::GrapheinTool::display(GLContextData& contextData) const
	{
	if(client==0)
		return;
	
	if(active)
		{
		/* Retrieve the current curve: */
		CurveMap::Iterator cIt=client->localCurves.findEntry(currentCurveId);
		if(!cIt.isFinished())
			{
			/* Render the last segment of the current curve: */
			glPushAttrib(GL_ENABLE_BIT|GL_LINE_BIT);
			glDisable(GL_LIGHTING);
			glLineWidth(cIt->getDest()->lineWidth);
			
			glPushMatrix();
			glMultMatrix(Vrui::getNavigationTransformation());
			
			glColor(cIt->getDest()->color);
			glBegin(GL_LINES);
			glVertex(lastPoint);
			glVertex(currentPoint);
			glEnd();
			
			glPopMatrix();
			
			glPopAttrib();
			}
		}
	}

void GrapheinClient::GrapheinTool::lineWidthSliderCallback(GLMotif::Slider::ValueChangedCallbackData* cbData)
	{
	/* Get the new line width: */
	newLineWidth=GLfloat(cbData->value);
	
	/* Update the line width value: */
	lineWidthValue->setValue(newLineWidth);
	}

void GrapheinClient::GrapheinTool::colorButtonSelectCallback(GLMotif::NewButton::SelectCallbackData* cbData)
	{
	/* Set the new line color: */
	newColor=curveColors[colorBox->getChildIndex(cbData->button)];
	}

void GrapheinClient::GrapheinTool::deleteCurvesCallback(Misc::CallbackData* cbData)
	{
	if(client==0)
		return;
	
	/* Delete all local curves: */
	for(CurveMap::Iterator cIt=client->localCurves.begin();!cIt.isFinished();++cIt)
		delete cIt->getDest();
	client->localCurves.clear();
	
	/* Send a curve deletion message: */
	{
	Threads::Mutex::Lock messageLock(client->messageMutex);
	writeMessage(DELETE_ALL_CURVES,client->message);
	}
	
	/* Deactivate the tool just in case: */
	active=false;
	}

/*******************************
Methods of class GrapheinClient:
*******************************/

GrapheinClient::GrapheinClient(void)
	:nextLocalCurveId(0),localCurves(17)
	{
	/* Register the Graphein tool class: */
	GrapheinToolFactory* grapheinToolFactory=new GrapheinToolFactory("GrapheinTool","Shared Curve Editor",Vrui::getToolManager()->loadClass("UtilityTool"),*Vrui::getToolManager());
	grapheinToolFactory->setNumButtons(1);
	grapheinToolFactory->setButtonFunction(0,"Draw Curves");
	Vrui::getToolManager()->addClass(grapheinToolFactory,Vrui::ToolManager::defaultToolFactoryDestructor);
	}

GrapheinClient::~GrapheinClient(void)
	{
	/* Delete all local curves: */
	for(CurveMap::Iterator cIt=localCurves.begin();!cIt.isFinished();++cIt)
		delete cIt->getDest();
	
	/* Uninstall tool manager callbacks: */
	Vrui::getToolManager()->getToolCreationCallbacks().remove(this,&GrapheinClient::toolCreationCallback);
	
	/* Unregister the Graphein tool class before the client is unloaded: */
	Vrui::getToolManager()->releaseClass("GrapheinTool");
	}

const char* GrapheinClient::getName(void) const
	{
	return protocolName;
	}

unsigned int GrapheinClient::getNumMessages(void) const
	{
	return MESSAGES_END;
	}

void GrapheinClient::initialize(CollaborationClient* sClient,Misc::ConfigurationFileSection& configFileSection)
	{
	/* Call the base class method: */
	ProtocolClient::initialize(sClient,configFileSection);
	}

void GrapheinClient::sendConnectRequest(Comm::NetPipe& pipe)
	{
	/* Send the length of the following message: */
	pipe.write<Card>(sizeof(Card));
	
	/* Send the client's protocol version: */
	pipe.write<Card>(protocolVersion);
	}

void GrapheinClient::receiveConnectReply(Comm::NetPipe& pipe)
	{
	/* Set the message buffer's endianness swapping behavior to that of the pipe: */
	message.setSwapOnWrite(pipe.mustSwapOnWrite());
	
	/* Register callbacks with the tool manager: */
	Vrui::getToolManager()->getToolCreationCallbacks().add(this,&GrapheinClient::toolCreationCallback);
	}

void GrapheinClient::receiveDisconnectReply(Comm::NetPipe& pipe)
	{
	/* Uninstall tool manager callbacks: */
	Vrui::getToolManager()->getToolCreationCallbacks().remove(this,&GrapheinClient::toolCreationCallback);
	}

ProtocolClient::RemoteClientState* GrapheinClient::receiveClientConnect(Comm::NetPipe& pipe)
	{
	/* Create a new remote client state object: */
	RemoteClientState* newClientState=new RemoteClientState;
	
	/* Read the number of existing curves in this message: */
	unsigned int numCurves=pipe.read<Card>();
	
	/* Read all curves: */
	for(unsigned int i=0;i<numCurves;++i)
		{
		/* Read the new curve's ID: */
		unsigned int newCurveId=pipe.read<Card>();
		
		/* Create the new curve and add it to the new client's curve set: */
		Curve* newCurve=new Curve;
		newClientState->curves.setEntry(CurveMap::Entry(newCurveId,newCurve));
		
		/* Read the curve's state: */
		newCurve->read(pipe);
		}
	
	return newClientState;
	}

bool GrapheinClient::receiveServerUpdate(ProtocolClient::RemoteClientState* rcs,Comm::NetPipe& pipe)
	{
	/* Get a handle on the remote client state object: */
	RemoteClientState* myRcs=dynamic_cast<RemoteClientState*>(rcs);
	if(myRcs==0)
		Misc::throwStdErr("GrapheinClient::receiveServerUpdate: Mismatching remote client state object type");
	
	/* Read the size of the following message: */
	unsigned int messageSize=pipe.read<Card>();
	
	/* Read the entire message into a new read buffer that has the same endianness as the pipe's read end: */
	IncomingMessage* msg=new IncomingMessage(messageSize);
	msg->setSwapOnRead(pipe.mustSwapOnRead());
	pipe.readRaw(msg->getMemory(),messageSize);
	
	/* Store the new buffer in the client's message list: */
	{
	Threads::Mutex::Lock messageBufferLock(myRcs->messageBufferMutex);
	myRcs->messages.push_back(msg);
	}
	
	return messageSize!=0;
	}

void GrapheinClient::sendClientUpdate(Comm::NetPipe& pipe)
	{
	/* Send accumulated state tracking messages all at once and then clear the message buffer: */
	{
	Threads::Mutex::Lock messageLock(messageMutex);
	message.writeToSink(pipe);
	message.clear();
	}
	
	/* Terminate the action list: */
	writeMessage(UPDATE_END,pipe);
	}

void GrapheinClient::frame(ProtocolClient::RemoteClientState* rcs)
	{
	/* Get a handle on the remote client state object: */
	RemoteClientState* myRcs=dynamic_cast<RemoteClientState*>(rcs);
	if(myRcs==0)
		Misc::throwStdErr("GrapheinClient::frame: Mismatching remote client state object type");
	
	/* Process the remote client's queued server update messages: */
	myRcs->processMessages();
	}

void GrapheinClient::glRenderAction(GLContextData& contextData) const
	{
	glPushAttrib(GL_ENABLE_BIT|GL_LINE_BIT);
	glDisable(GL_LIGHTING);
	
	/* Render all curves: */
	for(CurveMap::ConstIterator cIt=localCurves.begin();!cIt.isFinished();++cIt)
		{
		glLineWidth(cIt->getDest()->lineWidth);
		glColor(cIt->getDest()->color);
		glBegin(GL_LINE_STRIP);
		for(std::vector<Point>::const_iterator vIt=cIt->getDest()->vertices.begin();vIt!=cIt->getDest()->vertices.end();++vIt)
			glVertex(*vIt);
		glEnd();
		}
	
	glPopAttrib();
	}

void GrapheinClient::glRenderAction(const ProtocolClient::RemoteClientState* rcs,GLContextData& contextData) const
	{
	/* Get a handle on the Graphein state object: */
	const RemoteClientState* myRcs=dynamic_cast<const RemoteClientState*>(rcs);
	if(myRcs==0)
		Misc::throwStdErr("GrapheinClient::display: Remote client state object has mismatching type");
	
	/* Display the remote client's state: */
	myRcs->glRenderAction(contextData);
	}

void GrapheinClient::toolCreationCallback(Vrui::ToolManager::ToolCreationCallbackData* cbData)
	{
	/* Check if the new tool is a Graphein tool: */
	GrapheinTool* grapheinTool=dynamic_cast<GrapheinTool*>(cbData->tool);
	if(grapheinTool!=0)
		{
		/* Set the tool's client pointer: */
		grapheinTool->client=this;
		}
	}

}

/****************
DSO entry points:
****************/

extern "C" {

Collaboration::ProtocolClient* createObject(Collaboration::ProtocolClientLoader& objectLoader)
	{
	return new Collaboration::GrapheinClient;
	}

void destroyObject(Collaboration::ProtocolClient* object)
	{
	delete object;
	}

}
