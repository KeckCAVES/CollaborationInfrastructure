/***********************************************************************
V4L2VideoDevice - Wrapper class around video devices as represented by
the Video for Linux version 2 (V4L2) library.
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

#include <Collaboration/V4L2VideoDevice.h>

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/videodev2.h>
#include <sys/mman.h>
#include <string>
#include <vector>
#include <Misc/FunctionCalls.h>
#include <Misc/ThrowStdErr.h>
#include <Misc/StandardValueCoders.h>
#include <Misc/ConfigurationFile.h>
#include <Math/Math.h>
#include <Math/Constants.h>
#include <GLMotif/WidgetManager.h>
#include <GLMotif/StyleSheet.h>
#include <GLMotif/PopupWindow.h>
#include <GLMotif/RowColumn.h>
#include <GLMotif/Label.h>
#include <GLMotif/Slider.h>
#include <GLMotif/Margin.h>
#include <GLMotif/ToggleButton.h>
#include <GLMotif/DropdownBox.h>

namespace Collaboration {

namespace {

/****************
Helper functions:
****************/

double calcSizeMismatch(int width1,int height1,int width2,int height2) // Calculates mismatch value between two image sizes; 1.0 is perfect match
	{
	double result=1.0;
	if(width1>=width2)
		result*=double(width1)/double(width2);
	else
		result*=double(width2)/double(width1);
	if(height1>=height2)
		result*=double(height1)/double(height2);
	else
		result*=double(height2)/double(height1);
	return result;
	}

}

/********************************
Methods of class V4L2VideoDevice:
********************************/

void V4L2VideoDevice::setControl(unsigned int controlId,const char* controlTag,const Misc::ConfigurationFileSection& cfg)
	{
	/* Query the control's type and value range: */
	v4l2_queryctrl queryControl;
	memset(&queryControl,0,sizeof(v4l2_queryctrl));
	queryControl.id=controlId;
	if(ioctl(videoFd,VIDIOC_QUERYCTRL,&queryControl)!=0)
		return; // Fail silently; control is just not supported by camera
	
	/* Query the control's current value: */
	v4l2_control control;
	memset(&control,0,sizeof(v4l2_control));
	control.id=controlId;
	if(ioctl(videoFd,VIDIOC_G_CTRL,&control)!=0)
		return;
		// Misc::throwStdErr("V4L2VideoDevice::setControl: Error while querying control %s",controlTag);
	
	/* Retrieve the desired control value from the configuration file section: */
	int oldControlValue=control.value;
	if(queryControl.type==V4L2_CTRL_TYPE_INTEGER)
		control.value=cfg.retrieveValue<int>(controlTag,control.value);
	else if(queryControl.type==V4L2_CTRL_TYPE_BOOLEAN)
		control.value=cfg.retrieveValue<bool>(controlTag,control.value!=0)?1:0;
	else if(queryControl.type==V4L2_CTRL_TYPE_MENU)
		{
		/* Query the name of the currently selected menu choice: */
		v4l2_querymenu queryMenu;
		memset(&queryMenu,0,sizeof(queryMenu));
		queryMenu.id=controlId;
		queryMenu.index=control.value;
		if(ioctl(videoFd,VIDIOC_QUERYMENU,&queryMenu)!=0)
			return;
			// Misc::throwStdErr("V4L2VideoDevice::setControl: Error while querying control %s",controlTag);
		
		/* Retrieve the desired menu choice from the configuration file: */
		std::string menuChoice=cfg.retrieveValue<std::string>(controlTag,reinterpret_cast<char*>(queryMenu.name));
		
		/* Find the index of the selected menu choice: */
		int selectedMenuChoice=-1;
		for(int index=0;index<=queryControl.maximum;++index)
			{
			queryMenu.id=controlId;
			queryMenu.index=index;
			if(ioctl(videoFd,VIDIOC_QUERYMENU,&queryMenu)!=0)
				return;
				// Misc::throwStdErr("V4L2VideoDevice::setControl: Error while querying control %s",controlTag);
			if(menuChoice==reinterpret_cast<char*>(queryMenu.name))
				{
				selectedMenuChoice=index;
				break;
				}
			}
		if(selectedMenuChoice<0)
			return;
			// Misc::throwStdErr("V4L2VideoDevice::setControl: %s is not a valid choice for menu control %s",menuChoice.c_str(),controlTag);
		control.value=selectedMenuChoice;
		}
	else
		return;
		// Misc::throwStdErr("V4L2VideoDevice::setControl: %s is not a settable control",controlTag);
	
	if(control.value!=oldControlValue)
		{
		/* Set the control's value: */
		control.id=controlId;
		if(ioctl(videoFd,VIDIOC_S_CTRL,&control)!=0)
			return;
			// Misc::throwStdErr("V4L2VideoDevice::setControl: Error while setting value for control %s",controlTag);
		}
	}

