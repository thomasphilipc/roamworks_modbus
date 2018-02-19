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
volatile int time_tracker_sec=0;
//keeps account of heartbeat
volatile int hb_tracker_min;
// keeps track of the status of TCP
volatile int tcp_status=0;

char imei[14];
char logger_id[20];


int fix=1,course=0,speed=0,power,in7,bat,dop=0,satsused=0;
int REG0=0,REG1=0,REG2=0,REG3=0,REG4=0,REG5=0,REG6=0,REG7=0,REG8=0,REG9=0,REG10=0,REG11=0,REG12=0,REG13=0,REG14=0,REG15=0;
int REG16=0,REG17=0,REG18=0,REG19=0,REG20=0,REG21=0,REG22=0,REG23=0,REG24=0,REG25=0;
char sendtime[10]="",date[11]="";
double lat=0.0,lon=0.0,alt=0.0;
char  datatosend[1024];
char buff[100]; 

int ret = -1, i;
double size, lat, lon, alt;
char timestamp[26];
size_t timer1; 
int res,ret1;
char this[1024];
int data_length;
char buff[100]; 

int hodor=1;

char* temp_buff;

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

// below variables are to handle ignition
int ign_filter=0; //a filter implementation via counter to check if the state is stable for the entire duration
int ign_state=-1; // a state holder where -1 is indeterminant 
int pwr_state=1;  // a power state holder , we assume it is High on start up
int pwr_filter=0; // a filter implementation via counter to check if the state is stable for the entire duration


// define individual timers 
timer_t firstTimerID;
timer_t secondTimerID;
timer_t thirdTimerID;

int poll_ioline_state(int);
int prev_io_state=-1;

void update_info();

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
    if ((ign_state==1)&&(ret==2))
{
    ign_filter=0;
}
    if ((ign_state==0)&&(ret==1))
{
    ign_filter=0;
}

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
  printf("reached the thread with data as %s \n",(char *)data);
  // calls the send tcp data function by passing a pointer to data string
  ret=send_tcp_data((char *)data);
  if (ret<0)
{
    printf("Message sending failed\n");
}

  

}

pthread_t launch_thread_send_data(void *data)
{

pthread_t tid;
    printf("Calling a thread to send data as %s",data);
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

    //rc2 = makeTimer("Second Timer", &secondTimerID, 5, 5);

    //rc3 = makeTimer("Third Timer", &thirdTimerID, 10, 10);

    //return (rc1+rc2+rc3);
    return (rc1);
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
        if (time_tracker_min%1==0)
        {
        do_function = 1; // every 2 minutes check io lines
        }

        if ((time_tracker_min%1==0)&&(ign_state==1))
        {
        do_function = 2; // every 5 minutes send periodic data while ignition on
        }

        if (hb_tracker_min==720)
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
    

    alarm(1);
}

// function to read data over tcp
void read_tcp_data()
{
    char  read[1024];
    if( recv(sockfd, read , 2000 , 0) < 0)
    {
    printf("recv failed");
    }
    printf("Recieved data: %s",read);
    
    //perform action with the data

}


// function to send data over tcp
int send_tcp_data(void *data)
{

    

    ret = send(sockfd, data, strlen(data),0);
    // check the ret abd tge size of sent data ; if the match then all data has been sent    

    if (ret == (strlen(data)))
    {
        // check for response from server and print response will require business logic implementation 
        //if( recv(sockfd, this , 2000 , 0) < 0)
        //{
        //    printf("recv failed");
        //}
        //printf("Recieved reply: %s",this);
        printf("Sucess\n");
    return 0;

    }
    else
    {
    return -1;
    printf("Fail\n");    
    }




}


