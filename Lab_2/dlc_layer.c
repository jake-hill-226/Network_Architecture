/* ----------------------------------------------------
 * Name			: Jake Hill
 * e-mail		: hill.1957@osu.edu
 * CSE account	: hillja
 * ---------------------------------------------------- */

 /*
  CSE 3462
  Lab 2
  
  Program for use in network simulator.
  Implementation of go-back-n arq network protocol
  without setting p bit when sender's timer expires waiting for
  an RR frame response.
 
 */

/* --- DO NOT REMOVE OR MODIFY  #inclode STATEMENTS BELOW --- */

#include "cisePort.h"
#include "sim.h"
#include "component.h"
#include "comptypes.h"
#include "list.h"
#include "eventdefs.h"
#include "main.h"
#include "route_activity.h"
#include "sim_tk.h"
#include "dlc_layer.h"

/****************************************************/
/* --- Check to see if there is and A_PDU in transmission Buffer --- */
static int
window_open(DLC_Conn_Info_TYPE *dci)
{
 int result;
   /* Based on a number of a_pdu's in the transmission buffer 
      and values for snd_nxt, snd_una and window_size    
      determine if there is a new a_pdu ready to be sent */
 result = dci->snd_nxt;
 if(result < dci->snd_una){
  result += dci->snd_una;
 } else{
  result -= dci->snd_una;
 }
 if(result < 0){ 
  result *= -1;
 }
 int window_size = dci->window_size;
 if( window_size != result && DataInPDUBuffer(dci) > 0 && result != DataInPDUBuffer(dci)){
  result = 1;
 }else {
  result = 0;
 }

 return result; // result = 1, there is an a_pdu ready
                // result = 0, there is not
}

/**************************************************************/
/* --- DO NOT REMOVE OR MODIFY THIS FUNCTION --- */
static 
dlc_layer_receive(DLC_LAYER_ENTITY_TYPE *dlc_layer_entity,
		  GENERIC_LAYER_ENTITY_TYPE *generic_layer_entity,
		  PDU_TYPE *pdu)
{
	/* Gets the appropriate  DLC_Conn_Info_TYPE structure */
	DLC_Conn_Info_TYPE *dci;
	dci = Datalink_Get_Conn_Info(dlc_layer_entity,pdu);
      
	if (DatalinkFromApplication(generic_layer_entity)) {
	    FromApplicationToDatalink(dlc_layer_entity, pdu, dci);
	} else if (DatalinkFromPhysical(generic_layer_entity)) {
	    FromPhysicalToDatalink(dlc_layer_entity, pdu, dci);
	}
	return 0;
}

/**************************************************************/
/* --- YOU MUST HAVE THIS FUNCTION --- */
static
FromApplicationToDatalink(DLC_LAYER_ENTITY_TYPE *dlc_layer_entity, PDU_TYPE *pdu_from_application,
				   DLC_Conn_Info_TYPE *dci)
{  
  /* Insert the pdu from the applicaiton layer to the 
     transmission buffer. Use:
     InsertPDUIntoBuffer(dlc_layer_entity, pdu, dci); */
    InsertPDUIntoBuffer(dlc_layer_entity, pdu_from_application, dci);
  
  
  /* If possible send info frame. You may use: 
     SendInfo(dlc_layer_entity, dci); */
     
    if(window_open(dci)){
      SendInfo(dlc_layer_entity, dci);
    }
     
     
}

/**************************************************************/
/* --- Sending Frames from Physical Link to Datalink --- */
/* --- YOU MUST HAVE THIS FUNCTION --- */
static
FromPhysicalToDatalink(DLC_LAYER_ENTITY_TYPE *dlc_layer_entity,
					  PDU_TYPE *pdu_from_physical,
					  DLC_Conn_Info_TYPE *dci)
{

	/* Check and discard the pdu when error is detected or P/F 
        bit is set */
  if(pdu_from_physical->u.d_pdu.p_bit == 1){
    return 0;
  }
  if(pdu_from_physical->u.d_pdu.error){
    return 0;
  }
	
	/* If not discarded, check d_pdu.type and call an 
        appropriate function: 
	   if RR you may use and call DataLinkProcessRR()
	   if REJ you may use and call DataLinkProcessREJ()
	   if INFO you may use and call DataLinkProcessINFO() */
  switch(pdu_from_physical->u.d_pdu.type){
    case D_INFO :
      DatalinkProcessInfo(dlc_layer_entity, pdu_from_physical, dci);
      break;
    case D_RR :
      DatalinkProcessRR(dlc_layer_entity, pdu_from_physical, dci);
      break;
    case D_REJ :
      DatalinkProcessREJ(dlc_layer_entity, pdu_from_physical, dci);
      break;
  }
}


