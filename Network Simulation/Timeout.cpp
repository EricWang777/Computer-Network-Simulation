
#include "Timeout.h"
#include <iostream>
#include <string>
using namespace std;

Timeout::Timeout(string type, int socketfd, int timer, int label){
    _type = type;
    _socketfd = socketfd;
    _timer = timer * 4;
    _label = label;
}

bool Timeout::isTimeout(){
    return (_timer <= 0);
}

void Timeout::countDown(){
    _timer--;
}