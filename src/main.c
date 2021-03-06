#include <stdint.h>
#include <pthread.h>
#include <ti/drivers/GPIO.h>
#include <ti/drivers/PWM.h>
#include <ti/sysbios/BIOS.h>

#include "debug.h"

#include "global.h"
#include "communicatie.h"
#include "noodstop.h"
#include "MPPT.h"
#include "spi.h"

struct mainTreads {
  pthread_t comm;
  pthread_t noodstop;
  pthread_t mppt;
  pthread_t sysBeheer;
} treads;

void * mainTask(void *arg);

//TODO: maybe detach pthread?
//TODO: add stacksize option
pthread_t createSimplePTread(int prio, void * fn){
  struct sched_param priParam;
  priParam.sched_priority = prio;

  pthread_attr_t pAttrs;
  pthread_attr_init(&pAttrs);
  pthread_attr_setschedparam(&pAttrs, &priParam);

  pthread_t thread;
  if( pthread_create(&thread, &pAttrs, fn, NULL) != 0 ){
    ERROR("could not create thread")
    return 0;
  }

  return thread;
}

void startInit(){
  treads.comm = createSimplePTread(3, &comm_init);
  treads.noodstop = createSimplePTread(1, &noodstop_init);
  treads.mppt = createSimplePTread(1, &mppt_init);
  //TODO: add systeembeheer
}
void startSys(){
  treads.mppt = createSimplePTread(2, &mppt_start);
  treads.noodstop = createSimplePTread(4, &noodstop_start);
}
void stopSys(){
  //TODO: communication say what the reason for the emergency stop was if thare was one
  // pthread_exit(treads.comm);
  pthread_exit(treads.noodstop);
  pthread_exit(treads.mppt);

  treads.mppt = createSimplePTread(1, &mppt_deinit);
}

void initISR(){
  if(Status == WORKING || Status == INIT)
    return;
  Status = INIT;
  treads.sysBeheer = createSimplePTread(1, mainTask);
}
void startSysISR(){
  if(GPIO_read(CONFIG_GPIO_START)){
    if(Status != ALL_READY){
      Status = EXT_NOODSTOP;
      noodstop_activeerNoodstop();
      stopSys();
      return;
    }else{
      Status = WORKING;
    }
  }else{
    Status = SLEEP;
  }
}

void * mainTask(void *arg){
  Status_t lastState;
  while(1){
    INFO("system state changed from %s to %s", lastState, Status);
    switch (Status){
      case SLEEP:
        stopSys();
        break;
      case INIT:
        startInit();
        break;
      case ALL_READY:
        //TODO: say the system is ready
        break;
      case WORKING:
        startSys();
        break;
      case OVERHEAD:
      case OVERLOAD:
      case EXT_NOODSTOP:
        stopSys();
      case OVERSPEED: // don't stop on overload first wait for speed to go down
        return;
    }
    lastState = Status;
    while(Status == lastState)
      usleep(10);
  }
}

int main(void){
  Status = SLEEP;
  Board_init(); // initialise board
  GPIO_init();
  SPI_Init();
  PWM_init();
  //TODO: initilize UART
  BIOS_start(); // start the BIOS
  while(1)
    sleep(10);
}
