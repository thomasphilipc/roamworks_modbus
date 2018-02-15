#include <stdio.h>

#include <string.h>

#include <unistd.h>

#include <curl/curl.h> // not required 

#include "database.h"
#include "generic_info.h"

#include <signal.h>
#include <time.h>

#include <pthread.h> // not used


//below are libs for sockets
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netdb.h>
#include <poll.h>
#include <stdlib.h>
#include <errno.h>
#include <arpa/inet.h> 

#define script_ver "v0.1.7"


// below int set to 1 will exit out the application
volatile int reboot=0;
// below flag is set to handle functions
volatile int do_function=0;
// keeps track of the minutes elapsed
volatile int time_tracker_min=0;
// keeps track of seconds
volatile long long time_tracker_sec=0;
// keeps track of the status of TCP
volatile int tcp_status=0;

int hodor=1;

// below variables are to handle ignition
int ign_filter=0;
int ign_state=-1;
int pwr_state=1;


// define individual timers 
timer_t firstTimerID;
timer_t secondTimerID;
timer_t thirdTimerID;

int poll_ioline_state(int);
int prev_io_state=-1;

// timer functions that will called when a signal is fired based on the respective timer
void firstCB()
{
int ret;
printf("Function 1 from timer 1 called \n");
  ret = poll_ioline_state(prev_io_state);
    
    
    if ((ret==0)&&(pwr_state==1))
    {
    //confirm power loss and send message    
    // set pwr_state to 0 to indicate unit lost power    
    }
    
    if (ret==1 && ign_state==1)
    {
    // confirm ignition off and send message
        ign_filter--;
        if (ign_filter==-5)
        {
            ign_state=0;
            ign_filter=0;
            hodor=1;
        }
    // set ign_state to 0 to indicate ignition is OFF
    }
    
    if (ret==2 && ign_state<=0)
    {
        ign_filter++;
        if (ign_filter==5)
        {
            ign_state=1;
            ign_filter=0;
            hodor=1;
        }   
    // confirm ignition on and send message
    // set ign_state to 0 to indicate ignition is OFF
    }
    prev_io_state=ret;
}

void secondCB()
{
printf( "Function 2 from timer 2 called \n");
}

void thirdCB()
{
printf( "Function 3 from timer 3 called \n");
}

// a threaded function to manage on seperate thread. left unused for now - will incorporate for sending data or polling database
void * thread_func(void *data) 
{
    
int ret; 
  printf("reached the thread %s \n",(char *)data);
  ret=send_tcp_data((char *)data);
  if (ret<0)
{
    printf("Message sending failed\n");
}

  

}

pthread_t launch_thread_send_data(void *data)
{

pthread_t tid;
    printf("Calling a thread to send data %s",data);
    int ret = pthread_create(&tid, NULL, thread_func, (void *)data);
    if (ret != 0) 
    { 
        printf("Error from pthread: %d\n", ret); 
    }

    return tid;

}


// function to handle events raised by the timer signals
static void timerHandler( int sig, siginfo_t *si, void *uc )
{
    // tidp is stored with the name of the timer that raised the signal
    timer_t *tidp;
    tidp = si->si_value.sival_ptr;

    if ( *tidp == firstTimerID )
    // if signal originated by first timer then call function firstCB()
        firstCB();
    else if ( *tidp == secondTimerID )
    // if signal originated by second timer then call function secondCB()
        secondCB();
    else if ( *tidp == thirdTimerID )
    // if signal originated by third timer then call function thirdCB()
        thirdCB();
}

