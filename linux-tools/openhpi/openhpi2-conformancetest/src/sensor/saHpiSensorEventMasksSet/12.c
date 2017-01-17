/*
 * (C) Copyright University of New Hampshire 2005
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 59 Temple
 * Place - Suite 330, Boston, MA 02111-1307 USA.
 *
 * Author(s):
 *     Donald A. Barre <dbarre@unh.edu>
 *
 * Spec:        HPI-B.01.01
 * Function:    saHpiSensorEventMasksSet
 * Description:   
 *   Change the assert and deassert masks separately and verify
 *   that the event SAHPI_ET_SENSOR_ENABLE_CHANGE is generated.
 * Line:        P91-17:P91-18
 */

#include <stdio.h>
#include "saf_test.h"
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>

#define MAX_SENSORS 2000
#define POLL_COUNT 10  // poll for events after every "n" sensors

#define STATE_ADD_ASSERT      1
#define STATE_REMOVE_ASSERT   2
#define STATE_ADD_DEASSERT    3
#define STATE_REMOVE_DEASSERT 4

static int sensorCount;

typedef struct {
    SaHpiResourceIdT resourceId;
    SaHpiSensorNumT sensorNum;
    SaHpiEventStateT mask;
    SaHpiEventStateT asserted;
    SaHpiEventStateT deasserted;
    int state[4];
    int stateIndex;
	SaHpiBoolT sameMasks;
} SensorData;

static SensorData sensorData[MAX_SENSORS];

/**********************************************************************************
 *
 * 
 *
 **********************************************************************************/

int verifyEvents() {
    int i;
    int retval = SAF_TEST_PASS;

    for (i = 0; i < sensorCount; i++) {
        if (sensorData[i].sameMasks) {
            if (sensorData[i].stateIndex < 2) {
                retval = SAF_TEST_FAIL;
                m_print("Did not receive Sensor Enable Change event for Resource %d, sensor %d", 
                        sensorData[i].resourceId, sensorData[i].sensorNum);
            }
		} else {
            if (sensorData[i].stateIndex < 4) {
                retval = SAF_TEST_FAIL;
                m_print("Did not receive Sensor Enable Change event for Resource %d, sensor %d", 
                        sensorData[i].resourceId, sensorData[i].sensorNum);
            }
        }
	}

    return retval;
}


/************************************************************************
 *
 * Change a Sensor Event Mask and wait for the event
 * to be generated.
 *
 ************************************************************************/

int change_event_masks(SaHpiSessionIdT sessionId,
                       SaHpiResourceIdT resourceId,
                       SaHpiSensorNumT sensorNum,
                       SaHpiSensorEventMaskActionT action,
                       SaHpiEventStateT assertMask,
                       SaHpiEventStateT deassertMask)
{
        SaErrorT status;
        int retval = SAF_TEST_PASS;

        status = saHpiSensorEventMasksSet(sessionId, resourceId, sensorNum, action,
                                          assertMask, deassertMask);
        if (status != SA_OK) {
                retval = SAF_TEST_UNRESOLVED;
                e_print(saHpiSensorEventMasksSet, SA_OK, status);
        } 

        return retval;
}

/**********************************************************************************
 *
 * Check for any events that may have been mistakenly generated by the sensors.
 *
 **********************************************************************************/

