# RIGOL DM3058(E) Multimeter OSD & Control for Linux
RIGOL DM3058(E) Multimeter OSD & Control for linux with keyboard mode selection
Based on original Paul Daniels code for GDM-8341 https://github.com/inflex/gdm-8341  

It works also with DM3068 but displays as 5 ½ digits 
# Requirements

You will require the SDL2 development lib in linux

The default (currently) linux kernel will not assign a /dev/ttyUSBx port
to this meter without modifying cp210x.c to include the VID:PID pair
and recompiling as a new kernel module.  Hopefully this will be added
in the future to the mainline kernel.

Because this meter locks the front panel during USB communications I have added keyboard controls for the common modes I use,  continuity, volts, diode and resistance.  Try win-alt-c/v/d/r respectively.

# Setup

Build	 

	(linux) make
	
# Usage
	
   
Run from the command line

	./dm3058e-sdl -p /dev/ttyUSB0


### Keyboard bindings
	p : pause/unpause; use this for when you need to access the front panel
	q : quit

	(the following work anywhere in the X desktop, you do not have to be 'focused' on the app)
	win-alt-v : change to volts mode
	win-alt-r : change to resistance mode
	win-alt-c : change to continuity mode
	win-alt-d : change to diode mode

# DM3058(E) 