// function to make timers and assign the signals with the timer expires ; returns 0 on success and -1 on failure
static int makeTimer( char *name, timer_t *timerID, int expireS, int intervalS )
{
    struct sigevent         te;
    struct itimerspec       its;
    struct sigaction        sa;
    int                     sigNo = SIGRTMIN;

    /* Set up signal handler. */
    sa.sa_flags = SA_SIGINFO;
    // assigning below the signal- the function call that will be called by the timer on expiration
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
    // initialize the timer with timer id passed along, the signal that should be evoked
    timer_create(CLOCK_REALTIME, &te, timerID);
    // configure the intervals the timer should execute on
    its.it_interval.tv_sec = intervalS;
    its.it_interval.tv_nsec = 0;
    its.it_value.tv_sec = expireS;
    its.it_value.tv_nsec = 0;
    // set the timer in motion
    timer_settime(*timerID, 0, &its, NULL);

    return(0);
}





// function to fire all the timers ; returns 0 on succes and <0 on failure
static int srtSchedule( void )
{
    int rc1,rc2,rc3;
    rc1 = makeTimer("First Timer", &firstTimerID, 1, 1);

    rc2 = makeTimer("Second Timer", &secondTimerID, 5, 5);

    rc3 = makeTimer("Third Timer", &thirdTimerID, 10, 10);

    return (rc1+rc2+rc3);
}


// below is the main signal that will set flags
void sigalrm_handler( int sig )
{
    // below updates every second
    time_tracker_sec++;

    
    // we do a check and update our minute counter every 60 seconds
    //
    if(time_tracker_sec%60==0)
    {
        time_tracker_min++;
        //every 5 minute we send the periodic report
        if (time_tracker_min%2==0)
        {
        do_function = 1; // every 2 minutes check io lines
        }

        if (time_tracker_min%5==0)
        {
        do_function = 2; // every 5 minutes send poll data
        }

        if (time_tracker_min%30==0)
        {
        do_function = 3; // every hour 
        }
        // if the minute count is 720 i.e 12 hours then reset the count of min and sec to 0
        if (time_tracker_min==720)
        {
        time_tracker_sec=0;
        time_tracker_min=0;
        }
    }

    //below functions should be fired right away
    if ((ign_state==0)&&(hodor==1))
    { 
    do_function=4;
    hodor=0;
    }

    if ((ign_state==1)&&(hodor==1))
    { 
    do_function=5;
    hodor=0;
    }

    if (pwr_state==0)
    {
    do_function=6;
    }
    // re run the alarm for every second
    alarm(1);
}


// function to send data over tcp
int send_tcp_data(void *data)
{

    char protoname[] = "tcp";
    struct protoent *protoent;
    int ret;
    in_addr_t in_addr;
    in_addr_t server_addr;
    int sockfd;

    struct hostent *hostent;
    /* This is the struct used by INet addresses. */
    struct sockaddr_in sockaddr_in;
    //char *server_hostname = "87.201.44.16";
    char *server_hostname = "80.227.131.54";
    unsigned short server_port = 6102; 
    char this[2048];
    strcpy(this,data);
    


    /* Get socket. */
    // sets the protocol to TCP
    protoent = getprotobyname(protoname);
    if (protoent == NULL) {
        perror("getprotobyname");
        exit(EXIT_FAILURE);
    }
    sockfd = socket(AF_INET, SOCK_STREAM, protoent->p_proto);
    if (sockfd == -1) {
        perror("socket");
        tcp_status=-1;
        exit(EXIT_FAILURE);
    }

    /* Prepare sockaddr_in. */
    // gets ip from a dns
    hostent = gethostbyname(server_hostname);
    if (hostent == NULL) {
        fprintf(stderr, "error: gethostbyname(\"%s\")\n", server_hostname);
        exit(EXIT_FAILURE);
    }
    //sets up address from hostent(reverse ip)
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
         tcp_status=-1;
    }
    // for debug only
    size_t len = strlen(this); // will calculate number of non-0 symbols before first 0
    char * newBuf = (char *)malloc(len); // allocate memory for new array, don't forget to free it later
    memcpy(newBuf, this, len); // copy data from old buf to new one
    printf("Sending data : %s",this);   
    ret = send(sockfd, newBuf, strlen(newBuf),0);
    // check the ret abd tge size of sent data ; if the match then all data has been sent    

    if (ret == (strlen(newBuf)))
    {
        // check for response from server and print response will require business logic implementation 
        //if( recv(sockfd, this , 2000 , 0) < 0)
        //{
        //    printf("recv failed");
        //}
        //printf("Recieved reply: %s",this);
    return 0;
    free (newBuf);
    }
    else
    return -1;
    free (newBuf);
    //ret = shutdown(sockfd, SHUT_WR);
    //printf (" %d is the return for shutdown \n",ret);
    //ret= close(sockfd);
    //printf (" %d is the return for close \n",ret);



}