/********************************************************************/
/* --- Datalink Processing Functions for Info, RR, and REJ Frames --- */


static
DatalinkProcessRR(DLC_LAYER_ENTITY_TYPE *dlc_layer_entity,
				  PDU_TYPE *pdu,
				  DLC_Conn_Info_TYPE *dci)
{
     /* Check the address and if not correct discard frame and
         return 0;   */
  if( pdu->u.d_pdu.address != GetNodeID(dlc_layer_entity)){
    return 0;
  }
     /* Otherwise:  
        Free up space in the retransmission buffer. Use:
	   UpdatePDUBuffer(dlc_layer_entity,pdu,dci);  */
  UpdatePDUBuffer(dlc_layer_entity, pdu, dci);
	/* update snd_una  */
	dci->snd_una = pdu->u.d_pdu.number;
	
	/* Send as many info pdu's as allowed by window. You may 
        use: 
        window_open(dci)and SendInfo(dlc_layer_entity, dci); */
  while(window_open(dci)){
    SendInfo(dlc_layer_entity, dci);
  }
	/* Free pdu */
	pdu_free(pdu);
  
  printf("\n");
	return 0;
}


static
DatalinkProcessREJ(DLC_LAYER_ENTITY_TYPE *dlc_layer_entity,
				   PDU_TYPE *pdu,
				   DLC_Conn_Info_TYPE *dci)
{
     /* Check the address and if not correct discard frame and
         return 0;   */
     /* Otherwise, free up space in the retransmission buffer */
  if(pdu->u.d_pdu.address != GetNodeID(dlc_layer_entity)){
    return 0;
  }
  UpdatePDUBuffer(dlc_layer_entity, pdu, dci);  
     /* update snd_una and snd_nxt */
  dci->snd_una = pdu->u.d_pdu.number;
  dci->snd_nxt = dci->snd_una;
     /* Send as many pdu's as allowed by window */
  while(window_open(dci)){
    SendInfo(dlc_layer_entity, dci);
  }
	/* Free pdu */
  pdu_free(pdu);
    return 0;
}


static
DatalinkProcessInfo(DLC_LAYER_ENTITY_TYPE *dlc_layer_entity,
					PDU_TYPE *pdu,
					DLC_Conn_Info_TYPE *dci)
{
	PDU_TYPE *pdu_to_application;
  
  
     /* Check the address and if not correct discard frame and
         return 0;   */
         
  if(pdu->u.d_pdu.address != GetNodeID(dlc_layer_entity)){
    return 0;
  }
     /* Check if the pdu has the expected sequence number
        When out of sequence, then send REJ, discard pdu and 
        return 0 */
     /* You may use SendREJ() to send REJ*/ 
	if(pdu->u.d_pdu.number != dci->rcv_nxt){
	  SendREJ(dlc_layer_entity, pdu, dci);
	  return 0;
	}
	
	/* If expected PDU, then increment rcv_nxt,
        set rej_already_sent = 0 and RR is sent */
	/* You may use SendRR() to send RR */
	dci->rcv_nxt = (dci->rcv_nxt + 1) % MODULO;
	dci->rej_already_sent = 0;
	SendRR(dlc_layer_entity, pdu, dci);
	
	/* --- Send pdu to application : Same as Lab1 --- */
	pdu_to_application = pdu_alloc();
	pdu_to_application->type = TYPE_IS_A_PDU;
	pdu_to_application->u.a_pdu = pdu->u.d_pdu.a_pdu;
	send_pdu_to_application_layer( dlc_layer_entity, pdu_to_application);
	
	/* Free pdu */
	pdu_free(pdu);
	 
	return 0;
}

