/////////////////////////////////////////
/////////////////////////////////////////
//                                     //
//  ROAMWORKS MODBUS - MAESTRO Eseries //
//  application name : modbus_rw       //
//  applicaiton version: 0.1.13         //
//  updated last 27/02/18 : 14:30 PM   //
//  thomas.philip@roamworks.com        //
//                                     //
/////////////////////////////////////////
/////////////////////////////////////////






#include <stdio.h>
#include <string.h>
#include <unistd.h>

//used of modbus master
#include "database.h"
#include "generic_info.h"

// used for alarms and signals
#include <signal.h>
#include <time.h>

#include <pthread.h> // used for threading


//below are libs for sockets
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netdb.h>
#include <poll.h>
#include <stdlib.h>
#include <errno.h>
#include <arpa/inet.h> 

#define script_ver "v0.1.13"


// below int set to 1 will exit out the application
volatile int reboot=0;
// below flag is set to handle functions
volatile int do_function=0;
// keeps track of the minutes elapsed
volatile int time_tracker_min=0;
// keeps track of seconds
volatile int time_tracker_sec=0;
//keeps account of heartbeat
volatile int hb_tracker_min;
// keeps track of the status of TCP
volatile int tcp_status=0;
//keeps track of count of failed messages
int failed_msgs=1;
// the reporting rate and heartbeat rate is set as a variable to allow future usage in changing them OTA
int reporting_rate =2;
int heartbeat_rate=720;
// the below parameters are expected to be static and would not change after configuration
char imei[15];
char logger_id[20];

// the below parameters are the content that will be on the reports send
int fix=0,course=-1,speed=-1,power,in7,bat=-1,dop=-1,satsused=-1;
int REG0=-1,REG1=-1,REG2=-1,REG3=-1,REG4=-1,REG5=-1,REG6=-1,REG9=-1,REG10=-1,REG11=-1,REG14=-1,REG15=-1;
int REG18=-1,REG19=-1,REG20=-1,REG21=-1,REG22=-1,REG23=-1,REG24=-1,REG25=-1,REG26=-1,REG27=-1;
double REG8=-1,REG12=-1,REG16=-1,REG7=-1,REG17=-1,REG13=-1;
char sendtime[10]="",date[11]="";
double lat=0.0,lon=0.0,alt=0.0;
//below are various buffers used for read write and db polling
char  datatosend[1024]; // write
char buff[100];         // time
char read_buff[100];    // read
// below variable are to handle fucntion responses
int ret = -1, i, res,ret1;
// holds the timestamps
char timestamp[26];
// below flag is to handle tranistion changes
int hodor=1;


char protoname[] = "tcp";
struct protoent *protoent;
in_addr_t in_addr;
in_addr_t server_addr;
int sockfd;

struct hostent *hostent;
/* This is the struct used by INet addresses. */
struct sockaddr_in sockaddr_in;
// defaulted server name and port setting
char *server_hostname = "qaroam3.roamworks.com";
unsigned short server_port = 6102; 

// below variables are to handle ignition
int ign_filter=0; //a filter implementation via counter to check if the state is stable for the entire duration
int ign_state=-1; // a state holder where -1 is indeterminant 
int pwr_state=-1;  // a power state holder , we assume it is High on start up
int pwr_filter=0; // a filter implementation via counter to check if the state is stable for the entire duration

// below variables are for persistent data
char *pers_server_hostname="qaroam3.roamworks.com";
unsigned short pers_server_port=6102; 
int pers_reporting_rate=5;
int pers_heartbeat_rate=720;
int pers_ign_state=-1; 
int pers_pwr_state=-1;


// define individual timers 
timer_t firstTimerID;
timer_t secondTimerID;
timer_t thirdTimerID;

// declaration of function and variable necessary for ioline checks
int poll_ioline_state(int);
int prev_io_state=-1;

// declaration of functions
void update_info();
void send_tcp_data(void *data);
void send_poll_response();
void send_heartbeat();
void removeSubstring(char *s,const char *toremove);
void ready_device();

