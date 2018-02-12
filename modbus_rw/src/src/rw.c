#include <stdio.h>

#include <string.h>

#include <unistd.h>

#include <curl/curl.h>

#include "database.h"

#include "generic_info.h"

#include <signal.h>

#include <time.h>

#include <pthread.h>

#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netdb.h>
#include <poll.h>
#include <stdlib.h>
#include <errno.h>
#include <arpa/inet.h> 

#define script_ver "v0.1.6"

volatile int reboot=0;

void sigalrm_handler( int sig )
{
    reboot=1;
    alarm(5);
}


void poll_modbus_data()
{

int ret,i;


  char timestamp[26];
  //FILE *in;
  //FILE *grab;
  char time[26];
  char* sql_buff;
  char* temp_buff;

  int res,ret1;

double value;

printf("entered polling section for modbus_data\n");

char datatags[5][5];

    strcpy (datatags[0],"Tag1");
    strcpy (datatags[1],"Tag2");
    strcpy (datatags[2],"Tag3");
    strcpy (datatags[3],"Tag4");
    strcpy (datatags[4],"Tag5");



  // iterate through the above tags to obtain the value for each of them
    

    for (i = 0; i < 5; i++)
    {
    printf ("Tag Name = %s", datatags[i]);

      //int read_tag_latest_data_from_db(char *tagname,char *eqid,unsigned int polling_G_num,unsigned int upload_G_num,double value,char *timesatmp);  
      //return -1:Error 
      //return 0:success(valid data) 
      //return 1:success(Invalid data)
      value = 0;
      //ret = read_tag_all_data_from_db (datatags[i],"cpanel",1,1,"/tmp/abc");
      ret = read_tag_latest_data_from_db(datatags[i],"cpanel",1,1,&value,timestamp);  

       // int get_loggerid(char *logger_id);  //return 0:valid  -1:Invalid or Error
       // long int Database_used_size(); //return -1:Error  >=0:valid size

       // int update_timestamp_for_tagname(char *tagname,char *eqid,unsigned int upload_G_num,char* temp_buff);  //return 0:success -1:Error


 if (ret == 0)
	{
	  printf ("Tag : %lf - %d @ %s \n", value, ret, timestamp);
      // beforee this delete tags command you have to call  update timestamp command update_timestamp_for_tagname,
      printf ("Attempting to update data\n");
      ret1 = update_timestamp_for_tagname(datatags[i],"cpanel",1,temp_buff);  //return 0:success -1:Error
       if (ret1 == 0)
	    {
            printf ("updated timestamp on database %s \n", temp_buff);
            printf ("Attempting to delete from database\n");
            ret = delete_tags_data_from_db ();
            if (ret == 0)
            {
                printf ("Tags deleted from database \n");
            }
        }  
	}
else
    printf("Not sure what is wrong \n");

    }

}


void poll_ioline_state()
{
printf("entered polling section for iolines state\n");

int iostate;

iostate=get_gpio_value(0);
printf ("IO state for 0 is %d\n", iostate);

iostate=get_gpio_value(1);
printf ("IO state for 1 is %d\n", iostate);

iostate=get_gpio_value(2);
printf ("IO state for 2 is %d\n", iostate);
}



void ready_device()
{
 

 printf("The device has just rebooted \n");  
 int ret;
 char imei[20],logger_id[20];

 ret = get_loggerid(logger_id); 

 ret = get_imei (imei, 20);
 printf ("The script version %s is now running for device with imei=%s and the modbus configfile is %s \n", script_ver, imei, logger_id);
 
printf("This version contains the below \n");
printf("1. Modularize the functionality \n");
printf("2. Send data buffer over TCP \n");
printf("3. experiment timers and signals and alarms \n");
 
 
}


