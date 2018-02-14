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

#define script_ver "v0.1.7"

// define timers

timer_t firstTimerID;
timer_t secondTimerID;
timer_t thirdTimerID;

volatile int reboot=0;
volatile int do_function=0;
volatile int time_tracker_min=0;


// timer functions
void firstCB()
{
printf("Function 1 from timer 1 called \n");
}

void secondCB()
{

printf( "Function 2 from timer 2 called \n");
poll_ioline_state();
}

void thirdCB()
{

printf( "Function 3 from timer 3 called \n");
}

int guard(int r, char * err) 
{
if (r == -1) 
    { 
    perror(err); 
    exit(1); 
    } 
return r;
}


// function to handle events raised by the timer signals
static void timerHandler( int sig, siginfo_t *si, void *uc )
{
    timer_t *tidp;
    tidp = si->si_value.sival_ptr;

    if ( *tidp == firstTimerID )
        firstCB();
    else if ( *tidp == secondTimerID )
        secondCB();
    else if ( *tidp == thirdTimerID )
        thirdCB();
}

// function to make timers and assign the signals with the timer expires
static int makeTimer( char *name, timer_t *timerID, int expireS, int intervalS )
{
    struct sigevent         te;
    struct itimerspec       its;
    struct sigaction        sa;
    int                     sigNo = SIGRTMIN;

    /* Set up signal handler. */
    sa.sa_flags = SA_SIGINFO;
    sa.sa_sigaction = timerHandler;
    sigemptyset(&sa.sa_mask);
    if (sigaction(sigNo, &sa, NULL) == -1)
    {
        fprintf(stderr,"Failed to setup signal handling for %s.\n", name);
        return(-1);
    }

    /* Set and enable alarm */
    te.sigev_notify = SIGEV_SIGNAL;
    te.sigev_signo = sigNo;
    te.sigev_value.sival_ptr = timerID;
    timer_create(CLOCK_REALTIME, &te, timerID);

    its.it_interval.tv_sec = intervalS;
    its.it_interval.tv_nsec = 0;
    its.it_value.tv_sec = expireS;
    its.it_value.tv_nsec = 0;
    timer_settime(*timerID, 0, &its, NULL);

    return(0);
}





// function to fire all the timers
static int srtSchedule( void )
{
    int rc1,rc2,rc3;
    rc1 = makeTimer("First Timer", &firstTimerID, 1, 1);

    rc2 = makeTimer("Second Timer", &secondTimerID, 5, 5);

    rc3 = makeTimer("Third Timer", &thirdTimerID, 10, 10);

    return (rc1*rc2*rc3);
}



void sigalrm_handler( int sig )
{
    //reboot=1;
    time_tracker_min++;

    if (time_tracker_min%5==0)
    {
    do_function = 1; // send poll bus data
    }

    if (time_tracker_min%10==0)
    {
     do_function = 2; // send unix time stamp
    }

    if (time_tracker_min%2==0)
    {
     do_function = 3; // check io lines
    }

    // remove below line as this is only for debug

    alarm(60);
}



void send_tcp_data(void *data)
{


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
    char this[2048];
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

    printf("Sending data : %s",this);   
    ret = send(sockfd, this, strlen(this)+1,0);
    if (ret == (strlen(this)+1))
    {
    printf ("Success ! Message sent\n");
    
//printf (" %d is the return \n",ret);
  //  ret = shutdown(sockfd, SHUT_WR);
   // printf (" %d is the return for shutdown \n",ret);
   // ret= close(sockfd);
    //printf (" %d is the return for close \n",ret);
    }

     if( recv(sockfd, this , 2000 , 0) < 0)
    {
        printf("recv failed");
    }
    printf("Recieved reply: %s",this);


}