// function to poll modbus data
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

/*
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
            //      printf ("updated timestamp on database %s \n", temp_buff);
            //      printf ("Attempting to delete from database\n");
            //      ret = delete_tags_data_from_db ();
            //      if (ret == 0)
            //          {
            //              printf ("Tags deleted from database \n");
            //          }
            //   }  
        }
        else
        printf("Could not obtain data \n");

    }
*/

    //obtain imei number
    ret = get_imei (imei, 15);
    imei[strlen(imei)-1]='\0';

    // obtain time and date
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

    //get the modbus data values
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

    // prepare the format of the periodic CAN message
    snprintf(datatosend, sizeof(datatosend), "CANP36,%s,%s,%s,%s,%s,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d",imei,sendtime,date,lat,lon,fix,course,speed,navdist,alt,power,bat,in7,out,dop,satsused,REG0,REG1,REG2,REG3,REG4);
    //terminate with a NULL
    datatosend[strlen(datatosend)] = '\0';
    //DEBUG PURPOSE: show the data that has been sent     
    printf("%s",datatosend);

    //calling send tcp to send the data
    pthread_t thread_id = launch_thread_send_data((void*)datatosend);
    pthread_join(thread_id,NULL);



}

// function to check the io lines
int poll_ioline_state(int prev_io_state)
{

    // for Debug purpose only
    //printf("entered polling section for iolines state\n");
    // return -1 for power loss
    // return 0 for ignition off
    //  return 1 for ignition on

int ret;
int powerstate;
int ignstate;

    powerstate=get_gpio_value(30);
    printf ("IO state for digital input 1 (Power)(internal pin 30) is %d\n", powerstate);

    ignstate=get_gpio_value(31);
    printf ("IO state for digital input 2 (Ignition)(internal pin 31) is %d\n", ignstate);

    //iostate=get_gpio_value(32);
    //printf ("IO state for 2 is %d\n", iostate);

    //iostate=get_gpio_value(62);
    //printf ("IO state for 3 is %d\n", iostate);


    if (powerstate==0)
    {
        ret=0;
    }

     if ((powerstate==1)&&(ignstate==0))
    {
        ret=1;
    }

     if ((powerstate==1)&&(ignstate==1))
    {
        ret=2;
    }

    if (ret!=prev_io_state)
    {
        return ret;
    }
    else
        return -1;
}

void send_ignition_on()
{

char  datatosend[15];



    snprintf(datatosend, sizeof(datatosend), "Ignition ON");
    datatosend[strlen(datatosend)] = '\0';
    printf("%s",datatosend);

    //calling send tcp


    pthread_t thread_id = launch_thread_send_data((void*)datatosend);
    pthread_join(thread_id,NULL);
}

void send_ignition_off()
{

char  datatosend[15];



    snprintf(datatosend, sizeof(datatosend), "Ignition OFF");
    datatosend[strlen(datatosend)] = '\0';
    printf("%s",datatosend);

    //calling send tcp


    pthread_t thread_id = launch_thread_send_data((void*)datatosend);
    pthread_join(thread_id,NULL);
}

