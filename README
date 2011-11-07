================================================================================
LICENSE
================================================================================

    README for sidewinder-x6-macro-keys
    Copyright (C) 2011 Filip Wieladek

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.

================================================================================
DESCRIPTION
================================================================================

This is a user level driver for the Microsoft keyboard "Sidewinder X6". This user level driver enables the macros on the keyboard as well as allows to switch between profiles. This driver also supports the numpad as a macro pad (which can be configured).

================================================================================
REQUIREMENTS
================================================================================

To build this driver, you should have the following:
  * Linux
  * a C compiler (gcc)
  * linux header files
  * libusb1.0 header files

on Ubuntu you can do:
 sudo apt-get install build-essential
 sudo apt-get install libusb-1.0-0-dev

Tested on:
 * Ubuntu 11.10 64bit 
 * Fedora 15 64bit

================================================================================
BUILDING
================================================================================

To build the driver simply navigate to the place where the source files are and type:
	make

This will build the driver.

================================================================================
USAGE
================================================================================
You must run the driver as a super user, e.g. using sudo:
    sudo ./sidewinder-x6-macro-keys

This will run as a daemon in the background untill it is killed (using the kill command). If you want to run the driver in the forground, run it with the -f option:
    sudo ./sidewinder-x6-macro-keys -f
    
For security reasons it's better NOT to to let the root user execute your macros (as in the above examples), so you'll probably want to use your own user for this.
In order to make any user the "executor" of your macro scripts, just append the username like this:
    sudo ./sidewinder-x6-macro-keys -u supertux
    # or
    sudo ./sidewinder-x6-macro-keys -f -u supertux

NOTE: by default it should now find the user automatically if sudo was used

When the driver is first executed it will create the following directories:
    <HOME>/.sidewinderx6
    <HOME>/.sidewinderx6/p1 (Profile folder 1)
    <HOME>/.sidewinderx6/p2 (Profile folder 2)
    <HOME>/.sidewinderx6/p3 (Profile folder 3)
where <HOME> stands for the users home path. 

To create a macro, simply create a file called %x.sh in one of the profile folders. Eg. if you want a macro to be executed for button S5 in the keyboard, create a S5.sh script. Make sure you set it to executable (chmod +x S5.sh) otherwise it will not execute.

Additionally, you can trigger a script when a profile is loaded. To do this, create a load.sh scripts under the profile you want. E.g.:

echo notify-send -u low -i keyboard --expire-time=1000 --hint=int:transient:1 --category=device \'Eclipse Profile\' \'You have changed to the Sidewinder X6 Eclipse profile\' > ~/.sidewinderx6/p1/load.sh 


If you want to simulate keypresses, use google, there should be already applications with that functionality (xdotool, for example).

Finally, if you want to have your numpad working as a macro pad, create a file called macro_numpad and write the number 1 to it. E.g.:
	echo 1 > <HOME>/.sidewinderx6/p1/macro_numpad

If you want to use it as a numpad again, either delete the file, or write something other than a 1 into it:
	echo 0 > <HOME>/.sidewinderx6/p1/macro_numpad


================================================================================
KNOWN ISSUES
================================================================================

 * None