// function to poll modbus data
void send_modbus_data()
{

int ret;

//FILE *in;
//FILE *grab;
char* sql_buff;


int res,ret1;



printf("entered polling section for modbus_data\n");


    double value;
    char timestamp[26];


// variables to prepare periodic information 
 
    //get the modbus data values
    ret = read_tag_latest_data_from_db("Tag1","cpanel",1,1,&value,timestamp);    
    REG0=value;
        if (ret<0)
    {
    REG0=0;
    } 
    ret = read_tag_latest_data_from_db("Tag2","cpanel",1,1,&value,timestamp); 
    REG1=value;
        if (ret<0)
    {
    REG1=0;
    } 
    ret = read_tag_latest_data_from_db("Tag3","cpanel",1,1,&value,timestamp); 
    REG2=value;
        if (ret<0)
    {
    REG2=0;
    } 
    ret = read_tag_latest_data_from_db("Tag4","cpanel",1,1,&value,timestamp); 
    REG3=value;
        if (ret<0)
    {
    REG3=0;
    } 
    ret = read_tag_latest_data_from_db("Tag5","cpanel",1,1,&value,timestamp); 
    REG4=value;
    if (ret<0)
    {
    REG4=0;
    } 

    update_info();


    // prepare the format of the periodic CAN message
    snprintf(datatosend, sizeof(datatosend), "$CANP 36 %s,%s,%s,%f,%f,%d,%f,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\r",imei,sendtime,date,lat,lon,fix,alt,power,in7,dop,satsused,REG0,REG1,REG2,REG3,REG4);


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
    //printf ("IO state for digital input 1 (Power)(internal pin 30) is %d\n", powerstate);

    ignstate=get_gpio_value(31);
    //printf ("IO state for digital input 2 (Ignition)(internal pin 31) is %d\n", ignstate);

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
    
    // if the current and previous states are same then pass -1 else pass 0/1/2 as required

    if (ret!=prev_io_state)
    {
        return ret;
    }
    else
        return -1;
}



void update_info()
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
    
    strftime (buff, sizeof(buff), "%d-%m-%Y", tmp);
    snprintf(date, sizeof(date), "%s",buff);
    date[strlen(date)] = '\0';  

    
    // obtain gps info as lat,lon,alt

    printf("assigning lat\n");
    ret = get_gps_latitude (&lat);
    if (ret == 0)
    {
        printf (" get_gps_latitude %d \n", lat);
    }
    
    printf("assigning lat\n");
    ret = get_gps_longitude (&lon);		  
    if (ret == 0)
    {
        printf (" get_gps_longitude %d \n", lon);
    }

    printf("assigning lat\n");
    ret = get_gps_altitude (&alt);
    if (ret == 0)
    {
        printf (" altitude %d \n", alt);
    } 
    

    
    //ret =getgps_data_from_db(1,"/tmp/abc");

    // implement checks to validate the data


}

void send_power_loss()
{

    update_info();
    char pwr_command[1024];
    printf("sending power loss\n");

     // prepare the format of the powerloss message
    snprintf(pwr_command, sizeof(pwr_command), "$PWRL 0 %s,%s,%s,%f,%f,%d,,,,%d,,,,,,,,,,%s\r",imei,sendtime,date,lat,lon,fix,power,logger_id);

    //calling send tcp


    pthread_t thread_id = launch_thread_send_data((void*)pwr_command);
    pthread_join(thread_id,NULL);
}

void send_power_restore()
{
  
    update_info();

    char pwr_command[1024];

   

        // prepare the format of the power restore message
    snprintf(pwr_command, sizeof(pwr_command), "$PWRR 0 %s,%s,%s,%f,%f,%d,,,,%d,,,,,,,,,,%s\r",imei,sendtime,date,lat,lon,fix,power,logger_id);



    //calling send tcp



    pthread_t thread_id = launch_thread_send_data((void*)pwr_command);
    pthread_join(thread_id,NULL);
}

void send_ignition_off()
{
    update_info();
    char ign_command[1024];

    printf("sending ignition off\n");

            // prepare the format of the ignition OFF message
    snprintf(ign_command, sizeof(ign_command), "$IN8L 36 %s,%s,%s,%f,%f,%d,,,,%f,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\r",imei,sendtime,date,lat,lon,fix,alt,power,in7,dop,satsused,REG0,REG1,REG2,REG3,REG4);


    //calling send tcp


    pthread_t thread_id = launch_thread_send_data((void*)ign_command);
    pthread_join(thread_id,NULL);
}