// timer functions that will called when a signal is fired based on the respective timer
void firstCB()
{
// the following function checks the state of the io lines every second
int ret;
//printf("Function 1 from timer 1 called \n");
//printf(" Ignstate is %d and ignfilter is %d \n\n",ign_state,ign_filter);
    // gets the latest io line states  
    ret = poll_ioline_state(prev_io_state);
    // TO DO POWER LOSS AND RESTORE
    // if current read power state is LOW and the previous power state was HIGH/ on
    if ((ret==0)&&(pwr_state==1))
    {
    //confirm power loss and send message  
        pwr_filter--;
        if (pwr_filter==-5)
        {
            pwr_state=0;
            pwr_filter=0;
            hodor=1;
        }
    // set pwr_state to 0 to indicate unit lost power    
    }

    if ((ret>0)&&(pwr_state==-1))
    {
    //confirm power loss and send message  
        pwr_state=1;
        do_function=1;
        hodor=0;
    // set pwr_state to 0 to indicate unit lost power    
    }
    // if ignition is OFF and the current ignition state is ON
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

// to reset filtering on spikes

// if ignition is high and iostate ign high then reset filter
    if ((ign_state==1)&&(ret==2))
{
    ign_filter=0;
}
// if ignition is low and iostate ign is low reset filter
    if ((ign_state==0)&&(ret==1))
{
    ign_filter=0;
}

// i fpower state is high and power line is high reset filter and vice versa
    if ((pwr_state==1)&&(ret>0))
{
    pwr_filter=0;
}
    if ((pwr_state==0)&&(ret==0))
{
    pwr_filter=0;
}
    
    // save the last read io state
    prev_io_state=ret;
}


// to handle read tcp every 30 seconds
void secondCB()
{
    do_function=9;

}

// not used
//void thirdCB()
//{
//printf( "Function 3 from timer 3 called \n");
//}


// function to launch the send tcp in a thread
pthread_t launch_thread_send_data(void *data)
{

pthread_t tid;
    //printf("Calling a thread to send data as %s",data);
    ret = pthread_create(&tid, NULL, send_tcp_data, (void *)data);
    if (ret != 0) 
    { 
        printf("Error from pthread: %d\n", ret); 
    }

    return tid;

}


// function to identify and handle events raised by the timer signals
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
    //else if ( *tidp == thirdTimerID )
    // if signal originated by third timer then call function thirdCB()
    //  thirdCB();
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
    rc1 = makeTimer("First Timer", &firstTimerID, 1, 1);       // checks io line state every second for changes

    rc2 = makeTimer("Second Timer", &secondTimerID, 30, 30);   // defined to read tcp for new data every 30 seconds

    //rc3 = makeTimer("Third Timer", &thirdTimerID, 10, 10);

    //return (rc1+rc2+rc3);
    return (rc1+rc2);
}


// below is the main signal that will set flags
void sigalrm_handler( int sig )
{
    // below updates every second
    //printf(" time_tracker_sec=%d and time_tracker_min=%d \n\n",time_tracker_sec,time_tracker_min);
    time_tracker_sec++;

    
    // we do a check and update our minute counter every 60 seconds
    //
    if(time_tracker_sec%60==0)
    {

        hb_tracker_min=hb_tracker_min+1;

        if (ign_state<1)
        {
            time_tracker_min=0;
        }
        else
        {
            time_tracker_min=time_tracker_min+1;
        }
        //every 5 minute we send the periodic report
        if ((time_tracker_min%reporting_rate==0)&&(ign_state==1))
        {
            do_function = 2; // every 5 minutes send periodic data while ignition on
        }

        if (hb_tracker_min==heartbeat_rate)
        {
            do_function = 3;   // send heartbeat
            time_tracker_sec=0;
            hb_tracker_min=0;
        }
    }

    //below functions should be fired right away when a change in state occurs
    if ((ign_state==0)&&(hodor==1))
    { 
        do_function=4; 
        //ignition off
        hodor=0;
    }

    if ((pwr_state==0)&&(hodor==1))
    { 
        do_function=6; 
        //power loss
        hodor=0;
    }


    if ((ign_state==1)&&(hodor==1))
    { 
        do_function=5;
        // ignition on
        time_tracker_sec=0;
        time_tracker_min=0;
        hodor=0;
    }



    if ((pwr_state==1)&&(hodor==1))
    { 
        do_function=7; 
        //power restore
        hodor=0;
    }
    // re run the alarm for every second
    
    if ((tcp_status<1)&&(hodor=1))
    {
        // a tcp connection was lost then the device restablishes the connection
        printf("reconnectin tcp\n");
        tcp_status=connect_tcp();
        ready_device();
        hodor=0;
    }
    
    alarm(1);
}