int checkForEvents(SaHpiSessionIdT sessionId)
{
    int i;
    int retval = SAF_TEST_PASS;
    SaErrorT status;
    SaHpiEventT event;
    SaHpiSensorNumT sensorNum;
	SaHpiEventStateT assertEventMask;
	SaHpiEventStateT deassertEventMask;

    while (SAHPI_TRUE && retval == SAF_TEST_PASS) {
         status = saHpiEventGet(sessionId, SAHPI_TIMEOUT_IMMEDIATE, &event, NULL, NULL, NULL);
         if (status == SA_ERR_HPI_TIMEOUT) {
              break;
         } else if (status != SA_OK) {
              retval = SAF_TEST_UNRESOLVED;
              e_print(saHpiEventGet, SA_OK, status);
              break;
         } else if (event.EventType == SAHPI_ET_SENSOR_ENABLE_CHANGE) {
              sensorNum = event.EventDataUnion.SensorEnableChangeEvent.SensorNum;
			  assertEventMask = event.EventDataUnion.SensorEnableChangeEvent.AssertEventMask;
			  deassertEventMask = event.EventDataUnion.SensorEnableChangeEvent.DeassertEventMask;
              for (i = 0; i < sensorCount; i++) {
                  if ((sensorData[i].resourceId == event.Source) &&
                      (sensorData[i].sensorNum == sensorNum)) {

                       int oldState = sensorData[i].state[sensorData[i].stateIndex];
					   if (oldState == STATE_ADD_ASSERT) {
						   if (assertEventMask != sensorData[i].mask) {
							   retval = SAF_TEST_FAIL;
							   m_print("Event's AssertEventMask is not set to SensorRec.Events!");
							   break;
						   } 
					   } else if (oldState == STATE_REMOVE_ASSERT) {
						   if (assertEventMask != 0x0) {
							   retval = SAF_TEST_FAIL;
							   m_print("Event's AssertEventMask is not set to 0x0!");
							   break;
						   } 
					   } else if (oldState == STATE_ADD_DEASSERT) {
						   if (deassertEventMask != sensorData[i].mask) {
							   retval = SAF_TEST_FAIL;
							   m_print("Event's DeassertEventMask is not set to SensorRec.Events!");
							   break;
						   } 
					   } else if (oldState == STATE_REMOVE_DEASSERT) {
						   if (deassertEventMask != 0x0) {
							   retval = SAF_TEST_FAIL;
							   m_print("Event's DeassertEventMask is not set to 0x0!");
							   break;
						   } 
					   }

                       sensorData[i].stateIndex++;
                       SaHpiBoolT sameMasks = sensorData[i].sameMasks;
                       int index = sensorData[i].stateIndex;
                       if (index < 4) {
                           int state = sensorData[i].state[index];
                           if (state == STATE_ADD_ASSERT) {
                               retval = change_event_masks(sessionId, sensorData[i].resourceId, 
                                                           sensorData[i].sensorNum, 
                                                           SAHPI_SENS_ADD_EVENTS_TO_MASKS,
                                                           SAHPI_ALL_EVENT_STATES, 0x0);
                           } else if (state == STATE_REMOVE_ASSERT) {
                               retval = change_event_masks(sessionId, sensorData[i].resourceId, 
                                                           sensorData[i].sensorNum, 
                                                           SAHPI_SENS_REMOVE_EVENTS_FROM_MASKS,
                                                           SAHPI_ALL_EVENT_STATES, 0x0);
                           } else if (!sameMasks && state == STATE_ADD_DEASSERT) {
                               retval = change_event_masks(sessionId, sensorData[i].resourceId, 
                                                           sensorData[i].sensorNum, 
                                                           SAHPI_SENS_ADD_EVENTS_TO_MASKS,
                                                           0x0, SAHPI_ALL_EVENT_STATES);
                           } else if (!sameMasks && state == STATE_REMOVE_DEASSERT) {
                               retval = change_event_masks(sessionId, sensorData[i].resourceId, 
                                                           sensorData[i].sensorNum, 
                                                           SAHPI_SENS_REMOVE_EVENTS_FROM_MASKS,
                                                           0x0, SAHPI_ALL_EVENT_STATES);
                           }
					   }
                       break;
                  }
              }
         }
    }

    return retval;
}


/************************************************************************
 *
 * Restore the event masks.
 *
 ************************************************************************/

void reset_event_masks(SaHpiSessionIdT sessionId,
                       SaHpiResourceIdT resourceId,
                       SaHpiSensorNumT sensorNum,
                       SaHpiEventStateT mask,
                       SaHpiEventStateT assertMask,
                       SaHpiEventStateT deassertMask)
{
        SaErrorT status;

        // clear all of the bits
        status = saHpiSensorEventMasksSet(sessionId, resourceId, sensorNum,
                                          SAHPI_SENS_REMOVE_EVENTS_FROM_MASKS,
                                          mask, mask);
        if (status != SA_OK) {
                e_print(saHpiSensorEventMasksSet, SA_OK, status);
        } else {
                status = saHpiSensorEventMasksSet(sessionId, resourceId, sensorNum,
                                                  SAHPI_SENS_ADD_EVENTS_TO_MASKS,
                                                  assertMask, deassertMask);

                if (status != SA_OK) {
                        e_print(saHpiSensorEventMasksGet, SA_OK, status);
                }
        }
}

