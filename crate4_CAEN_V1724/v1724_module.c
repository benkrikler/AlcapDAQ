#include <stdio.h>
#include <stdlib.h>

#include <unistd.h>
#include <sys/io.h>
#include <sys/time.h> 
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>

#include "midas.h"

#include "crate.h"
#include "diag.h"

// CAEN includes
#include "CAENDigitizer.h"

#define NBOARDS    1             /* Number of connected boards */
static int handle[NBOARDS];
static uint32_t V1724_BASE[NBOARDS] = {0x32100000};
static char      *caen_data_buffer[NBOARDS];         // data buffers used by CAEN
static uint32_t  caen_data_buffer_size[NBOARDS];
static char      *data_buffer[NBOARDS];              // data buffers used by MIDAS
static uint32_t  data_buffer_size[NBOARDS];
static uint32_t  data_size[NBOARDS];



static INT v1724_init();
static void v1724_exit();
static INT v1724_bor();
static INT v1724_eor();
static INT v1724_poll_live();
static INT v1724_read(char *pevent); // MIDAS readout routine 
static void v1724_readout(); // read data from digitizer buffers

bool block_sig;
typedef struct timespec timer_start;

struct readout_module v1724_module = {
  v1724_init,             // init
  v1724_exit,             // exit
  NULL,                   // pre_bor
  v1724_bor,              // bor
  v1724_eor,              // eor
  v1724_poll_live,        // poll_live
  NULL,                   // poll_dead
  NULL,                   // start_block
  NULL,                   // stop_block
  v1724_read              // read
};

// ======================================================================
// ODB structures
// ======================================================================

// board settings
typedef struct s_v1724_odb_board 
{ 
  DWORD vme_base;                     ///< VME base address of the module
  BYTE  enabled;                      ///< don't readout the module if false    
  DWORD wf_length;                    ///< waveform length, samples
} S_V1724_ODB_BOARD;

// board settings
typedef struct s_v1724_odb_board_info
{
  DWORD rev_nr;                      ///< revision number
} S_V1724_ODB_BOARD_INFO;

#define V1724_BOARD_INFO_STR "\
[.]\n\
revision = WORD : 0\n\
"


//channel settings
typedef struct s_v1724_odb_channel
{ 
  BYTE  enabled;                      ///< don't readout the module if false    
  DWORD wf_length;                    ///< waveform length, samples
  DWORD trig_thr;                     ///< trigger threshold
  DWORD dac;                          ///< DC offset
  BYTE  polarity;                     ///< Pulse polarity (0-positive, 1-negative)
} S_V1724_ODB_CHANNEL;

#define V1724_ODB_CHANNEL_STR "\
enabled = BYTE : 0x0\n\
wf length = DWORD : 0x20\n\
Trigger Threshold = DWORD : 0x4000\n\
DC offset =  DWORD : 0x0\n\
polarity = BYTE : 0x0\n\
"

