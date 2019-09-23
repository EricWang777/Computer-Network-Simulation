#include "Util.h"
#include <iostream>
#include <string.h>
#include <openssl/sha.h>
#include <ctime>
#include <sys/time.h>
#include <vector>
using namespace std;

// helper function
string Util::timeStamp(){
    timeval tim;
    gettimeofday(&tim, NULL);
    
    string time = ctime(&tim.tv_sec);
    time = time.substr(0, time.length()-1);
    int zeroNum = 6 - to_string(tim.tv_usec).length();
    string zero = "";
    for(int i = 0; i < zeroNum; i++){
        zero += "0";
    }
    return "[Date: " + time.substr(0, 11) + time.substr(20, 4) + 
           " " + time.substr(11, 8) + "." + to_string(tim.tv_usec) + zero + "]";
}

string Util::getValueByKey(string message, string key){
    size_t found = message.find(key);
    string ret = "";
    if(found == string::npos) return "";
    else{
        int startIndex = found + key.length() + 2; // consume ": "
        for(int i = startIndex; message[i] != '\r' && message[i] != '\n' && message[i] != ';'; i++){
            ret += message[i];
        }
    }
    return ret;
}

string Util::getLocation(string message){
    size_t found = message.find("location");
    string ret = "";
    if(found == string::npos) return "";
    else{
        int startIndex = found + 9; // consume ": "
        for(int i = startIndex; message[i] != '\r' && message[i] != '\n'; i++){
            ret += message[i];
        }
    }
    return ret;
}

string GetObjID(char node_id[40])
{
    char obj_category[8] = "msg";
    char returned_obj_id[SHA_DIGEST_LENGTH];
    char hexstring_of_obj_id[(SHA_DIGEST_LENGTH<<1)+1];
    static unsigned long seq_no=0L;
    static struct timeval node_start_time;
    static char hexchar[]="0123456789abcdef";
    char unique_str[128];

    if (seq_no++ == 0L) {
        gettimeofday(&node_start_time, NULL);
    }
    snprintf(unique_str, sizeof(unique_str), "%s_%ld_%s_%ld",
            node_id, node_start_time.tv_sec, obj_category, (long)seq_no);
    SHA1((unsigned char*)unique_str, strlen(unique_str), (unsigned char*)returned_obj_id);
    for (int i=0; i < SHA_DIGEST_LENGTH; i++) {
        unsigned char ch=(unsigned char)returned_obj_id[i];
        int hi_nibble=(int)(unsigned int)((ch>>4)&0x0f);
        int lo_nibble=(int)(unsigned int)(ch&0x0f);

        hexstring_of_obj_id[i<<1] = hexchar[hi_nibble];
        hexstring_of_obj_id[(i<<1)+1] = hexchar[lo_nibble];
    }
    hexstring_of_obj_id[SHA_DIGEST_LENGTH<<1] = '\0';
    return string(hexstring_of_obj_id);
}


string Util::genDisReq(int ttl, string nodeID){
    char node_id[40];
    strcpy(node_id, nodeID.c_str());
    string msgid = GetObjID(node_id);
    string header = string("DISCOREQ 353NET/1.0 NONE/1.0\r\n") + 
                    string("TTL: ") + to_string(ttl) + "\r\n" + 
                    "Flood: 1\r\n" + 
                    "MessageID: " + msgid + "\r\n" +
                    "Content-Length: 0\r\n" +
                    "\r\n";
    return header;
}

string Util::genDisResp(int ttl, string msgid, string nodeID, int location){
    string header = string("DISCORSP 353NET/1.0 NONE/1.0\r\n") + 
                    string("TTL: ") + to_string(ttl) + "\r\n" + 
                    "Flood: 0\r\n" + 
                    "Respond-To: " + msgid + "\r\n" +
                    "From: " + nodeID + "; location=" + to_string(location) + "\r\n" +
                    "Content-Length: 0\r\n" +
                    "\r\n";
    return header;
}

string Util::genHello(string nodeID, string keepAlive){
    char node_id[40];
    strcpy(node_id, nodeID.c_str());
    string msgid = GetObjID(node_id);
    string header = string("SAYHELLO 353NET/1.0 NONE/1.0\r\n") + 
                    "TTL: 1\r\n" + 
                    "Flood: 0\r\n" + 
                    "MessageID: " + msgid + "\r\n" +
                    "From: " + nodeID + "\r\n" +
                    "KeepAlive-Timeout: " + keepAlive + "\r\n" +
                    "Content-Length: 0\r\n" +
                    "\r\n";
    return header;
}