// function to read data over tcp
void read_tcp_data(void)
{

    int nread;    
    bzero(read_buff,1024);
    nread=recv(sockfd, read_buff , 1024 ,MSG_DONTWAIT);
    if(nread>0)
    {
        // the read data is checked for certain " words and a response is generated  if poll is there a poll is send if heartbeat then a heartbeat is send
        read_buff[nread-1]='\0';
        printf("Message recieved on TCP with \nlength %d \n data: %s\n",nread,read_buff);

        if (strstr(read_buff, "poll") != NULL) 
        {
            send_poll_response();
        }
        else if (strstr(read_buff, "heartbeat") != NULL) 
        {
            send_heartbeat();
        }
        else if (strstr(read_buff, "rate,") != NULL) 
        {
            change_reporting_rates(read_buff);
        }
        else if (strstr(read_buff, "server") != NULL) 
        {
            change_server(read_buff);
        }

        bzero(read_buff,1024);
    }
}



// function to send data over tcp
void send_tcp_data(void *data)
{   

    // call remove substring to remove -1 which indicates values not avaialable
    removeSubstring(data,"-1");    

    ret = send(sockfd, data, strlen(data),0);
    // check the ret abd tge size of sent data ; if the match then all data has been sent    

    if (ret == (strlen(data)))
    {

        printf("Message sent via tcp\n");
    }
    else
    {
        printf("%d count of Message Sending failed\n",failed_msgs);    
        failed_msgs++;    
        tcp_status=-1;
        hodor=1;
    }
}

// function to remove Not Available values
void removeSubstring(char *s,const char *toremove)
{
  while( s=strstr(s,toremove) )
    memmove(s,s+strlen(toremove),1+strlen(s+strlen(toremove)));
}


