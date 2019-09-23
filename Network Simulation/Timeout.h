#include <iostream>
#include <string>
using namespace std;

#ifndef TIMEOUT_H
#define TIMEOUT_H

class Timeout{
public:
    Timeout(string type, int socketfd, int timer, int label);
    // member variables
    string _type;
    int _socketfd;
    int _timer;
    int _label;

    // public function
    bool isTimeout();
    void countDown();

    // used for pa6 unicast message
    string _content;
};

#endif /* !TIMEOUT_H */