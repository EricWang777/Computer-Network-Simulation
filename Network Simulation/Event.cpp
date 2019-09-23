#include "Event.h"
#include <iostream>
#include <string>

using namespace std;

Event::Event(string content, string nodeID, int socketfd, bool send){
    _content = content;
    _nodeID = nodeID;
    _socketfd = socketfd;
    _send = send;
}

Event::Event(string content, int socketfd, bool send){
    _content = content;
    _nodeID = "";
    _socketfd = socketfd;
    _send = send;
}