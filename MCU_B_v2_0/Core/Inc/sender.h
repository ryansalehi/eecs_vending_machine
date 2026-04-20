#ifndef SENDER_H
#define SENDER_H

void UART_Init();

// helper function for sending an open door message to the security box
void UART_SendMessage(char* message);

//TODO: add more communication functions, mainly for heartbeat messages and ack messages
//TODO: heartbeat message and door instructions need to have 1.session ID for ignoring stale requests 2. message counter to ignore replays 3. hardcoded encryption keys

#endif /* SENDER_H */
