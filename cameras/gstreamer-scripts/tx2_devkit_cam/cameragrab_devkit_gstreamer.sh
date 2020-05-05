#!/bin/bash
gst-launch-1.0 nvarguscamerasrc name="neilcam"  ! \
	'video/x-raw(memory:NVMM),width=1280, height=720, framerate=30/1, format=NV12' !  \
	nvegltransform ! \
	nveglglessink -e