void send_tcp_data(char data[])
{

    printf("Entered section to send data\n");
    char buffer[BUFSIZ];
    char protoname[] = "tcp";
    struct protoent *protoent;
    int ret;
    in_addr_t in_addr;
    in_addr_t server_addr;
    int sockfd;
    size_t getline_buffer = 0;
    ssize_t nbytes_read, user_input_len;
    struct hostent *hostent;
    /* This is the struct used by INet addresses. */
    struct sockaddr_in sockaddr_in;
    char *server_hostname = "80.227.131.54";
    unsigned short server_port = 6102; 
    char this[1024];
    strcpy(this,data);
    


    /* Get socket. */
    protoent = getprotobyname(protoname);
    if (protoent == NULL) {
        perror("getprotobyname");
        exit(EXIT_FAILURE);
    }
    sockfd = socket(AF_INET, SOCK_STREAM, protoent->p_proto);
    if (sockfd == -1) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    /* Prepare sockaddr_in. */
    hostent = gethostbyname(server_hostname);
    if (hostent == NULL) {
        fprintf(stderr, "error: gethostbyname(\"%s\")\n", server_hostname);
        exit(EXIT_FAILURE);
    }
    in_addr = inet_addr(inet_ntoa(*(struct in_addr*)*(hostent->h_addr_list)));
    if (in_addr == (in_addr_t)-1) {
        fprintf(stderr, "error: inet_addr(\"%s\")\n", *(hostent->h_addr_list));
        exit(EXIT_FAILURE);
    }
    // Define ip and port for the server
    sockaddr_in.sin_addr.s_addr = in_addr;
    sockaddr_in.sin_family = AF_INET;
    sockaddr_in.sin_port = htons(server_port);

    /* Do the actual connection. */
    if (connect(sockfd, (struct sockaddr*)&sockaddr_in, sizeof(sockaddr_in)) == -1) {
         printf ("Socket creation failed \n");
    }

   
    ret = send(sockfd, this, strlen(this)+1,0);
    printf (" %d is the return \n",ret);
    ret = shutdown(sockfd, SHUT_WR);
    printf (" %d is the return for shutdown \n",ret);
    ret= close(sockfd);
    printf (" %d is the return for close \n",ret);
}


// MAIN PROGRAM CALL STARTS BELOW

int main (int argc, char *argv[])
{


  int ret = -1, i;


  double size, lat, lon, alt;

  char loggerid[15], timestamp[26], timestamp_full[300];
  FILE *in;
  FILE *grab;
  char* sql_buff;
  char* temp_buff;
  size_t timer1; 
  int res,ret1;
  char this[1024];
  int data_length;



    


    struct sigaction sact;


    sigemptyset (&sact.sa_mask);
    sact.sa_flags = 0;
    sact.sa_handler = sigalrm_handler;
    sigaction(SIGALRM, &sact, NULL);
    

    ready_device();
    
    poll_ioline_state();
    


    

    
    alarm(60);  /* Request SIGALRM in 60 seconds */
  printf("Alarm called and should fire in 10 seconds\n");





// Below is the main loop that will run the business logic
    do
{
    
    printf("Waiting and running in the loop until reboot is set to true by a timer signal\n");


    poll_modbus_data();

    snprintf(this, sizeof(this), "Following data is to to be send via tcp:  %d \n",(int)time(NULL));
    data_length = strlen(this);
    this[data_length] = '\0';
    
    
    send_tcp_data(this);

    // a sleep for 2 seconds
    sleep(2);

}
    while (!reboot);
   
    
    return 0;}

/*

 CURL *curl;
  curl_global_init(CURL_GLOBAL_ALL);
  curl = curl_easy_init();
  curl_easy_setopt(curl, CURLOPT_VERBOSE, 1);
  //curl_easy_setopt(curl, CURLOPT_URL, "http://httpbin.org/post");
  //curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 0);
  //curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writer);
  //curl_easy_setopt(curl, CURLOPT_POST, 1);
  //curl_easy_setopt(curl, CURLOPT_POSTFIELDS, "foo=bar&foz=baz");
  //Perform the request, res will get the return code 
printf("Trying to send data \n");
    res = curl_easy_perform(curl);
    // Check for errors 
    if(res != CURLE_OK)
      fprintf(stderr, "curl_easy_perform() failed: %s\n",
              curl_easy_strerror(res));
 
    // always cleanup 
    curl_easy_cleanup(curl);

   
*/
// write code to send data over tcp

    
/*
  ret = get_gps_time (time, 26);
    if (ret == 0)
    {
        printf (" time %s \n", time);
    }

    ret = get_gps_latitude (&lat);
    if (ret == 0)
    {
        printf (" get_gps_latitude %d \n", lat);
    }

    ret = get_gps_longitude (&lon);		  
    if (ret == 0)
    {
        printf (" get_gps_longitude %d \n", lon);
    }

    ret = get_gps_altitude (&alt);
    if (ret == 0)
    {
        printf (" altitude %d \n", alt);
    } 
*/


 /*   
    ret = getgps_data_from_db (1, sql_buff);
    if (ret == 0)
    {
        printf ("printing gps data"); printf (sql_buff);
    }

    for (i = 0; i < 3; i++)
    {
        iostate = get_gpio_value (i); printf ("%d : %d \n", i, iostate);
    }


    ret = get_gps_time (time, 26);
    if (ret == 0)
    {
        printf (" time %s \n", time);
    }

    ret = get_gps_latitude (&lat);
    if (ret == 0)
    {
        printf (" get_gps_latitude %d \n", lat);
    }

    ret = get_gps_longitude (&lon);		  
    if (ret == 0)
    {
        printf (" get_gps_longitude %d \n", lon);
    }

    ret = get_gps_altitude (&alt);
    if (ret == 0)
    {
        printf (" altitude %d \n", alt);
    } 

  
*/





