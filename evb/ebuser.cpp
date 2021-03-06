/********************************************************************\

  Name:         ebuser.c
  Created by:   Pierre-Andre Amaudruz

  Contents:     User section for the Event builder

  ** muCap version **

  $Log: ebuser.cpp,v $
  Revision 1.4  2006/01/25 02:03:27  mucap
  (Fred)
  Commented out redundant printf().

  Revision 1.3  2005/10/31 14:11:37  mucap
  (Fred)
  Update to new major version of MIDAS event builder.

  Revision 1.11  2004/10/07 20:08:34  pierre
  1.9.5

  Revision 1.10  2004/10/04 23:55:57  pierre
  move ebuilder into equipment list

  Revision 1.9  2004/09/29 16:25:04  pierre
  change Ebuilder structure

  Revision 1.8  2004/01/08 06:46:43  pierre
  Doxygen the file

  Revision 1.7  2003/12/03 00:57:20  pierre
  ctlM fix

  Revision 1.6  2002/09/28 00:49:08  pierre
  Add EB_USER_ERROR example

  Revision 1.5  2002/09/25 18:38:03  pierre
  correct: header passing, user field, abort run

  Revision 1.4  2002/07/13 05:46:10  pierre
  added ybos comments

  Revision 1.3  2002/06/14 04:59:46  pierre
  revised for ybos

  Revision 1.2  2002/01/17 23:34:29  pierre
  doc++ format

  Revision 1.1.1.1  2002/01/17 19:49:54  pierre
  Initial Version

\********************************************************************/
/** @file ebuser.c
The Event builder user file
*/

#include <stdio.h>
#include "midas.h"
#include "mevb.h"
#include "ybos.h"

#include "mucap_compress.h"
#define NUMCRATES 5

/*-- Globals -------------------------------------------------------*/

/* The frontend name (client name) as seen by other MIDAS clients   */
char *frontend_name = "Ebuilder";

/* The frontend file name, don't change it */
char *frontend_file_name = __FILE__;

/* frontend_loop is called periodically if this variable is TRUE    */
BOOL ebuilder_call_loop = FALSE;

/* A frontend status page is displayed with this frequency in ms */
INT display_period = 3000;

/* maximum event size produced by this frontend */
INT max_event_size = MAX_EVENT_SIZE;

/* maximum event size for fragmented events (EQ_FRAGMENTED) */
INT max_event_size_frag = MAX_EVENT_SIZE;

/* buffer size to hold events */
INT event_buffer_size = EVENT_BUFFER_SIZE;

/**
Globals */
INT lModulo = 100;              ///< Global var for testing

/*-- Function declarations -----------------------------------------*/
INT ebuilder_init();
INT ebuilder_exit();
INT eb_begin_of_run(INT, char *, char *);
INT eb_end_of_run(INT, char *);
INT ebuilder_loop();
INT ebuser(INT, BOOL mismatch, EBUILDER_CHANNEL *, EVENT_HEADER *, void *, INT *);
INT read_scaler_event(char *pevent, INT off);

/*-- Equipment list ------------------------------------------------*/
EQUIPMENT equipment[] = {
   {"EB",                /* equipment name */
    {1, 0,                   /* event ID, trigger mask */
     "SYSTEM",               /* event buffer */
     0,                      /* equipment type */
     0,                      /* event source */
     "MIDAS",                /* format */
     TRUE,                   /* enabled */
     },
    },

  {""}
};

/********************************************************************/
/********************************************************************/

/********************************************************************/
INT ebuilder_init()
{
  return EB_SUCCESS;
}

/********************************************************************/
INT ebuilder_exit()
{
  return EB_SUCCESS;
}

/********************************************************************/
INT ebuilder_loop()
{
  return EB_SUCCESS;
}