void V4L2VideoDevice::integerControlChangedCallback(Misc::CallbackData* cbData)
	{
	/* Get the proper callback data type: */
	GLMotif::Slider::ValueChangedCallbackData* myCbData=dynamic_cast<GLMotif::Slider::ValueChangedCallbackData*>(cbData);
	if(myCbData==0)
		return;
	
	/* Determine the video device's control ID by parsing the widget name: */
	v4l2_control control;
	memset(&control,0,sizeof(v4l2_control));
	const char* name=myCbData->slider->getName();
	if(strncmp(name,"Slider",6)!=0)
		return;
	char* endPtr;
	control.id=strtoul(name+6,&endPtr,10);
	if(endPtr==name)
		return;
	
	/* Get the new control value: */
	control.value=int(Math::floor(myCbData->value+0.5f));
	
	/* Change the video device's control value: */
	ioctl(videoFd,VIDIOC_S_CTRL,&control);
	}

void V4L2VideoDevice::booleanControlChangedCallback(Misc::CallbackData* cbData)
	{
	/* Get the proper callback data type: */
	GLMotif::ToggleButton::ValueChangedCallbackData* myCbData=dynamic_cast<GLMotif::ToggleButton::ValueChangedCallbackData*>(cbData);
	if(myCbData==0)
		return;
	
	/* Determine the video device's control ID by parsing the widget name: */
	v4l2_control control;
	memset(&control,0,sizeof(v4l2_control));
	const char* name=myCbData->toggle->getName();
	if(strncmp(name,"ToggleButton",12)!=0)
		return;
	char* endPtr;
	control.id=strtoul(name+12,&endPtr,10);
	if(endPtr==name)
		return;
	
	/* Get the new control value: */
	control.value=myCbData->set?1:0;
	
	/* Change the video device's control value: */
	ioctl(videoFd,VIDIOC_S_CTRL,&control);
	}

void V4L2VideoDevice::menuControlChangedCallback(Misc::CallbackData* cbData)
	{
	/* Get the proper callback data type: */
	GLMotif::DropdownBox::ValueChangedCallbackData* myCbData=dynamic_cast<GLMotif::DropdownBox::ValueChangedCallbackData*>(cbData);
	if(myCbData==0)
		return;
	
	/* Determine the video device's control ID by parsing the widget name: */
	v4l2_control control;
	memset(&control,0,sizeof(v4l2_control));
	const char* name=myCbData->dropdownBox->getName();
	if(strncmp(name,"DropdownBox",11)!=0)
		return;
	char* endPtr;
	control.id=strtoul(name+11,&endPtr,10);
	if(endPtr==name)
		return;
	
	/* Get the new control value: */
	control.value=myCbData->newSelectedItem;
	
	/* Change the video device's control value: */
	ioctl(videoFd,VIDIOC_S_CTRL,&control);
	}

void* V4L2VideoDevice::streamingThreadMethod(void)
	{
	Threads::Thread::setCancelState(Threads::Thread::CANCEL_ENABLE);
	Threads::Thread::setCancelType(Threads::Thread::CANCEL_ASYNCHRONOUS);
	
	while(true)
		{
		/* Dequeue the next available frame buffer: */
		v4l2_buffer buffer;
		memset(&buffer,0,sizeof(v4l2_buffer));
		buffer.type=V4L2_BUF_TYPE_VIDEO_CAPTURE;
		if(frameBuffersMemoryMapped)
			buffer.memory=V4L2_MEMORY_MMAP;
		else
			buffer.memory=V4L2_MEMORY_USERPTR;
		if(ioctl(videoFd,VIDIOC_DQBUF,&buffer)!=0)
			break;
		
		/* Find the frame buffer object, and fill in its capture state: */
		FrameBuffer& frame=frameBuffers[buffer.index];
		frame.sequence=buffer.sequence;
		frame.used=buffer.bytesused;
		
		/* Call the streaming callback: */
		(*streamingCallback)(frame);
		
		/* Put the frame buffer back into the capture queue: */
		if(ioctl(videoFd,VIDIOC_QBUF,&buffer)!=0)
			break;
		}
	
	return 0;
	}