INT v1724_init()
{
  printf("Opening CAEN VME interface ...");
  fflush(stdout);

  CAEN_DGTZ_ErrorCode ret;
  CAEN_DGTZ_BoardInfo_t BoardInfo[NBOARDS];
  uint32_t data;

  for (int iboard=0; iboard<NBOARDS; iboard++)
    {
      ret = CAEN_DGTZ_OpenDigitizer(CAEN_DGTZ_USB,0,0,V1724_BASE[iboard],&handle[iboard]);
      if(ret != CAEN_DGTZ_Success) 
	{
	  cm_msg(MERROR,"v1724_init","Can't open digitizer at address 0x%08x\n",V1724_BASE[iboard]);
	  return FE_ERR_HW;
	}

      ret = CAEN_DGTZ_GetInfo(handle[iboard], &(BoardInfo[iboard]));
      if ( ret != CAEN_DGTZ_Success )
	{
	  cm_msg(MERROR,"v1724_init","Cannot get board info. Error 0x%08x\n",ret);
	  return FE_ERR_HW;
	}
      printf("\nConnected to CAEN Digitizer Model %s, recognized as board %d\n", BoardInfo[iboard].ModelName, iboard);
      printf("\tROC FPGA Release is %s\n", BoardInfo[iboard].ROC_FirmwareRel);
      printf("\tAMC FPGA Release is %s\n", BoardInfo[iboard].AMC_FirmwareRel);
      
      ret = CAEN_DGTZ_Reset(handle[iboard]);                                               /* Reset Digitizer */
      if ( ret != CAEN_DGTZ_Success )
	{
	  cm_msg(MERROR,"v1724_init","Cannot reset the board. Error 0x%08x\n",ret);
	  return FE_ERR_HW;
	}

      ret = CAEN_DGTZ_GetInfo(handle[iboard], &(BoardInfo[iboard]));
      if ( ret != CAEN_DGTZ_Success )
	{
	  cm_msg(MERROR,"v1724_init","Cannot get board info. Error 0x%08x\n",ret);
	  return FE_ERR_HW;
	}
 
      // =====================================================================================
      // Enable channels
      // =====================================================================================
      ret = CAEN_DGTZ_SetChannelEnableMask(handle[iboard],0xFF);  // enable all channels
      if ( ret != CAEN_DGTZ_Success )
	{
	  cm_msg(MERROR,"v1724_init","Cannot Enable channels. Error 0x%08x\n",ret);
	  return FE_ERR_HW;
	}

      // =====================================================================================
      // Acquisition control
      // =====================================================================================
      ret = CAEN_DGTZ_SetAcquisitionMode(handle[iboard],CAEN_DGTZ_S_IN_CONTROLLED); 
      if ( ret != CAEN_DGTZ_Success )
	{
	  cm_msg(MERROR,"v1724_init","Cannot configure Acquisition Control. Error 0x%08x\n",ret);
	  return FE_ERR_HW;
	}

      // =====================================================================================
      // Setup 1024 data buffers (max.)
      // =====================================================================================
      ret = CAEN_DGTZ_WriteRegister(handle[iboard], 0x800C, 0xA);
      if ( ret != CAEN_DGTZ_Success )
	{
	  cm_msg(MERROR,"v1724_init","Cannot configure data buffers. Error 0x%08x\n",ret);
	  return FE_ERR_HW;
	}


      // =====================================================================================
      // Configure max. number of buffers for BLT readout
      // =====================================================================================
      ret = CAEN_DGTZ_SetMaxNumEventsBLT(handle[iboard],1024); 
      if ( ret != CAEN_DGTZ_Success )
	{
	  cm_msg(MERROR,"v1724_init","Cannot configure data buffers for BLT readout. Error 0x%08x\n",ret);
	  return FE_ERR_HW;
	}
    
      // =====================================================================================
      // Record length
      // =====================================================================================
      ret = CAEN_DGTZ_SetRecordLength(handle[iboard], 256);
      if ( ret != CAEN_DGTZ_Success )
	{
	  cm_msg(MERROR,"v1724_init","Cannot SetRecordLength. Error 0x%08x\n",ret);
	  return FE_ERR_HW;
	}

      // =====================================================================================
      // Trigger mode
      // =====================================================================================
      ret = CAEN_DGTZ_SetChannelSelfTrigger(handle[iboard], CAEN_DGTZ_TRGMODE_ACQ_ONLY, 0xFF); // all channels can trigger
      //ret = CAEN_DGTZ_SetChannelSelfTrigger(handle[iboard],CAEN_DGTZ_TRGMODE_ACQ_AND_EXTOUT, 0xFF); // all channels can trigger
      if ( ret != CAEN_DGTZ_Success )
	{
	  cm_msg(MERROR,"v1724_init","Cannot SetCahnngelSelfTrigger. Error 0x%08x\n",ret);
	  return FE_ERR_HW;
	}

      // =====================================================================================
      // Trigger threshold
      // =====================================================================================
      for (uint32_t ichan=0; ichan<8; ichan++)
	{
	  ret = CAEN_DGTZ_SetChannelTriggerThreshold(handle[iboard], ichan, 15000);
	  if ( ret != CAEN_DGTZ_Success )
	    {
	      cm_msg(MERROR,"v1724_init","Cannot SetChannelTriggerThreshold. Error 0x%08x\n",ret);
	      return FE_ERR_HW;
	    }
	}
 

      // =====================================================================================
      // DC offset
      // =====================================================================================
      for (uint32_t ichan=0; ichan<8; ichan++)
	{
	  ret = CAEN_DGTZ_SetChannelDCOffset(handle[iboard], ichan, 450);
	  if ( ret != CAEN_DGTZ_Success )
	    {
	      cm_msg(MERROR,"v1724_init","Cannot SetChannelDCOffset. Error 0x%08x\n",ret);
	      return FE_ERR_HW;
	    }
	}
      
      // =====================================================================================
      // Pulse polarity
      // =====================================================================================
      for (uint32_t ichan=0; ichan<8; ichan++)
	{
	  //ret = CAEN_DGTZ_SetChannelPulsePolarity(handle[iboard], ichan, CAEN_DGTZ_PulsePolarityPositive);
	  ret = CAEN_DGTZ_SetChannelPulsePolarity(handle[iboard], ichan, CAEN_DGTZ_PulsePolarityNegative);
	  if ( ret != CAEN_DGTZ_Success )
	    {
	      cm_msg(MERROR,"v1724_init","Cannot SetChannelPulsePolarity. Error 0x%08x\n",ret);
	      return FE_ERR_HW;
	    }
	}
      
#if 0
      // =====================================================================================
      // Set Zero suppresion parameters
      // =====================================================================================
      for (uint32_t ichan=0; ichan<8; ichan++)
	{
	  ret = CAEN_DGTZ_SetChannelZSParams(handle[iboard], ichan, CAEN_DGTZ_ZS_FINE, 20, 4);
	  if ( ret != CAEN_DGTZ_Success )
	    {
	      cm_msg(MERROR,"v1724_init","Cannot SetChannelZSParams. Error 0x%08x\n",ret);
	      return FE_ERR_HW;
	    }
	}


      // =====================================================================================
      // Zero suppresion mode
      // =====================================================================================
      ret = CAEN_DGTZ_SetZeroSuppressionMode(handle[iboard], CAEN_DGTZ_ZS_ZLE);  
      if ( ret != CAEN_DGTZ_Success )
	{
	  cm_msg(MERROR,"v1724_init","Cannot SetZeroSuppressionMode. Error 0x%08x\n",ret);
	  return FE_ERR_HW;
	}
#endif
     

      // =====================================================================================
      // Allocate memory buffer to hold data received from digitizer
      // =====================================================================================
      caen_data_buffer[iboard] = NULL;
      ret = CAEN_DGTZ_MallocReadoutBuffer(handle[iboard], &(caen_data_buffer[iboard]), &(caen_data_buffer_size[iboard]));  
      if ( ret != CAEN_DGTZ_Success )
	{
	  cm_msg(MERROR,"v1724_init","Cannot MallocReadoutBuffer. Error 0x%08x\n",ret);
	  return FE_ERR_HW;
	}
      printf("Readout buffer size: %i\n",caen_data_buffer_size[iboard]);


      // =====================================================================================
      // Allocate memory buffer to hold all data from digitizer
      // =====================================================================================
      data_buffer_size[iboard] = 0x2000000; // 32 MB
      data_buffer[iboard] = (char*)malloc(data_buffer_size[iboard]);
      if ( !data_buffer[iboard] )
	{
	  cm_msg(MERROR,"v1724_init","Cannot allocate memory for data buffers.\n");
	  return FE_ERR_HW;
	}


    }

  printf("  [done]\n");

  return SUCCESS;
}