string Util::genKeepAlive(){
    string header = "KEEPALIV 353NET/1.0 NONE/1.0\r\n" + 
                    string("TTL: 1\r\n") +
                    "Flood: 0\r\n" +
                    "Content-Length: 0\r\n" +
                    "\r\n"; 
    return header;
}

string Util::genCheckReq(int ttl, string nodeID){
    char node_id[40];
    strcpy(node_id, nodeID.c_str());
    string msgid = GetObjID(node_id);
    string header = "CHECKREQ 353NET/1.0 NONE/1.0\r\n" + 
                    string("TTL: ") + to_string(ttl) + "\r\n" +
                    "Flood: 1\r\n" +
                    "MessageID: " + msgid + "\r\n" +
                    "Content-Length: 0\r\n" +
                    "\r\n"; 
    return header;
}

string Util::genCheckResp(int ttl, string msgid){
    string header = "CHECKRSP 353NET/1.0 NONE/1.0\r\n" +
                    string("TTL: ") + to_string(ttl) + "\r\n" +
                    "Flood: 0\r\n" +
                    "Respond-To: " + msgid + "\r\n" +
                    "Content-Length: 0\r\n" +
                    "\r\n";
    return header;
}

string Util::genGoodbye(int reason){
    string header = "SGOODBYE 353NET/1.0 NONE/1.0\r\n" +
                    string("TTL: 1\r\n") +
                    "Flood: 0\r\n" +
                    "Reason: " + to_string(reason) + "\r\n" + 
                    "Content-Length: 0\r\n" +
                    "\r\n";
    return header;
}

string Util::genLinkReq(int ttl, string nodeID, string content){
    char node_id[40];
    strcpy(node_id, nodeID.c_str());
    string msgid = GetObjID(node_id);
    string header = "LINKSREQ 353NET/1.0 NONE/1.0\r\n" + 
                    string("TTL: ") + to_string(ttl) + "\r\n" +
                    "Flood: 1\r\n" +
                    "MessageID: " + msgid + "\r\n" +
                    "From: " + nodeID + "\r\n" + 
                    "Content-Length: " + to_string(content.length()) + "\r\n" +
                    "\r\n" +
                    content;
    return header;
}

string Util::genLinkResp(int ttl, string nodeID, string msgid, string content){
    string header = "LINKSRSP 353NET/1.0 NONE/1.0\r\n" + 
                    string("TTL: ") + to_string(ttl) + "\r\n" +
                    "Flood: 0\r\n" +
                    "Respond-To: " + msgid + "\r\n" +
                    "From: " + nodeID + "\r\n" +
                    "Content-Length: " + to_string(content.length()) + "\r\n" +
                    "\r\n" +
                    content;
    return header;
}

string Util::genLinkUpdate(int ttl, string nodeID, string content){
    char node_id[40];
    strcpy(node_id, nodeID.c_str());
    string msgid = GetObjID(node_id);
    string header = "LINKSUPD 353NET/1.0 NONE/1.0\r\n" + 
                    string("TTL: ") + to_string(ttl) + "\r\n" +
                    "Flood: 1\r\n" +
                    "MessageID: " + msgid + "\r\n" +
                    "From: " + nodeID + "\r\n" + 
                    "Content-Length: " + to_string(content.length()) + "\r\n" +
                    "\r\n" +
                    content;
    return header;
}

string Util::genDebugReq(int ttl, string nodeID, string content){
    char node_id[40];
    strcpy(node_id, nodeID.c_str());
    string msgid = GetObjID(node_id);
    string header = "DEBUGREQ 353NET/1.0 NONE/1.0\r\n" + 
                    string("TTL: ") + to_string(ttl) + "\r\n" +
                    "Flood: 1\r\n" +
                    "MessageID: " + msgid + "\r\n" +
                    "From: " + nodeID + "\r\n" +
                    "Content-Length: " + to_string(content.length()) + "\r\n" +
                    "\r\n" +
                    content;
    return header;
}