V4L2VideoDevice::V4L2VideoDevice(const char* videoDeviceName)
	:videoFd(-1),
	 canRead(false),canStream(false),
	 frameBuffersMemoryMapped(false),numFrameBuffers(0),frameBuffers(0),
	 streamingCallback(0)
	{
	/* Open the video device: */
	videoFd=open(videoDeviceName,O_RDWR); // Read/write access is required, even for capture only!
	if(videoFd<0)
		Misc::throwStdErr("V4L2VideoDevice::V4L2VideoDevice: Unable to open V4L2 video device %s",videoDeviceName);
	
	/* Check that the device can capture video: */
	{
	v4l2_capability videoCap;
	if(ioctl(videoFd,VIDIOC_QUERYCAP,&videoCap)!=0)
		{
		close(videoFd);
		Misc::throwStdErr("V4L2VideoDevice::V4L2VideoDevice: Error while querying capabilities of V4L2 video device %s",videoDeviceName);
		}
	
	/* Check for capture capability: */
	if((videoCap.capabilities&V4L2_CAP_VIDEO_CAPTURE)==0)
		{
		close(videoFd);
		Misc::throwStdErr("V4L2VideoDevice::V4L2VideoDevice: V4L2 video device %s does not support video capture",videoDeviceName);
		}
	
	/* Check for supported I/O modes: */
	canRead=(videoCap.capabilities&V4L2_CAP_READWRITE);
	canStream=(videoCap.capabilities&V4L2_CAP_STREAMING);
	}
	}

V4L2VideoDevice::~V4L2VideoDevice(void)
	{
	if(streamingCallback!=0)
		{
		/* Stop the background streaming thread: */
		streamingThread.cancel();
		streamingThread.join();
		
		/* Delete the streaming callback: */
		delete streamingCallback;
		}
	
	/* Release all allocated frame buffers: */
	for(unsigned int i=0;i<numFrameBuffers;++i)
		{
		/* Unmap a memory-mapped buffer; delete[] a user-space buffer: */
		if(frameBuffersMemoryMapped)
			{
			if(frameBuffers[i].start!=0)
				munmap(frameBuffers[i].start,frameBuffers[i].size);
			}
		else
			delete[] frameBuffers[i].start;
		}
	delete[] frameBuffers;
	
	/* Close the video device, if it was ever opened: */
	if(videoFd>=0)
		close(videoFd);
	}

V4L2VideoDevice::VideoFormat V4L2VideoDevice::getVideoFormat(void) const
	{
	VideoFormat result;
	
	/* Query the current image format: */
	v4l2_format videoFormat;
	memset(&videoFormat,0,sizeof(v4l2_format));
	videoFormat.type=V4L2_BUF_TYPE_VIDEO_CAPTURE;
	if(ioctl(videoFd,VIDIOC_G_FMT,&videoFormat)!=0)
		Misc::throwStdErr("V4L2VideoDevice::getVideoFormat: Error while querying image format");
	
	result.pixelFormat=videoFormat.fmt.pix.pixelformat;
	result.size[0]=videoFormat.fmt.pix.width;
	result.size[1]=videoFormat.fmt.pix.height;
	result.lineSize=videoFormat.fmt.pix.bytesperline;
	result.frameSize=videoFormat.fmt.pix.sizeimage;
	
	/* Query the current frame interval: */
	v4l2_streamparm streamParameters;
	memset(&streamParameters,0,sizeof(v4l2_streamparm));
	streamParameters.type=V4L2_BUF_TYPE_VIDEO_CAPTURE;
	if(ioctl(videoFd,VIDIOC_G_PARM,&streamParameters)!=0)
		Misc::throwStdErr("V4L2VideoDevice::getVideoFormat: Error while querying capture frame rate");
	result.frameIntervalCounter=streamParameters.parm.capture.timeperframe.numerator;
	result.frameIntervalDenominator=streamParameters.parm.capture.timeperframe.denominator;
	
	return result;
	}

V4L2VideoDevice::VideoFormat& V4L2VideoDevice::setVideoFormat(V4L2VideoDevice::VideoFormat& newFormat)
	{
	v4l2_format format;
	memset(&format,0,sizeof(v4l2_format));
	format.type=V4L2_BUF_TYPE_VIDEO_CAPTURE;
	format.fmt.pix.width=newFormat.size[0];
	format.fmt.pix.height=newFormat.size[1];
	format.fmt.pix.pixelformat=newFormat.pixelFormat;
	format.fmt.pix.field=V4L2_FIELD_ANY;
	if(ioctl(videoFd,VIDIOC_S_FMT,&format)!=0)
		Misc::throwStdErr("V4L2VideoDevice::setVideoFormat: Error while setting image format");
	
	v4l2_streamparm streamParameters;
	memset(&streamParameters,0,sizeof(v4l2_streamparm));
	streamParameters.type=V4L2_BUF_TYPE_VIDEO_CAPTURE;
	streamParameters.parm.capture.timeperframe.numerator=newFormat.frameIntervalCounter;
	streamParameters.parm.capture.timeperframe.denominator=newFormat.frameIntervalDenominator;
	if(ioctl(videoFd,VIDIOC_S_PARM,&streamParameters)!=0)
		Misc::throwStdErr("V4L2VideoDevice::setVideoFormat: Error while setting capture frame rate");
	
	/* Update the format structure: */
	newFormat.pixelFormat=format.fmt.pix.pixelformat;
	newFormat.size[0]=format.fmt.pix.width;
	newFormat.size[1]=format.fmt.pix.height;
	newFormat.lineSize=format.fmt.pix.bytesperline;
	newFormat.frameSize=format.fmt.pix.sizeimage;
	newFormat.frameIntervalCounter=streamParameters.parm.capture.timeperframe.numerator;
	newFormat.frameIntervalDenominator=streamParameters.parm.capture.timeperframe.denominator;
	
	return newFormat;
	}