/********************************************************************/
/**
Hook to the event builder task at PreStart transition.
@param rn run number
@param UserField argument from /Ebuilder/Settings
@param error error string to be passed back to the system.
@return EB_SUCCESS
*/
INT eb_begin_of_run(INT rn, char *UserField, char *error)
{
#if 0
  extern EBUILDER_SETTINGS ebset;
  extern EBUILDER_CHANNEL  ebch[MAX_CHANNELS];
  extern HNDLE hDB;
    
  int n;
  int mask = 0;
  
  char keyName[256]; 
  BOOL enabled;
  int size;
  
  /* 
   * Determine which crates are enabled and set up the mask 
   * accordingly.
   */
  for(n = 1; n <= NUMCRATES; n++) {
    sprintf(keyName, "/Equipment/Crate %d/Settings/Enabled", n);
    size = sizeof(enabled);
    db_get_value(hDB, 0, keyName, &enabled, &size, TID_BOOL, FALSE);
    if(size == sizeof(enabled) && enabled) {
       ebch[n-1].trigger_mask = (1 << (n-1));
       mask |= ebch[n-1].trigger_mask;
    } else {
       ebch[n-1].trigger_mask = 0;
    }
  }

  cm_msg(MINFO, "eb_begin_of_run", "Setting event mask to 0x%x", mask);
  ebset.trigger_mask = mask;
#endif

  // Initialize online compression
  compress_load_all();

  return EB_SUCCESS;
}

/********************************************************************/
/**
Hook to the event builder task at completion of event collection after
receiving the Stop transition.
@param rn run number
@param error error string to be passed back to the system.
@return EB_SUCCESS
*/
INT eb_end_of_run(INT rn, char *error)
{
   return EB_SUCCESS;
}

/********************************************************************/
/**
Hook to the event builder task after the reception of
all fragments of the same serial number. The destination
event has already the final EVENT_HEADER setup with
the data size set to 0. It is than possible to
add private data at this point using the proper
bank calls.

The ebch[] array structure points to nfragment channel structure
with the following content:
\code
typedef struct {
    char  name[32];         // Fragment name (Buffer name).
    DWORD serial;           // Serial fragment number.
    char *pfragment;        // Pointer to fragment (EVENT_HEADER *)
    ...
} EBUILDER_CHANNEL;
\endcode

The correct code for including your own MIDAS bank is shown below where
\b TID_xxx is one of the valid Bank type starting with \b TID_ for
midas format or \b xxx_BKTYPE for Ybos data format.
\b bank_name is a 4 character descriptor.
\b pdata has to be declared accordingly with the bank type.
Refers to the ebuser.c source code for further description.

<strong>
It is not possible to mix within the same destination event different event format!
</strong>

\code
  // Event is empty, fill it with BANK_HEADER
  // If you need to add your own bank at this stage

  bk_init(pevent);
  bk_create(pevent, bank_name, TID_xxxx, &pdata);
  *pdata++ = ...;
  *dest_size = bk_close(pevent, pdata);
  pheader->data_size = *dest_size + sizeof(EVENT_HEADER);
\endcode

For YBOS format, use the following example.

\code
  ybk_init(pevent);
  ybk_create(pevent, "EBBK", I4_BKTYPE, &pdata);
  *pdata++ = 0x12345678;
  *pdata++ = 0x87654321;
  *dest_size = ybk_close(pevent, pdata);
  *dest_size *= 4;
  pheader->data_size = *dest_size + sizeof(YBOS_BANK_HEADER);
\endcode
@param nfrag Number of fragment.
@param mismatch Midas Serial number mismatch flag.
@param ebch  Structure to all the fragments.
@param pheader Destination pointer to the header.
@param pevent Destination pointer to the bank header.
@param dest_size Destination event size in bytes.
@return EB_SUCCESS
*/
INT eb_user(INT nfrag, BOOL mismatch, EBUILDER_CHANNEL * ebch
            , EVENT_HEADER * pheader, void *pevent, INT * dest_size)
{
  if (mismatch){
    printf("Serial number do not match across fragments\n");
    for (int i = 0; i < nfrag; i++) {
      int serial = ((EVENT_HEADER *) ebch[i].pfragment)->serial_number;
      printf("Ser[%i]:%d ", i + 1, serial);
    }
    printf("\n");
    return EB_USER_ERROR;
  }

  // Initialize output event
  bk_init32(pevent);

  // Loop over the event fragments, performing compression into the output event
  for(int i = 0; i < nfrag; i++) {
    void *fragment =  ebch[i].pfragment;

    if(fragment != NULL) {
      compress_event(((EVENT_HEADER *) fragment) + 1, pevent);
      pheader->serial_number =
        ((EVENT_HEADER *) ebch[i].pfragment)->serial_number;
    }
  }

  // Set the size of the output event properly
  pheader->data_size = *dest_size = bk_size(pevent);

  // printf("Returning size %d\n", pheader->data_size);

  return EB_SUCCESS;

}
