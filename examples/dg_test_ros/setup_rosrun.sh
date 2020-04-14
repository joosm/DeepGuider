#!/bin/bash

## Set path of deepguider and ros workspace
DGDIR="/work/deepguider"        # path of DeepGuider source root
ROSDIR="/work/dg_ros"           # path of ROS workspace

## Check if specified DeepGuider directory exists.
if [ ! -d $DGDIR ]; then
    echo "Error: The specified DeepGuider directory '$DGDIR' doesn't exists." 
    echo "Modify it to valid path."
    exit
fi

## Check if specified ROS workspace exists.
if [ ! -d $ROSDIR ]; then
    echo "Error: The specified ROS workspace '$ROSDIR' doesn't exists." 
    echo "Create it first or specify valid ROS workspace path."
    exit
fi

## Make symbolic links to ROS workspace
mkdir -p ~/$ROSDIR/src
ln -sf $DGDIR/examples/dg_test_ros $ROSDIR/src/
ln -sf $DGDIR/src/vps/data_vps $ROSDIR/
ln -sf $DGDIR/src/poi_recog/model $ROSDIR/
ln -sf $DGDIR/bin/data $ROSDIR/

## Copy example shell scripts to ROS workspace
cp $DGDIR/examples/dg_test_ros/dg_run.sh $ROSDIR/
cp $DGDIR/examples/dg_test_ros/dg_databag.sh $ROSDIR/