void V4L2VideoDevice::configure(const Misc::ConfigurationFileSection& cfg)
	{
	/***********************************************************
	Read a video format description from the configuration file:
	***********************************************************/
	
	/* Query the video device's current image format and capture frame rate as defaults: */
	std::string pixelFormatFourCC;
	unsigned int size[2];
	double frameInterval;
	
	{
	v4l2_format videoFormat;
	memset(&videoFormat,0,sizeof(v4l2_format));
	videoFormat.type=V4L2_BUF_TYPE_VIDEO_CAPTURE;
	if(ioctl(videoFd,VIDIOC_G_FMT,&videoFormat)==0)
		{
		unsigned int pixelFormat=videoFormat.fmt.pix.pixelformat;
		for(int i=0;i<4;++i,pixelFormat>>=8)
			pixelFormatFourCC.push_back(pixelFormat&0xffU);
		size[0]=videoFormat.fmt.pix.width;
		size[1]=videoFormat.fmt.pix.height;
		}
	else
		{
		pixelFormatFourCC="MJPG";
		size[0]=320;
		size[1]=240;
		}
	v4l2_streamparm streamParameters;
	memset(&streamParameters,0,sizeof(v4l2_streamparm));
	streamParameters.type=V4L2_BUF_TYPE_VIDEO_CAPTURE;
	if(ioctl(videoFd,VIDIOC_G_PARM,&streamParameters)==0)
		frameInterval=double(streamParameters.parm.capture.timeperframe.numerator)/double(streamParameters.parm.capture.timeperframe.denominator);
	else
		frameInterval=1.0/15.0;
	}
	
	/* Read the video format from the configuration file: */
	pixelFormatFourCC=cfg.retrieveValue<std::string>("./pixelFormat",pixelFormatFourCC);
	if(pixelFormatFourCC.length()!=4)
		Misc::throwStdErr("V4L2VideoDevice::configure: \"%s\" is not a valid pixel format",pixelFormatFourCC.c_str());
	unsigned int pixelFormat=0;
	for(int i=0;i<4;++i)
		pixelFormat=(pixelFormat<<8)|(unsigned int)(pixelFormatFourCC[3-i]);
	size[0]=cfg.retrieveValue<unsigned int>("./width",size[0]);
	size[1]=cfg.retrieveValue<unsigned int>("./height",size[1]);
	frameInterval=1.0/cfg.retrieveValue<double>("./frameRate",1.0/frameInterval);
	
	/*************************************************************************
	Find the most closely matching video format the video device has to offer:
	*************************************************************************/
	
	VideoFormat bestFormat;
	bestFormat.pixelFormat=0U; // Mark the format as invalid
	
	/* Find the best-matching pixel format (must be exact match in the current implementation): */
	{
	v4l2_fmtdesc formatDesc;
	memset(&formatDesc,0,sizeof(v4l2_fmtdesc));
	formatDesc.index=0;
	formatDesc.type=V4L2_BUF_TYPE_VIDEO_CAPTURE;
	while(ioctl(videoFd,VIDIOC_ENUM_FMT,&formatDesc)==0)
		{
		if(formatDesc.pixelformat==pixelFormat)
			{
			bestFormat.pixelFormat=formatDesc.pixelformat;
			break;
			}
		
		/* Go to the next format: */
		++formatDesc.index;
		}
	}
	if(bestFormat.pixelFormat==0U)
		Misc::throwStdErr("V4L2VideoDevice::configure: Video device does not support pixel format \"%s\"",pixelFormatFourCC.c_str());
	
	/* Find the best-matching image size for the selected pixel format: */
	{
	double bestSizeMismatch=Math::Constants<double>::max;
	v4l2_frmsizeenum frameSizeEnum;
	memset(&frameSizeEnum,0,sizeof(v4l2_frmsizeenum));
	frameSizeEnum.index=0;
	frameSizeEnum.pixel_format=bestFormat.pixelFormat;
	while(ioctl(videoFd,VIDIOC_ENUM_FRAMESIZES,&frameSizeEnum)==0)
		{
		int formatWidth,formatHeight;
		if(frameSizeEnum.type==V4L2_FRMSIZE_TYPE_DISCRETE)
			{
			/* Consider the explicit frame size: */
			formatWidth=frameSizeEnum.discrete.width;
			formatHeight=frameSizeEnum.discrete.height;
			}
		else
			{
			/* Find the best-matching format among the variable ones: */
			if(size[0]<=frameSizeEnum.stepwise.min_width)
				formatWidth=frameSizeEnum.stepwise.min_width;
			else if(size[0]>=frameSizeEnum.stepwise.max_width)
				formatWidth=frameSizeEnum.stepwise.max_width;
			else
				formatWidth=((size[0]-frameSizeEnum.stepwise.min_width+frameSizeEnum.stepwise.step_width/2)/frameSizeEnum.stepwise.step_width)*frameSizeEnum.stepwise.step_width+frameSizeEnum.stepwise.min_width;
			if(size[1]<=frameSizeEnum.stepwise.min_height)
				formatHeight=frameSizeEnum.stepwise.min_height;
			else if(size[1]>=frameSizeEnum.stepwise.max_height)
				formatHeight=frameSizeEnum.stepwise.max_height;
			else
				formatHeight=((size[1]-frameSizeEnum.stepwise.min_height+frameSizeEnum.stepwise.step_height/2)/frameSizeEnum.stepwise.step_height)*frameSizeEnum.stepwise.step_height+frameSizeEnum.stepwise.min_height;
			}
		
		/* Calculate the mismatch value to the requested frame size: */
		double sizeMismatch=calcSizeMismatch(size[0],size[1],formatWidth,formatHeight);
		if(bestSizeMismatch>sizeMismatch)
			{
			/* Store the current frame size: */
			bestFormat.size[0]=formatWidth;
			bestFormat.size[1]=formatHeight;
			bestSizeMismatch=sizeMismatch;
			}
		
		/* Go to the next frame size: */
		++frameSizeEnum.index;
		}
	}
	
	/* Find the best-matching frame interval for the selected pixel format and image size: */
	{
	double bestIntervalMismatch=Math::Constants<double>::max;
	v4l2_frmivalenum frameIntervalEnum;
	memset(&frameIntervalEnum,0,sizeof(v4l2_frmivalenum));
	frameIntervalEnum.index=0;
	frameIntervalEnum.pixel_format=bestFormat.pixelFormat;
	frameIntervalEnum.width=bestFormat.size[0];
	frameIntervalEnum.height=bestFormat.size[1];
	while(ioctl(videoFd,VIDIOC_ENUM_FRAMEINTERVALS,&frameIntervalEnum)==0)
		{
		unsigned int formatIntervalCounter,formatIntervalDenominator;
		if(frameIntervalEnum.type==V4L2_FRMIVAL_TYPE_DISCRETE)
			{
			/* Consider the explicit frame interval: */
			formatIntervalCounter=frameIntervalEnum.discrete.numerator;
			formatIntervalDenominator=frameIntervalEnum.discrete.denominator;
			}
		else
			{
			/* Find the best-matching frame interval: */
			if(frameInterval<=double(frameIntervalEnum.stepwise.min.numerator)/double(frameIntervalEnum.stepwise.min.denominator))
				{
				formatIntervalCounter=frameIntervalEnum.stepwise.min.numerator;
				formatIntervalDenominator=frameIntervalEnum.stepwise.min.denominator;
				}
			else if(frameInterval>=double(frameIntervalEnum.stepwise.max.numerator)/double(frameIntervalEnum.stepwise.max.denominator))
				{
				formatIntervalCounter=frameIntervalEnum.stepwise.max.numerator;
				formatIntervalDenominator=frameIntervalEnum.stepwise.max.denominator;
				}
			else
				{
				/* We're assuming that the denominators of min, max, and step are all the same: */
				formatIntervalDenominator=frameIntervalEnum.stepwise.step.denominator;
				unsigned int i=(unsigned int)Math::floor((frameInterval*double(formatIntervalDenominator)-double(frameIntervalEnum.stepwise.min.numerator))/double(frameIntervalEnum.stepwise.step.numerator)+0.5);
				formatIntervalCounter=frameIntervalEnum.stepwise.min.numerator+i*frameIntervalEnum.stepwise.step.numerator;
				}
			}
		
		/* Calculate the mismatch value to the requested frame interval: */
		double formatInterval=double(formatIntervalCounter)/double(formatIntervalDenominator);
		double intervalMismatch=frameInterval>formatInterval?frameInterval/formatInterval:formatInterval/frameInterval;
		if(bestIntervalMismatch>intervalMismatch)
			{
			/* Store the current frame interval: */
			bestFormat.frameIntervalCounter=formatIntervalCounter;
			bestFormat.frameIntervalDenominator=formatIntervalDenominator;
			bestIntervalMismatch=intervalMismatch;
			}
		
		/* Go to the next frame interval: */
		++frameIntervalEnum.index;
		}
	}
	
	/****************************************************************************
	Set the video device's image format and capture frame rate to the best match:
	****************************************************************************/
	
	setVideoFormat(bestFormat);
	
	/****************************************
	Set the video device's standard controls:
	****************************************/
	
	setControl(V4L2_CID_BRIGHTNESS,"brightness",cfg);
	setControl(V4L2_CID_CONTRAST,"contrast",cfg);
	setControl(V4L2_CID_SATURATION,"saturation",cfg);
	setControl(V4L2_CID_HUE,"hue",cfg);
	setControl(V4L2_CID_AUTO_WHITE_BALANCE,"autoWhiteBalance",cfg);
	setControl(V4L2_CID_GAMMA,"gamma",cfg);
	setControl(V4L2_CID_EXPOSURE,"exposure",cfg);
	setControl(V4L2_CID_AUTOGAIN,"autoGain",cfg);
	setControl(V4L2_CID_GAIN,"gain",cfg);
	setControl(V4L2_CID_POWER_LINE_FREQUENCY,"powerLineFrequency",cfg);
	setControl(V4L2_CID_WHITE_BALANCE_TEMPERATURE,"whiteBalanceTemperature",cfg);
	setControl(V4L2_CID_SHARPNESS,"sharpness",cfg);
	setControl(V4L2_CID_BACKLIGHT_COMPENSATION,"gamma",cfg);
	}