/**********************************************************************************
 *
 * Restore.
 *
 **********************************************************************************/

void restoreSensors(SaHpiSessionIdT sessionId) {
    int       i;

    for (i = 0; i < sensorCount; i++) {
        reset_event_masks(sessionId, sensorData[i].resourceId, 
                          sensorData[i].sensorNum, sensorData[i].mask, 
                          sensorData[i].asserted, sensorData[i].deasserted);
    }
}


SaHpiBoolT canTest(SaHpiRdrT *rdr) {
        return ((sensorCount < MAX_SENSORS) &&
                (rdr->RdrType == SAHPI_SENSOR_RDR) &&
                (rdr->RdrTypeUnion.SensorRec.EventCtrl == SAHPI_SEC_PER_EVENT) &&
                (rdr->RdrTypeUnion.SensorRec.Events != 0x0));
}

/************************************************************************
 *
 * Test changing the assert and deassert event masks.  Change
 * them separately.
 *
 ************************************************************************/

int testSensor(SaHpiSessionIdT session, SaHpiResourceIdT resource, SaHpiRdrT *rdr, SaHpiBoolT sameMasks)
{
        int retval = SAF_TEST_NOTSUPPORT;
        SaErrorT status;
        SaHpiSensorNumT s_num;
        SaHpiEventStateT Assertsaved, Deassertsaved;

        s_num = rdr->RdrTypeUnion.SensorRec.Num;

        status = saHpiSensorEventMasksGet(session, resource, s_num,
                                          &Assertsaved, &Deassertsaved);

        if (status != SA_OK) {
                retval = SAF_TEST_UNRESOLVED;
                e_print(saHpiSensorEventMasksGet, SA_OK, status);
        } else {
                sensorData[sensorCount].resourceId = resource;
                sensorData[sensorCount].sensorNum = s_num;
                sensorData[sensorCount].mask = rdr->RdrTypeUnion.SensorRec.Events;
                sensorData[sensorCount].asserted = Assertsaved;
                sensorData[sensorCount].deasserted = Deassertsaved;
                sensorData[sensorCount].stateIndex = 0;
                sensorData[sensorCount].sameMasks = sameMasks;

                // Only change the Assert Mask

                if (Assertsaved == 0) {
                        sensorData[sensorCount].state[0] = STATE_ADD_ASSERT;
                        sensorData[sensorCount].state[1] = STATE_REMOVE_ASSERT;
                        retval = change_event_masks(session, resource, s_num,
                                                    SAHPI_SENS_ADD_EVENTS_TO_MASKS,
                                                    SAHPI_ALL_EVENT_STATES, 0x0);
                 } else {
                        sensorData[sensorCount].state[0] = STATE_REMOVE_ASSERT;
                        sensorData[sensorCount].state[1] = STATE_ADD_ASSERT;
                        retval = change_event_masks(session, resource, s_num,
                                                    SAHPI_SENS_REMOVE_EVENTS_FROM_MASKS,
                                                    SAHPI_ALL_EVENT_STATES, 0x0);
                 }

                 if (Deassertsaved == 0) {
                        sensorData[sensorCount].state[2] = STATE_ADD_DEASSERT;
                        sensorData[sensorCount].state[3] = STATE_REMOVE_DEASSERT;
                 } else {
                        sensorData[sensorCount].state[2] = STATE_REMOVE_DEASSERT;
                        sensorData[sensorCount].state[3] = STATE_ADD_DEASSERT;
                 }

                 sensorCount++;
        }

        if (retval == SAF_TEST_PASS && sensorCount % POLL_COUNT == 0) {
                retval = checkForEvents(session);
        }

        return retval;
}

/**********************************************************************************
 *
 * Test a resource.
 *
 **********************************************************************************/

