/////////////////////////////////////////
/////////////////////////////////////////
//                                     //
//  ROAMWORKS MODBUS - MAESTRO Eseries //
//  application name : modbus_rw       //
//  application version: 1.0.0_6         //
//  updated last 20/03/18 : 16:10 PM   //
//  thomas.philip@roamworks.com        //
//                                     //
/////////////////////////////////////////
/////////////////////////////////////////

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
//1.0.0_4
// convert engine hours from seconds to hours
// REG21 as int 
// remove server change feature cause of a NI
//1.0.0_5
// fixed an issue where the application crashes out when there is no data connectivity
// adjusted logic to monitor data and tcp connection prior to sending message.
//1.0.0_6
// fixed bug with imei being overwritten 
// restart over the air
// implemented logging
// log file in etc



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

#define script_ver "1.0.0_6"


// below int set to 1 will exit out the application
volatile int stop=0;
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
int force_update_gps_rate=5;
// the below parameters are expected to be static and wouf not change after configuration
char imei[16]="35xxxxxxxxxxxxx";
char logger_id[15]="default_config";

// the below parameters are the content that will be on the reports send
int fix=0,course=-1,speed=-1,power,in7,bat=-1;
double REG0=-1,REG1=-1,REG2=-1,REG3=-1,REG4=-1,REG5=-1,REG6=-1,REG9=-1,REG10=-1,REG11=-1,REG14=-1,REG15=-1;
double REG18=-1,REG19=-1,REG20=-1,REG22=-1,REG23=-1,REG24=-1;
double REG8=-1,REG12=-1,REG16=-1,REG7=-1,REG17=-1,REG13=-1;
double REG25=-1,REG26=-1,REG27=-1,REG28=-1,REG29=-1,REG30=-1;
double REG31=-1,REG32=-1,REG33=-1,REG34=-1,REG35=-1,REG36=-1;
int REG21=-1;

double dop=-1,satsused=-1;
char sendtime[10]="",date[11]="";
double lat=0.0,lon=0.0,alt=0.0;
double lastknownlat=0.0,lastknownlon=0.0,lastknownalt=0.0;
//below are various buffers used for read write and db polling
char  datatosend[1024]; // write
char buff[100];         // time
char read_buff[100];    // read
char log[250];
// below variable are to handle fucntion responses
int ret = -1, i, res,ret1;
// hofs the timestamps
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
char *server_hostname;
unsigned short server_port; 

// below variables are to handle ignition
int ign_filter=0; //a filter implementation via counter to check if the state is stable for the entire duration
int ign_state=-1; // a state hofer where -1 is indeterminant 
int pwr_state=-1;  // a power state hofer , we assume it is High on start up
int pwr_filter=0; // a filter implementation via counter to check if the state is stable for the entire duration

// below variables are for persistent data or defaults
char *pers_server_hostname="qaroam3.roamworks.com";
unsigned short pers_server_port=6102; 
int pers_reporting_rate=1;
int pers_heartbeat_rate=720;
int pers_ign_state=-1; // initialized state
int pers_pwr_state=-1; // to identify fresh app install
int keep_alive=250;
int tcp_downtime=0;


// define individual timers 
timer_t firstTimerID;
timer_t secondTimerID;
timer_t thirdTimerID;
timer_t fourthTimerID;

// declaration of function and variable necessary for ioline checks
int poll_ioline_state(int);
int prev_io_state=-1;

// faults
int a0,a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a12,a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40;
int a41,a42,a43,a44,a45,a46,a47,a48,a49,a50,a51,a52,a53,a54,a55,a56,a57,a58,a59,a60,a61,a62,a63,a64,a65,a66,a67,a68;

// declaration of functions
void update_info();
void send_tcp_data(void *data);
void send_poll_response();
void send_heartbeat();
void filterString(char *s,const char *toremove);
void ready_device();
void force_gps_update(void);
void force_gps_update_thread(void);



int logger(void *log)
{
    
    time_t now = time (0);
    strftime (buff, 100, "%Y-%m-%d %H:%M:%S.000", localtime (&now));
    printf("writing log to modbus_rw.log %s \n",log);
    FILE *fp;

    fp = fopen("/etc/modbus_rw.log","a");

    if (fp)
    {
        fprintf(fp,"%s : %s \n",buff,log);
        fclose(fp);
        sleep(2);
        ret=read_per();
        return 0;
    }
    else
    printf("Writing to log failed\n");

    return -1;
    
}