// function to poll modbus data
void send_modbus_data(void)
{

int ret;




int res,ret1;



printf("entered polling section for modbus_data\n");
REG0=-1,REG1=-1,REG2=-1,REG3=-1,REG4=-1,REG5=-1,REG6=-1,REG7=-1,REG8=-1,REG9=-1,REG10=-1,REG11=-1,REG12=-1,REG13=-1,REG14=-1,REG15=-1;
REG16=-1,REG17=-1,REG18=-1,REG19=-1,REG20=-1,REG21=-1,REG22=-1,REG23=-1,REG24=-1,REG25=-1;REG26=-1,REG27=-1;

    double value;
    char timestamp[26];


// variables to prepare periodic information 
 
    //get the modbus data values and set to -1 if not available
    ret = read_tag_latest_data_from_db("Tag1","DSEPANEL",1,1,&value,timestamp);    
    REG0=value;
    if (ret<0)
    {
        REG0=-1;
    } 
    ret = read_tag_latest_data_from_db("Tag2","DSEPANEL",1,1,&value,timestamp); 
    REG1=value;
    if (ret<0)
    {
        REG1=-1;
    } 
    ret = read_tag_latest_data_from_db("Tag3","DSEPANEL",3,1,&value,timestamp); 
    REG2=value;
    if (ret<0)
    {
        REG2=-1;
    } 
    ret = read_tag_latest_data_from_db("Tag4","DSEPANEL",4,1,&value,timestamp); 
    REG3=value;
    if (ret<0)
    {
        REG3=-1;
    } 
    ret = read_tag_latest_data_from_db("Tag5","DSEPANEL",4,1,&value,timestamp); 
    REG4=value;
    if (ret<0)
    {
        REG4=-1;
    }
   ret = read_tag_latest_data_from_db("Tag6","DSEPANEL",4,1,&value,timestamp); 
    REG5=value;
    if (ret<0)
    {
        REG5=-1;
    } 
    ret = read_tag_latest_data_from_db("Tag7","DSEPANEL",1,1,&value,timestamp); 
    REG6=value;
    if (ret<0)
    {
        REG6=-1;
    } 
    ret = read_tag_latest_data_from_db("Tag8","DSEPANEL",1,1,&value,timestamp);    
    REG7=value;
    if (ret<0)
    {
        REG7=-1;
    } 
    ret = read_tag_latest_data_from_db("Tag9","DSEPANEL",1,1,&value,timestamp); 
    REG8=value;
    if (ret<0)
    {
        REG8=-1;
    } 

    ret = read_tag_latest_data_from_db("Tag11","DSEPANEL",1,1,&value,timestamp); 
    REG10=value;
    if (ret<0)
    {
        REG10=-1;
    } 
    ret = read_tag_latest_data_from_db("Tag12","DSEPANEL",1,1,&value,timestamp); 
    REG11=value;
    if (ret<0)
    {
        REG11=-1;
    } 
        ret = read_tag_latest_data_from_db("Tag13","DSEPANEL",1,1,&value,timestamp);    
    REG12=value;
    if (ret<0)
    {
        REG12=-1;
    } 

    ret = read_tag_latest_data_from_db("Tag15","DSEPANEL",1,1,&value,timestamp); 
    REG14=value;
    if (ret<0)
    {
        REG14=-1;
    } 
    ret = read_tag_latest_data_from_db("Tag16","DSEPANEL",1,1,&value,timestamp); 
    REG15=value;
    if (ret<0)
    {
        REG15=-1;
    } 
    ret = read_tag_latest_data_from_db("Tag17","DSEPANEL",1,1,&value,timestamp); 
    REG16=value;
    if (ret<0)
    {
        REG16=-1;
    } 

    ret = read_tag_latest_data_from_db("Tag20","DSEPANEL",1,1,&value,timestamp); 
    REG19=value;
    if (ret<0)
    {
        REG19=-1;
    } 
    ret = read_tag_latest_data_from_db("Tag21","DSEPANEL",5,1,&value,timestamp); 
    REG20=value;
    if (ret<0)
    {
        REG20=-1;
    } 
    ret = read_tag_latest_data_from_db("Tag22","DSEPANEL",2,1,&value,timestamp); 
    REG21=value;
    if (ret<0)
    {
        REG21=-1;
    } 
        ret = read_tag_latest_data_from_db("Tag23","DSEPANEL",6,1,&value,timestamp);    
    REG22=value;
    if (ret<0)
    {
        REG22=-1;
    } 
    ret = read_tag_latest_data_from_db("Tag24","DSEPANEL",2,1,&value,timestamp); 
    REG23=value;
    if (ret<0)
    {
        REG23=-1;
    } 



    update_info();
    // prepare the format of the periodic CAN message
    snprintf(datatosend, sizeof(datatosend), "$CANP 36 %s,%s,%s,%f,%f,%d,%f,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\r",imei,sendtime,date,lat,lon,fix,alt,power,in7,dop,satsused,REG0,REG1,REG2,REG3,REG4,REG5,REG6,REG7,REG8,REG9,REG10,REG11,REG12,REG13,REG14,REG15,REG16,REG17,REG18,REG19,REG20,REG21,REG22,REG23,REG24,REG25,REG26);

    //calling send tcp to send the data
    pthread_t thread_id = launch_thread_send_data((void*)datatosend);
    pthread_join(thread_id,NULL);

}

