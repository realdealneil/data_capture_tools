#!/bin/bash

case $1 in
	start)
		echo "Starting script, path=$2"
		./v4l2_pipeline.sh /home/ubuntu/sd_card/$2 &
		./MavlinkComTest /home/ubuntu/sd_card/$2 &
		;;
	stop) 
		echo "Stopping script"
		killall -SIGINT gst-launch-1.0
		killall MavlinkComTest
		;;
esac