GLMotif::Widget* V4L2VideoDevice::createControlPanel(GLMotif::WidgetManager* widgetManager)
	{
	/* Get the style sheet: */
	const GLMotif::StyleSheet* ss=widgetManager->getStyleSheet();
	
	/* Create the control panel dialog window: */
	GLMotif::PopupWindow* controlPanelPopup=new GLMotif::PopupWindow("V4L2VideoDeviceControlPanelPopup",widgetManager,"Video Source Control");
	controlPanelPopup->setResizableFlags(true,false);
	
	GLMotif::RowColumn* controlPanel=new GLMotif::RowColumn("ControlPanel",controlPanelPopup,false);
	controlPanel->setNumMinorWidgets(2);
	
	/* Enumerate all controls exposed by the V4L2 video device: */
	v4l2_queryctrl queryControl;
	memset(&queryControl,0,sizeof(v4l2_queryctrl));
	queryControl.id=V4L2_CTRL_FLAG_NEXT_CTRL;
	while(ioctl(videoFd,VIDIOC_QUERYCTRL,&queryControl)==0)
		{
		/* Create a control row for the current control: */
		char widgetName[40];
		
		/* Create a label naming the control: */
		snprintf(widgetName,sizeof(widgetName),"Label%u",queryControl.id);
		new GLMotif::Label(widgetName,controlPanel,reinterpret_cast<char*>(queryControl.name));
		
		/* Query the control's current value: */
		v4l2_control control;
		memset(&control,0,sizeof(v4l2_control));
		control.id=queryControl.id;
		if(ioctl(videoFd,VIDIOC_G_CTRL,&control)!=0)
			Misc::throwStdErr("V4L2VideoDevice::createControlPanel: Error while querying control value");
		
		/* Create a widget to change the control's value: */
		if(queryControl.type==V4L2_CTRL_TYPE_INTEGER)
			{
			/* Create a slider: */
			snprintf(widgetName,sizeof(widgetName),"Slider%u",queryControl.id);
			GLMotif::Slider* controlSlider=new GLMotif::Slider(widgetName,controlPanel,GLMotif::Slider::HORIZONTAL,ss->fontHeight*10.0f);
			controlSlider->setValueRange(queryControl.minimum,queryControl.maximum,queryControl.step);
			controlSlider->setValue(control.value);
			controlSlider->getValueChangedCallbacks().add(this,&V4L2VideoDevice::integerControlChangedCallback);
			}
		else if(queryControl.type==V4L2_CTRL_TYPE_BOOLEAN)
			{
			/* Create a toggle button inside a margin: */
			snprintf(widgetName,sizeof(widgetName),"Margin%u",queryControl.id);
			GLMotif::Margin* controlMargin=new GLMotif::Margin(widgetName,controlPanel,false);
			controlMargin->setAlignment(GLMotif::Alignment::LEFT);
			
			snprintf(widgetName,sizeof(widgetName),"ToggleButton%u",queryControl.id);
			GLMotif::ToggleButton* controlToggleButton=new GLMotif::ToggleButton(widgetName,controlMargin,"Enabled");
			controlToggleButton->setBorderWidth(0.0f);
			controlToggleButton->setHAlignment(GLFont::Left);
			controlToggleButton->setToggle(control.value!=0);
			controlToggleButton->getValueChangedCallbacks().add(this,&V4L2VideoDevice::booleanControlChangedCallback);
			
			controlMargin->manageChild();
			}
		else if(queryControl.type==V4L2_CTRL_TYPE_MENU)
			{
			/* Query the names of all menu choices: */
			std::vector<std::string> menuChoices;
			for(int menuItem=0;menuItem<=queryControl.maximum;++menuItem)
				{
				v4l2_querymenu queryMenu;
				memset(&queryMenu,0,sizeof(queryMenu));
				queryMenu.id=queryControl.id;
				queryMenu.index=menuItem;
				if(ioctl(videoFd,VIDIOC_QUERYMENU,&queryMenu)==0)
					menuChoices.push_back(reinterpret_cast<char*>(queryMenu.name));
				}
			
			/* Create a drop-down box inside a margin: */
			snprintf(widgetName,sizeof(widgetName),"Margin%u",queryControl.id);
			GLMotif::Margin* controlMargin=new GLMotif::Margin(widgetName,controlPanel,false);
			controlMargin->setAlignment(GLMotif::Alignment::LEFT);
			
			snprintf(widgetName,sizeof(widgetName),"DropdownBox%u",queryControl.id);
			GLMotif::DropdownBox* controlDropdownBox=new GLMotif::DropdownBox(widgetName,controlMargin,menuChoices);
			controlDropdownBox->setSelectedItem(control.value);
			controlDropdownBox->getValueChangedCallbacks().add(this,&V4L2VideoDevice::menuControlChangedCallback);
			
			controlMargin->manageChild();
			}
		
		/* Query the next control: */
		queryControl.id|=V4L2_CTRL_FLAG_NEXT_CTRL;
		}
	
	controlPanel->manageChild();
	
	return controlPanelPopup;
	}