string Util::genDebugResp(int ttl, string nodeID, string msgid, string content){
    string header = "DEBUGRSP 353NET/1.0 NONE/1.0\r\n" + 
                    string("TTL: ") + to_string(ttl) + "\r\n" +
                    "Flood: 0\r\n" +
                    "Respond-To: " + msgid + "\r\n" +
                    "From: " + nodeID + "\r\n" +
                    "Content-Length: " + to_string(content.length()) + "\r\n" +
                    "\r\n" +
                    content;
    return header;
}

string Util::genUniReq(int ttl, string src_nodeID, string dest_nodeID){
    char node_id[40];
    strcpy(node_id, src_nodeID.c_str());
    string msgid = GetObjID(node_id);
    string content = "Traceroute-Request=" + msgid;

    string header = "UNICAMSG 353NET/1.0 TRACEROUTE/1.0\r\n" + 
                    string("TTL: ") + to_string(ttl) + "\r\n" +
                    "From: " + src_nodeID + "\r\n" +
                    "To: " + dest_nodeID + "\r\n" +
                    "Content-Length: " + to_string(content.length()) + "\r\n" +
                    "\r\n" +
                    content;
    return header;
}

string Util::genUniResp(int ttl, string src_nodeID, string dest_nodeID, string msgid){
    string content = "Traceroute-Response=" + msgid;

    string header = "UNICAMSG 353NET/1.0 TRACEROUTE/1.0\r\n" + 
                    string("TTL: ") + to_string(ttl) + "\r\n" +
                    "From: " + src_nodeID + "\r\n" +
                    "To: " + dest_nodeID + "\r\n" +
                    "Content-Length: " + to_string(content.length()) + "\r\n" +
                    "\r\n" +
                    content;
    return header;
}

string Util::genUniZero(int ttl, string src_nodeID, string dest_nodeID, string msgid){
    string content = "Traceroute-ZeroTTL=" + msgid;

    string header = "UNICAMSG 353NET/1.0 TRACEROUTE/1.0\r\n" + 
                    string("TTL: ") + to_string(ttl) + "\r\n" +
                    "From: " + src_nodeID + "\r\n" +
                    "To: " + dest_nodeID + "\r\n" +
                    "Content-Length: " + to_string(content.length()) + "\r\n" +
                    "\r\n" +
                    content;
    return header;
}

void Util::Delog(string nodeID, string message){
    cerr << "\n[" << nodeID << "] " << message << endl;
}

string Util::decreTTL(string message){
    
    string ttl_str = Util::getValueByKey(message, "TTL");
    if(ttl_str == "") return message;
    int ttl = stoi(ttl_str) - 1;
    ttl_str = to_string(ttl);
    vector<string> vec = Util::splitString(message, '\n');
    string ret = "";
    for(size_t i = 0; i < vec.size()-1; i++){
        if(vec[i].substr(0, 3) == "TTL"){
            ret = ret + "TTL: " + ttl_str + "\r\n";
        }else{
            ret = ret + vec[i] + "\n";
        }
    }
    
    return ret;   
}

string Util::decreUniTTL(string message){
    message += "\n";
    message = Util::decreTTL(message);
    return message.substr(0, message.length()-1);
}

/* Begin code (derived) from [https://stackoverflow.com/questions/236129/the-most-elegant-way-to-iterate-the-words-of-a-string] */
/* If the code you got requires you to include its copyright information, put copyright here. */

vector<string> Util::splitString(string str, char delimiter){
    std::vector<std::string> tokens;
    std::size_t start = 0, end = 0;
    while ((end = str.find(delimiter, start)) != std::string::npos)
    {
        if (end != start)
        {
            tokens.push_back(str.substr(start, end - start));
        }
        start = end + 1;
    }
    if (end != start)
    {
        tokens.push_back(str.substr(start));
    }
    return tokens;
}
/* End code from [https://stackoverflow.com/questions/236129/the-most-elegant-way-to-iterate-the-words-of-a-string] */

string Util::getContent(string message){
    size_t found = message.find("\r\n\r\n");
    string content = message.substr(found+4);
    return content;
}