// function to check the io lines
int poll_ioline_state(int prev_io_state)
{

    // return -1 for power loss
    // return 0 for ignition off
    //  return 1 for ignition on

int ret;
int powerstate;
int ignstate;

    powerstate=get_gpio_value(30);


    ignstate=get_gpio_value(31);


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
    
    // if the current and previous states are same then pass -1 else pass 0/1/2 as required

    if (ret!=prev_io_state)
    {
        return ret;
    }
    else
        return -1;
}


// function to update the basic information time and gps info
void update_info(void)
{
    int ret;

    // power and ignition state   
    power=pwr_state;
    in7=ign_state;

 
   // obtain time and date
    time_t t;
    struct tm *tmp;
    t = time(NULL);
    tmp = localtime(&t);

    strftime (buff, sizeof(buff), "%H:%M:%S", tmp);
    snprintf(sendtime, sizeof(sendtime), "%s",buff);
    sendtime[strlen(sendtime)] = '\0';
    
    strftime (buff, sizeof(buff), "%d.%m.%Y", tmp);
    snprintf(date, sizeof(date), "%s",buff);
    date[strlen(date)] = '\0';  

    
    // obtain gps info as lat,lon,alt

    ret = get_gps_latitude (&lat);
    if (ret < 0)
    {
        lat=0.0;
        fix=0;
    }

    ret = get_gps_longitude (&lon);		  
    if (ret < 0)
    {
        lon=0.0;
    }

    ret = get_gps_altitude (&alt);
    if (ret < 0)
    {
        alt=0.0;
    } 



}

// function to send poll response
void send_poll_response(void)
{
    update_info();

    char poll_command[1024];
    printf("sending poll resp\n");

     // prepare the format of the powerloss message
    snprintf(poll_command, sizeof(poll_command), "$POLLR 0 %s,%s,%s,%f,%f,%d,,,,%d,,,,,,,,,,%s\r",imei,sendtime,date,lat,lon,fix,power,logger_id);

    pthread_t thread_id = launch_thread_send_data((void*)poll_command);
    pthread_join(thread_id,NULL);
}

// function to send power up
void send_power_up(void)
{
    update_info();
    
    char pwr_command[1024];
    printf("sending power up\n");

     // prepare the format of the powerloss message
    snprintf(pwr_command, sizeof(pwr_command), "$PWRUP 0 %s,%s,%s,%f,%f,%d,,,,%d,,-1,-1,-1,,,,,,%s\r",imei,sendtime,date,lat,lon,fix,power,logger_id);
    
    pthread_t thread_id = launch_thread_send_data((void*)pwr_command);
    pthread_join(thread_id,NULL);
}

// function to send power loss
void send_power_loss(void)
{
    update_info();
    
    char pwr_command[1024];
    printf("sending power loss\n");

     // prepare the format of the powerloss message
    snprintf(pwr_command, sizeof(pwr_command), "$PWRL 0 %s,%s,%s,%f,%f,%d,,,,%d,,,,,,,,,,%s\r",imei,sendtime,date,lat,lon,fix,power,logger_id);

    pthread_t thread_id = launch_thread_send_data((void*)pwr_command);
    pthread_join(thread_id,NULL);
}

// function to send power restore
void send_power_restore(void)
{
  
    update_info();

    char pwr_command[1024];
    // prepare the format of the power restore message
    snprintf(pwr_command, sizeof(pwr_command), "$PWRR 0 %s,%s,%s,%f,%f,%d,,,,%d,,,,,,,,,,%s\r",imei,sendtime,date,lat,lon,fix,power,logger_id);

    pthread_t thread_id = launch_thread_send_data((void*)pwr_command);
    pthread_join(thread_id,NULL);
}