unsigned int V4L2VideoDevice::allocateFrameBuffers(unsigned int requestedNumFrameBuffers)
	{
	/* Check if the device supports streaming: */
	if(!canStream)
		Misc::throwStdErr("VideoDevice::allocateFrameBuffers: Video device does not support streaming I/O");
	
	/* Request the given number of frame buffers: */
	frameBuffersMemoryMapped=true;
	v4l2_requestbuffers requestBuffers;
	memset(&requestBuffers,0,sizeof(v4l2_requestbuffers));
	requestBuffers.type=V4L2_BUF_TYPE_VIDEO_CAPTURE;
	requestBuffers.memory=V4L2_MEMORY_MMAP;
	requestBuffers.count=requestedNumFrameBuffers;
	if(ioctl(videoFd,VIDIOC_REQBUFS,&requestBuffers)==0)
		{
		/* Allocate the frame buffer set: */
		numFrameBuffers=requestBuffers.count;
		frameBuffers=new FrameBuffer[numFrameBuffers];
		bool frameBuffersOk=true;
		for(unsigned int i=0;i<numFrameBuffers;++i)
			{
			/* Query the frame buffer's size and device space offset: */
			v4l2_buffer buffer;
			memset(&buffer,0,sizeof(v4l2_buffer));
			buffer.type=V4L2_BUF_TYPE_VIDEO_CAPTURE;
			buffer.memory=V4L2_MEMORY_MMAP;
			buffer.index=i;
			if(ioctl(videoFd,VIDIOC_QUERYBUF,&buffer)!=0)
				{
				/* That's a nasty error: */
				frameBuffersOk=false;
				break;
				}
			
			/* Map the device driver space frame buffer into application space: */
			void* bufferStart=mmap(0,buffer.length,PROT_READ|PROT_WRITE,MAP_SHARED,videoFd,buffer.m.offset);
			if(bufferStart==MAP_FAILED)
				{
				/* That's another nasty error: */
				frameBuffersOk=false;
				break;
				}
			frameBuffers[i].index=i;
			frameBuffers[i].start=reinterpret_cast<unsigned char*>(bufferStart);
			frameBuffers[i].size=buffer.length;
			}
		
		if(!frameBuffersOk)
			{
			/* Release all successfully allocated frame buffers: */
			releaseFrameBuffers();
			
			/* Signal an error: */
			Misc::throwStdErr("VideoDevice::allocateFrameBuffers: Error while allocating memory-mapped streaming buffers");
			}
		}
	else
		{
		/* Video device can't do memory-mapped I/O; fall back to user pointer I/O: */
		frameBuffersMemoryMapped=false;
		requestBuffers.type=V4L2_BUF_TYPE_VIDEO_CAPTURE;
		requestBuffers.memory=V4L2_MEMORY_USERPTR;
		if(ioctl(videoFd,VIDIOC_REQBUFS,&requestBuffers)!=0)
			Misc::throwStdErr("VideoDevice::allocateFrameBuffers: Error while allocating user-space streaming buffers");
		
		/* Determine the required frame buffer size: */
		VideoFormat currentFormat=getVideoFormat();
		
		/* Allocate all frame buffers using good old new[]: */
		numFrameBuffers=requestedNumFrameBuffers;
		frameBuffers=new FrameBuffer[numFrameBuffers];
		for(unsigned int i=0;i<numFrameBuffers;++i)
			{
			frameBuffers[i].index=i;
			frameBuffers[i].start=new unsigned char[currentFormat.frameSize];
			frameBuffers[i].size=currentFormat.frameSize;
			}
		}
	
	return numFrameBuffers;
	}

