#include <stdio.h>
#include <stdlib.h>

#include <unistd.h>
#include <sys/io.h>

#include "midas.h"

#include "crate.h"
#include "diag.h"
#include "odb_wrapper.h"

#define DEFINE_RPC_LIST
#include "rpc_mucap.h"

INT rpc_ready_for_cycle(INT index, void *prpc_param[]);
INT rpc_request_stop(INT index, void *prpc_param[]);
INT rpc_master_init();
INT rpc_master_pre_bor();
INT rpc_master_bor();
INT rpc_master_eor();
INT rpc_master_poll_live();
INT rpc_master_poll_dead();
INT rpc_master_read(char *pevent);

struct {
  BOOL enabled;
  BOOL synchronous;
  BOOL participating;
  BOOL ready[3]; // index indicates RAM cycle (index 0 not used) 
  HNDLE conn;
} crate[MAX_CRATES];

INT event_number;
INT request_stop_event;
static BOOL enable_rpc_master;

struct readout_module rpc_master_module = {
  rpc_master_init,        // init
  NULL,                   // exit
  rpc_master_pre_bor,     // pre_bor
  rpc_master_bor,         // bor
  rpc_master_eor,         // eor
  rpc_master_poll_live,   // poll_live
  rpc_master_poll_dead,   // poll_dead
  NULL,                   // start_block
  NULL,                   // stop_block
  rpc_master_read,        // read
};

/*
 * rpc_master_init
 *
 */
INT rpc_master_init()
{
  // Determine whether we ought to be the master
  enable_rpc_master = 
    odb_get_bool("/Equipment/Crate %d/Settings/Master", crate_number);
  if(!enable_rpc_master) {
    return SUCCESS;
  }

  // register the RPC function that we provide
  rpc_register_functions(rpc_list_mucap, NULL);
  rpc_register_function(RPC_READY_FOR_CYCLE, rpc_ready_for_cycle);
  rpc_register_function(RPC_REQUEST_STOP, rpc_request_stop);

  return SUCCESS;
}

/*
 * rpc_master_pre_bor()
 */
INT rpc_master_pre_bor()
{
  diag_print(1, "Clearing crate ready flags\n");

  for(int i = 0; i < MAX_CRATES; i++) {
    for(int j = 0; j <= 2; j++) {
      crate[i].ready[j] = FALSE;
    }
  }

  return SUCCESS;
}

/*
 * rpc_master_bor()
 */
INT rpc_master_bor()
{
  if(!enable_rpc_master) {
    return SUCCESS;
  }

  for(int i = 0; i < MAX_CRATES; i++) {
    if(i != crate_number) {
      bool enabled = false;
      if(odb_find_key("/Equipment/Crate %d", i) != NULL) {
        enabled = odb_get_bool("/Equipment/Crate %d/Settings/Enabled", i);
      }
      crate[i].enabled = enabled;


      if(enabled) { 
        crate[i].synchronous = 
          odb_get_bool("/Equipment/Crate %d/Settings/Synchronous", i);

        char crate_label[10];
        sprintf(crate_label, "Crate %d", i);
        cm_connect_client(crate_label, &crate[i].conn);
        rpc_set_option(crate[i].conn, RPC_OTRANSPORT, RPC_FTCP);
        rpc_set_option(crate[i].conn, RPC_NODELAY, TRUE);
      }
    }
  }

  event_number = 1;
  request_stop_event = 0;

  return SUCCESS;
}

/*
 *
 */
INT rpc_master_eor()
{
  if(!enable_rpc_master) {
    return SUCCESS;
  }

  for(int i = 0; i < MAX_CRATES; i++) {
    if(i != crate_number && crate[i].enabled) {
      cm_disconnect_client(crate[i].conn, 0);
    }
  }

  return SUCCESS;
}

/*
 *
 */
INT rpc_ready_for_cycle(INT index, void *prpc_param[])
{
  INT crate_number = CINT(0);
  INT ram_cycle = CINT(1);

  diag_print(2, "Crate %d is ready for a new cycle on ram=%d.\n",
             crate_number, ram_cycle);

  crate[crate_number].ready[ram_cycle] = TRUE;

#if 0
  if(ram_cycle == 0) {
    crate[crate_number].ready[1] = crate[crate_number].ready[2] = TRUE;
  } else {
    crate[crate_number].ready[ram_cycle] = TRUE;
  }
#endif

  return SUCCESS;
}

/*
 *
 */
INT rpc_request_stop(INT index, void *prpc_param[])
{
  INT crate_number = CINT(0);
  INT event_number_in = CINT(1);

  diag_print(2, "Crate %d requests end of block %d\n", 
             crate_number, event_number_in);

  if(event_number_in != event_number) {
    diag_print(0, 
      "Crate %d requests end of block %d--not the right event number (%d)\n", 
       crate_number, event_number_in, event_number);
    return SUCCESS;
  }

  request_stop_event = event_number_in;

  return SUCCESS;
}

/*
 * Returns whether a particular crate is participating in this cycle.
 */
BOOL crate_is_participating(INT crate_number)
{
  return crate[crate_number].participating;
}

/*
 * Returns the TDC400 RAM number (1 or 2) associated with the current 
 * event number.
 */
INT cycle_ram()
{
  return ((event_number + 1) % 2) + 1; 
}

INT rpc_master_read(char *pevent)
{
  if(!enable_rpc_master) {
    return SUCCESS;
  }

  int ram = cycle_ram();

  // Send a message to other crates that the cycle is now over
  for(int i = 0; i < MAX_CRATES; i++) {
    if(i != crate_number && crate[i].enabled) {
      if(crate[i].synchronous || crate[i].participating) {
        diag_print(2, "Sending RPC_END_OF_CYCLE to crate %d RAM %d event %d\n",
            i, ram, event_number);
        rpc_client_call(crate[i].conn, RPC_END_OF_CYCLE, ram, event_number);
      }
    }
   }
 
  // Increment event number
  event_number++;

  return SUCCESS;
}

INT rpc_master_poll_live()
{
  if(!enable_rpc_master) {
    return SUCCESS;
  }

  // Yield to receive any pending RPCs
  cm_yield(0);

  // Have we received a request to end the event?
  if(request_stop_event == event_number) {
    diag_print(2, "Stopping event.\n");
    return FE_NEED_STOP; 
  } 

  return SUCCESS;
}

INT rpc_master_poll_dead()
{
  if(!enable_rpc_master) {
    return SUCCESS;
  }

  // Yield to receive any pending RPCs
  cm_yield(0);

  // Have we received notice from each of the enabled crates that
  // it is ready to start? 

  BOOL ready_to_start = TRUE;

  INT ram = cycle_ram(); 

  for(int i = 0; i < MAX_CRATES; i++) {
    if(i != crate_number && crate[i].enabled && crate[i].synchronous && 
       !(crate[i].ready[ram] || crate[i].ready[0])) {
      ready_to_start = FALSE; 
      break;
    }
  }

  if(ready_to_start) {
    for(int i = 0; i < MAX_CRATES; i++) {
      crate[i].participating = (crate[i].ready[ram] || crate[i].ready[0]);

      crate[i].ready[ram] = FALSE;
      crate[i].ready[0] = FALSE;
    }

    return FE_NEED_START;
  } else {
    return SUCCESS;
  }
}

