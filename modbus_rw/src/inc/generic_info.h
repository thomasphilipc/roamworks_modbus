//generic_info header file
#ifndef __GENERIC_INFO__
#define __GENERIC_INFO__

//Errors
#define SUCCESS 0
#define FAILURE -1
#define FILE_NOT_FOUND -2
#define FILE_READ_ERROR -3
#define FILE_WRITE_ERROR -4
#define NOT_APPLICABLE -5
#define _ERROR -6
#define GPS_NO_FIX -7
#define GPS_ERROR -8
#define DEVICE_NOT_SUPPORTED -9
#define RTC_NOT_FOUND -10

#define CONNECTED 1
#define DISCONNECTED 0

#define NOT_APPLICABLE_STR "N/A"
#define ERROR_STR "ERROR"
#define GPS_NO_FIX_STR "No Fix"
#define GPS_ERROR_STR "GPS ERROR"

//Device API
int get_imei(char *imei,unsigned int size);  
int get_kernel_version(char *kernel_ver);
int get_local_time(char *localtime);
int get_firmware_version(char *firmware_ver);
long long int get_system_uptime();
int get_model_name(char *model,unsigned int size);
int get_gpio_value(unsigned int gpionum);
int set_gpio_value(unsigned int gpionum,unsigned int value);
int get_pid(char *pid,unsigned int size); 
int set_rtctime(char *timebuf);
int get_rtctime(char *timebuf,unsigned int size);

//Lan API
int get_lan_status();
int get_lan_ip(char *lanip,unsigned int size);
long long int get_lan_uptime();
long long int get_lan_tx();
long long int get_lan_rx();
int get_lan_mac_addr(char *macaddr,unsigned int size);

//Wan API
int get_wan_status();
int get_wan_ip(char *wanip,unsigned int size);
int get_wan_gateway(char *wangateway,unsigned int size);
int get_wan_dns(char *wandns,unsigned int size);
long long int get_wan_uptime();
long long int get_wan_tx();
long long int get_wan_rx();
int get_wan_mac_addr(char *macaddr,unsigned int size);

//Cellular API
int get_cellular_status();
int get_cellular_ip(char *cellip,unsigned int size);
int get_cellular_gateway(char *cellgateway,unsigned int size);
int get_cellular_dns(char *celldns,unsigned int size);
long long int get_cellular_uptime();
long long int get_cellular_tx();
long long int get_cellular_rx();
int get_signal_strength();
int get_roaming_status(char *roamstatus,unsigned int size);
int get_operatorname(char *opname,unsigned int size);
int get_operatornumber(char *opnumber,unsigned int size);
int get_networkstatus(char *networkstatus,unsigned int size);
int get_imsi(char *imsi,unsigned int size);
int get_cellid(char *cellid,unsigned int size);
int get_gprs_lac(char *lac,unsigned int size);

//WIFI API
int get_wifi_status();
int get_wifi_ip(char *wifiip,unsigned int size);
int get_wifi_gateway(char *wifigateway,unsigned int size);
int get_wifi_dns(char *wifidns,unsigned int size);
long long int get_wifi_uptime();
long long int get_wifi_tx();
long long int get_wifi_rx();
int get_wifi_clients_info(char *wifi_clients_info,unsigned int size);

//GPS API
int get_gps_time(char *time,unsigned int size);
int get_gps_latitude(double *lat);
int get_gps_longitude(double *longitude);
int get_gps_altitude(double *altitude);

#endif