void V4L2VideoDevice::startStreaming(void)
	{
	/* Enqueue all frame buffers: */
	for(unsigned int i=0;i<numFrameBuffers;++i)
		enqueueFrame(frameBuffers[i]);
	
	/* Start streaming: */
	int streamType=V4L2_BUF_TYPE_VIDEO_CAPTURE;
	if(ioctl(videoFd,VIDIOC_STREAMON,&streamType)!=0)
		Misc::throwStdErr("VideoDevice::startStreaming: Error starting streaming video capture");
	}

void V4L2VideoDevice::startStreaming(V4L2VideoDevice::StreamingCallback* newStreamingCallback)
	{
	/* Store the streaming callback: */
	if(streamingCallback!=0)
		delete streamingCallback;
	streamingCallback=newStreamingCallback;
	
	/* Enqueue all frame buffers: */
	for(unsigned int i=0;i<numFrameBuffers;++i)
		enqueueFrame(frameBuffers[i]);
	
	/* Start streaming: */
	int streamType=V4L2_BUF_TYPE_VIDEO_CAPTURE;
	if(ioctl(videoFd,VIDIOC_STREAMON,&streamType)!=0)
		Misc::throwStdErr("VideoDevice::startStreaming: Error starting streaming video capture");
	
	/* Start the background capture thread: */
	streamingThread.start(this,&V4L2VideoDevice::streamingThreadMethod);
	}