/**************************************************************/
/* --- DO NOT CHNAGE NAME OF THIS FUNCTION --- */
/* The function is automatically called when the timer expires*/
static
DatalinkTimerExpired(DLC_LAYER_ENTITY_TYPE *dlc_layer_entity,
					 DLC_Conn_Info_TYPE *dci)
{
	
	/* Send as many pdu's as allowed by window.
	   You may use: window_open() and SendInfo(); */
	dci->snd_nxt = dci->snd_una;
  while(window_open(dci)){
    SendInfo(dlc_layer_entity, dci);
  }
	return 0;
}

/*************************************************************
  Sending Info, RR, and REJ Functions
*/
static
SendInfo(DLC_LAYER_ENTITY_TYPE *dlc_layer_entity,
			  DLC_Conn_Info_TYPE *dci)
{
     /* get a_pdu to send */
	PDU_TYPE *pdu_to_send;
	pdu_to_send = GetPDUFromBuffer(dci);
  
	/* Copy it to a new d_pdu and fill the remaining fields */
	PDU_TYPE *d_pdu = pdu_alloc();
	d_pdu->type = TYPE_IS_D_PDU;
	d_pdu->u.d_pdu.a_pdu = pdu_to_send->u.a_pdu;
	d_pdu->u.d_pdu.type = D_INFO;
		d_pdu->u.d_pdu.p_bit = NO;
	d_pdu->u.d_pdu.number = dci->snd_nxt;
	d_pdu->u.d_pdu.address = GetReceiverID(dlc_layer_entity);
	d_pdu->u.d_pdu.error = NO;
	/* increment snd_nxt */
	dci->snd_nxt = (dci->snd_nxt + 1) % MODULO;
     /* Set timer. Use:
	   SetTimer(dlc_layer_entity,dci);    */
  SetTimer(dlc_layer_entity, dci);
	/* --- Send d_pdu to physical layer: Same as Lab1 --- */
	
	send_pdu_to_physical_layer(dlc_layer_entity, d_pdu);
  return 0;
}


static
SendRR(DLC_LAYER_ENTITY_TYPE *dlc_layer_entity, PDU_TYPE *pdu,
	   DLC_Conn_Info_TYPE *dci)
{
	/* Allocate a new d_pdu and fill in the needed fields */
	PDU_TYPE *d_pdu = pdu_alloc();
	d_pdu->type = TYPE_IS_D_PDU;
	d_pdu->u.d_pdu.a_pdu = pdu->u.a_pdu;
	d_pdu->u.d_pdu.type = D_RR;
	d_pdu->u.d_pdu.p_bit = NO;
	d_pdu->u.d_pdu.number = dci->rcv_nxt;
	//TODO Check to see what address should be
	d_pdu->u.d_pdu.address = GetReceiverID(dlc_layer_entity);
	d_pdu->u.d_pdu.error = NO;
	/* Send to d_pdu to physical layer */
  send_pdu_to_physical_layer(dlc_layer_entity, d_pdu);
  
	return 0;
}

static
SendREJ(DLC_LAYER_ENTITY_TYPE *dlc_layer_entity, PDU_TYPE *pdu,
		DLC_Conn_Info_TYPE *dci)
{
	/* Don't send REJ if rej_already_sent = 1, but 
	   send RR */
  if(dci->rej_already_sent){
    return 0;
  }
	/* Allocate a new d_pdu and fill in the needed fields */
	PDU_TYPE *d_pdu = pdu_alloc();
	d_pdu->type = TYPE_IS_D_PDU;
	d_pdu->u.d_pdu.a_pdu = pdu->u.a_pdu;
	d_pdu->u.d_pdu.type = D_REJ;
		d_pdu->u.d_pdu.p_bit = NO;
	d_pdu->u.d_pdu.number = dci->rcv_nxt;
	//TODO Check to see what address should be
	d_pdu->u.d_pdu.address = GetReceiverID(dlc_layer_entity);
	d_pdu->u.d_pdu.error = NO;
	/* Send to d_pdu to physical layer */
  send_pdu_to_physical_layer(dlc_layer_entity, d_pdu);
	/* rej_already_sent set to 1 */
  dci->rej_already_sent = 1;
	return 0;
}
