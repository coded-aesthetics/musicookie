Neard - Explore NFC
===================

Neard - Explore NFC is a daemon that provides a DBUS API for easy access to the ExploreNFC's board functionality.
This DBus Daemon provides a DBUS API compatible with [Neard](http://git.kernel.org/cgit/network/nfc/neard.git) but uses the NXP Reader library as a backend.

Things you can do with Neard - ExploreNFC:

* Read NFC tags 
* Write NFC tags 
* Push NDEF messages to another NFC device (P2P) 
* Receive NDEF messages from another NFC device (P2P) 

Examples provided:

* Basic NFC tag reading / P2P sharing 
* Navigate to web page at the tap of a tag/P2P share 
* Connect to Wi-Fi with a tag

Copyright NXP (C) 2014-2016 NXP Semiconductors. All Rights Reserved.

![alt text][logo]

Getting Started
===============

Requirements
------------

To run this software, you need to have a Raspberry Pi and an Explore-NFC board connected to it as shown in the picture above.
This software has been successfully tested with the Raspbian "jessie" distribution, but should also work with other Linux distributions.

Dependencies
------------

* GLib >= 2.40 
* GThread >= 2.40 
* GIO >= 2.40 
* GIO-Unix >= 2.40 
* NeardAL (for the examples)
* connman (optional)

For wiringPi, the installation instructions are here:
http://wiringpi.com/download-and-install/

NeardAL:
https://github.com/connectivity/neardal

###Activating SPI
If SPI has not been enabled yet, please activate it using the following procedure:
Use the raspi-config tool (`sudo raspi-config`), navigate to "Advanced Options" > "SPI" and select "Yes".
To enable SPI now without rebooting, execute:
```shell
sudo modprobe spi-bcm2708 #Raspberry Pi
sudo modprobe spi-bcm2835 #Raspberry Pi 2
```  

Installing Raspbian binaries
----------------------------

Download the following packages:
```shell
neard-explorenfc_0.9-1_armhf.deb libneardal0_0.14.3-1_armhf.deb
```

Install using:
```shell
sudo apt-get install libglib2.0 glib-networking-services libreadline
sudo dpkg-i libneardal0_0.14.3-1_armhf.deb neard-explorenfc_0.9-1_armhf.deb
```

Building from source
--------------------

On Raspbian, install the dependencies using the following command:
```shell
sudo apt-get install libglib2.0-dev glib-networking-services
```

And the following packages needed to build from source:
```shell
sudo apt-get install build-essential libtool cmake
```

Install NeardAL:
```shell
sudo apt-get install libreadline-dev autoconf
wget https://github.com/AppNearMe/neardal/archive/0.14-3.tar.gz
tar -xzf 0.14-3.tar.gz
cd neardal-0.14-3
./autogen.sh --prefix=/usr --sysconfdir=/etc --localstatedir=/var
make
sudo make install
```

Download the source:
```shell
wget https://www.nxp.com/neard-explorenfc/neard-explorenfc-0.9.tar.gz
tar -xzf neard-explorenfc-0.9.tar.gz
cd neard-explorenfc-0.9
```

Create the configure script:
```shell
mkdir build
cd build
cmake -DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_INSTALL_SYSCONFDIR=/etc ..
```

Or for a debug build:
```shell
cmake -DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_INSTALL_SYSCONFDIR=/etc -DCMAKE_BUILD_TYPE=Debug ..
```

Make the executable:
```shell
make
```

Install it:
```shell
sudo make install
```

To use the daemon as a service (Debian-based Linux):
```shell
sudo cp /usr/share/neard-explorenfc/explorenfcd /etc/init.d/
sudo chmod +x /etc/init.d/explorenfcd 
sudo update-rc.d explorenfcd defaults
```

You can then start the service:
```shell
sudo service explorenfcd start
```

To uninstall, execute this command from the build directory:
```shell
sudo xargs rm < install_manifest.txt
```

Examples
========

Examples are in the ```examples/``` directory.

Basic example 
-------------

This example shows you how to read a tag or receive a message over peer to peer and display information about it.
If you are executing this program over SSH you will need to run it in super-user mode due to the DBUS policy. 

```shell
explorenfc-basic
```

###Packaging

You will need to setup you GPG signature to sign the packages.

Clone the Debian packaging repository:
```shell
git clone .../neard-explorenfc-debian.git
```

If you want to restart the packaging process make sure to go back to a clean state.
```shell
git reset --hard
git clean -d -f
```

Build the package:
```shell
git-buildpackage
```

The packages will be built in the parent directory.

#### Maintaining the package

To produce a new version of the package, the new upstream source must be merged into the 'upstream' branch of the packaging repository, then tagged with the new version's number.

Once done, the upstream branch must be merged into the master branch of the packaging repository. Update the relevant files in the debian directory (notably changelog) and commit these changes.

Push all changes to the remote repository, including the new tags.

###Options

* ```-k``` Keep the program running (does not exit on first tag detection)
* ```-a``` Use specified adapter (defaults to ```/org/neard/nfc0```)

###Example outputs

![alt text][tag_tap]

####Tag containing a Smart Poster record 
```shell
explorenfc-basic
Waiting for tag or device...
Tag found
Found record /org/neard/nfc0/tag0/record0
Record type: 	SmartPoster
URI: 	http://www.nxp.com
Title: 	NXP
Action: 	
Language: 	en
Encoding: 	UTF-8
```

####Tag containing a Text record 
```shell
explorenfc-basic
Waiting for tag or device...
Tag found
Found record /org/neard/nfc0/tag0/record0
Record type: 	Text
URI: 	
Title: 	Hello NFC World!
Action: 	
Language: 	en
Encoding: 	UTF-8
```

####Device sending a URI record 
```shell
explorenfc-basic
Waiting for tag or device...
Device found
Found record /org/neard/nfc0/device0/record0
Record type: 	URI
URI: 	http://www.nxp.com/
Title: 	
Action: 	
Language: 	
Encoding: 	UTF-8
```

![alt text][device_tap]

Browser example 
---------------

This example will read a tag or receive a NDEF message containing an URL over peer to peer and open the default browser.
You need to have a graphic interface session running for this to work.

```shell
explorenfc-browser
```

###Options

* ```-k``` Keep the program running (does not exit on first tag detection)
* ```-a``` Use specified adapter (defaults to ```/org/neard/nfc0```)

![alt text][rapi_browser]

Wi-Fi example 
-------------

This example shows how to read a tag or receive a NDEF message containing a WiFi connection data over peer to peer and pass relevant data to WPASupplicant so that your RaspberryPi can connect to the network.
WPA Supplicant needs to be installed and configured properly for this example to work. You also need to have WiFi adapter plugged in and configured.

Before using the example, add the ```-u``` option to the in the ```WPA_SUP_OPTIONS``` variable in the ```/etc/wpa_supplicant/functions.sh``` file, then reinit the daemon using:

```shell
sudo ifdown wlan0
sudo ifup wlan0
```

```shell
sudo explorenfc-wifi-connect
```

###Options

* ```-k``` Keep the program running (does not exit on first tag detection)
* ```-a``` Use specified adapter (defaults to ```/org/neard/nfc0```)
* ```-i``` Use specified network interface (defaults to ```wlan0```)

####Example connection

```shell
sudo explorenfc-wifi-connect
Waiting for tag or device...
on_name_acquired() : :org.neardal
SSID: MyWiFi
MAC: FF:FF:FF:FF:FF:FF
Authentication: WPA2-PSK
Encryption: AES
Key: TestPassphrase
```

Once the connection is successfull:
```shell
ifconfig wlan0
wlan0     Link encap:Ethernet  HWaddr 4c:60:de:XX:XX:XX  
          inet addr:192.168.0.121  Bcast:192.168.0.255  Mask:255.255.255.0
          UP BROADCAST RUNNING MULTICAST  MTU:1500  Metric:1
          RX packets:54 errors:0 dropped:47 overruns:0 frame:0
          TX packets:30 errors:0 dropped:0 overruns:0 carrier:0
          collisions:0 txqueuelen:1000 
          RX bytes:55489 (54.1 KiB)  TX bytes:29754 (29.0 KiB)

```

For debbuging purposes you can use the ```wpa_cli``` (run with ```sudo```) utility or read ```/var/log/syslog```'s output.


Example connection (```sudo wpa_cli```):
```shell
<3>CTRL-EVENT-SCAN-STARTED 
<3>CTRL-EVENT-SCAN-RESULTS 
<3>SME: Trying to authenticate with 00:16:b6:XX:XX:XX (SSID='MyWiFi' freq=2462 MHz)
<3>Trying to associate with 00:16:b6:XX:XX:XX (SSID='MyWiFi' freq=2462 MHz)
<3>Associated with 00:16:b6:XX:XX:XX
<3>WPA: Key negotiation completed with 00:16:b6:XX:XX:XX [PTK=CCMP GTK=TKIP]
<3>CTRL-EVENT-CONNECTED - Connection to 00:16:b6:XX:XX:XX completed [id=2 id_str=]
<3>CTRL-EVENT-DISCONNECTED bssid=00:16:b6:XX:XX:XX reason=3 locally_generated=1
<3>CTRL-EVENT-SCAN-STARTED 
<3>CTRL-EVENT-SCAN-RESULTS 
<3>SME: Trying to authenticate with 00:16:b6:XX:XX:XX (SSID='MyWiFi' freq=2462 MHz)
<3>Trying to associate with 00:16:b6:XX:XX:XX (SSID='MyWiFi' freq=2462 MHz)
<3>Associated with 00:16:b6:XX:XX:XX
<3>WPA: Key negotiation completed with 00:16:b6:XX:XX:XX [PTK=CCMP GTK=TKIP]
<3>CTRL-EVENT-CONNECTED - Connection to 00:16:b6:XX:XX:XX completed [id=3 id_str=]
```

###Creating NFC tags containing WiFi parameters

The easiest option is to use the NXP TagWriter app for Android:
https://play.google.com/store/apps/details?id=com.nxp.nfc.tagwriter

Select "Write tags" > "New dataset" > "WiFi". You can then select the relevant WiFi network and edit more parameters on the next screen, before writing the tag.

![alt text][tag_writer_wifi]

NeardAL
=======

NeardAL is a library that makes it easy to interface with Neard and Neard-ExploreNFC. It is developed here:
https://github.com/connectivity/neardal

NCL tool
--------

NeardAL provides a tool called NCL to interact with Neard/Neard-ExploreNFC using a command line interface. 
We are listing some commonly used commands.

Start polling (initiator mode)
```shell
start_poll /org/neard/nfc0 Initiator
```

Stop polling
```shell
stop_poll /org/neard/nfc0
```

Write a NFC tag with an URL
```shell
write --type=SmartPoster --uri=http://www.nxp.com --tag /org/neard/nfc0/tag0
```

Push a NDEF message to a P2P device (phone, tablet)
```shell
push --type=SmartPoster --uri=http://www.nxp.com --dev /org/neard/nfc0/device0
```

Creating your own programs
--------------------------

NeardAL can be used as a library that you can use within your C/C++ programs.

You can look through the examples' files to get started or browse the NeardAL Doxygen documentation.

Testing
=======

Running the daemon locally
--------------------------

```shell
sudo explorenfcd -n -d
```
The options `-n` and `-d` mean that the program will not run in daemon mode and will activate debug output.

Test tools
----------

Various Python scripts from the Neard code base (under the `test/` directory) can be run to test Neard-ExploreNFC.

Programming tags
----------------

To program tags quickly, you can use the NXP TagWriter app for Android:
https://play.google.com/store/apps/details?id=com.nxp.nfc.tagwriter

![alt text][tag_writer]

Further documentation
=====================

###In the doc/ directory

* neard-api/: The DBUS API exposed by the Neard-ExploreNFC daemon
* doxygen/: The internal architecture's documentation

###On the internet

*  https://01.org/linux-nfc/ : neard/neardAL's developer website
*  http://www.element14.com/community/community/designcenter/explorenfc : Element14's website for the Explore-NFC board


[logo]: explorenfc.jpg
[tag_tap]: tag-tap.jpg
[device_tap]: device-tap.jpg
[rapi_browser]: rapi-browser.png
[tag_writer]: tag-writer.jpg
[tag_writer_wifi]: tag-writer-wifi.png