const V4L2VideoDevice::FrameBuffer& V4L2VideoDevice::getNextFrame(void)
	{
	/* Dequeue the next available frame buffer: */
	v4l2_buffer buffer;
	memset(&buffer,0,sizeof(v4l2_buffer));
	buffer.type=V4L2_BUF_TYPE_VIDEO_CAPTURE;
	if(frameBuffersMemoryMapped)
		buffer.memory=V4L2_MEMORY_MMAP;
	else
		buffer.memory=V4L2_MEMORY_USERPTR;
	if(ioctl(videoFd,VIDIOC_DQBUF,&buffer)!=0)
		Misc::throwStdErr("VideoDevice::getNextFrame: Error while dequeueing frame buffer");
	
	/* Find the frame buffer object, and fill in its capture state: */
	FrameBuffer& frame=frameBuffers[buffer.index];
	frame.sequence=buffer.sequence;
	frame.used=buffer.bytesused;
	
	return frame;
	}

void V4L2VideoDevice::enqueueFrame(const V4L2VideoDevice::FrameBuffer& frame)
	{
	v4l2_buffer buffer;
	memset(&buffer,0,sizeof(v4l2_buffer));
	buffer.index=frame.index;
	buffer.type=V4L2_BUF_TYPE_VIDEO_CAPTURE;
	if(frameBuffersMemoryMapped)
		buffer.memory=V4L2_MEMORY_MMAP;
	else
		{
		buffer.memory=V4L2_MEMORY_USERPTR;
		buffer.m.userptr=reinterpret_cast<unsigned long>(frame.start);
		buffer.length=frame.size;
		}
	if(ioctl(videoFd,VIDIOC_QBUF,&buffer)!=0)
		Misc::throwStdErr("VideoDevice::enqueueFrame: Error while enqueueing frame buffer");
	}

void V4L2VideoDevice::stopStreaming(void)
	{
	if(streamingCallback!=0)
		{
		/* Stop the background streaming thread: */
		streamingThread.cancel();
		streamingThread.join();
		
		/* Delete the streaming callback: */
		delete streamingCallback;
		streamingCallback=0;
		}
	
	/* Stop streaming: */
	int streamType=V4L2_BUF_TYPE_VIDEO_CAPTURE;
	if(ioctl(videoFd,VIDIOC_STREAMOFF,&streamType)!=0)
		Misc::throwStdErr("VideoDevice::stopStreaming: Error stopping streaming video capture");
	}

void V4L2VideoDevice::releaseFrameBuffers(void)
	{
	/* Release all successfully allocated buffers: */
	for(unsigned int i=0;i<numFrameBuffers;++i)
		{
		if(frameBuffersMemoryMapped)
			{
			if(frameBuffers[i].start!=0)
				munmap(frameBuffers[i].start,frameBuffers[i].size);
			}
		else
			delete[] frameBuffers[i].start;
		}
	numFrameBuffers=0;
	delete[] frameBuffers;
	frameBuffers=0;
	
	if(frameBuffersMemoryMapped)
		{
		/* Release all requested buffers on the driver side: */
		v4l2_requestbuffers requestBuffers;
		memset(&requestBuffers,0,sizeof(v4l2_requestbuffers));
		requestBuffers.type=V4L2_BUF_TYPE_VIDEO_CAPTURE;
		requestBuffers.memory=V4L2_MEMORY_MMAP;
		requestBuffers.count=0;
		ioctl(videoFd,VIDIOC_REQBUFS,&requestBuffers);
		frameBuffersMemoryMapped=false;
		}
	}

}
