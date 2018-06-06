//database.h
#ifndef __DATABASE__
#define __DATABASE__


int get_loggerid(char *logger_id);  //return 0:valid  -1:Invalid or Error
long int Database_used_size(); //return -1:Error  >=0:valid size
int read_tag_all_data_from_db(char *tagname,char *eqid,unsigned int polling_G_num,unsigned int upload_G_num,char *out_file);  //return 0:success -1:Error
int update_timestamp_for_tagname(char *tagname,char *eqid,unsigned int upload_G_num,char* temp_buff);  //return 0:success -1:Error
int read_tag_latest_data_from_db(char *tagname,char *eqid,unsigned int polling_G_num,unsigned int upload_G_num,double *value,char *timesatmp);  //return -1:Error 0:success(valid data) 1:success(Invalid data)
int delete_tags_data_from_db(void); //return 0:success -1:Failure
int database_delete_signal_recieved(void);
//Reporting Agent APIs
int getgps_data_from_db(unsigned int polling_G_num,char *out_file); //return 0:success -1:Error
int getlan_data_from_db(unsigned int polling_G_num,char *out_file); //return 0:success -1:Error
int getwan_data_from_db(unsigned int polling_G_num,char *out_file); //return 0:success -1:Error
int getcellular_data_from_db(unsigned int polling_G_num,char *out_file); //return 0:success -1:Error
int getgeneralinfo_data_from_db(unsigned int polling_G_num,char *out_file); //return 0:success -1:Error

int update_timestamp_for_gps(unsigned int polling_G_num,char* temp_buff); //return 0:success -1:Error
int update_timestamp_for_lan(unsigned int polling_G_num,char* temp_buff); //return 0:success -1:Error
int update_timestamp_for_wan(unsigned int polling_G_num,char* temp_buff); //return 0:success -1:Error
int update_timestamp_for_cellular(unsigned int polling_G_num,char* temp_buff); //return 0:success -1:Error
int update_timestamp_for_generalinfo(unsigned int polling_G_num,char* temp_buff); //return 0:success -1:Error
#endif
