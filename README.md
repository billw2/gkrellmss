# GKrellMSS - GKrellM Sound Scope

Copyright (c) 2001-2020 by Bill Wilson.  This program is free software
released under the GNU General Public License V2
Read the COPYRIGHT file for more info.

\Author: Bill Wilson
Email:  billw--at--gkrellm.net

Git clone with:
	$ git clone https://github.com/billw2/gkrellmss


Requirements
------------
GKrellM version 2.0.0 or greater.  Esound and/or ALSA >= 0.9.7

Should be compiled under at least GKrellM 2.1.1 for best theme
scaling results.

About
-----
GKrellMSS displays a VU meter showing left and right channel audio
levels and also has a chart that shows combined left and right audio
channels as an oscilloscope trace.

There are two buttons to the left of the VU Meter which select an oscope
horizontal sweep speed ranging from 100 microseconds (usec) per division
to 50 miliseconds (msec) per division.  There are 5 horizontal divisions,
so a trace sweep time can range from 500 usec (1/2000 sec) to 
250 msec (1/4 sec).  The oscope trace is triggered by a positive zero
crossing audio signal to give nice stable displays.

There is also a sensitivity level adjustment for the VU Meter and oscope
chart.  Use the mouse wheel to adjust, or left click and drag the
sensitivity krell.

When the mouse is in the chart area:
    1) Two buttons appear.  The right button toggles the display mode
       between oscilloscope and spectrum analyzer.  The left button
       pops up an option menu for selecting the sound source and running
       some Esound esdctl functions (if Esound support is compiled in).
    2) If in spectrum analyzer mode, the mouse position highlights a
       spectrum bar and the corresponding center frequency  of the bar
       is displayed in the upper left of the chart.  If the left mouse
       button is clicked, the selected bar will remain highlighted until
       the mouse is clicked again.