void v1724_exit()
{

  CAEN_DGTZ_ErrorCode ret;
  for (int iboard=0; iboard<NBOARDS; iboard++)
    {

      // =====================================================================================
      // Free memory allocated for data buffer
      // =====================================================================================
      ret = CAEN_DGTZ_FreeReadoutBuffer(&(caen_data_buffer[iboard]));  
      if ( ret != CAEN_DGTZ_Success )
	{
	  cm_msg(MERROR,"v1724_exit","Cannot FreeReadoutBuffer. Error 0x%08x\n",ret);
	}

      // =====================================================================================
      // Close digitizer
      // =====================================================================================
      ret = CAEN_DGTZ_CloseDigitizer(handle[iboard]);
      if ( ret != CAEN_DGTZ_Success )
	{
	  cm_msg(MERROR,"v1724_exit","Cannot CloseDigitizer. Error 0x%08x\n",ret);
	}

      // =====================================================================================
      // Free memory buffers
      // =====================================================================================
      free( data_buffer[iboard] );      
    }
}

INT v1724_bor()
{

  CAEN_DGTZ_ErrorCode ret;
  for (int iboard=0; iboard<NBOARDS; iboard++)
    { 

      // =====================================================================================
      // Clear digitizer data buffers
      // =====================================================================================
      ret = CAEN_DGTZ_ClearData( handle[iboard] );
      if ( ret != CAEN_DGTZ_Success )
	{
	  cm_msg(MERROR,"v1724_init","Cannot clear CAEN data buffers. Error 0x%08x\n",ret);
	  return FE_ERR_HW;
	}

      // =====================================================================================
      // Enable acquisition
      // =====================================================================================
      ret = CAEN_DGTZ_SWStartAcquisition(handle[iboard]);
      if ( ret != CAEN_DGTZ_Success )
	{
	  cm_msg(MERROR,"v1724_init","Cannot CAEN_DGTZ_SWStartAcquisition. Error 0x%08x\n",ret);
	  return FE_ERR_HW;
	}

      data_size[iboard] = 0;
    }

  return SUCCESS;
}