// function to send ignition off
void send_ignition_off(void)
{
    update_info();
    
    char ign_command[1024];

    printf("sending ignition off\n");

            // prepare the format of the ignition OFF message
    snprintf(ign_command, sizeof(ign_command), "$IN8L 36 %s,%s,%s,%f,%f,%d,,,,%f,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\r",imei,sendtime,date,lat,lon,fix,alt,power,in7,dop,satsused,REG0,REG1,REG2,REG3,REG4);

    pthread_t thread_id = launch_thread_send_data((void*)ign_command);
    pthread_join(thread_id,NULL);
}



void send_ignition_on(void)
{

    update_info();

    char ign_command[1024];
    printf("sending ignition on\n");

    // prepare the format of the Ignition ON message
    snprintf(ign_command, sizeof(ign_command), "$IN8H 36 %s,%s,%s,%f,%f,%d,,,,%f,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\r",imei,sendtime,date,lat,lon,fix,alt,power,in7,dop,satsused,REG0,REG1,REG2,REG3,REG4);

    pthread_t thread_id = launch_thread_send_data((void*)ign_command);
    pthread_join(thread_id,NULL);
}

// function to send heartbeat
void send_heartbeat(void)
{
    update_info();

    char heartbeat_command[1024];
    printf("sending heartbeat\n");

    snprintf(heartbeat_command, sizeof(heartbeat_command)-1, "$HEA 36 \r");

    pthread_t thread_id = launch_thread_send_data((void*)heartbeat_command);
    pthread_join(thread_id,NULL);
}

// function to send the sign on 
void ready_device(void)
{
 
    //for DEBUG purpose only
    //printf("The device has just rebooted \n");  
    int ret;

    char  datatosend[100];

    snprintf(datatosend, sizeof(datatosend), "$<MSG.Info.ServerLogin>\r$IMEI=%s\r$SUCCESS\r$<end>\r",imei);
    datatosend[strlen(datatosend)] = '\0';
    printf("%s",datatosend);

    pthread_t thread_id = launch_thread_send_data((void*)datatosend);
    pthread_join(thread_id,NULL);
 
}

// function to check tcp status
int check_tcp_status(void)
{

int error = 0;
socklen_t len = sizeof (error);
int retval = getsockopt (sockfd, SOL_SOCKET, SO_ERROR, &error, &len);

if (retval != 0) {
    /* there was a problem getting the error code */
    fprintf(stderr, "error getting socket error code: %s\n", strerror(retval));
    return -1;
}

if (error != 0) {
    /* socket has a non zero error status */
    fprintf(stderr, "socket error: %s\n", strerror(error));
     return -1;
}
  return 0;  
}

// function to establist tcp connection
int connect_tcp(void)
{

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
    else
    printf ("the sock id is %d \n ",sockfd);

    /* Prepare sockaddr_in. */
    // gets ip from a dns
    printf ("the servername is %s \n ",server_hostname);
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
         printf ("TCP  connection failed closing socket\n");
         ret = shutdown(sockfd, SHUT_WR);
         printf (" %d is the return for shutdown \n",ret);
         ret= close(sockfd);
         printf (" %d is the return for close \n",ret);
         tcp_status=-1;
    }
    else
        tcp_status=1;
    hodor=0;
    return tcp_status;
}

int change_server(char *line)
{
read_per();
// expect text in format <server(string),server_hostname(string),server_port(int)>
//            not implemented                                //
//  char *server_hostname = "qaroam3.roamworks.com";         //
//  unsigned short server_port = 6102;                       //
//               for future                                  //


printf("%s\n",line);
            char* pt;
            pt = strtok(line,",");
            int a,i=1;            
            while (i<4)
            {
            
            switch (i)
            {
            case 1: 
                    printf("Changing server and port\n");
                    break;
            case 2:
                    
                    server_hostname=pt;
                    printf("new server name is %s\n",server_hostname);
                    pers_server_hostname=server_hostname;
                    break;
            case 3: 
                    a = atoi(pt);
                    server_port=a;
                    printf("new port is %d\n",server_port);
                    pers_server_port=server_port;
                    break;
            }
            pt = strtok (NULL,",");
            i++;
            }   
            write_per(); 
}

