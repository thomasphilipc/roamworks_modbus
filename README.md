# Maestro Modbus
Current Stable Version: 
Current Build Version:

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
9) Update the server configuration OTA
10) Update the reporting rate configuration OTA

Version Chanelog:
0.1.13 
Database persistence 
OTA server & reporting rate change

0.1.14
rename reboot variable to stop
introduce application restart 

0.1.15
removed the control argument to select local dev server


Task to do
Delete database periodically to control usage 
    ? When to delete - after every week 
Controlled restart of the application every week 

Known Issues/ Limitations

If a TCP connection is lost, the device will attempt to reconnect tcp and has no managed method of trying
If Internet is not available there is no fallback

Future Enhancements

Add SMS Fallback
Build and Engineering interface
Log information and make this available
Monitor App crash 
Forward modbus writing using Modbus TCP