void send_ignition_on()
{

    update_info();
    char ign_command[1024];
    printf("sending ignition on\n");

    // prepare the format of the Ignition ON message
    snprintf(ign_command, sizeof(ign_command), "$IN8H 36 %s,%s,%s,%f,%f,%d,,,,%f,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\r",imei,sendtime,date,lat,lon,fix,alt,power,in7,dop,satsused,REG0,REG1,REG2,REG3,REG4);

    //calling send tcp


    pthread_t thread_id = launch_thread_send_data((void*)ign_command);
    pthread_join(thread_id,NULL);
}

void send_heartbeat()
{
    update_info();

    char heartbeat_command[1024];
    printf("sending heartbeat\n");

    snprintf(heartbeat_command, sizeof(heartbeat_command)-1, "$HEA 36 \r");

    //calling send tcp


    pthread_t thread_id = launch_thread_send_data((void*)heartbeat_command);
    pthread_join(thread_id,NULL);
}

// function to send the sign on 
void ready_device()
{
 
    //for DEBUG purpose only
    //printf("The device has just rebooted \n");  
int ret;



    // to give info to the user on terminal  
    printf ("The script version %s is now running for device with imei=%s and the modbus configfile is %s \n", script_ver, imei, logger_id);



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
char  datatosend[] ="Application Stopped running \r";
    pthread_t thread_id = launch_thread_send_data((void*)datatosend);
    pthread_join(thread_id,NULL);
    //setting reboot to 1 will exit the worker loop
    reboot=1;

}


// MAIN PROGRAM CALL STARTS BELOW

int main (int argc, char *argv[])
{




    
    //get the loggerid - this will be from the CSV file uploaded to master_modbus
    ret = get_loggerid(logger_id); 


    //obtain imei number
    ret = get_imei (imei, 15);
    imei[strlen(imei)-1]='\0';



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
    // open a thread to read the data

    pthread_t tid;
    printf("Calling a thread to read data");
    ret = pthread_create(&tid, NULL, read_tcp_data,NULL);
    if (ret != 0) 
    { 
        printf("Error from pthread: %d\n", ret); 
    }




    // create a signal and assign the alarm handler
    struct sigaction sact;
    sigemptyset (&sact.sa_mask);
    sact.sa_flags = 0;
    sact.sa_handler = sigalrm_handler;
    sigaction(SIGALRM, &sact, NULL);
    
    //sends the login message
    ready_device();

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
    
    sleep(2);

    // check ignition and power
    // return -1 for power loss
    // return 0 for ignition off
    //  return 1 for ignition on
  



    switch(do_function)
    {

    case 1:
            printf("I got hit by a 1 and will check my IO lines now\n");
            //poll_ioline_state();
            do_function=0;
            break;

    case 2:
            printf("I got hit by a 2 and will send the periodic data on tcp now\n");          
            send_modbus_data();                     
            do_function=0;
            break;

    case 3:
            printf("I got hit by a 3 and this occurs every 12 hours and resets the time tracker counters to zeros\n");
            send_heartbeat();
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
            printf("I got hit by a 6 and this is power loss\n");
            send_power_loss();
            do_function=0;
            break;

    case 7:
            printf("I got hit by a 7 and this is power restore\n");
            send_power_restore();
            do_function=0;
            break;

    default:
            break;
    }


}
    while (!reboot);
    ret = shutdown(sockfd, SHUT_WR);
    printf (" %d is the return for shutdown \n",ret);
    ret= close(sockfd);
    printf (" %d is the return for close \n",ret);
   
    exit(0);
    return 0;}






 /*   
    ret = getgps_data_from_db (1, sql_buff);
    if (ret == 0)
    {
        printf ("printing gps data"); printf (sql_buff);
    }


*/





