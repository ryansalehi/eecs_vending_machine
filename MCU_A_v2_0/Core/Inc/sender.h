#ifndef DOOR_COMMS_H
#define DOOR_COMMS_H

// helper function for sending an open door message to the security box
void DoorComms_SendTestMessage(void);

//TODO: add more communication functions, mainly for heartbeat messages and ack messages
//TODO: heartbeat message and door instructions need to have 1.session ID for ignoring stale requests 2. message counter to ignore replays 3. hardcoded encryption keys

#endif /* DOOR_COMMS_H */