int change_reporting_rates(char *line)
{
// expect text in format <rate(string),reportingrate(int),heartbeatrate(int)>
//        not implemented         //
//     heartbeat_rate variable    //
//     reporting_rate variable    //

printf("text received at reporting rate change function is %s\n",line);
            read_per();            
            char* t;
            t = strtok(line,",");

            int i=1,a;            
            while (i<4)
            {
            
            switch (i)
            {
            case 1: 
                    printf("Changing reporting rate OTA\n");
                    break;
            case 2:
                    reporting_rate=atoi(t);
                    printf("new reporting rate %d \n",reporting_rate);
                    pers_reporting_rate=reporting_rate;
                    break;
            case 3: 
                    a = atoi(t);
                    printf("new heartbeat rate is %d\n",a);
                    heartbeat_rate=a;
                    pers_heartbeat_rate=heartbeat_rate;
                    break;
            }
            t = strtok (NULL,",");
            i++;
            }  
    write_per(); 
}




// function to handle termination of application
void  INThandler(int sig)
{

char  c;
    signal(sig, SIG_IGN);
    printf("Application Closing Gracefully\n");
char  datatosend[] ="Application Stopped running \r";
    pthread_t thread_id = launch_thread_send_data((void*)datatosend);
    pthread_join(thread_id,NULL);
    //setting reboot to 1 will exit the worker loop
    reboot=1;

}

int write_per()
{
    printf("writing data to modbus_rw_config.dat\n");
    FILE *fp;

    fp = fopen("/etc/modbus_rw_config.dat","w");

    if (fp)
    {
        printf(" writinh following data to file : %d,%d,%d,%d,%d,%s,%d \n",pers_reporting_rate,pers_heartbeat_rate,failed_msgs,pers_ign_state,pers_pwr_state,pers_server_hostname,pers_server_port);
        fprintf(fp,"%d,%d,%d,%d,%d,%s,%d",pers_reporting_rate,pers_heartbeat_rate,failed_msgs,pers_ign_state,pers_pwr_state,pers_server_hostname,pers_server_port);
        fclose(fp);
        sleep(2);
        ret=read_per();
        return 0;
    }
    else
    printf("Writing to config failed\n");

    return -1;
}

int read_per()
{
    printf("reading data from modbus_rw_config.dat\n");

    char line[1024];
    FILE *fp;

    fp = fopen("/etc/modbus_rw_config.dat","r");
    int i=1;

    if (fp)
    {
        while (fgets(line,1024,fp))
        {
            printf("%s\n",line);
            char* pt;
            pt = strtok(line,",");
            int a;            
            while (pt != NULL)
            {
            
            switch (i)
            {
            case 1: 
                    a = atoi(pt);
                    pers_reporting_rate=a;
                   
                    break;
            case 2:

                    a = atoi(pt);
                    pers_heartbeat_rate=a;

                    break;
            case 3: 
                    a = atoi(pt);
                    failed_msgs=a;
                    break;
            case 4:
                    a = atoi(pt);
                    pers_ign_state=a;
                   
                    break;
            case 5:
                    a = atoi(pt);
                    pers_pwr_state=a;
                    
                    break;
            case 6:

                    pers_server_hostname= pt;                  
                    break;
            case 7:
                    a = atoi(pt);
                    pers_server_port=a;

                  
                    break;
            default:    
                    i++;
            }
            pt = strtok (NULL,",");
            i++;
            }                                                                                                                                                                   
        }        
        fclose(fp);    
    }
    else
    {
    ret=write_per();
    if (ret<0)
    {
        printf("Writing to config failed\n");
    }
    else
    printf("Creating modbus_rw_config.dat\n");
    }    
    return 0;

}


// MAIN PROGRAM CALL STARTS BELOW

