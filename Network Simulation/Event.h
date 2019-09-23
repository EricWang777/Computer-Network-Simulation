#include <iostream>
#include <string>
using namespace std;

#ifndef EVENT_H
#define EVENT_H

class Event{
public:
    Event(string content, string nodeID, int socketfd, bool send);
    Event(string content, int socketfd, bool send);

    // member variable
    string _content; // content of this event, need further process
    string _nodeID; // the node id on the other end
    int _socketfd; // this event behaves on this socketfd
    bool _send; // true, this message need to be sent
                // false, this event is received from socket
};

#endif /* !EVENT_H */