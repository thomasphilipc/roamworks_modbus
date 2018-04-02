# Maestro Modbus
Current Stable Version: 
Current Build Version:1.0.0_6

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
11) Restarts Applicatiion weekly
12) Sends the fault and the associated value of the fault
13) Sends a ping to keep the tcp connection alive

Version Chanelog:
0.1.13 
Database persistence 
OTA server & reporting rate change

0.1.14
rename reboot variable to stop
introduce application restart 

0.1.15
removed the control argument to select local dev server

0.1.16
Added logic for application controlled restart

0.1.18
Changed pollling modbus logic, fixed a bug in numbering of registers

0.1.18.1
Renamed function names and some cleaning of unused lines

0.1.18.3
Added keep alive for preventing tcp disconnect
corrected fix logic always being false even with valid fix

1.0.0_4
Release Candidate 
//1.0.0_1
//add last known gps info if new not avaialable (avoid 0,0)
//include num of sat and hdop information
//some prep work for 1.1.0 (Alarm) 
//1.0.0_2
//added the dtc for 1.1.0 (Fault)
//1.0.0_3
//correct the issues with code calling wrong polling group for Registers
//modify alarm function diag to show only required data
//changed control flag to 36
// hardcoded power and ignition values
// included new timer to force gps update on devices
// included new timer to force gps update on devices
//1.0.0_4
// convert engine hours from seconds to hours
// REG21 as int 
// remove server change feature cause of a NI
//1.0.0_6
// added log
// handling of socket closing.
// fixed bug with imei being overwritten 
// restart over the air
// implemented logging
// log file in etc

Task to do
Delete database periodically to control usage 
    ? When to delete - after every week 
Controlled restart of the application every week 

Known Issues/ Limitations

If a TCP connection is lost, the device will attempt to reconnect tcp and has no managed method of trying
If Internet is not available there is no fallback

Future Enhancements

Add SMS Fallback
Build an Engineering interface - for troubleshooting
Log information and make this available
Monitor App crash 
Forward modbus writing using Modbus TCP