void Util::writeLog(ofstream& log, char category, string message, string nodeID){
    log << timeStamp() << "> " << category << " "; // time stamp and category
    string type = message.substr(0, 8);\
    // pa6
    if(type == "UNICAMSG"){
        log << "TRACEROUTE ";
        if(nodeID == "") log << "* ";
        else log << nodeID <<  " ";
        log << getValueByKey(message, "TTL") << " ";
        log << getValueByKey(message, "From") << " ";
        log << getValueByKey(message, "To") << " "; 
        log << getValueByKey(message, "Content-Length") << " ";
        log << getContent(message);
        log << endl;
        return;
    } 
    log << type << " ";
    if(type == "SAYHELLO"){
        if(category == 'i') log << nodeID << " ";
        else log << getValueByKey(message, "From") << " ";
    }else{
        if(nodeID == "") log << "* ";
        else log << nodeID <<  " ";
    }
    
    // determine neighbor value
    // if(category == 'i'){
    //     // initiate by myself, from is always my node id, useless
    //     if(nodeID == "") log << "* ";
    //     else log << nodeID <<  " ";
    // }
    // else if(category == 'u'){
    //     if(nodeID == "") log << "* ";
    //     else log << nodeID <<  " ";
    // }
    // else{
    //     // if has node id, then use node id
    //     if(nodeID != "") log << nodeID << " ";
    //     // if not has node id, get from 'From'
    //     else if(Util::getValueByKey(message, "From") != "") log << Util::getValueByKey(message, "From") << " ";
    //     // both field has nothing, *
    //     else log << "* ";
    // }
    
    // ttl
    log << getValueByKey(message, "TTL") << " ";
    // flood
    string flood = getValueByKey(message, "Flood");
    if(flood == "0") log << "- ";
    else log << "F ";
    // length
    log << getValueByKey(message, "Content-Length") << " ";
    // msg-dependent data
    if(type == "DISCOREQ"){
        log << getValueByKey(message, "MessageID") << " ";
    }
    else if(type == "DISCORSP"){
        log << getValueByKey(message, "Respond-To") << " ";
        log << Util::getValueByKey(message, "From") << " ";
        log << stoi(Util::getLocation(message)) << " ";
    }
    else if(type == "SAYHELLO"){
        log << getValueByKey(message, "MessageID") << " ";
        log << Util::getValueByKey(message, "From") << " ";
        log << Util::getValueByKey(message, "KeepAlive-Timeout") << " ";
    }
    else if(type == "KEEPALIV"){
        
    }
    else if(type == "CHECKREQ"){
        log << getValueByKey(message, "MessageID") << " ";
    }
    else if(type == "CHECKRSP"){
        log << getValueByKey(message, "Respond-To") << " ";
    }
    else if(type == "SGOODBYE"){
        log << getValueByKey(message, "Reason") << " ";
    }
    else if(type == "LINKSREQ" || type == "LINKSUPD"){
        log << getValueByKey(message, "MessageID") << " ";
        log << getValueByKey(message, "From") << " ";
        // content
        string content = Util::getContent(message);
        // changeb \n to ,
        for(size_t i = 0; i < content.length(); i++){
            if(content[i] == '\n') content[i] = ',';
        }
        log << "(" << content.substr(0, content.length()-1) << ")";
    }
    else if(type == "LINKSRSP"){
        log << getValueByKey(message, "Respond-To") << " ";
        log << getValueByKey(message, "From") << " ";
        // content
        string content = Util::getContent(message);
        // changeb \n to ,
        for(size_t i = 0; i < content.length(); i++){
            if(content[i] == '\n') content[i] = ',';
        }
        log << "(" << content.substr(0, content.length()-1) << ")";
    }
    else if(type == "DEBUGREQ"){
        log << getValueByKey(message, "MessageID") << " ";
        log << getValueByKey(message, "From") << " ";
        // content
        string content = Util::getContent(message);
        // changeb \n to ,
        for(size_t i = 0; i < content.length(); i++){
            if(content[i] == '\n') content[i] = ',';
        }
        log << "(" << content.substr(0, content.length()-1) << ")";
    }
    else if(type == "DEBUGRSP"){
        log << getValueByKey(message, "Respond-To") << " ";
        log << getValueByKey(message, "From") << " ";
        // content
        string content = Util::getContent(message);
        // changeb \n to ,
        for(size_t i = 0; i < content.length(); i++){
            if(content[i] == '\n') content[i] = ',';
        }
        log << "(" << content.substr(0, content.length()-1) << ")";
    }
    
    log << endl;
    log.flush();

}

double Util::difftime(timeval& start, timeval& end){
    return end.tv_sec - start.tv_sec + (double)(end.tv_usec - start.tv_usec)/1000000;
}