void poll_modbus_data()
{

int ret,i;


  char timestamp[26];
  //FILE *in;
  //FILE *grab;
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

  // variables to prepare periodic information 

  int fix=1,course=2,speed=3,navdist=4,alt=5,power=6,bat=7,in7=8,out=9,dop=10,satsused=11;
  int REG0=0,REG1=1,REG2=2,REG3=3,REG4=4,REG5=5,REG6=6,REG7=7,REG8=8,REG9=9,REG10=10,REG11=11,REG12=12,REG13=13,REG14=14,REG15=15;
  int REG16=16,REG17=17,REG18=18,REG19=19,REG20=20,REG21=21,REG22=22,REG23=23,REG24=24,REG25=25;
  char sendtime[10]="",date[11]="",lat[]="dummylat",lon[]="dummylon";
  char imei[14];
    char  datatosend[1024];
  char buff[100]; 

  // iterate through the above tags to obtain the value for each of them
    

    for (i = 0; i < 5; i++)
    {
    printf ("Tag Name = %s", datatags[i]);


      value = 0;

      ret = read_tag_latest_data_from_db(datatags[i],"cpanel",1,1,&value,timestamp);  




 if (ret == 0)
	{
	  printf ("Tag : %lf - %d @ %s \n", value, ret, timestamp);

     // printf ("Attempting to update data\n");
     // ret1 = update_timestamp_for_tagname(datatags[i],"cpanel",1,temp_buff);  //return 0:success -1:Error
     //  if (ret1 == 0)
	  //  {
       //     printf ("updated timestamp on database %s \n", temp_buff);
        //    printf ("Attempting to delete from database\n");
         //   ret = delete_tags_data_from_db ();
          //  if (ret == 0)
           // {
            //    printf ("Tags deleted from database \n");
            //}
     //   }  
	}
else
    printf("Could not obtain data \n");

    }






    ret = get_imei (imei, 15);
    imei[strlen(imei)-1]='\0';


    time_t t;
    struct tm *tmp;

    t = time(NULL);
    tmp = localtime(&t);

    strftime (buff, sizeof(buff), "%H:%M:%S", tmp);
    snprintf(sendtime, sizeof(sendtime), "%s",buff);
    sendtime[strlen(sendtime)] = '\0';
    
    strftime (buff, sizeof(buff), "%d-%m-%Y", tmp);
    snprintf(date, sizeof(date), "%s",buff);
    date[strlen(date)] = '\0';

    ret = read_tag_latest_data_from_db("Tag1","cpanel",1,1,&value,timestamp); 
    REG0=value;
    ret = read_tag_latest_data_from_db("Tag2","cpanel",1,1,&value,timestamp); 
    REG1=value;
    ret = read_tag_latest_data_from_db("Tag3","cpanel",1,1,&value,timestamp); 
    REG2=value;
    ret = read_tag_latest_data_from_db("Tag4","cpanel",1,1,&value,timestamp); 
    REG3=value;
    ret = read_tag_latest_data_from_db("Tag5","cpanel",1,1,&value,timestamp); 
    REG4=value;





snprintf(datatosend, sizeof(datatosend), "CANP36,%s,%s,%s,%s,%s,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d",imei,sendtime,date,lat,lon,fix,course,speed,navdist,alt,power,bat,in7,out,dop,satsused,REG0,REG1,REG2,REG3,REG4);
datatosend[strlen(datatosend)] = '\0';
printf("%s",datatosend);

//calling send tcp
send_tcp_data(&datatosend);


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
 char imei[14],logger_id[20];

 ret = get_loggerid(logger_id); 

 ret = get_imei (imei, 15);
 printf ("The script version %s is now running for device with imei=%s and the modbus configfile is %s \n", script_ver, imei, logger_id);
 



printf("This version contains the below \n");
printf("1. Modularize the functionality \n");
printf("2. Send data buffer over TCP \n");
printf("3. experiment timers and signals and alarms \n");


char  datatosend[100];



snprintf(datatosend, sizeof(datatosend), "&<MSG.Info.ServerLogin>\r<&IMEI=%s\r&<end>\r",imei);
datatosend[strlen(datatosend)] = '\0';
printf("%s",datatosend);

//calling send tcp
send_tcp_data(&datatosend);

 
}

void * thread_func() 
{
 
  printf("reached the thread \n");

  

}


void  INThandler(int sig)
{
     char  c;

     signal(sig, SIG_IGN);
     printf("Application Exiting Gracefully\n");
  char  datatosend[] ="Application Stopped running";
  send_tcp_data(&datatosend);
     reboot=1;
          

}


// MAIN PROGRAM CALL STARTS BELOW

int main (int argc, char *argv[])
{


  int ret = -1, i;


  double size, lat, lon, alt;

  char timestamp[26];
  FILE *in;
  FILE *grab;
  char* sql_buff;
  char* temp_buff;
  size_t timer1; 
  int res,ret1;
  char this[1024];
  int data_length;
  char buff[100]; 
  char imei[14];




    struct sigaction sact;


    sigemptyset (&sact.sa_mask);
    sact.sa_flags = 0;
    sact.sa_handler = sigalrm_handler;
    sigaction(SIGALRM, &sact, NULL);
    
    //sends the login message
    ready_device();
    
    //poll_ioline_state();

    //threading a open tcp read connection
    pthread_t thread_id;
    printf("Calling a thread");
    int ret2 = pthread_create(&thread_id, NULL, thread_func, NULL);
    if (ret2 != 0) 
    { 
        printf("Error from pthread: %d\n", ret2); 
    }
    
    signal(SIGINT, INThandler);

    ret=srtSchedule();
    if (ret==0)
{
printf("THE TIMERS STARTED ROCKSTAR");
}
    
    alarm(60);  /* Request SIGALRM in 60 seconds */
  printf("Alarm called and should fire in 60 seconds\n");





// Below is the main loop that will run the business logic
    do
{
    


    // buff here is to print the time on the console for debug purpose
    time_t now = time (0);
    strftime (buff, 100, "%Y-%m-%d %H:%M:%S.000", localtime (&now));
    printf ("%s\n", buff);


    //poll_modbus_data();

    snprintf(this, sizeof(this), "Unix Time:  %d \n",(int)time(NULL));
    data_length = strlen(this);
    this[data_length] = '\0';
    //send_tcp_data(this);
    
    


    // a sleep for 1 seconds
    sleep(1);

    switch(do_function){

    case 1:
            printf("I got hit by a 1 and will poll the modbus now\n");
            poll_modbus_data();
            do_function=0;
            break;

    case 2:
            printf("I got hit by a 2 and will send the data on tcp now\n");
            send_tcp_data(&this);
            do_function=0;
            break;

    case 3:
            printf("I got hit by a 3 and will check my IO lines\n");
            poll_ioline_state();    
            do_function=0;
            break;
    default:
            printf("No Action Taken\n");
}


}
    while (!reboot);
   
    exit(0);
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