INT v1724_eor()
{

  CAEN_DGTZ_ErrorCode ret;
  for (int iboard=0; iboard<NBOARDS; iboard++)
    { 
      // =====================================================================================
      // Disable acquisition
      // =====================================================================================
      ret =  CAEN_DGTZ_SWStopAcquisition(handle[iboard]);
      if ( ret != CAEN_DGTZ_Success )
	{
	  cm_msg(MERROR,"v1724_init","Cannot CAEN_DGTZ_SWStopAcquisition. Error 0x%08x\n",ret);
	}
    }

  return SUCCESS;
}

INT v1724_read(char *pevent)
{
  printf("Read out data from digitizer(s)\n");

  // =====================================================================================
  // Read out remaining data from the digitizer
  // =====================================================================================
  v1724_readout();

  // =====================================================================================
  // Fill MIDAS event
  // =====================================================================================
  bk_init32(pevent);
  char bk_name[80];
  char *pdata;
  for (int iboard=0; iboard<NBOARDS; iboard++)
    {
      sprintf(bk_name, "CDG%i", iboard);
      bk_create(pevent, bk_name, TID_BYTE, &pdata);
      if ( data_size[iboard] > MAX_EVENT_SIZE )
	{
	  cm_msg(MERROR,"v1724_read","Event size is too large. Truncating data...\n");
	  data_size[iboard] = MAX_EVENT_SIZE;
	}
      memcpy(pdata, data_buffer[iboard], data_size[iboard]);
      pdata += data_size[iboard];
      bk_close(pevent, pdata);
      // reset data couner for the next event
      data_size[iboard] = 0;
    }

  return SUCCESS;
}

void v1724_readout()
{

  CAEN_DGTZ_ErrorCode ret;  
  for (int iboard=0; iboard<NBOARDS; iboard++)
    {
      uint32_t rdata;
      ret = CAEN_DGTZ_ReadRegister(handle[iboard], 0x812C, &rdata);
      //printf("Event Stored: 0x%08x ret = %i\n",rdata, ret);
      if ( ret != CAEN_DGTZ_Success )
	{
	  cm_msg(MERROR,"v1724_readout","Cannot read register 0x812C. Error 0x%08x\n",ret);
	}
      
      //ret = CAEN_DGTZ_ReadRegister(handle[iboard], 0x814C, &rdata);
      //printf("Event size: 0x%08x ret = %i\n",rdata, ret);

      if ( rdata > 0 )
	{
	  // =====================================================================================
	  // Read data
	  // =====================================================================================
	  uint32_t caen_data_size;
	  ret = CAEN_DGTZ_ReadData(handle[iboard], CAEN_DGTZ_SLAVE_TERMINATED_READOUT_MBLT, caen_data_buffer[iboard], &caen_data_size);
	  if ( ret != CAEN_DGTZ_Success )
	    {
	      cm_msg(MERROR,"v1724_readout","Cannot DGTZ_ReadData. Error 0x%08x\n",ret);
	    }
	  printf("data size: %i\n", caen_data_size);

#if 0
	  uint32_t numEvents;
	  CAEN_DGTZ_GetNumEvents(handle[iboard],caen_data_buffer[iboard],caen_data_size,&numEvents);
	  printf("numEvents: %i\n", numEvents);
#endif 
	  
	  if ( data_size[iboard] + caen_data_size < data_buffer_size[iboard] )
	    {
	      memcpy( (data_buffer[iboard] + data_size[iboard]), caen_data_buffer[iboard], caen_data_size);
	      data_size[iboard] += caen_data_size;
	    }
	}
    }
  
}

INT v1724_poll_live()
{

  v1724_readout();

  return SUCCESS;
}