// function to send the sign on 
void ready_device()
{
 
    //for DEBUG purpose only
    //printf("The device has just rebooted \n");  
int ret;
char imei[14],logger_id[20];
    
    //get the loggerid - this will be from the CSV file uploaded to master_modbus
    ret = get_loggerid(logger_id); 

    //get the imei
    ret = get_imei (imei, 15);

    // to give info to the user on terminal  
    printf ("The script version %s is now running for device with imei=%s and the modbus configfile is %s \n", script_ver, imei, logger_id);
    printf("This release of the script contains the below \n");
    printf("1. Modularization of the functionality \n");
    printf("2. Modbus data is now read \n");
    printf("3. Send data buffer over TCP \n");
    printf("4. Timers and Signals to control events \n");
    printf("5. IO lines are now read\n");


char  datatosend[100];



    snprintf(datatosend, sizeof(datatosend), "$<MSG.Info.ServerLogin>\r$IMEI=%s\r$SUCCESS\r$<end>\r",imei);
    datatosend[strlen(datatosend)] = '\0';
    printf("%s",datatosend);

    //calling send tcp


    pthread_t thread_id = launch_thread_send_data((void*)datatosend);
    pthread_join(thread_id,NULL);

 
}





// function to handle termination of application
void  INThandler(int sig)
{

char  c;
    signal(sig, SIG_IGN);
    printf("Application Closing Gracefully\n");
char  datatosend[] ="Application Stopped running \n";
    pthread_t thread_id = launch_thread_send_data((void*)datatosend);
    pthread_join(thread_id,NULL);
    //setting reboot to 1 will exit the worker loop
    reboot=1;

}


// MAIN PROGRAM CALL STARTS BELOW

int main (int argc, char *argv[])
{


int ret = -1, i;
double size, lat, lon, alt;
char timestamp[26];
size_t timer1; 
int res,ret1;
char this[1024];
int data_length;
char buff[100]; 
char imei[14];



    // create a signal and assign the alarm handler
    struct sigaction sact;
    sigemptyset (&sact.sa_mask);
    sact.sa_flags = 0;
    sact.sa_handler = sigalrm_handler;
    sigaction(SIGALRM, &sact, NULL);
    
    //sends the login message
    ready_device();
    
    //poll_ioline_state();

    //threading related
    char *data ="testing threaded function\n"; 
   
    pthread_t thread_id = launch_thread_send_data((void*)data);
    pthread_join(thread_id,NULL);



    
    // to capture the cntrl+c
    signal(SIGINT, INThandler);

    //start the auxillary timers
    ret=srtSchedule();
    if (ret==0)
    {
    printf("THE AUXILLARY TIMERS HAVE STARTED");
    }
    
    alarm(1);  /* Request SIGALRM each second*/




// Below is the main loop that will run the business logic
    do
{
    


    // DEBUG purpose buff here is to print the time on the console 
    time_t now = time (0);
    strftime (buff, 100, "%Y-%m-%d %H:%M:%S.000", localtime (&now));
    printf ("%s\n", buff);
    


    // check ignition and power
    // return -1 for power loss
    // return 0 for ignition off
    //  return 1 for ignition on
  


    // a sleep for 1 seconds
    sleep(1);

    switch(do_function)
    {

    case 1:
            printf("I got hit by a 1 and will check my IO lines now\n");
            //poll_ioline_state();
            do_function=0;
            break;

    case 2:
            printf("I got hit by a 2 and will send the periodic data on tcp now\n");
            poll_modbus_data();            
            do_function=0;
            break;

    case 3:
            printf("I got hit by a 3 and this occurs only once an hour\n");
            do_function=0;
            break;

    case 4:
            printf("I got hit by a 4 and this is ignition off\n");
            send_ignition_off();
            do_function=0;
            break;

    case 5:
            printf("I got hit by a 5 and this is ignition on\n");
            send_ignition_on();
            do_function=0;
            break;
    
    case 6:
            printf("I got hit by a 6 and this is power\n");
            do_function=0;
            break;

    default:
            break;
    }


}
    while (!reboot);
   
    exit(0);
    return 0;}



    
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





