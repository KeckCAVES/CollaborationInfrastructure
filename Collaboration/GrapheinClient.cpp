/***********************************************************************
GrapheinClient - Client object to implement the Graphein shared
annotation protocol.
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

#include <Collaboration/GrapheinClient.h>

#include <iostream>
#include <Misc/ThrowStdErr.h>
#include <Geometry/OrthogonalTransformation.h>
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
	:curves(101)
	{
	}

GrapheinClient::RemoteClientState::~RemoteClientState(void)
	{
	/* Delete all curves in the hash table: */
	for(CurveHasher::Iterator cIt=curves.begin();!cIt.isFinished();++cIt)
		delete cIt->getDest();
	}

void GrapheinClient::RemoteClientState::display(GLContextData& contextData) const
	{
	glPushAttrib(GL_ENABLE_BIT|GL_LINE_BIT);
	glDisable(GL_LIGHTING);
	
	/* Render all curves: */
	{
	Threads::Mutex::Lock curveLock(curveMutex);
	
	for(CurveHasher::ConstIterator cIt=curves.begin();!cIt.isFinished();++cIt)
		{
		glLineWidth(cIt->getDest()->lineWidth);
		glColor(cIt->getDest()->color);
		glBegin(GL_LINE_STRIP);
		for(std::vector<Point>::const_iterator vIt=cIt->getDest()->vertices.begin();vIt!=cIt->getDest()->vertices.end();++vIt)
			glVertex(*vIt);
		glEnd();
		}
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
	:Vrui::Tool(factory,inputAssignment),
	 client(0),
	 controlDialogPopup(0),lineWidthValue(0),colorBox(0),
	 newLineWidth(3.0f),newColor(255,0,0),
	 active(false)
	{
	/* Get the style sheet: */
	const GLMotif::StyleSheet* ss=Vrui::getWidgetManager()->getStyleSheet();
	
	/* Build the tool control dialog: */
	controlDialogPopup=new GLMotif::PopupWindow("GrapheinToolControlDialog",Vrui::getWidgetManager(),"Annotation Tool Settings");
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
	Vrui::popupPrimaryWidget(controlDialogPopup,Vrui::getNavigationTransformation().transform(Vrui::getDisplayCenter()));
	}

GrapheinClient::GrapheinTool::~GrapheinTool(void)
	{
	delete controlDialogPopup;
	}

void GrapheinClient::GrapheinTool::buttonCallback(int deviceIndex,int buttonIndex,Vrui::InputDevice::ButtonCallbackData* cbData)
	{
	if(client==0)
		return;
	
	/* Check if the button has just been pressed: */
	if(cbData->newButtonState)
		{
		/* Activate the tool: */
		active=true;
		
		/* Start a new curve: */
		currentCurveId=client->nextCurveId;
		++client->nextCurveId;
		Curve* newCurve=new Curve;
		newCurve->lineWidth=newLineWidth;
		newCurve->color=newColor;
		const Vrui::NavTransform& invNav=Vrui::getInverseNavigationTransformation();
		lastPoint=invNav.transform(getDevicePosition(0));
		newCurve->vertices.push_back(lastPoint);
		client->curves.setEntry(CurveHasher::Entry(currentCurveId,newCurve));
		
		/* Enqueue a curve action: */
		client->actions.push_back(CurveAction(ADD_CURVE,client->curves.findEntry(currentCurveId)));
		}
	else
		{
		/* Retrieve the current curve: */
		CurveHasher::Iterator curveIt=client->curves.findEntry(currentCurveId);
		if(!curveIt.isFinished())
			{
			/* Add the final dragging point to the curve: */
			if(currentPoint!=lastPoint)
				{
				/* Enqueue a curve action: */
				client->actions.push_back(CurveAction(APPEND_POINT,client->curves.findEntry(currentCurveId),currentPoint));
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
		currentPoint=invNav.transform(getDevicePosition(0));
		
		/* Check if the dragging point is far enough away from the most recent curve vertex: */
		if(Geometry::sqrDist(currentPoint,lastPoint)>=Math::sqr(Vrui::getUiSize()*invNav.getScaling()))
			{
			/* Enqueue a curve action: */
			client->actions.push_back(CurveAction(APPEND_POINT,client->curves.findEntry(currentCurveId),currentPoint));
			
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
		CurveHasher::Iterator curveIt=client->curves.findEntry(currentCurveId);
		if(!curveIt.isFinished())
			{
			/* Render the last segment of the current curve: */
			glPushAttrib(GL_ENABLE_BIT|GL_LINE_BIT);
			glDisable(GL_LIGHTING);
			glLineWidth(curveIt->getDest()->lineWidth);
			
			glPushMatrix();
			glMultMatrix(Vrui::getNavigationTransformation());
			
			glColor(curveIt->getDest()->color);
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
	/* Enqueue a curve action: */
	client->actions.push_back(CurveAction(DELETE_ALL_CURVES));
	
	/* Deactivate the tool just in case: */
	active=false;
	}

/*******************************
Methods of class GrapheinClient:
*******************************/

GrapheinClient::GrapheinClient(void)
	:nextCurveId(0),curves(101)
	{
	/* Register callbacks with the tool manager: */
	Vrui::getToolManager()->getToolCreationCallbacks().add(this,&GrapheinClient::toolCreationCallback);
	}

GrapheinClient::~GrapheinClient(void)
	{
	/* Delete all curves in the hash table: */
	for(CurveHasher::Iterator cIt=curves.begin();!cIt.isFinished();++cIt)
		delete cIt->getDest();
	
	/* Uninstall tool manager callbacks: */
	Vrui::getToolManager()->getToolCreationCallbacks().remove(this,&GrapheinClient::toolCreationCallback);
	}

const char* GrapheinClient::getName(void) const
	{
	return protocolName;
	}

unsigned int GrapheinClient::getNumMessages(void) const
	{
	return MESSAGES_END;
	}

void GrapheinClient::initialize(CollaborationClient& collaborationClient,Misc::ConfigurationFileSection& configFileSection)
	{
	}

void GrapheinClient::sendConnectRequest(CollaborationPipe& pipe)
	{
	/* Send the length of the following message: */
	pipe.write<unsigned int>(sizeof(unsigned int));
	
	/* Send the client's protocol version: */
	pipe.write<unsigned int>(protocolVersion);
	}

void GrapheinClient::receiveConnectReply(CollaborationPipe& pipe)
	{
	/* Register the Graphein tool class: */
	GrapheinToolFactory* grapheinToolFactory=new GrapheinToolFactory("GrapheinTool","Annotation Tool",0,*Vrui::getToolManager());
	grapheinToolFactory->setNumDevices(1);
	grapheinToolFactory->setNumButtons(0,1);
	Vrui::getToolManager()->addClass(grapheinToolFactory,Vrui::ToolManager::defaultToolFactoryDestructor);
	}

void GrapheinClient::sendClientUpdate(CollaborationPipe& pipe)
	{
	{
	Threads::Mutex::Lock curveLock(curveMutex);
	
	/* Send the curve action list to the server: */
	for(CurveActionList::const_iterator aIt=actions.begin();aIt!=actions.end();++aIt)
		{
		switch(aIt->action)
			{
			case ADD_CURVE:
				/* Send a creation message: */
				pipe.writeMessage(ADD_CURVE);
				
				/* Send the new curve's ID: */
				pipe.write<unsigned int>(aIt->curveIt->getSource());
				
				/* Send the new curve: */
				writeCurve(pipe,*(aIt->curveIt->getDest()));
				
				break;
			
			case APPEND_POINT:
				/* Send an append message: */
				pipe.writeMessage(APPEND_POINT);
				
				/* Send the affected curve's ID: */
				pipe.write<unsigned int>(aIt->curveIt->getSource());
				
				/* Send the new vertex: */
				pipe.write<Scalar>(aIt->newVertex.getComponents(),3);
				
				/* Append the new vertex to the curve: */
				aIt->curveIt->getDest()->vertices.push_back(aIt->newVertex);
				
				break;
			
			case DELETE_CURVE:
				/* Send a delete message: */
				pipe.writeMessage(DELETE_CURVE);
				
				/* Send the affected curve's ID: */
				pipe.write<unsigned int>(aIt->curveIt->getSource());
				
				/* Delete the curve: */
				delete aIt->curveIt->getDest();
				curves.removeEntry(aIt->curveIt);
				
				break;
			
			case DELETE_ALL_CURVES:
				/* Send a delete message: */
				pipe.writeMessage(DELETE_ALL_CURVES);
				
				/* Delete all curves: */
				for(CurveHasher::Iterator cIt=curves.begin();!cIt.isFinished();++cIt)
					delete cIt->getDest();
				
				curves.clear();
				
				break;
			
			default:
				; // Just to make g++ happy
			}
		}
	
	/* Terminate the action list: */
	pipe.writeMessage(UPDATE_END);
	
	/* Clear the action list: */
	actions.clear();
	}
	}

GrapheinClient::RemoteClientState* GrapheinClient::receiveClientConnect(CollaborationPipe& pipe)
	{
	/* Create a new remote client state object: */
	RemoteClientState* newClientState=new RemoteClientState;
	
	/* Receive the number of existing curves in the remote client: */
	unsigned int numCurves=pipe.read<unsigned int>();
	
	/* Receive all curves: */
	for(unsigned int i=0;i<numCurves;++i)
		{
		/* Receive the curve's ID: */
		unsigned int curveId=pipe.read<unsigned int>();
		
		/* Create a new curve and add it to the remote client state: */
		Curve* newCurve=new Curve;
		newClientState->curves.setEntry(CurveHasher::Entry(curveId,newCurve));
		
		/* Receive the curve: */
		readCurve(pipe,*newCurve);
		}
	
	/* Return the new client state object: */
	return newClientState;
	}

void GrapheinClient::receiveServerUpdate(ProtocolClient::RemoteClientState* rcs,CollaborationPipe& pipe)
	{
	/* Get a handle on the remote client state object: */
	RemoteClientState* myRcs=dynamic_cast<RemoteClientState*>(rcs);
	if(myRcs==0)
		Misc::throwStdErr("GrapheinClient::receiveServerUpdate: Mismatching remote client state object type");
	
	/* Receive a list of curve action messages from the client: */
	{
	Threads::Mutex::Lock curveLock(myRcs->curveMutex);
	
	CollaborationPipe::MessageIdType message;
	while((message=pipe.readMessage())!=UPDATE_END)
		switch(message)
			{
			case ADD_CURVE:
				{
				/* Read the new curve's ID: */
				unsigned int newCurveId=pipe.read<unsigned int>();
				
				/* Create a new curve object and add it to the client state's set: */
				Curve* newCurve=new Curve;
				myRcs->curves.setEntry(CurveHasher::Entry(newCurveId,newCurve));
				
				/* Receive the new curve from the pipe: */
				readCurve(pipe,*newCurve);
				
				break;
				}
			
			case APPEND_POINT:
				{
				/* Read the affected curve's ID: */
				unsigned int curveId=pipe.read<unsigned int>();
				
				/* Read the new vertex position: */
				Point newVertex;
				pipe.read<Scalar>(newVertex.getComponents(),3);
				
				/* Retrieve the affected curve: */
				CurveHasher::Iterator curveIt=myRcs->curves.findEntry(curveId);
				if(!curveIt.isFinished())
					{
					/* Append the vertex to the curve: */
					curveIt->getDest()->vertices.push_back(newVertex);
					}
				
				break;
				}
			
			case DELETE_CURVE:
				{
				/* Read the affected curve's ID: */
				unsigned int curveId=pipe.read<unsigned int>();
				
				/* Retrieve the affected curve: */
				CurveHasher::Iterator curveIt=myRcs->curves.findEntry(curveId);
				if(!curveIt.isFinished())
					{
					/* Delete the curve: */
					delete curveIt->getDest();
					myRcs->curves.removeEntry(curveIt);
					}
				
				break;
				}
			
			case DELETE_ALL_CURVES:
				{
				/* Delete all curves in the hash table: */
				for(CurveHasher::Iterator cIt=myRcs->curves.begin();!cIt.isFinished();++cIt)
					delete cIt->getDest();
				
				myRcs->curves.clear();
				
				break;
				}
			
			default:
				Misc::throwStdErr("GrapheinClient::receiveServerUpdate: received unknown curve action message %u",message);
			}
	}
	}

void GrapheinClient::frame(ProtocolClient::RemoteClientState* rcs)
	{
	}

void GrapheinClient::display(GLContextData& contextData) const
	{
	glPushAttrib(GL_ENABLE_BIT|GL_LINE_BIT);
	glDisable(GL_LIGHTING);
	
	/* Render all curves: */
	for(CurveHasher::ConstIterator cIt=curves.begin();!cIt.isFinished();++cIt)
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

void GrapheinClient::display(const ProtocolClient::RemoteClientState* rcs,GLContextData& contextData) const
	{
	/* Get a handle on the Graphein state object: */
	const RemoteClientState* myRcs=dynamic_cast<const RemoteClientState*>(rcs);
	if(myRcs==0)
		Misc::throwStdErr("GrapheinClient::display: Remote client state object has mismatching type");
	
	/* Display the remote client's state: */
	myRcs->display(contextData);
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