int testResource(SaHpiSessionIdT sessionId, SaHpiResourceIdT resourceId, SaHpiBoolT sameMasks) {
     int retval = SAF_TEST_NOTSUPPORT;
     int response;
     SaErrorT error;
     SaHpiEntryIdT nextEntryId;
     SaHpiEntryIdT entryId;
     SaHpiRdrT rdr;
     SaHpiBoolT pass = SAHPI_FALSE;

     nextEntryId = SAHPI_FIRST_ENTRY;
     while (nextEntryId != SAHPI_LAST_ENTRY && retval == SAF_TEST_NOTSUPPORT) {
          entryId = nextEntryId;
          error = saHpiRdrGet(sessionId, resourceId, entryId, &nextEntryId, &rdr);
          if (error == SA_ERR_HPI_NOT_PRESENT) {
              break;
          } else if (error != SA_OK) {
              retval = SAF_TEST_UNRESOLVED;
              e_print(saHpiRdrGet, SA_OK, error);
          } else {
              if (canTest(&rdr)) {
                   response = testSensor(sessionId, resourceId, &rdr, sameMasks);
                   if (response == SAF_TEST_PASS) {
                       pass = SAHPI_TRUE;
                   } else {
                       retval = response;
                   }
              }
          }
     }

     if (retval == SAF_TEST_NOTSUPPORT && pass) {
         retval = SAF_TEST_PASS;
     }

     return retval;
}

/**********************************************************************************
 *
 * Test a domain.
 *
 **********************************************************************************/

int TestDomain(SaHpiSessionIdT sessionId)
{
     int i;
     int retval = SAF_TEST_NOTSUPPORT;
     int response;
     SaErrorT error;
     SaHpiEntryIdT nextEntryId;
     SaHpiEntryIdT entryId;
     SaHpiRptEntryT rptEntry;
     SaHpiBoolT pass = SAHPI_FALSE;
	 SaHpiBoolT evtDeasserts = SAHPI_FALSE;

	 // sleep for 5 seconds to avoid spurrious events showing up.
	 sleep(5);

     sensorCount = 0;
     error = saHpiSubscribe(sessionId);
     if (error != SA_OK) {
         e_print(saHpiSubscribe, SA_OK, error);
         retval = SAF_TEST_UNRESOLVED;
     } else {
         nextEntryId = SAHPI_FIRST_ENTRY;
         while (nextEntryId != SAHPI_LAST_ENTRY && retval == SAF_TEST_NOTSUPPORT) {
              entryId = nextEntryId;
              error = saHpiRptEntryGet(sessionId, entryId, &nextEntryId, &rptEntry);
              if (error == SA_ERR_HPI_NOT_PRESENT) {
                  break;
              } else if (error != SA_OK) {
                  retval = SAF_TEST_UNRESOLVED;
                  e_print(saHpiRptEntryGet, SA_OK, error);
              } else if (rptEntry.ResourceCapabilities & SAHPI_CAPABILITY_SENSOR) {
				  if (rptEntry.ResourceCapabilities & SAHPI_CAPABILITY_EVT_DEASSERTS) {
	 			      evtDeasserts = SAHPI_TRUE;
				  }
                  response = testResource(sessionId, rptEntry.ResourceId, evtDeasserts);
                  if (response == SAF_TEST_PASS) {
                      pass = SAHPI_TRUE;
                  } else {
                      retval = response;
                  }
              }
         }

         if (retval == SAF_TEST_NOTSUPPORT && pass) {
             retval = SAF_TEST_PASS;
             // Check for any remaining events that may have been generated.
             // We will pause for 5 seconds to give the last sensor time to
             // generate an event.

             for (i = 0; i < 4 && retval == SAF_TEST_PASS; i++) {
                 sleep(5);
                 retval = checkForEvents(sessionId);
             }

             if (retval == SAF_TEST_PASS) {
                 retval = verifyEvents();
             }
         }

         restoreSensors(sessionId);

         error = saHpiUnsubscribe(sessionId);
         if (error != SA_OK) {
              e_print(saHpiUnsubscribe, SA_OK, error);
         }
     }

     return retval;
}

/**********************************************************************************
 *
 * Main Program 
 *
 **********************************************************************************/

int main(int argc, char **argv) {
     return process_all_domains(NULL, NULL, TestDomain);
}