// timer functions that will called when a signal is fired based on the respective timer
void MonitorIOLines()
{
// the following function checks the state of the io lines every second
int ret;


    // gets the latest io line states  
    ret = poll_ioline_state(prev_io_state);

    // if current read power state is LOW and the previous power state was HIGH/ on
    if ((ret==0)&&(pwr_state==1))
    {
    //confirm power loss and send message  
        pwr_filter--;
        if (pwr_filter==-5)
        {
            pwr_state=0;
            pwr_filter=0;
            hodor=2;
        }
    // set pwr_state to 0 to indicate unit lost power    
    }

    if (ret>0)
    {
        if(pwr_state==-1)
        {
        //confirm power rest and send message  
        pwr_state=1;
        do_function=1;
        }
        else if(pwr_state==0)
        {
        //confirm power rest and send message  
        pwr_state=1;
        hodor=3;
        }
           
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
void Read_TCP_Data()
{
    do_function=9;

}


void RestartApplication()
{

do_function=10;
}


// function to launch the send tcp in a thread
pthread_t launch_thread_send_data(void *data)
{

pthread_t tid;
    //printf("Calling a thread to send data as %s",data);
    ret = pthread_create(&tid, NULL, send_tcp_data, (void *)data);
    if (ret != 0) 
    { 
        sprintf(log," ThreadLaunchData - Error from pthread: %d\n", ret); 
        logger(log);
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
    // if signal originated by first timer then call function MonitorIOLines()
        MonitorIOLines();
    else if ( *tidp == secondTimerID )
    // if signal originated by second timer then call function Read_TCP_Data()
        Read_TCP_Data();
    else if ( *tidp == thirdTimerID )
    // if signal originated by third timer then call function RestartApplication()
      RestartApplication();
    else if ( *tidp == fourthTimerID )
    // if signal originated by third timer then call function force_gps_update()
      force_gps_update();


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
        sprintf(log, " MakeTimer -Failed to setup signal handling for %s.\n", name);
        return(-1);
    }

    /* Set and enable alarm */
    te.sigev_notify = SIGEV_SIGNAL;
    te.sigev_signo = sigNo;
    te.sigev_value.sival_ptr = timerID;
    // initialize the timer with timer id passed along, the signal that shouf be evoked
    timer_create(CLOCK_REALTIME, &te, timerID);
    // configure the intervals the timer shouf execute on
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
    int rc1,rc2,rc3,rc4;
    rc1 = makeTimer("First Timer", &firstTimerID, 1, 1);       // checks io line state every second for changes

    rc2 = makeTimer("Second Timer", &secondTimerID, 30, 30);   // defined to read tcp for new data every 30 seconds

    rc3 = makeTimer("Third Timer", &thirdTimerID, 1800, 0);

    rc4 = makeTimer("Fourth Timer", &fourthTimerID, 55, 55);   // force GPS update



    return (rc1+rc2+rc3+rc4);
    //return (rc1+rc2);
}


// below is the main signal that will set flags
void tick_handler( int sig )
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


   // send ping
        if ((time_tracker_sec%keep_alive==0))
        {
            do_function = 11; // every 3 minutes send a ping
        }

    //below functions shouf be fired right away when a change in state occurs
    if ((ign_state==0)&&(hodor==1))
    { 
        do_function=4; 
        //ignition off
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


        if ((pwr_state==0)&&(hodor==2))
    { 
        do_function=6; 
        //power loss
        hodor=0;
    }

        if ((pwr_state==1)&&(hodor==3))
    { 
        do_function=7; 
        //power restore
        hodor=0;
    }



    // re run the alarm for every second
    // keep counter to track tcp downtime
    if (tcp_status<1)
    {
        // a tcp connection was lost then the device restablishes the connection
       tcp_downtime++;
    }
    
    alarm(1);
}

// function to read data over tcp
void read_tcp_data(void)
{
    if (tcp_status>0)
{

    int nread;    
    bzero(read_buff,1024);
    nread=recv(sockfd, read_buff , 1024 ,MSG_DONTWAIT);
    if(nread>0)
    {
        // the read data is checked for certain " words and a response is generated  if poll is there a poll is send if heartbeat then a heartbeat is send
        read_buff[nread-1]='\0';
        sprintf(log,"TCP_read - Message recieved on TCP with \nlength %d \n data: %s\n",nread,read_buff);
        logger(log);

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
        else if (strstr(read_buff, "reboot") != NULL) 
        {
            RestartApplication();
        }

        bzero(read_buff,1024);
    }
}
else
sprintf(log,"TCP_data - No data connection \n");
logger(log);
}



// function to send data over tcp
void send_tcp_data(void *data)
{   
    if (tcp_status<0)
{
    int ret=connect_tcp();

    //sends the login message
    ready_device();
}
else
{
    sprintf(log," Send_TCP_DATA - %s\n",data);
logger(log);
    //printf("data before removing 0 is %s of length %d \n\n",data,strlen(data));
    // call remove substring to remove -1 which indicates values not avaialable
    filterString(data,"-1.000000");    
    filterString(data,"-1"); 
    //printf("data being send is %s of length %d \n\n",data,strlen(data));
    ret = send(sockfd, data, strlen(data),0);
    // check the ret abd tge size of sent data ; if the match then all data has been sent    

    if (ret == (strlen(data)))
    {

        sprintf(log,"Message sent via tcp\n");
        logger(log);
    }
    else
    {
        sprintf(log,"%d count of Message Sending failed and tcp downtime is %d\n",failed_msgs,tcp_downtime);    
        logger(log);
        failed_msgs++;  
        ret = shutdown(sockfd, SHUT_WR);
        ret= close(sockfd);  
        tcp_status=-1;
    }
}
}

// function to remove Not Available values
void filterString(char *s,const char *toremove)
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
REG16=-1,REG17=-1,REG18=-1,REG19=-1,REG20=-1,REG21=-1,REG22=-1,REG23=-1,REG24=-1,REG25=-1,REG26=-1,REG27=-1,REG28=-1,REG29=-1,REG30=-1;
double eng_hours=-1;
char timestamp[26];


// variables to prepare periodic information 
 
    //get the modbus data values and set to -1 if not available



    ret = read_tag_latest_data_from_db("Tag0","DSEPANEL",1,1,&REG0,timestamp); 
    printf("CoolantTemp value : %lf\n",REG0);  
    if (ret!=0)
    {
        REG0=-1;
    } 
    ret = read_tag_latest_data_from_db("Tag1","DSEPANEL",1,1,&REG1,timestamp); 
    printf("Engine RPM :%lf\n",REG1); 
    if (ret!=0)
    {
        REG1=-1;
    } 
    ret = read_tag_latest_data_from_db("Tag2","DSEPANEL",1,1,&REG2,timestamp); 
    printf("Generator Frequency :%lf\n",REG2); 
    if (ret!=0)
    {
        REG2=-1;
    } 

    ret = read_tag_latest_data_from_db("Tag3","DSEPANEL",2,1,&REG3,timestamp); 
    printf("Engine Torque value :%lf\n",REG3); 

    if (ret!=0)
    {
        REG3=-1;
    } 


    ret = read_tag_latest_data_from_db("Tag4","DSEPANEL",3,1,&REG4,timestamp); 
    printf("Average L to L Voltage :%lf\n",REG4); 

    if (ret!=0)
    {
        REG4=-1;
    } 
       ret = read_tag_latest_data_from_db("Tag5","DSEPANEL",3,1,&REG5,timestamp); 
    printf("Average L to N Voltage :%lf\n",REG5); 

    if (ret!=0)
    {
        REG5=-1;
    }
    ret = read_tag_latest_data_from_db("Tag6","DSEPANEL",3,1,&REG6,timestamp); 
    printf("Average AC RMS Current :%lf\n",REG6); 
    if (ret!=0)
    {
        REG6=-1;
    }

    ret = read_tag_latest_data_from_db("Tag7","DSEPANEL",1,1,&REG7,timestamp); 
    printf("Phase AB line to line V value :%lf\n",REG7); 

    if (ret!=0)
    {
        REG7=-1;
    } 

    ret = read_tag_latest_data_from_db("Tag8","DSEPANEL",1,1,&REG8,timestamp); 
    printf("Phase A line to Neutral V value :%lf\n",REG8); 

    if (ret!=0)
    {
        REG8=-1;
    } 


    ret = read_tag_latest_data_from_db("Tag9","DSEPANEL",1,1,&REG9,timestamp); 
    printf("Phase A current value :%lf\n",REG9); 

    if (ret!=0)
    {
        REG9=-1;
    } 

    // tag 10 not used
    printf("Tag10 not used\n");

    ret = read_tag_latest_data_from_db("Tag11","DSEPANEL",1,1,&REG11,timestamp);  
    printf("Phase BC line to line Vvalue :%lf\n",REG11);   

    if (ret!=0)
    {
        REG11=-1;
    } 

    ret = read_tag_latest_data_from_db("Tag12","DSEPANEL",1,1,&REG12,timestamp);  
    printf("Phase B line to Neutral V value :%lf\n",REG12); 
 
    if (ret!=0)
    {
        REG12=-1;
    } 

    ret = read_tag_latest_data_from_db("Tag13","DSEPANEL",1,1,&REG13,timestamp); 
    printf("Phase B current value :%lf\n",REG13); 

    if (ret!=0)
    {
        REG13=-1;
    } 

    // tag 14 not used
    printf("Tag14 not used\n");

    ret = read_tag_latest_data_from_db("Tag15","DSEPANEL",1,1,&REG15,timestamp); 
    printf("Phase CA line to line V value :%lf\n",REG15); 

    if (ret<0)
    {
        REG15=-1;
    } 


   ret = read_tag_latest_data_from_db("Tag16","DSEPANEL",1,1,&REG16,timestamp); 
    printf("Phase C line to Neutral V value :%lf\n",REG16); 

    if (ret!=0)
    {
        REG16=-1;
    } 
   
    ret = read_tag_latest_data_from_db("Tag17","DSEPANEL",1,1,&REG17,timestamp);  
    printf("Phase C current value :%lf\n",REG17);   

    if (ret<0)
    {
        REG17=-1;
    } 

    // tag 18 not used
    printf("Tag18 not used\n");
    // tag 19 not used
    printf("Tag19 not used\n");

    ret = read_tag_latest_data_from_db("Tag20","DSEPANEL",1,1,&REG20,timestamp);  
    printf("OilPressure : %lf\n",REG20);  
    if (ret!=0)
    {
        REG20=-1;
    } 

    ret = read_tag_latest_data_from_db("Tag21","DSEPANEL",6,1,&eng_hours,timestamp); 
    int y = (int)eng_hours;
    REG21=(int)(y / 3600);
    printf("Engine Hour Meter value :%d\n",REG21); 

    if (ret!=0)
    {
        REG21=-1.000000;
    } 

    ret = read_tag_latest_data_from_db("Tag22","DSEPANEL",2,1,&REG22,timestamp); 
    printf("Fuel Consumption Rate value :%lf\n",REG22); 

    if (ret!=0)
    {
        REG22=-1;
    } 

    ret = read_tag_latest_data_from_db("Tag23","DSEPANEL",6,1,&REG23,timestamp); 
    printf("Total Fuel used value :%lf\n",REG23); 

    if (ret!=0)
    {
        REG23=-1;
    } 

    ret = read_tag_latest_data_from_db("Tag24","DSEPANEL",1,1,&REG24,timestamp); 
    printf("Current Fuel level Percent value :%lf\n",REG24); 

    if (ret<0)
    {
        REG24=-1;
    } 
   
   

  


    
    //CANP 36 &(IMEI),&(Time),&(Date),&(Lat),&(Lon),&(Fix),&(Course),&(Speed),&(NavDist),&(Alt),&(Power),&(Bat),&(IN7),&(Out0),&(DOP),&(SatsUsed), &(REG0),&(REG1),&(REG2),&(REG3),&(REG4),&(REG5),&(REG6),&(REG7),&(REG8),&(REG9),&(REG10),&(REG11),&(REG12),&(REG13),&(REG14),&(REG15),& (REG16),&(REG17),&(REG18),&(REG19),&(REG20),&(REG21),&(REG22),&(REG23),&(REG24)

    update_info();
    // prepare the format of the periodic CAN message
    snprintf(datatosend, sizeof(datatosend), "$CANP 36 %s,%s,%s,%lf,%lf,%d,0,0,0,%lf,%d,0,%d,,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%d,%lf,%lf,%lf\r",imei,sendtime,date,lat,lon,fix,alt,power,in7,dop,satsused,REG0,REG1,REG2,REG3,REG4,REG5,REG6,REG7,REG8,REG9,REG10,REG11,REG12,REG13,REG14,REG15,REG16,REG17,REG18,REG19,REG20,REG21,REG22,REG23,REG24);

    //calling send tcp to send the data
    pthread_t thread_id = launch_thread_send_data((void*)datatosend);
    pthread_join(thread_id,NULL);

}

// function to check the io lines
int poll_ioline_state(int prev_io_state)
{

    // return 0 for power loss
    // return 1 for ignition off
    //  return 2 for ignition on

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
    

    //get the loggerid - this will be from the CSV file uploaded to master_modbus
    ret = get_loggerid(logger_id); 
    //obtain imei number
    ret = get_imei (imei, 15); 

 
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

    ret=additional_gps_data();
    fix=1;
    // obtain gps info as lat,lon,alt

    ret = get_gps_latitude (&lat);
    if (ret < 0)
    {
        lat=lastknownlat;
        fix=0;
    }

    ret = get_gps_longitude (&lon);		  
    if (ret < 0)
    {
        lon=lastknownlon;
    }

    ret = get_gps_altitude (&alt);
    if (ret < 0)
    {
        alt=lastknownalt;
    } 


    // assigning the lastknown values with the current values
    lastknownlat= lat;
    lastknownlon=lon;
    lastknownalt=alt;

}

// function to send poll response
void send_poll_response(void)
{
    update_info();

    char poll_command[1024];
    printf("sending poll resp\n");

     // prepare the format of the powerloss message
    snprintf(poll_command, sizeof(poll_command), "$POLLR 0 %s,%s,%s,%lf,%lf,%d,,,,%d,,,,,,,,,,%s\r",imei,sendtime,date,lat,lon,fix,power,logger_id);

    pthread_t thread_id = launch_thread_send_data((void*)poll_command);
    pthread_join(thread_id,NULL);
}


void send_ping(void)
{
    
    char ping_command[1024];
    printf("sending ping-keep alive\n");

     // prepare the ping
    snprintf(ping_command, sizeof(ping_command), "ping\r");

    pthread_t thread_id = launch_thread_send_data((void*)ping_command);
    pthread_join(thread_id,NULL);
}

// function to send power up
void send_power_up(void)
{
    update_info();
    
    char pwr_command[1024];
    printf("sending power up\n");

     // prepare the format of the powerloss message
    snprintf(pwr_command, sizeof(pwr_command), "$PWRUP 36 %s,%s,%s,%lf,%lf,%d,0,0,0,%lf,1,0,0,,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf\r",imei,sendtime,date,lat,lon,fix,alt,dop,satsused,REG0,REG1,REG2,REG3,REG4,REG5,REG6,REG7,REG8,REG9,REG10,REG11,REG12,REG13,REG14,REG15,REG16,REG17,REG18,REG19,REG20,REG21,REG22,REG23,REG24);

    
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
    snprintf(pwr_command, sizeof(pwr_command), "$PWRL 36 %s,%s,%s,%lf,%lf,%d,0,0,0,%lf,0,0,0,,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf\r",imei,sendtime,date,lat,lon,fix,alt,dop,satsused,REG0,REG1,REG2,REG3,REG4,REG5,REG6,REG7,REG8,REG9,REG10,REG11,REG12,REG13,REG14,REG15,REG16,REG17,REG18,REG19,REG20,REG21,REG22,REG23,REG24);


    pthread_t thread_id = launch_thread_send_data((void*)pwr_command);
    pthread_join(thread_id,NULL);
}

// function to send power restore
void send_power_restore(void)
{
  
    update_info();

    char pwr_command[1024];
    // prepare the format of the power restore message
    snprintf(pwr_command, sizeof(pwr_command), "$PWRR 36 %s,%s,%s,%lf,%lf,%d,0,0,0,%lf,1,0,0,,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf\r",imei,sendtime,date,lat,lon,fix,alt,dop,satsused,REG0,REG1,REG2,REG3,REG4,REG5,REG6,REG7,REG8,REG9,REG10,REG11,REG12,REG13,REG14,REG15,REG16,REG17,REG18,REG19,REG20,REG21,REG22,REG23,REG24);

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
    snprintf(ign_command, sizeof(ign_command), "$IN8L 36 %s,%s,%s,%lf,%lf,%d,0,0,0,%lf,%d,0,0,,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf\r",imei,sendtime,date,lat,lon,fix,alt,power,dop,satsused,REG0,REG1,REG2,REG3,REG4,REG5,REG6,REG7,REG8,REG9,REG10,REG11,REG12,REG13,REG14,REG15,REG16,REG17,REG18,REG19,REG20,REG21,REG22,REG23,REG24);

    pthread_t thread_id = launch_thread_send_data((void*)ign_command);
    pthread_join(thread_id,NULL);
}



void send_ignition_on(void)
{

    update_info();


    char ign_command[1024];
    printf("sending ignition on\n");

    // prepare the format of the Ignition ON message


    snprintf(ign_command, sizeof(ign_command), "$IN8H 36 %s,%s,%s,%lf,%lf,%d,0,0,0,%lf,%d,0,1,,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf\r",imei,sendtime,date,lat,lon,fix,alt,power,dop,satsused,REG0,REG1,REG2,REG3,REG4,REG5,REG6,REG7,REG8,REG9,REG10,REG11,REG12,REG13,REG14,REG15,REG16,REG17,REG18,REG19,REG20,REG21,REG22,REG23,REG24);


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
        //get the loggerid - this will be from the CSV file uploaded to master_modbus
    ret = get_loggerid(logger_id); 
    //obtain imei number
    ret = get_imei (imei, 15); 

    //for DEBUG purpose only
    //printf("The device has just stoped \n");  
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
int ret;
int error = 0;
socklen_t len = sizeof (error);
int retval = getsockopt (sockfd, SOL_SOCKET, SO_ERROR, &error, &len);

if (retval != 0) 
{
    /* there was a problem getting the error code */
    fprintf(stderr, "error getting socket error code: %s\n", strerror(retval));
    return -1;
}

if (error != 0) {
    /* socket has a non zero error status */
    fprintf(stderr, "socket error: %s\n", strerror(error));
     return -1;
}



char value[20];

ret=get_cellular_ip(value, 20);
if (ret>=0)
{
    printf("Cell Ip is %s \n",value);

    char *hostname;
    struct hostent *hostinfo;

    hostname = "qaroam3.roamworks.com";
    hostinfo = gethostbyname (hostname);

    if (hostinfo == NULL)
        printf("Internet Connection Exists !\n");
    else
        printf("No Internet Connection !\n");

    return 0; 
}
else
    return -1; 
}




// function to establist tcp connection
int connect_tcp(void)
{

    /* Get socket. */
    // sets the protocol to TCP
    protoent = getprotobyname(protoname);
    
    if (protoent == NULL) 
    {
        perror("getprotobyname");
        exit(EXIT_FAILURE);
    }
    sockfd = socket(AF_INET, SOCK_STREAM, protoent->p_proto);

    if (sockfd == -1) 
    {
        perror("socket");
        return -1;
        exit(EXIT_FAILURE);
    }
    else
    sprintf (log,"the sock id is %d \n ",sockfd);
logger(log);

    read_per();
    server_hostname=pers_server_hostname;
    server_port=pers_server_port;
    

    /* Prepare sockaddr_in. */
    // gets ip from a dns
    sprintf (log,"the servername is %s \n ",server_hostname);
logger(log);
    hostent = gethostbyname(server_hostname);

    if (hostent == NULL) 
    {
       // printf("error: gethostbyname %s\n", server_hostname);
        sprintf(log,"No data connectivity, cant resolve dns \n");
logger(log);
        tcp_status=-1;
                 ret = shutdown(sockfd, SHUT_WR);
         //printf (" %d is the return for shutdown \n",ret);
         ret= close(sockfd);
        // printf (" %d is the return for close \n",ret);
    }
    else
    {

    //sets up address from hostent(reverse ip)
    in_addr = inet_addr(inet_ntoa(*(struct in_addr*)*(hostent->h_addr_list)));

    if (in_addr == (in_addr_t)-1) 
    {
        printf("error: inet_addr(\"%s\")\n", *(hostent->h_addr_list));

    }
    // Define ip and port for the server
    sockaddr_in.sin_addr.s_addr = in_addr;
    sockaddr_in.sin_family = AF_INET;
    sockaddr_in.sin_port = htons(server_port);


        /* Do the actual connection. */
    if (connect(sockfd, (struct sockaddr*)&sockaddr_in, sizeof(sockaddr_in)) == -1) 
    {
         sprintf (log,"TCP  connection failed closing socket\n");
logger(log);
         ret = shutdown(sockfd, SHUT_WR);
         //printf (" %d is the return for shutdown \n",ret);
         ret= close(sockfd);
        // printf (" %d is the return for close \n",ret);
         tcp_status=-1;
    }
    else
        tcp_status=1;
        tcp_downtime=0;
    }
    hodor=0;
    return tcp_status;
}

int change_server(char *line)
{
read_per();


//printf("%s\n",line);
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
                    
                    //server_hostname=pt;
                    printf("new server name is %s\n",server_hostname);
                    //pers_server_hostname=server_hostname;
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

sprintf(log,"text received at reporting rate change function is %s\n",line);
logger(log);
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
    printf("Application Closing\n");
//char  datatosend[] ="Application Stopped running \r";
//    pthread_t thread_id = launch_thread_send_data((void*)datatosend);
//    pthread_join(thread_id,NULL);
    //setting stop to 1 will exit the worker loop
    stop=1;

}

int write_per()
{
   // printf("writing data to modbus_rw.conf\n");
    FILE *fp;

    fp = fopen("/etc/modbus_rw.conf","w");

    if (fp)
    {
        //printf(" writinh following data to file : %d,%d,%d,%d,%d,%s,%d \n",pers_reporting_rate,pers_heartbeat_rate,failed_msgs,pers_ign_state,pers_pwr_state,pers_server_hostname,pers_server_port);
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
    //printf("reading data from modbus_rw.conf\n");

    char line[1024];
    FILE *fp;

    fp = fopen("/etc/modbus_rw.conf","r");
    int i=1;

    if (fp)
    {
        while (fgets(line,1024,fp))
        {
            //printf("%s\n",line);
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

                    //pers_server_hostname= pt;                  
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
    sprintf(log,"Creating modbus_rw.conf\n");
logger(log);
    }    
    return 0;

}


// the function  reads data from the gpsfile 
int additional_gps_data()
{

//printf(" Reading additional GPS data \n");
 char line[1024];
    FILE *fp;

    fp = fopen("/tmp/gpsfile","r");
    int linenum=1;

    if (fp)
    {
        while (fgets(line,1024,fp))
        {
            //printf("%s is line number %d \n",line,linenum);
            char* t;
            t = strtok(line,"\t");
            
            int i=1,a;            
            while (i<3)
            {
            
            switch (i)
            {
            case 1: 
                    break;
            case 2:

                    switch(linenum)
{   
                    case 1:
                    printf(log,"Time is %s \n",t);
                            break;
                    case 2:
                    printf("Latitude is %s \n",t);
                            break;
                    //case 3:
                    //printf("N/S is %s \n",t);
                    //        break;
                    case 4:
                    printf("Longitude is %s \n",t);
                            break;
                    //case 5:
                    //printf("E/W is %s \n",t);
                    //        break;
                    //case 6:
                    //printf("PositionFix is %s \n",t);
                    //        break;
                    case 7:
                    printf("Number of Satellites is %s \n",t);
                            satsused=atoi(t);
                            break;
                    case 8:
                    printf("Hdop is %s \n",t);
                            dop=atof(t);
                            break;
                    //case 9:
                    //printf("Altitude is %s \n",t);
                    //        break;
                    //case 10:
                    //printf("Status is %s \n",t);
                    //        break;
}
                   break;

            }
            t = strtok (NULL,",");
            i++;
            }  
linenum++;
                                                                                                                                                            
        }        
        fclose(fp);  

  
    }
    else
    sprintf(log,"Add_GPS_Data - No fix available \n");
logger(log);

return 0;
}

void get_cellular_data(void)
{

printf("Entered function to get cellular data\n");

int ret=0;
int signal_strength=0;
int tcp_status=-1;
char value[]="";
char value2[]="";

unsigned int size=50;



tcp_status=get_cellular_status();
printf("Ret : %d\n",tcp_status);
if (tcp_status>=0)
{
printf("Cellular status is %d\n",tcp_status);
}

ret=get_cellular_ip(value, 20);
printf("Ret : %d\n",ret);
if (ret>=0){
printf("Cell Ip is %s \n",value);
}

ret=get_operatorname(value,15);
printf("Ret : %d\n",ret);
if (ret>=0)
{
printf("Operator Name is %s\n",value);
}

signal_strength=get_signal_strength();
printf("Ret : %d\n",signal_strength);
if (signal_strength>=0)
{
printf("signal strength is %d\n",signal_strength);
}

ret=get_imsi(value,15);
printf("Ret : %d\n",ret);
if (ret>=0)
{
printf("IMSI is %s\n",value);
}

ret=get_cellid(value,size);
printf("Ret : %d\n",ret);
if (ret>=0)
{printf("Cellid is %s\n",value);
}

ret=get_gprs_lac(value,size);
printf("Ret : %d\n",ret);
if (ret>=0)
{
printf("lac is %s ",value);
}




ret=get_networkstatus(value2,14);
printf("Ret : %d\n",ret);
if (ret>=0){
printf("Network status is %s\n",value2);
}



}

void parse_alarm(int state)
{

switch(state)
{
   case 0: 
            printf("Disabled Alarm\n");
            break;
   case 1: 
            printf("Not Active Alarm\n");
            break;
   case 2: 
            printf("Warning Alarm\n");
            break;
   case 3: 
            printf("Shutdown Alarm\n");
            break;
   case 4: 
            printf("Electrical Trip Alarm\n");
            break;
   case 8: 
            printf("Inactive Alarm\n");
            break;
   case 9: 
            printf("Inactive Alarm\n");
            break;

   case 10: 
            printf("Active Alarm\n");
            break;

   case 15: 
            printf("Unimplemented Alarm\n");
            break;
   default: 

            break;



}
}


void coverttonibbles( int recievedint)
{

int shifted1 = (recievedint >> 12);
printf("The 1st nibble value is %d \n",shifted1);
parse_alarm(shifted1);

int shifted2 = ((recievedint & 3840 )>> 8 );
printf("The 2nd nibble value is %d \n",shifted2);
parse_alarm(shifted2);

int shifted3 = ((recievedint & 240) >> 4);
printf("The 3rd nibble value is %d \n",shifted3);
parse_alarm(shifted3);

int shifted4 = (recievedint & 15);
printf("The 4th nibble value is %d \n",shifted4);
parse_alarm(shifted4);

}


void send_fault(int dtc1, int dtc2)
{

    int dtc= dtc1;
    int dtcext = dtc2;
    update_info();
    // prepare the format of the periodic CAN message
    snprintf(datatosend, sizeof(datatosend), "$FLT 36 %s,%s,%s,%lf,%lf,%d,0,0,0,%lf,%d,0,%d,,%s,%d,%d\r",imei,sendtime,date,lat,lon,fix,alt,power,in7,dop,satsused,logger_id,dtc,dtcext);

    //calling send tcp to send the data
    pthread_t thread_id = launch_thread_send_data((void*)datatosend);
    pthread_join(thread_id,NULL);

}


void poll_faults(void)
{

int ret;




printf("entered polling section for faults\n");

REG25=-1,REG26=-1,REG27=-1,REG28=-1,REG29=-1,REG30=-1,REG31=-1,REG32=-1,REG33=-1,REG34=-1;

  ret = read_tag_latest_data_from_db("Tag25","DSEPANEL",4,1,&REG25,timestamp);  
    printf("DTC 1 : %lf\n",REG25);  
    if (ret!=0)
    {
        REG25=-1;
    } 

  ret = read_tag_latest_data_from_db("Tag26","DSEPANEL",4,1,&REG26,timestamp);  
    printf("DTC 1 extended : %lf\n",REG26);  
    if (ret!=0)
    {
        REG26=-1;
    } 

    if ((REG25>0)&&(REG26>0))
{
send_fault(REG25,REG26);
}

  ret = read_tag_latest_data_from_db("Tag27","DSEPANEL",4,1,&REG27,timestamp);  
    printf("DTC 2 : %lf\n",REG27);  
    if (ret!=0)
    {
        REG27=-1;
    } 

  ret = read_tag_latest_data_from_db("Tag28","DSEPANEL",4,1,&REG28,timestamp);  
    printf("DTC 2 extended : %lf\n",REG28);  
    if (ret!=0)
    {
        REG28=-1;
    } 

    if ((REG27>0)&&(REG28>0))
{
send_fault(REG27,REG28);
}

  ret = read_tag_latest_data_from_db("Tag29","DSEPANEL",4,1,&REG29,timestamp);  
    printf("DTC 3 : %lf\n",REG29);  
    if (ret!=0)
    {
        REG29=-1;
    } 

  ret = read_tag_latest_data_from_db("Tag30","DSEPANEL",4,1,&REG30,timestamp);  
    printf("DTC 3 extended : %lf\n",REG30);  
    if (ret!=0)
    {
        REG30=-1;
    } 

    if ((REG29>0)&&(REG30>0))
{
send_fault(REG29,REG30);
}


  ret = read_tag_latest_data_from_db("Tag31","DSEPANEL",4,1,&REG31,timestamp);  
    printf("DTC 4 : %lf\n",REG31);  
    if (ret!=0)
    {
        REG31=-1;
    } 

  ret = read_tag_latest_data_from_db("Tag32","DSEPANEL",4,1,&REG32,timestamp);  
    printf("DTC 4 extended : %lf\n",REG32);  
    if (ret!=0)
    {
        REG32=-1;
    } 


    if ((REG31>0)&&(REG32>0))
{
send_fault(REG31,REG32);
}

  ret = read_tag_latest_data_from_db("Tag33","DSEPANEL",4,1,&REG33,timestamp);  
    printf("DTC 5 : %lf\n",REG33);  
    if (ret!=0)
    {
        REG33=-1;
    } 

  ret = read_tag_latest_data_from_db("Tag34","DSEPANEL",4,1,&REG34,timestamp);  
    printf("DTC 5 extended : %lf\n",REG34);  
    if (ret!=0)
    {
        REG34=-1;
    } 



    if ((REG33>0)&&(REG34>0))
{
send_fault(REG33,REG34);
}


}


void send_alarm(int alarm, int value)
{
    int alm= alarm;
    int val = value;

printf(" Alarm will be send for alarm %d with value %d \n",alm,val);
/*

    update_info();

    snprintf(datatosend, sizeof(datatosend), "$ALM 36 %s,%s,%s,%lf,%lf,%d,0,0,0,%lf,%d,0,%d,,%s,%d,%d\r",imei,sendtime,date,lat,lon,fix,alt,power,in7,dop,satsused,logger_id,alm,val);


    pthread_t thread_id = launch_thread_send_data((void*)datatosend);
    pthread_join(thread_id,NULL);
*/
}


void poll_alarms(void)
{

int ret;



printf("entered section to read alarms\n");
double ALM0=-1,ALM1=-1,ALM2=-1,ALM3=-1,ALM4=-1,ALM5=-1,ALM6=-1,ALM7=-1,ALM8=-1,ALM9=-1,ALM10=-1,ALM11=-1,ALM12=-1,ALM13=-1,ALM14=-1,ALM15=-1, ALM16=-1,ALM17=-1;
int recievedint;

    double value;
    char timestamp[26];


// variables to prepare periodic information 
 
    //get the modbus data values and set to -1 if not available



    ret = read_tag_latest_data_from_db("Alm0","DSEPANEL",5,1,&ALM0,timestamp); 
    printf("Alarm Value 1: %lf\n",ALM0);  
    recievedint= (int)ALM0;
    if (ret==0)
    {
        int shifted1 = (recievedint >> 12);
        printf("a0 is %d \n",shifted1);
        //parse_alarm(shifted1);
        if((shifted1 != 1) && (a0!=shifted1))
        {
        send_alarm(1,shifted1);
        a0=shifted1;
        }

        int shifted2 = ((recievedint & 3840 )>> 8 );
        printf("a1 is %d \n",shifted2);
        //parse_alarm(shifted2);
        if((shifted2 != 1) && (a1!=shifted2))
        {
        send_alarm(2,shifted2);
        a1=shifted2;
        }

        int shifted3 = ((recievedint & 240) >> 4);
        printf("a2 is %d \n",shifted3);       
        //parse_alarm(shifted3);ma
        if((shifted3 != 1) && (a2!=shifted3))
        {
        send_alarm(3,shifted3);
        a2=shifted3;
        }

        int shifted4 = (recievedint & 15);
        printf("a3 is %d \n",shifted4);
        //parse_alarm(shifted4);
        if((shifted4 != 1) && (a2!=shifted4))
        {
        send_alarm(4,shifted4);
        a3=shifted4;
        }
    } 

    ret = read_tag_latest_data_from_db("Alm1","DSEPANEL",5,1,&ALM1,timestamp); 
    printf("Alarm Value 2: %lf\n",ALM1);  
    recievedint= (int)ALM1;
    if (ret==0)
    {
        int shifted1 = (recievedint >> 12);
        printf("a4 is %d \n",shifted1);
        //parse_alarm(shifted1);
        if((shifted1 != 1) && (a4!=shifted1))
        {
        send_alarm(5,shifted1);
        a4=shifted1;
        }

        int shifted2 = ((recievedint & 3840 )>> 8 );
        printf("a5 is %d \n",shifted2);
        //parse_alarm(shifted2);
        if((shifted2 != 1) && (a5!=shifted2))
        {
        send_alarm(6,shifted2);
        a5=shifted2;
        }

        int shifted3 = ((recievedint & 240) >> 4);
        printf("a6 is %d \n",shifted3);       
        //parse_alarm(shifted3);
        if((shifted3 != 1) && (a6!=shifted3))
        {
        send_alarm(7,shifted3);
        a6=shifted3;
        }

        int shifted4 = (recievedint & 15);
        printf("a7 is %d \n",shifted4);
        //parse_alarm(shifted4);
        if((shifted4 != 1) && (a7!=shifted4))
        {
        send_alarm(8,shifted4);
        a7=shifted4;
        }
    } 

    ret = read_tag_latest_data_from_db("Alm2","DSEPANEL",5,1,&ALM2,timestamp); 
        printf("Alarm Value 3: %lf\n",ALM2);  
    recievedint= (int)ALM2;
    if (ret==0)
    {
        int shifted1 = (recievedint >> 12);
        printf("a8 is %d \n",shifted1);
        //parse_alarm(shifted1);
        if((shifted1 != 1) && (a8!=shifted1))
        {
        send_alarm(9,shifted1);
        a8=shifted1;
        }

        int shifted2 = ((recievedint & 3840 )>> 8 );
        printf("a9 is %d \n",shifted2);
        //parse_alarm(shifted2);
        if((shifted2 != 1) && (a9 !=shifted2))
        {
        send_alarm(10,shifted2);
        a9=shifted2;
        }

        int shifted3 = ((recievedint & 240) >> 4);
        printf("a10 is %d \n",shifted3);       
        //parse_alarm(shifted3);
        if((shifted3 != 1) && (a10!=shifted3))
        {
        send_alarm(11,shifted3);
        a10=shifted3;
        }

        int shifted4 = (recievedint & 15);
        printf("a11 is %d \n",shifted4);
        //parse_alarm(shifted4);
        if((shifted4 != 1) && (a11!=shifted4))
        {
        send_alarm(12,shifted4);
        a11=shifted4;
        }
    } 

    ret = read_tag_latest_data_from_db("Alm3","DSEPANEL",5,1,&ALM3,timestamp); 
       printf("Alarm Value 4: %lf\n",ALM3);  
    recievedint= (int)ALM3;
    if (ret==0)
    {
        int shifted1 = (recievedint >> 12);
        printf("a12 is %d \n",shifted1);
        //parse_alarm(shifted1);
        if((shifted1 != 1) && (a12!=shifted1))
        {
        send_alarm(13,shifted1);
        a12=shifted1;
        }

        int shifted2 = ((recievedint & 3840 )>> 8 );
        printf("a13 is %d \n",shifted2);
        //parse_alarm(shifted2);
        if((shifted2 != 1) && (a13 !=shifted2))
        {
        send_alarm(14,shifted2);
        a13=shifted2;
        }

        int shifted3 = ((recievedint & 240) >> 4);
        printf("a14 is %d \n",shifted3);       
        //parse_alarm(shifted3);
        if((shifted3 != 1) && (a14!=shifted3))
        {
        send_alarm(15,shifted3);
        a14=shifted3;
        }

        int shifted4 = (recievedint & 15);
        printf("a15 is %d \n",shifted4);
        //parse_alarm(shifted4);
        if((shifted4 != 1) && (a15!=shifted4))
        {
        send_alarm(16,shifted4);
        a15=shifted4;
        }
    } 

    ret = read_tag_latest_data_from_db("Alm4","DSEPANEL",5,1,&ALM4,timestamp); 
        printf("Alarm Value 5: %lf\n",ALM4);  
    recievedint= (int)ALM4;
    if (ret==0)
    {
        int shifted1 = (recievedint >> 12);
        printf("a16 is %d \n",shifted1);
        //parse_alarm(shifted1);
        if((shifted1 != 1) && (a16!=shifted1))
        {
        send_alarm(17,shifted1);
        a16=shifted1;
        }

        int shifted2 = ((recievedint & 3840 )>> 8 );
        printf("a17 is %d \n",shifted2);
        //parse_alarm(shifted2);
        if((shifted2 != 1) && (a17 !=shifted2))
        {
        send_alarm(18,shifted2);
        a17=shifted2;
        }

        int shifted3 = ((recievedint & 240) >> 4);
        printf("a18 is %d \n",shifted3);       
        //parse_alarm(shifted3);
        if((shifted3 != 1) && (a18!=shifted3))
        {
        send_alarm(19,shifted3);
        a18=shifted3;
        }

        int shifted4 = (recievedint & 15);
        printf("a19 is %d \n",shifted4);
        //parse_alarm(shifted4);
        if((shifted4 != 1) && (a19!=shifted4))
        {
        send_alarm(20,shifted4);
        a19=shifted4;
        }
    } 
    
    ret = read_tag_latest_data_from_db("Alm5","DSEPANEL",5,1,&ALM5,timestamp); 
       printf("Alarm Value 6: %lf\n",ALM5);  
    recievedint= (int)ALM5;
    if (ret==0)
    {
        int shifted1 = (recievedint >> 12);
        printf("a20 is %d \n",shifted1);
        //parse_alarm(shifted1);
        if((shifted1 != 1) && (a20!=shifted1))
        {
        send_alarm(21,shifted1);
        a20=shifted1;
        }

        int shifted2 = ((recievedint & 3840 )>> 8 );
        printf("a21 is %d \n",shifted2);
        //parse_alarm(shifted2);
        if((shifted2 != 1) && (a21 !=shifted2))
        {
        send_alarm(22,shifted2);
        a21=shifted2;
        }

        int shifted3 = ((recievedint & 240) >> 4);
        printf("a22 is %d \n",shifted3);       
        //parse_alarm(shifted3);
        if((shifted3 != 1) && (a22!=shifted3))
        {
        send_alarm(23,shifted3);
        a22=shifted3;
        }

        int shifted4 = (recievedint & 15);
        printf("a23 is %d \n",shifted4);
        //parse_alarm(shifted4);
        if((shifted4 != 1) && (a23!=shifted4))
        {
        send_alarm(24,shifted4);
        a23=shifted4;
        }
    } 
    

    ret = read_tag_latest_data_from_db("Alm6","DSEPANEL",5,1,&ALM6,timestamp); 
        printf("Alarm Value 7: %lf\n",ALM6);  
    recievedint= (int)ALM6;
    if (ret==0)
    {
        int shifted1 = (recievedint >> 12);
        printf("a24 is %d \n",shifted1);
        //parse_alarm(shifted1);
        if((shifted1 != 1) && (a24!=shifted1))
        {
        send_alarm(25,shifted1);
        a24=shifted1;
        }

        int shifted2 = ((recievedint & 3840 )>> 8 );
        printf("a25 is %d \n",shifted2);
        //parse_alarm(shifted2);
        if((shifted2 != 1) && (a25 !=shifted2))
        {
        send_alarm(26,shifted2);
        a25=shifted2;
        }

        int shifted3 = ((recievedint & 240) >> 4);
        printf("a26 is %d \n",shifted3);       
        //parse_alarm(shifted3);
        if((shifted3 != 1) && (a26!=shifted3))
        {
        send_alarm(27,shifted3);
        a26=shifted3;
        }

        int shifted4 = (recievedint & 15);
        printf("a27 is %d \n",shifted4);
        //parse_alarm(shifted4);
        if((shifted4 != 1) && (a27!=shifted4))
        {
        send_alarm(28,shifted4);
        a27=shifted4;
        }
    } 
    

    ret = read_tag_latest_data_from_db("Alm7","DSEPANEL",5,1,&ALM7,timestamp); 
        printf("Alarm Value 8: %lf\n",ALM7);  
    recievedint= (int)ALM7;
    if (ret==0)
    {
        int shifted1 = (recievedint >> 12);
        printf("a28 is %d \n",shifted1);
        //parse_alarm(shifted1);
        if((shifted1 != 1) && (a28!=shifted1))
        {
        send_alarm(29,shifted1);
        a28=shifted1;
        }

        int shifted2 = ((recievedint & 3840 )>> 8 );
        printf("a29 is %d \n",shifted2);
        //parse_alarm(shifted2);
        if((shifted2 != 1) && (a29 !=shifted2))
        {
        send_alarm(30,shifted2);
        a29=shifted2;
        }

        int shifted3 = ((recievedint & 240) >> 4);
        printf("a30 is %d \n",shifted3);       
        //parse_alarm(shifted3);
        if((shifted3 != 1) && (a30!=shifted3))
        {
        send_alarm(31,shifted3);
        a30=shifted3;
        }

        int shifted4 = (recievedint & 15);
        printf("a31 is %d \n",shifted4);
        //parse_alarm(shifted4);
        if((shifted4 != 1) && (a31!=shifted4))
        {
        send_alarm(32,shifted4);
        a31=shifted4;
        }
    } 
    

    ret = read_tag_latest_data_from_db("Alm8","DSEPANEL",5,1,&ALM8,timestamp); 
       printf("Alarm Value 9: %lf\n",ALM8);  
    recievedint= (int)ALM8;
    if (ret==0)
    {
        int shifted1 = (recievedint >> 12);
        printf("a32 is %d \n",shifted1);
        //parse_alarm(shifted1);
        if((shifted1 != 1) && (a32!=shifted1))
        {
        send_alarm(33,shifted1);
        a32=shifted1;
        }

        int shifted2 = ((recievedint & 3840 )>> 8 );
        printf("a33 is %d \n",shifted2);
        //parse_alarm(shifted2);
        if((shifted2 != 1) && (a33 !=shifted2))
        {
        send_alarm(34,shifted2);
        a33=shifted2;
        }

        int shifted3 = ((recievedint & 240) >> 4);
        printf("a34 is %d \n",shifted3);       
        //parse_alarm(shifted3);
        if((shifted3 != 1) && (a34!=shifted3))
        {
        send_alarm(35,shifted3);
        a34=shifted3;
        }

        int shifted4 = (recievedint & 15);
        printf("a35 is %d \n",shifted4);
        //parse_alarm(shifted4);
        if((shifted4 != 1) && (a35!=shifted4))
        {
        send_alarm(36,shifted4);
        a35=shifted4;
        }
    } 

    ret = read_tag_latest_data_from_db("Alm9","DSEPANEL",5,1,&ALM9,timestamp); 
        printf("Alarm Value 10: %lf\n",ALM9);  
    recievedint= (int)ALM9;
    if (ret==0)
    {
        int shifted1 = (recievedint >> 12);
        printf("a36 is %d \n",shifted1);
        //parse_alarm(shifted1);
        if((shifted1 != 1) && (a36!=shifted1))
        {
        send_alarm(37,shifted1);
        a36=shifted1;
        }

        int shifted2 = ((recievedint & 3840 )>> 8 );
        printf("a37 is %d \n",shifted2);
        //parse_alarm(shifted2);
        if((shifted2 != 1) && (a37 !=shifted2))
        {
        send_alarm(38,shifted2);
        a37=shifted2;
        }

        int shifted3 = ((recievedint & 240) >> 4);
        printf("a38 is %d \n",shifted3);       
        //parse_alarm(shifted3);
        if((shifted3 != 1) && (a38!=shifted3))
        {
        send_alarm(39,shifted3);
        a38=shifted3;
        }

        int shifted4 = (recievedint & 15);
        printf("a39 is %d \n",shifted4);
        //parse_alarm(shifted4);
        if((shifted4 != 1) && (a39!=shifted4))
        {
        send_alarm(40,shifted4);
        a39=shifted4;
        }
    } 

    ret = read_tag_latest_data_from_db("Alm10","DSEPANEL",5,1,&ALM10,timestamp); 
        printf("Alarm Value 11: %lf\n",ALM10);  
    recievedint= (int)ALM10;
    if (ret==0)
    {
        int shifted1 = (recievedint >> 12);
        printf("a40 is %d \n",shifted1);
        //parse_alarm(shifted1);
        if((shifted1 != 1) && (a40!=shifted1))
        {
        send_alarm(41,shifted1);
        a40=shifted1;
        }

        int shifted2 = ((recievedint & 3840 )>> 8 );
        printf("a41 is %d \n",shifted2);
        //parse_alarm(shifted2);
        if((shifted2 != 1) && (a41 !=shifted2))
        {
        send_alarm(42,shifted2);
        a41=shifted2;
        }

        int shifted3 = ((recievedint & 240) >> 4);
        printf("a42 is %d \n",shifted3);       
        //parse_alarm(shifted3);
        if((shifted3 != 1) && (a42!=shifted3))
        {
        send_alarm(43,shifted3);
        a42=shifted3;
        }

        int shifted4 = (recievedint & 15);
        printf("a43 is %d \n",shifted4);
        //parse_alarm(shifted4);
        if((shifted4 != 1) && (a43!=shifted4))
        {
        send_alarm(44,shifted4);
        a43=shifted4;
        }
    } 

    ret = read_tag_latest_data_from_db("Alm11","DSEPANEL",5,1,&ALM11,timestamp);  
        printf("Alarm Value 12: %lf\n",ALM11);  
    recievedint= (int)ALM1;
    if (ret==0)
    {
        int shifted1 = (recievedint >> 12);
        printf("a44 is %d \n",shifted1);
        //parse_alarm(shifted1);
        if((shifted1 != 1) && (a44!=shifted1))
        {
        send_alarm(45,shifted1);
        a44=shifted1;
        }

        int shifted2 = ((recievedint & 3840 )>> 8 );
        printf("a45 is %d \n",shifted2);
        //parse_alarm(shifted2);
        if((shifted2 != 1) && (a45 !=shifted2))
        {
        send_alarm(46,shifted2);
        a45=shifted2;
        }

        int shifted3 = ((recievedint & 240) >> 4);
        printf("a46 is %d \n",shifted3);       
        //parse_alarm(shifted3);
        if((shifted3 != 1) && (a46!=shifted3))
        {
        send_alarm(47,shifted3);
        a46=shifted3;
        }

        int shifted4 = (recievedint & 15);
        printf("a47 is %d \n",shifted4);
        //parse_alarm(shifted4);
        if((shifted4 != 1) && (a47!=shifted4))
        {
        send_alarm(48,shifted4);
        a47=shifted4;
        }
    } 
    ret = read_tag_latest_data_from_db("Alm12","DSEPANEL",5,1,&ALM12,timestamp);  
        printf("Alarm Value 13: %lf\n",ALM12);  
    recievedint= (int)ALM12;
    if (ret==0)
    {
        int shifted1 = (recievedint >> 12);
        printf("a48 is %d \n",shifted1);
        //parse_alarm(shifted1);
        if((shifted1 != 1) && (a48!=shifted1))
        {
        send_alarm(49,shifted1);
        a48=shifted1;
        }

        int shifted2 = ((recievedint & 3840 )>> 8 );
        printf("a49 is %d \n",shifted2);
        //parse_alarm(shifted2);
        if((shifted2 != 1) && (a49 !=shifted2))
        {
        send_alarm(50,shifted2);
        a49=shifted2;
        }

        int shifted3 = ((recievedint & 240) >> 4);
        printf("a50 is %d \n",shifted3);       
        //parse_alarm(shifted3);
        if((shifted3 != 1) && (a50!=shifted3))
        {
        send_alarm(51,shifted3);
        a50=shifted3;
        }

        int shifted4 = (recievedint & 15);
        printf("a51 is %d \n",shifted4);
        //parse_alarm(shifted4);
        if((shifted4 != 1) && (a51!=shifted4))
        {
        send_alarm(52,shifted4);
        a51=shifted4;
        }
    } 

    ret = read_tag_latest_data_from_db("Alm13","DSEPANEL",5,1,&ALM13,timestamp); 
        printf("Alarm Value 14: %lf\n",ALM13);  
    recievedint= (int)ALM13;
    if (ret==0)
    {
        int shifted1 = (recievedint >> 12);
        printf("a52 is %d \n",shifted1);
        //parse_alarm(shifted1);
        if((shifted1 != 1) && (a52!=shifted1))
        {
        send_alarm(53,shifted1);
        a52=shifted1;
        }

        int shifted2 = ((recievedint & 3840 )>> 8 );
        printf("a53 is %d \n",shifted2);
        //parse_alarm(shifted2);
        if((shifted2 != 1) && (a53 !=shifted2))
        {
        send_alarm(54,shifted2);
        a53=shifted2;
        }

        int shifted3 = ((recievedint & 240) >> 4);
        printf("a54 is %d \n",shifted3);       
        //parse_alarm(shifted3);
        if((shifted3 != 1) && (a54!=shifted3))
        {
        send_alarm(55,shifted3);
        a54=shifted3;
        }

        int shifted4 = (recievedint & 15);
        printf("a55 is %d \n",shifted4);
        //parse_alarm(shifted4);
        if((shifted4 != 1) && (a55!=shifted4))
        {
        send_alarm(56,shifted4);
        a55=shifted4;
        }
    } 

    read_tag_latest_data_from_db("Alm14","DSEPANEL",5,1,&ALM14,timestamp); 
        printf("Alarm Value 15: %lf\n",ALM14);  
    recievedint= (int)ALM14;
    if (ret==0)
    {
        int shifted1 = (recievedint >> 12);
        printf("a56 is %d \n",shifted1);
        //parse_alarm(shifted1);
        if((shifted1 != 1) && (a56!=shifted1))
        {
        send_alarm(57,shifted1);
        a56=shifted1;
        }

        int shifted2 = ((recievedint & 3840 )>> 8 );
        printf("a57 is %d \n",shifted2);
        //parse_alarm(shifted2);
        if((shifted2 != 1) && (a57 !=shifted2))
        {
        send_alarm(58,shifted2);
        a57=shifted2;
        }

        int shifted3 = ((recievedint & 240) >> 4);
        printf("a58 is %d \n",shifted3);       
        //parse_alarm(shifted3);
        if((shifted3 != 1) && (a58!=shifted3))
        {
        send_alarm(59,shifted3);
        a58=shifted3;
        }

        int shifted4 = (recievedint & 15);
        printf("a59 is %d \n",shifted4);
        //parse_alarm(shifted4);
        if((shifted4 != 1) && (a59!=shifted4))
        {
        send_alarm(60,shifted4);
        a59=shifted4;
        }
    } 

    ret = read_tag_latest_data_from_db("Alm15","DSEPANEL",5,1,&ALM15,timestamp); 
        printf("Alarm Value 16: %lf\n",ALM15);  
    recievedint= (int)ALM15;
    if (ret==0)
    {
        int shifted1 = (recievedint >> 12);
        printf("a60 is %d \n",shifted1);
        //parse_alarm(shifted1);
        if((shifted1 != 1) && (a60!=shifted1))
        {
        send_alarm(61,shifted1);
        a60=shifted1;
        }

        int shifted2 = ((recievedint & 3840 )>> 8 );
        printf("a61 is %d \n",shifted2);
        //parse_alarm(shifted2);
        if((shifted2 != 1) && (a61 !=shifted2))
        {
        send_alarm(62,shifted2);
        a61=shifted2;
        }

        int shifted3 = ((recievedint & 240) >> 4);
        printf("a62 is %d \n",shifted3);       
        //parse_alarm(shifted3);
        if((shifted3 != 1) && (a62!=shifted3))
        {
        send_alarm(63,shifted3);
        a62=shifted3;
        }

        int shifted4 = (recievedint & 15);
        printf("a63 is %d \n",shifted4);
        //parse_alarm(shifted4);
        if((shifted4 != 1) && (a63!=shifted4))
        {
        send_alarm(64,shifted4);
        a63=shifted4;
        }
    } 
    
    ret = read_tag_latest_data_from_db("Alm16","DSEPANEL",5,1,&ALM16,timestamp); 
        printf("Alarm Value 17: %lf\n",ALM16);  
    recievedint= (int)ALM16;
    if (ret==0)
    {
        int shifted1 = (recievedint >> 12);
        printf("a64 is %d \n",shifted1);
        //parse_alarm(shifted1);
        if((shifted1 != 1) && (a64!=shifted1))
        {
        send_alarm(65,shifted1);
        a64=shifted1;
        }

        int shifted2 = ((recievedint & 3840 )>> 8 );
        printf("a65 is %d \n",shifted2);
        //parse_alarm(shifted2);
        if((shifted2 != 1) && (a65 !=shifted2))
        {
        send_alarm(66,shifted2);
        a65=shifted2;
        }

        int shifted3 = ((recievedint & 240) >> 4);
        printf("a66 is %d \n",shifted3);       
        //parse_alarm(shifted3);
        if((shifted3 != 1) && (a66!=shifted3))
        {
        send_alarm(67,shifted3);
        a66=shifted3;
        }

        int shifted4 = (recievedint & 15);
        printf("a67 is %d \n",shifted4);
        //parse_alarm(shifted4);
        if((shifted4 != 1) && (a67!=shifted4))
        {
        send_alarm(68,shifted4);
        a67=shifted4;
        }
    } 


    ret = read_tag_latest_data_from_db("Alm17","DSEPANEL",5,1,&ALM17,timestamp);  
       printf("Alarm Value 18: %lf\n",ALM17);  
    recievedint= (int)ALM17;
    if (ret==0)
    {
        int shifted1 = (recievedint >> 12);
        printf("a68 is %d \n",shifted1);
        //parse_alarm(shifted1);
        if((shifted1 != 1) && (a68!=shifted1))
        {
        send_alarm(69,shifted1);
        a68=shifted1;
        }

    } 

   

}


void force_gps_update(void)
{
    if (ign_state>0)
    {
    printf("Forcing GPS to update\n");
    pthread_t tid;
    //printf("Calling a thread to send data as %s",data);
    ret = pthread_create(&tid, NULL, force_gps_update_thread, NULL);
    if (ret != 0) 
        printf("Error from pthread: %d\n", ret); 


    }

}

void force_gps_update_thread(void)
{
system("/usr/bin/gps_poll > /dev/null &");
}



// MAIN PROGRAM CALL STARTS BELOW

int main (int argc, char *argv[])
{ 


if((argc>1) && atoi(argv[1]) == 0)
{
printf(" test functionality \n");
}
else if( (argc>1) && atoi(argv[1]) == 1)
{
poll_alarms();
}
else if( (argc>1) )
{   double value=atof(argv[1]);
    int y = (int)value;
    double z =(y / 3600);
    printf("Engine Hour Meter value :%lf\n",z);
}
else 
{

char* timestamps;
int ret;
double values;
     sleep(10);   

    sprintf(log,"Modbus_RW started - Initialization in process \n"); 
    logger(log);  
    ret=read_per();
    ign_state=pers_ign_state;
    pwr_state=pers_pwr_state;
    reporting_rate=pers_reporting_rate;
    heartbeat_rate=pers_heartbeat_rate;
    if (ign_state<0)
    {
    sprintf(log,"fresh install will create the config file\n");
    logger(log);
    write_per();   
    }    
 

    // sleep to wait for initialisation
    sleep(30);

    
    //get the loggerid - this will be from the CSV file uploaded to master_modbus
    ret = get_loggerid(logger_id); 
    //obtain imei number
    ret = get_imei (imei, 15);



    // create a signal/alarm to tick every second and assign the tick handler
    struct sigaction sact;
    memset(&sact,0 , sizeof sact);
    sigemptyset (&sact.sa_mask);
    sact.sa_flags = 0;
    sact.sa_handler = tick_handler;
    sigaction(SIGALRM, &sact, NULL);



    sprintf (log,"The script version %s is now running for device with imei=%s and the modbus configfile is %s and should report to the server\n", script_ver, imei, logger_id);
    logger(log);
    tcp_status=connect_tcp();   
    if (tcp_status>0)
{
    sprintf(log,"Data connection present\n");
    logger(log);
        //sends the login message
    ready_device();
}
    else
    sprintf(log,"Data connection not available\n");
    logger(log);
    
    
    


    // to capture the cntrl+c INTerupts
    signal(SIGINT, INThandler);

    //start the auxillary timers
    ret=srtSchedule();
    if (ret==0)
    {
    sprintf(log,"4 additional timers started, monitor io lines, read tcp data , restart application, force gps update\n");
logger(log);
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

    // The below loop will never exit the vaule of the do_fuction will be changed from the signal
    switch(do_function)
    {

    case 1:
            sprintf(log,"Power UP\n");
            logger(log);
            send_power_up();
            do_function=0;
            read_per();
            pers_pwr_state=pwr_state;
            write_per();
            break;

    case 2:
            sprintf(log,"Periodic Report\n"); 
            logger(log);         
            send_modbus_data();                     
            do_function=0;
            break;

    case 3:
            sprintf(log,"Heartbeat\n");
            logger(log);
            send_heartbeat();
            do_function=0;
            break;

    case 4:
            sprintf(log,"Ignition OFF\n");
            logger(log);
            send_ignition_off();
            read_per();
            pers_ign_state=ign_state;
            do_function=0;
            write_per();
            break;

    case 5:
            sprintf(log,"Ignition ON\n");
            logger(log);
            send_ignition_on();
            read_per();
            pers_ign_state=ign_state;
            do_function=0;
            write_per();
            break;
    
    case 6:
            sprintf(log,"Power Loss\n");
            logger(log);
            send_power_loss();
            do_function=0;
            read_per();
            pers_pwr_state=pwr_state;
            ret=write_per();
            break;

    case 7:
            sprintf(log,"Power Restore\n");
            logger(log);
            send_power_restore();
            do_function=0;
            read_per();
            pers_pwr_state=pwr_state;
            ret=write_per();
            break;
    
    case 8:
            sprintf(log,"Poll Response\n");
            logger(log);
            send_poll_response();
            do_function=0;
            break;

    case 9:
            sprintf(log,"checking for incoming messages\n");
            logger(log);
            read_tcp_data();
            do_function=0;
            break;

    case 10: 
            sprintf(log,"restart the application\n");
            logger(log);
            alarm(0);
            sleep(3);
            ret = shutdown(sockfd, SHUT_WR);
            sprintf (log," %d is the return for shutdown \n",ret);
            logger(log);
            ret= close(sockfd);
            sprintf (log," %d is the return for close \n",ret);
            logger(log);
            execve("/usr/bin/modbus_rw",NULL,NULL);
            printf("Restart shouf have occured and this line will not be shown \n");  

    case 11:
            sprintf(log,"keep alive\n");
            logger(log);
            send_ping();
            do_function=0;
            break;
    
    case 12:
            sprintf(log,"force GPS update\n");
            logger(log);
            force_gps_update();       
            do_function=0;
            break;
    default:

            break;
    }


}
    while (!stop);
    read_per();
    write_per();
    ret = shutdown(sockfd, SHUT_WR);
    ret= close(sockfd);
    sprintf(log,"The application has closed \n");
    logger(log);



}  

    exit(0);

    return 0;

}