int main (int argc, char *argv[])
{   
        ret=read_per();
    ign_state=pers_ign_state;
    pwr_state=pers_pwr_state;
    reporting_rate=pers_reporting_rate;
    heartbeat_rate=pers_heartbeat_rate;
    if (ign_state<0)
    {
    printf("fresh install\n");
    write_per();   
    }    
    printf("Initialization in process \n");    

    // sleep to wait for initialisation
    sleep(3);
    

    //get the loggerid - this will be from the CSV file uploaded to master_modbus
    ret = get_loggerid(logger_id); 
    //obtain imei number
    ret = get_imei (imei, 15);

    // create a signal and assign the alarm handler
    struct sigaction sact;
    sigemptyset (&sact.sa_mask);
    sact.sa_flags = 0;
    sact.sa_handler = sigalrm_handler;
    sigaction(SIGALRM, &sact, NULL);


    // if any argument is passed then points to local
    if(argc>1)
    {
        // to give info to the user on terminal  
        printf ("The script version %s is now running for device with imei=%s and the modbus configfile is %s and should report to local \n", script_ver, imei, logger_id);
        // if first start then set hostname and port
        server_hostname = "80.227.131.54";
        unsigned short server_port = 6102; 
        // conncect tcp
        connect_tcp();  
    }

    else
    {
        // if anything else is passed then points to server 
        // to give info to the user on terminal  
        printf ("The script version %s is now running for device with imei=%s and the modbus configfile is %s and should report to the server\n", script_ver, imei, logger_id);
        connect_tcp();   
    }
    
    //sends the login message
    ready_device();

    // to capture the cntrl+c
    signal(SIGINT, INThandler);

    //start the auxillary timers
    ret=srtSchedule();
    if (ret==0)
    {
    printf("THE AUXILLARY TIMERS HAVE STARTED\n");
    }
    
    alarm(1);  /* Request SIGALRM each second*/
    



// Below is the main loop that will run the business logic
    do
{ 
    // DEBUG purpose buff here is to print the time on the console 
    //time_t now = time (0);
    //strftime (buff, 100, "%Y-%m-%d %H:%M:%S.000", localtime (&now));
    //printf ("%s\n", buff);
    
    sleep(2);

    // check ignition and power
    // return -1 for power loss
    // return 0 for ignition off
    //  return 1 for ignition on

    switch(do_function)
    {

    case 1:
            printf("Power UP\n");
            send_power_up();
            do_function=0;
            read_per();
            pers_pwr_state=pwr_state;
            write_per();
            break;

    case 2:
            printf("Periodic Report\n");          
            send_modbus_data();                     
            do_function=0;
            break;

    case 3:
            printf("Heartbeat\n");
            send_heartbeat();
            do_function=0;
            break;

    case 4:
            printf("Ignition OFF\n");
            send_ignition_off();
read_per();
pers_ign_state=ign_state;
            do_function=0;
write_per();
            break;

    case 5:
            printf("Ignition ON\n");
            send_ignition_on();
read_per();
pers_ign_state=ign_state;
            do_function=0;
write_per();
            break;
    
    case 6:
            printf("Power Loss\n");
            send_power_loss();
            do_function=0;
read_per();
pers_pwr_state=pwr_state;
            ret=write_per();
            break;

    case 7:
            printf("Power Restore\n");
            send_power_restore();
            do_function=0;
read_per();
pers_pwr_state=pwr_state;
            ret=write_per();
            break;
    
    case 8:
            printf("Poll Response\n");
            send_poll_response();
            do_function=0;
            break;

    case 9:
            printf("checking for incoming messages\n");
            read_tcp_data();
            do_function=0;
            break;

    default:

            break;
    }


}
    while (!reboot);
    read_per();
    write_per();
    ret = shutdown(sockfd, SHUT_WR);
    printf (" %d is the return for shutdown \n",ret);
    ret= close(sockfd);
    printf (" %d is the return for close \n",ret);


   
    exit(0);
    return 0;
}

//// ignore below ////
// documentation to be created //


