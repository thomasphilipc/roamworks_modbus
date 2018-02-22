# Maestro Modbus
Version

Modbus_rw is an application that runs on the Maestro RTU. The application gathers modbus data and ignition, power information and packages this data which is then send to the ROAM application


Functionality 
1) Sends Power UP message - When the device/application restarts on sensing power input
2) Sends Power Loss message - When the device senses loss of power input
3) Sends Power Restore message - When the device senses power input
4) Sends Periodic Modbus data - Send at a regular interval when ignition is ON
5) Sends a Heartbeat - Send at a regualar interval 
6) Sends a response to Poll - Send as a response to a command receieved from platform
7) Sends Ignition Off - Send when ignition power goes from high to low for 5 seconds
8) Sends Ignition Off - Send when ignition power goes to low from high for 5 seconds


Installation

Below are the dependencies for the application
Modbusmaster_V3.2  (3.2.5)
libgeneric_apis_2.1  (2.1)

Steps
1) Power the maestro
2) connect to the maestro via a web browser
3) Update the packages list from http://43.252.193.234/packages  
4) Install modbusmaster_v3.2
5) Reboot the maestro
6) Install Libgeneric_apis
7) Enable the reporting agents, gps , serial 
8) Upload the modbusmaster config csv and enable
9) Update the init.d  file restart applciation on reboot

Application Logic 

A primary recurring 1 second alarm/timer is run to monitor change in states and to keep a track of time. During each cycle the system flags a control to execute the required operation

There are some auxillary timers/alarms that control the reading from tcp line at every 30 seconds and monitor IO lines for changes every second


Known Issues/ Limitations
If a TCP connection is lost, the device will attempt to reconnect tcp indefintely 
The device does not maintain its previous states on power loss
If Internet is not available there is no fallback
