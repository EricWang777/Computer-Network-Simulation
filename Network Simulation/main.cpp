#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <signal.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <iostream>
#include <sstream>
#include <thread>
#include <memory>
#include <mutex>
#include <atomic>
#include <fstream>
#include <unordered_map>
#include <vector>
#include <pthread.h>
#include <queue>
#include "Util.h"
#include "Event.h"
#include "Timeout.h"
#include <map>
#include <algorithm>
#include <set>
#include <ctime>
#include <sys/time.h>

using namespace std;
// general
const string WHITE_SPACE = "\t ";
unordered_map<string, string> config;
mutex main_mutex;
map<string, int> nodeidToSockfd; // check duplicate connection between famous node
vector<string> neighbor;
map<string, vector<string>> adjacencyList; // store all nodes' neighbors except for node itself
unordered_map<string, int> messCache;
ofstream logFile;
int master_socket_fd = -1;

bool validateCommandLine(int argc, char *argv[]);
bool parseConfig(string path);
bool isFamous();
void writePID();
void writeServerInfo(string addr, int port);
string selfNodeID();
string readMessage(int socketfd); // read a single message from socketfd
void addNeighbor(string nodeID);
void printNeighbors();
void closeConnection(int socketfd);
void closeConnection(string nodeID);
void printNetgraph();
void removeFromAdjList(string nodeA, string nodeB); // A as key, B as remove target
string getIdByFd(int socketfd);
string nextHopTo(string dest_node);
void printForwardingTable();
// Restart
bool restart = false;
bool termin = false;
void CleanUp(); // Clean up all pubilc data to restart
// event queue
queue<Event *> q;
pthread_mutex_t m = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cv = PTHREAD_COND_INITIALIZER;

void add_event(Event *event);
Event* wait_for_event();
// Discovery Mode
map<int, string> locToNodeID; // used only in discovery mode
int temporary_socket_fd = -1; // -1 means not used
bool inDisMode = false;
string disNodeID = "";
// Participate Mode
vector<string> sentMessageID; // store all sent messageID, waiting for response
// Check Phase
bool hasFamousNeighbor();
int checkTimeoutLabel;


// Time out 
atomic<int> nextLabel(1); 
vector<Timeout*> timeoutList;
pthread_mutex_t timeout_m = PTHREAD_MUTEX_INITIALIZER;

int getCurrLabel();
void add_timeout(Timeout* t);
void remove_timeout(int label);
void reset_timeout_keepAliveTimer(int socketfd);

// Thread
pthread_t console_thread_id;
pthread_t handle_events_id;
pthread_t handle_timeouts_id;
pthread_t open_socket_id;
void tryConnect();
void *console(void *arg);
void *open_socket(void* arg);
void *handle_connection(void *arg);
void *famous_connect_famous(void* arg);
void *handle_events(void* arg);
void *handle_timeoutList(void* arg);
void *regular_connect_discover(void *arg);
void *regular_connect_participate(void* arg);

// Unicast Message
int uni_ttl = 0;
const int UNI_TIMEOUT_LABEL = getCurrLabel();
timeval start_tim;
string DEST_NODE;


// -----------------------------------Main------------------------------------
int main(int argc, char *argv[])
{
// Init Once
    signal(SIGPIPE, SIG_IGN);
    // validate the command line
    if (!validateCommandLine(argc, argv))
    {
        return 0;
    }
    if (!parseConfig(string(argv[argc - 1])))
        return 0;
    writePID();
    logFile.open(config["root"] + "/" + config["log"]);

    if(argc == 3){
        // Reset
        string tempLogPath = config["root"] + "/" + config["log"];
        string tempPidPath = config["root"] + "/" + config["pid"];
        string tempStartupPath = config["root"] + "/startup_neighbors.txt";
        remove(tempLogPath.c_str());
        remove(tempPidPath.c_str());
        remove(tempStartupPath.c_str());
        return 0;
    }

    do{
// Init
        restart = false;
// Run
        pthread_create(&open_socket_id, NULL, open_socket, NULL);
        // create handle_event thread
        pthread_create(&handle_events_id, NULL, handle_events, NULL);
        // create handle_timeout thread
        pthread_create(&handle_timeouts_id, NULL, handle_timeoutList, NULL);
        // connect
        tryConnect();
        // open socket
        
        // run forever
        while(!restart && !termin){
            
        }
// Clean Up
    }while(restart);

    // test field

// Clean Up Once
    return 0;
}
// -----------------------------------Main------------------------------------

// Definiation of function
void *console(void *arg)
{
    while (true)
    {
        cout << selfNodeID() << "> ";
        string command;
        getline(cin, command);
        if (command == "neighbors")
        {
            printNeighbors();
            
        }
        else if (command == "netgraph")
        {
            printNetgraph();
        }
        else if (command == "shutdown")
        {
            Event* event = new Event("SHUTDOWN", -1, true);
            add_event(event);
            cerr << "Console thread terminated\n";
            break;
        }
        else if (command == "debug")
        {
            string neiContent = "";
            for(map<string, int>::iterator it = nodeidToSockfd.begin(); 
                it != nodeidToSockfd.end(); ++it)
            {
                neiContent += it->first + "\n";
            }
            string debugReq = Util::genDebugReq(stoi(config["ttl"]), selfNodeID(), neiContent);
            Event* debugReqEvent = new Event(debugReq, 0, false);
            add_event(debugReqEvent);
        }
        else if(command == "forwarding"){
            // print the forwarding table
            printForwardingTable();
        }
        else if(command.find("traceroute") != string::npos){
                vector<string> vec = Util::splitString(command, ' ');
                if(vec.size() < 2){
                    cerr << "Command not recognized. Valid commands are:\n"
                         << "\tforwarding\n\tneighbors\n\tnetgraph\n\ttraceroute targetnode\n\tdebug\n\tshutdown\n";
                    continue;
                }
                if(inDisMode){
                    cerr << selfNodeID() << " is in discovery mode\n";
                    continue;
                }
                if(nodeidToSockfd.size() == 0){
                    cerr << selfNodeID() << " has no neighbors\n";
                    continue;
                }
                DEST_NODE = vec[1];
                uni_ttl = 1; // initalize the uni_ttl to 1, increase after
                string uniReq = Util::genUniReq(uni_ttl, selfNodeID(), DEST_NODE);
                Event* uniReqEvent = new Event(uniReq, 0, false);
                add_event(uniReqEvent);
                gettimeofday(&start_tim, NULL);
        }
        else
        {
            cerr << "Command not recognized. Valid commands are:\n"
                 << "\tforwarding\n\tneighbors\n\tnetgraph\n\ttraceroute targetnode\n\tdebug\n\tshutdown\n";
        }
        this_thread::sleep_for(chrono::milliseconds(500));
    }
    return NULL;
}

void *open_socket(void* arg)
{

    char server_name[256];
    int reuse_addr = 1;
    struct addrinfo hints;
    struct addrinfo *res;

    gethostname(server_name, sizeof(server_name));
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = 0;
    hints.ai_flags = AI_NUMERICSERV | AI_ADDRCONFIG;
    getaddrinfo(server_name, config["port"].c_str(), &hints, &res);
    master_socket_fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    setsockopt(master_socket_fd, SOL_SOCKET, SO_REUSEADDR, (void *)(&reuse_addr), sizeof(int));
    bind(master_socket_fd, res->ai_addr, res->ai_addrlen);
    listen(master_socket_fd, 1);

    {
        /* the following will show you exactly where your server thinks it's running at */
        struct sockaddr_in server_addr;
        socklen_t server_len = (socklen_t)sizeof(server_addr);
        getsockname(master_socket_fd, (struct sockaddr *)(&server_addr), &server_len);
        //writeServerInfo(inet_ntoa(server_addr.sin_addr), (int)htons((uint16_t)(server_addr.sin_port & 0x0ffff)));
        //cerr << inet_ntoa(server_addr.sin_addr) << ": " << (int)htons((uint16_t)(server_addr.sin_port & 0x0ffff)) << endl;
    }

    // open console
    pthread_create(&console_thread_id, NULL, console, NULL);

    for (;;)
    {
        struct sockaddr_in cli_addr;
        unsigned int clilen = sizeof(cli_addr);
        int newsockfd;
        pthread_t thread_id;
        newsockfd = accept(master_socket_fd, (struct sockaddr *)(&cli_addr), &clilen);
        pthread_create(&thread_id, NULL, handle_connection, (void *)(unsigned long)newsockfd);
        
    }
    //close(master_socket_fd);
    return NULL;
}

bool isFamous()
{
    bool famous = false;
    string myAddr = config["host"] + "_" + config["port"];
    int famousCount = stoi(config["count"]);
    for (int i = 0; i < famousCount; i++)
    {
        string famousAddr = config[to_string(i)];
        if (myAddr == famousAddr)
        {
            famous = true;
            break;
        }
    }
    return famous;
}

bool validateCommandLine(int argc, char *argv[])
{
    // pa5 [-reset] configfile
    if (argc == 2)
    {
        if (string(argv[1])[0] == '-')
        {
            cerr << "Command line malformed: need config file\n";
            cerr << "Usage: pa5 [-reset] configfile\n";
            return false;
        }
    }
    if (argc == 3)
    {
        if (string(argv[1])[0] != '-')
        {
            cerr << "Command line malformed: need optional field\n";
            cerr << "Usage: pa5 [-reset] configfile\n";
            return false;
        }
        if (string(argv[1]).substr(1) != "reset")
        {
            cerr << "Command line malformed: invalid option command\n";
            cerr << "Usage: pa5 [-reset] configfile\n";
            return false;
        }
        if (string(argv[2])[0] == '-')
        {
            cerr << "Command line malformed: need config file\n";
            cerr << "Usage: pa5 [-reset] configfile\n";
            return false;
        }
    }
    if (argc < 2)
    {
        cerr << "Command line malformed: too few arguments\n";
        return false;
    }
    if (argc > 3)
    {
        cerr << "Command line malformed: too many arguments\n";
        return false;
    }
    return true;
}

bool parseConfig(string path)
{
    ifstream in(path.c_str());
    if (!in.is_open())
    {
        cerr << "Config file could not be opened\n";
        return false;
    }
    string line;
    int lineNum = 0;
    bool hasConfig = false;
    bool hasParams = false;
    bool hasFamous = false;

    while (getline(in, line))
    {
        lineNum++;
        if (line.empty())
            continue;
        size_t found = line.find_first_not_of(WHITE_SPACE);
        if (found == string::npos || line[found] == ';')
            continue;
        // not comment line
        if (line.find(WHITE_SPACE) != string::npos)
        {
            cerr << "Config malformed at line " << lineNum << ": Cannot have while space here\n";
            return false; // cannot have space
        }

        if (line[0] == '[')
        {
            // section name
            string sectionName = line.substr(1, line.length() - 2);
            if (sectionName == "config")
                hasConfig = true;
            else if (sectionName == "params")
                hasParams = true;
            else if (sectionName == "famous")
                hasFamous = true;
            continue;
        }
        vector<string> keyValue = Util::splitString(line, '=');
        if (keyValue.size() != 2)
        {
            cerr << "Config malformed at line " << lineNum << ": Must have two fields\n";
            return false;
        }
        config.insert(make_pair(keyValue[0], keyValue[1]));
    }

    // check if config has all required
    {
        if (!hasConfig)
        {
            cerr << "Config file doesn't have config section\n";
            return false;
        }
        if (!hasParams)
        {
            cerr << "Config file doesn't have Params section\n";
            return false;
        }
        if (!hasFamous)
        {
            cerr << "Config file doesn't have Famous section\n";
            return false;
        }
        if (config.find("host") == config.end())
        {
            cerr << "Config file doesn't have 'host' key\n";
            return false;
        }
        if (config.find("port") == config.end())
        {
            cerr << "Config file doesn't have 'port' key\n";
            return false;
        }
        if (config.find("app_port") == config.end())
        {
            cerr << "Config file doesn't have 'app_port' key\n";
            return false;
        }
        if (config.find("location") == config.end())
        {
            cerr << "Config file doesn't have 'location' key\n";
            return false;
        }
        if (config.find("root") == config.end())
        {
            cerr << "Config file doesn't have 'root' key\n";
            return false;
        }
        if (config.find("pid") == config.end())
        {
            cerr << "Config file doesn't have 'pid' key\n";
            return false;
        }
        if (config.find("log") == config.end())
        {
            cerr << "Config file doesn't have 'log' key\n";
            return false;
        }
        if (config.find("ttl") == config.end())
        {
            cerr << "Config file doesn't have 'ttl' key\n";
            return false;
        }
        if (config.find("num_startup_neighbors") == config.end())
        {
            cerr << "Config file doesn't have 'num_startup_neighbors' key\n";
            return false;
        }
        if (config.find("msg_life_time") == config.end())
        {
            cerr << "Config file doesn't have 'msg_life_time' key\n";
            return false;
        }
        if (config.find("keep_alive_timeout") == config.end())
        {
            cerr << "Config file doesn't have 'keep_alive_timeout' key\n";
            return false;
        }
        if (config.find("check_timeout") == config.end())
        {
            cerr << "Config file doesn't have 'check_timeout' key\n";
            return false;
        }
        if (config.find("discovery_timeout") == config.end())
        {
            cerr << "Config file doesn't have 'discovery_timeout' key\n";
            return false;
        }
        if (config.find("discovery_retry_interval") == config.end())
        {
            cerr << "Config file doesn't have 'discovery_retry_interval' key\n";
            return false;
        }
        if (config.find("famous_retry_interval") == config.end())
        {
            cerr << "Config file doesn't have 'famous_retry_interval' key\n";
            return false;
        }
        if (config.find("count") == config.end())
        {
            cerr << "Config file doesn't have 'count' key\n";
            return false;
        }
    }

    return true;
}


void writePID()
{
    ofstream pidFile(config["root"] + "/" + config["pid"]);
    pidFile << (int)getpid() << endl;
    pidFile.close();
}

string selfNodeID(){
    return config["host"] + "_" + config["port"];
}

void writeServerInfo(string addr, int port)
{
    logFile << "Server listening at " << addr << ":" << port << endl;
    string root = config["root"];

    if (root[0] != '/')
    {
        // not absolute path
        if (root.substr(0, 2) == "./")
        {
            root = root.substr(2);
        }
        char tempBuf[80];
        root = string(getcwd(tempBuf, sizeof(tempBuf))) + "/" + root;
    }

    logFile << "Server root at '" << root << "'\n";
    logFile << endl;
    logFile.flush();
}

void *handle_connection(void *arg)
{
    int socketfd = (int)(unsigned long)arg;
    
    while(true){
        string message = readMessage(socketfd);
        if(message == ""){
            //closeConnection(socketfd);
            break; // socket closed
        }
        // write into log file
        Util::writeLog(logFile, 'r', message, getIdByFd(socketfd));

        if(message.substr(0, 8) == "SAYHELLO"){
            // hello message
            string id = Util::getValueByKey(message, "From");
            string hello = Util::genHello(selfNodeID(), config["keep_alive_timeout"]);
            if(nodeidToSockfd.find(id) == nodeidToSockfd.end()){
                // first time connection
                write(socketfd, hello.c_str(), hello.length());   
                // write into log file
                Util::writeLog(logFile, 'i', hello, id); 
                            
                // build up connection object
                //Util::Delog(id, "Connection established");
                // add this connection to nodeidToSockfd
                nodeidToSockfd.insert(make_pair(id, socketfd));

                // Create Keep Active send timeout
                Timeout* keepAliveSend = new Timeout("KEEPALIVESEND", 
                                         socketfd,
                                         stoi(config["keep_alive_timeout"])/2,
                                         getCurrLabel());
                add_timeout(keepAliveSend);
                // Create Keep Aive Receive Timeout
                int KEEPALIVE_LABEL = getCurrLabel(); // KEEPALIVE_LABEL is used for this connection to 
                                                    // check keep alive receive timeout, const
                Timeout* keepAliveRec = new Timeout("KEEPALIVEREC", 
                                                    socketfd,
                                                    stoi(config["keep_alive_timeout"]),
                                                    KEEPALIVE_LABEL);
                add_timeout(keepAliveRec);

                // // 1. send Link State Update to the other nodes
                // string neiContent = "";
                // for(map<string, int>::iterator it = nodeidToSockfd.begin(); 
                //     it != nodeidToSockfd.end(); ++it)
                // {
                //     neiContent += it->first + "\n";
                // }
                // string linkUpdate = Util::genLinkUpdate(stoi(config["ttl"]),
                //                                         selfNodeID(),
                //                                         neiContent);
                // for(map<string, int>::iterator it = nodeidToSockfd.begin();
                //     it != nodeidToSockfd.end(); ++it)
                // {
                //     // not send update to connector
                //     if(it->second == socketfd) continue;
                //     write(it->second, linkUpdate.c_str(), linkUpdate.length());
                //     // write into log file
                //     Util::writeLog(logFile, 'i', linkUpdate, it->first);
                // }

                // 2. generate Link State Request Message
                string neiContent = "";
                for(map<string, int>::iterator it = nodeidToSockfd.begin(); 
                    it != nodeidToSockfd.end(); ++it)
                {
                    neiContent += it->first + "\n";
                }
                string linkReq = Util::genLinkReq(stoi(config["ttl"]), selfNodeID(), neiContent);
                // 2. flood the message to this connection
                write(socketfd, linkReq.c_str(), linkReq.length());
                // write into log file
                Util::writeLog(logFile, 'i', linkReq, getIdByFd(socketfd));

                // [modified] flood the message to the network
                for(map<string, int>::iterator it = nodeidToSockfd.begin(); 
                    it != nodeidToSockfd.end(); ++it)
                {
                    write(it->second, linkReq.c_str(), linkReq.length());
                    Util::writeLog(logFile, 'i', linkReq, it->first);
                }

                // 3. Add the message id to sentMessageID, later check for response
                sentMessageID.push_back(Util::getValueByKey(linkReq, "MessageID"));
                
            }else{
                closeConnection(socketfd);
                return NULL;
            }
        }else if(message.substr(0, 8) == "DISCOREQ"){
            // discover request message
            //Util::Delog("Unknown", "Discover Request");
            Event* event = new Event(message, socketfd, true);
            add_event(event);
            
        }else if(message.substr(0, 8) == "DISCORSP"){
            // discover response message
            //Util::Delog("Unknown", "Discover Response");
            Event* event = new Event(message, socketfd, true);
            add_event(event);
        }else if(message.substr(0, 8) == "LINKSREQ"){
            // Link State Request message
            //Util::Delog(Util::getValueByKey(message, "From"), "Link State Request");
            Event* event = new Event(message, socketfd, true);
            add_event(event);
        }else if(message.substr(0, 8) == "LINKSRSP"){
            // Link State Request message
            //Util::Delog(Util::getValueByKey(message, "From"), "Link State Response");
            Event* event = new Event(message, socketfd, true);
            add_event(event);
        }else if(message.substr(0, 8) == "KEEPALIV"){
            // Keep Alive message
            Event* event = new Event(message, socketfd, true);
            add_event(event);
        }else if(message.substr(0, 8) == "LINKSUPD"){
            //til::Delog(Util::getValueByKey(message, "From"), "Link State Update");
            Event* event = new Event(message, socketfd, true);
            add_event(event);
        }else if(message.substr(0, 8) == "CHECKREQ"){
            //Util::Delog(selfNodeID(), "Check Request");
            Event* event = new Event(message, socketfd, true);
            add_event(event);
        }else if(message.substr(0, 8) == "CHECKRSP"){
            // Check Response
            //Util::Delog(selfNodeID(), "Check Response");
            Event* event = new Event(message, socketfd, true);
            add_event(event);
        }else if(message.substr(0, 8) == "DEBUGREQ"){
            // Link State Request message
            //Util::Delog(Util::getValueByKey(message, "From"), "Debug Request");
            Event* event = new Event(message, socketfd, true);
            add_event(event);
        }else if(message.substr(0, 8) == "DEBUGRSP"){
            // Link State Request message
            //Util::Delog(Util::getValueByKey(message, "From"), "Debug Response");
            Event* event = new Event(message, socketfd, true);
            add_event(event);
        }else if(message.substr(0, 8) == "SGOODBYE"){
            // SAY GOODBYE message
            //Util::Delog(selfNodeID(), "Say Goodbye: " + Util::getValueByKey(message, "Reason"));
            // Event* event = new Event(message, socketfd, true);
            // add_event(event);
            bool active = !(getIdByFd(socketfd) == "");
            closeConnection(socketfd);
            if(active){
                // 4. Generate neighbor content 
                string neiContent = "";
                for(map<string, int>::iterator it = nodeidToSockfd.begin(); 
                    it != nodeidToSockfd.end(); ++it)
                {
                    neiContent += it->first + "\n";
                }
                string linkUpdate = Util::genLinkUpdate(stoi(config["ttl"]),
                                                        selfNodeID(),
                                                        neiContent);
                Event* linkUpdEvent = new Event(linkUpdate, 0, false);
                add_event(linkUpdEvent);
            }

            // 5. Check Phase
            // 5.1 check if has famous neighbor
            if(!hasFamousNeighbor()){
                // 5.2 not have famous neighbor, create send request events
                string checkReq = Util::genCheckReq(stoi(config["ttl"]),
                                                    selfNodeID());
                Event* checkReqEvent = new Event(checkReq, 0, false);
                add_event(checkReqEvent);
            }
        }
        else if(message.substr(0, 8) == "UNICAMSG"){
            // pa6
            //Util::Delog(selfNodeID(), "TraceRoute Message");
            Event* event = new Event(message, socketfd, true);
            add_event(event);
        }
    }
    return NULL;
}

void add_event(Event *w)
{
    pthread_mutex_lock(&m);
    q.push(w);                // must only access q if mutex is locked
    pthread_cond_signal(&cv); // must only access cv if mutex is locked
    pthread_mutex_unlock(&m);
}

void add_timeout(Timeout* t){
    pthread_mutex_lock(&timeout_m);
    timeoutList.push_back(t);
    pthread_mutex_unlock(&timeout_m);
}

void remove_timeout(int label){
    pthread_mutex_lock(&timeout_m);
    for(size_t i = 0; i < timeoutList.size(); i++){ 
            if(timeoutList[i] != nullptr && timeoutList[i]->_label == label){
                delete timeoutList[i];
                timeoutList.erase(timeoutList.begin() + i);
            }
        }
    pthread_mutex_unlock(&timeout_m);
}

void reset_timeout_keepAliveTimer(int socketfd){
    pthread_mutex_lock(&timeout_m);
    for(size_t i = 0; i < timeoutList.size(); i++){
            if(timeoutList[i] != nullptr && 
               timeoutList[i]->_type == "KEEPALIVEREC" && 
               timeoutList[i]->_socketfd == socketfd)
            {
                //timeoutList[i]->_timer = stoi(config["keep_alive_timeout"]) * 8;
                Timeout* temp = new Timeout(timeoutList[i]->_type, timeoutList[i]->_socketfd,
                                            stoi(config["keep_alive_timeout"]), timeoutList[i]->_label);
                
                timeoutList.push_back(temp);
                delete timeoutList[i];
                timeoutList[i] = nullptr;
                break;
            }
        }
    pthread_mutex_unlock(&timeout_m);
}

void *handle_timeoutList(void* arg){
    while(true){
        this_thread::sleep_for(chrono::milliseconds(250)); // sleep for 0.25 secs
        pthread_mutex_lock(&timeout_m);
        //cerr << "---------------" << endl;
        for(size_t i = 0; i < timeoutList.size(); i++){
            if(timeoutList[i] != nullptr){
                //cerr << timeoutList[i]->_type << ": " << timeoutList[i]->_timer << endl;
                timeoutList[i]->countDown();
                if(timeoutList[i]->isTimeout()){
                    // process time out event, change to Event object and put to event queue
                    if(timeoutList[i]->_type == "DISCOREQ"){
                        // discovery request time out
                        // 1. generate corresponding event
                        Event* event = new Event("TIMEOUT_DISCOREQ", timeoutList[i]->_socketfd, false);
                        add_event(event);
                    }
                    else if(timeoutList[i]->_type == "SAYHELLO"){
                        // say hello time out
                        // 1. generate corresponding event
                        Event* event = new Event("TIMEOUT_SAYHELLO", timeoutList[i]->_socketfd, false);
                        add_event(event);
                    }
                    else if(timeoutList[i]->_type == "KEEPALIVESEND"){
                        // time to send another keep alive message
                        Event* event = new Event("TIMEOUT_KEEPALIVESEND", timeoutList[i]->_socketfd, false);
                        add_event(event);
                        // reset the timer
                        //timeoutList[i]->_timer = stoi(config["keep_alive_timeout"]) * 2;
                    }
                    else if(timeoutList[i]->_type == "KEEPALIVEREC"){
                        // keep alive receive time out
                        // connection lost
                        Event* event = new Event("TIMEOUT_KEEPALIVEREC", timeoutList[i]->_socketfd, false);
                        add_event(event);
                    }
                    else if(timeoutList[i]->_type == "CHECK"){
                        // Check Time out
                        // not in primary partition
                        Event* event = new Event("TIMEOUT_CHECK", timeoutList[i]->_socketfd, false);
                        add_event(event);
                    }
                    else if(timeoutList[i]->_type == "DEBUG"){
                        // Check Time out
                        // not in primary partition
                        printNetgraph();
                    }
                    else if(timeoutList[i]->_type == "UNICAMSG"){
                        // Unicast message time out
                        // generate new unicast message
                        string dest_node = Util::getValueByKey(timeoutList[i]->_content, "To");
                        //Util::Delog(selfNodeID() + "->" + dest_node, "UNICAMSG Timeout");

                        uni_ttl++; // increase uni_ttl by 1
                        string uniReq = Util::genUniReq(uni_ttl, selfNodeID(), dest_node);
                        Event* uniReqEvent = new Event(uniReq, 0, false);
                        add_event(uniReqEvent);
                    }
                    // To-do
                    delete timeoutList[i];
                    timeoutList.erase(timeoutList.begin() + i);
                    i--;
                }
            }
        }
        pthread_mutex_unlock(&timeout_m);
    }
    return NULL;
}

Event *wait_for_event()
{
    Event *w;

    pthread_mutex_lock(&m);
    while (q.empty())
    { // must only access q if mutex is locked
        // temporarily unlock mutex and sleep, will return with mutex locked
        pthread_cond_wait(&cv, &m); // must only access cv if mutex is locked
    }
    w = q.front(); // must only access q if mutex is locked
    q.pop();
    pthread_mutex_unlock(&m);
    return w;
}

void tryConnect(){
    string filePath = config["root"] + "/startup_neighbors.txt";
    ifstream startFile(filePath.c_str());
    if(isFamous()){
        // if this node is famous node, connect to all other famous node
        int famousCount = stoi(config["count"]);
        string myNodeID = selfNodeID();
        for(int i = 0; i < famousCount; i++){
            string nodeID = config[to_string(i)];  
            // not myself
            if(nodeID == myNodeID) continue;

            string port = Util::splitString(nodeID, '_')[1];
            pthread_t thread_id;
            pthread_create(&thread_id, NULL, famous_connect_famous, (void*)(unsigned long)stoi(port));
        }

    }else if(startFile.is_open()){
        // has neighbor file
        string line;
        while(getline(startFile, line)){
            string port = Util::splitString(line, '_')[1];
            pthread_t thread_id;
            pthread_create(&thread_id, NULL, regular_connect_participate, (void*)(unsigned long)stoi(port));
        }
    }else{
        // if not, connect according to neighbor.txt or pick one famous node
        // TO-DO, connect to one famous node
        for(int i = 0; i < stoi(config["count"]); i++){
            string nodeID = config[to_string(i)];
            string port = Util::splitString(nodeID, '_')[1];
            pthread_t thread_id;
            pthread_create(&thread_id, NULL, regular_connect_discover, (void*)(unsigned long)stoi(port));
            break;
        }
        
    }
}


void* famous_connect_famous(void* arg){
    
    string hostname = "127.0.1.1";
    string port = to_string((int)(unsigned long)arg);
    string str = hostname + "_" + port;
    
    // preparation
    char server_name[256];
    int client_socket_fd;
    struct addrinfo hints;
    struct addrinfo* res;
    struct sockaddr_in soc_address;
    strncpy(server_name, hostname.c_str(), sizeof(hostname));
    memset(&hints,0,sizeof(hints));
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = 0;
    hints.ai_flags = AI_NUMERICSERV|AI_ADDRCONFIG;
    getaddrinfo(server_name, port.c_str(), &hints, &res);
    client_socket_fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    memset(&soc_address, 0, sizeof(soc_address));
    if (*server_name >= '0' && *server_name <= '9') {
        soc_address.sin_addr.s_addr = inet_addr(server_name);
    } else {
        struct hostent *p_hostent;

        p_hostent = gethostbyname(server_name);
        memcpy(&soc_address.sin_addr, p_hostent->h_addr, p_hostent->h_length);
    }
    soc_address.sin_family = AF_INET;
    soc_address.sin_port = htons((unsigned short)stoi(port));
    // preparation finish
    
    while(connect(client_socket_fd, (struct sockaddr*)&soc_address, sizeof(soc_address)) < 0)
    {
        // connection fail, retry after interval
        this_thread::sleep_for(chrono::seconds(stoi(config["famous_retry_interval"])));
    }
    if(nodeidToSockfd.find(str) != nodeidToSockfd.end()){
        // if connection already established
        closeConnection(client_socket_fd);
        return NULL;
    }
    // now famous node has connected to the other famous node
    // send hello message
    string hello = Util::genHello(selfNodeID(), config["keep_alive_timeout"]);
    write(client_socket_fd, hello.c_str(), hello.length());
    // write into log file
    Util::writeLog(logFile, 'i', hello, str);

    string response = readMessage(client_socket_fd); // get immediate response
    // write into log file
    Util::writeLog(logFile, 'r', response, str);
    if(response.substr(0, 8) == "SAYHELLO"){
        //Util::Delog(str, "Connection established");
        nodeidToSockfd.insert(make_pair(str, client_socket_fd));
        
    }else{
        // response not SYAHELLO
        closeConnection(client_socket_fd);
        return NULL;
    }
    // build up new connection
    // Create Keep Alive Send Timeout
    Timeout* keepAliveSend = new Timeout("KEEPALIVESEND", 
                                         client_socket_fd,
                                         stoi(config["keep_alive_timeout"])/2,
                                         getCurrLabel());
    add_timeout(keepAliveSend);
    // Create Keep Aive Receive Timeout
    int KEEPALIVE_LABEL = getCurrLabel(); // KEEPALIVE_LABEL is used for this connection to 
                                          // check keep alive receive timeout, const
    Timeout* keepAliveRec = new Timeout("KEEPALIVEREC", 
                                         client_socket_fd,
                                         stoi(config["keep_alive_timeout"]),
                                         KEEPALIVE_LABEL);
    add_timeout(keepAliveRec);


    // send Link State Request Message
    // 1. generate Link State Request Message
    string neiContent = "";
    for(map<string, int>::iterator it = nodeidToSockfd.begin(); 
        it != nodeidToSockfd.end(); ++it)
    {
        neiContent += it->first + "\n";
    }
    string linkReq = Util::genLinkReq(stoi(config["ttl"]), selfNodeID(), neiContent);
    // 2. flood the message to this connection
    write(client_socket_fd, linkReq.c_str(), linkReq.length());
    // write into log file
    Util::writeLog(logFile, 'i', linkReq, getIdByFd(client_socket_fd));

    // 3. Add the message id to sentMessageID, later check for response
    sentMessageID.push_back(Util::getValueByKey(linkReq, "MessageID"));
    // start continuous reading from this socket
    while (true)
    {
        string message = readMessage(client_socket_fd);
        if(message == "") {
            //closeConnection(client_socket_fd);
            //Util::Delog(str, "Server socket shutdown");
            break;
        }
        // write into log file
        Util::writeLog(logFile, 'r', message, getIdByFd(client_socket_fd));

        if(message.substr(0, 8) == "DISCORSP"){
            // discover response message
            //Util::Delog(str, "Discover Response");
            Event* event = new Event(message, client_socket_fd, true);
            add_event(event);
        }else if(message.substr(0, 8) == "DISCOREQ"){
            // discover requset message
            //Util::Delog(str, "Discover Request");
            Event* event = new Event(message, client_socket_fd, true);
            add_event(event);
        }else if(message.substr(0, 8) == "LINKSREQ"){
            // Link State Request message
            //Util::Delog(Util::getValueByKey(message, "From"), "Link State Request");
            Event* event = new Event(message, client_socket_fd, true);
            add_event(event);
        }else if(message.substr(0, 8) == "LINKSRSP"){
            // Link State Response message
            //Util::Delog(Util::getValueByKey(message, "From"), "Link State Response");
            Event* event = new Event(message, client_socket_fd, true);
            add_event(event);
        }else if(message.substr(0, 8) == "LINKSUPD"){
            // Link State Update message
            //Util::Delog(Util::getValueByKey(message, "From"), "Link State Update");
            Event* event = new Event(message, client_socket_fd, true);
            add_event(event);
        }else if(message.substr(0, 8) == "KEEPALIV"){
            // Keep Alive message
            Event* event = new Event(message, client_socket_fd, true);
            add_event(event);
        }else if(message.substr(0, 8) == "CHECKREQ"){
            //Util::Delog(selfNodeID(), "Check Request");
            Event* event = new Event(message, client_socket_fd, true);
            add_event(event);
        }else if(message.substr(0, 8) == "CHECKRSP"){
            //Util::Delog(selfNodeID(), "Check Response");
            Event* event = new Event(message, client_socket_fd, true);
            add_event(event);
        }else if(message.substr(0, 8) == "DEBUGREQ"){
            // Link State Request message
            //Util::Delog(Util::getValueByKey(message, "From"), "Debug Request");
            Event* event = new Event(message, client_socket_fd, true);
            add_event(event);
        }else if(message.substr(0, 8) == "DEBUGRSP"){
            // Link State Request message
            //Util::Delog(Util::getValueByKey(message, "From"), "Debug Response");
            Event* event = new Event(message, client_socket_fd, true);
            add_event(event);
        }else if(message.substr(0, 8) == "SGOODBYE"){
            // SAY GOODBYE message
            //Util::Delog(selfNodeID(), "Say Goodbye: " + Util::getValueByKey(message, "Reason"));
            // Event* event = new Event(message, socketfd, true);
            // add_event(event);
            bool active = !(getIdByFd(client_socket_fd) == "");
            closeConnection(client_socket_fd);
            if(active){
                // 4. Generate neighbor content 
                string neiContent = "";
                for(map<string, int>::iterator it = nodeidToSockfd.begin(); 
                    it != nodeidToSockfd.end(); ++it)
                {
                    neiContent += it->first + "\n";
                }
                string linkUpdate = Util::genLinkUpdate(stoi(config["ttl"]),
                                                        selfNodeID(),
                                                        neiContent);
                Event* linkUpdEvent = new Event(linkUpdate, 0, false);
                add_event(linkUpdEvent);
            }

            // 5. Check Phase
            // 5.1 check if has famous neighbor
            if(!hasFamousNeighbor()){
                // 5.2 not have famous neighbor, create send request events
                string checkReq = Util::genCheckReq(stoi(config["ttl"]),
                                                    selfNodeID());
                Event* checkReqEvent = new Event(checkReq, 0, false);
                add_event(checkReqEvent);
            }
        }
        else if(message.substr(0, 8) == "UNICAMSG"){
            // pa6
            //Util::Delog(selfNodeID(), "TraceRoute Message");
            Event* event = new Event(message, client_socket_fd, true);
            add_event(event);
        }
    }

    return NULL;
}

void* handle_events(void* arg){
    // handle all events in event q
    while(true){
        Event* event = wait_for_event();
        
        if(event->_content.substr(0, 8) == "DISCOREQ"){
            // discover request event
            // 0. check cache 
            string messageID = Util::getValueByKey(event->_content, "MessageID");
            if(messCache.find(messageID) != messCache.end()){
                // drop the message
                continue;
            }
            messCache.insert(make_pair(messageID, event->_socketfd));
            // 1. generate discorver response message
            
            string disResp = Util::genDisResp(stoi(config["ttl"]), 
                                              messageID, 
                                              selfNodeID(), 
                                              stoi(config["location"]));
                // send it back to event orginator
            write(event->_socketfd, disResp.c_str(), disResp.length());
            // write into log file
            Util::writeLog(logFile, 'i', disResp, getIdByFd(event->_socketfd));

            // 2. send the request to all neighbors
            for(map<string, int>::iterator it = nodeidToSockfd.begin();
                it != nodeidToSockfd.end(); ++it)
            {
                // do not send request again to the sender
                //if(it->second == event->_socketfd) continue;
                string message = Util::decreTTL(event->_content);
                write(it->second, message.c_str(), message.length());
                // write into log file
                Util::writeLog(logFile, 'f', message, it->first);
            }   
        }else if(event->_content.substr(0, 8) == "DISCORSP"){
            // discover response event

            // 1. check cache
            string messageID = Util::getValueByKey(event->_content, "Respond-To");
            if(messCache.find(messageID) != messCache.end()){
                // we find the cache, get the socketfd from its originator
                int socketfd = messCache.find(messageID)->second;
                // decrement TTL by 1 and write to that socket
                string message = Util::decreTTL(event->_content);
                write(socketfd, message.c_str(), message.length());
                
                // write into log file
                Util::writeLog(logFile, 'u', message, getIdByFd(socketfd));
            }
        }else if(event->_content.substr(0, 8) == "LINKSREQ"){
            // Link State Request event
            // 0. Check Cache
            string fromNodeID = Util::getValueByKey(event->_content, "From");
            if(fromNodeID == selfNodeID()) continue;
            string messageID = Util::getValueByKey(event->_content, "MessageID");
            if(messCache.find(messageID) != messCache.end()){
                // drop the message
                continue;
            }

            messCache.insert(make_pair(messageID, event->_socketfd));
            // 1. Add to adjacency list
            
                // update adjacnecy list
            string reqCont = Util::getContent(event->_content);
            vector<string> vec = Util::splitString(reqCont, '\n');
            vector<string> list;
            for(size_t i = 0; i < vec.size()-1; i++){
                list.push_back(vec[i]);
            }
            // Update adjacnecy list
            vector<string> oldList = adjacencyList[fromNodeID];
            for(string s: oldList){
                if(find(list.begin(), list.end(), s) == list.end()){
                    // cannot find that node id, which means that node already lost
                    removeFromAdjList(s, fromNodeID);
                }
            }
            adjacencyList[fromNodeID] = list;


            // 2. Flood the request to all neighbors except for sender
            // [modified] include sender
            for(map<string, int>::iterator it = nodeidToSockfd.begin();
                it != nodeidToSockfd.end(); ++it)
            {
                // do not send request again to the sender
                if(it->second == event->_socketfd) continue;
                string message = Util::decreTTL(event->_content);
                write(it->second, message.c_str(), message.length());
                // write into log file
                Util::writeLog(logFile, 'f', message, it->first);
            }
            // 3. generate Link State Response Message
            string respContent = "";
            for(map<string, int>::iterator it = nodeidToSockfd.begin(); 
                it != nodeidToSockfd.end(); ++it)
            {
                respContent += it->first + "\n";
            }
            string linkResp = Util::genLinkResp(stoi(config["ttl"]),
                                                selfNodeID(),
                                                messageID,
                                                respContent);
            
            // 4. Send Link State Response Message to orginator
            write(event->_socketfd, linkResp.c_str(), linkResp.length());
            // write into log file
            Util::writeLog(logFile, 'i', linkResp, getIdByFd(event->_socketfd));
        }else if(event->_content.substr(0, 8) == "LINKSRSP"){
            // Link State response event
            // 1. Check message Cache
            string messageID = Util::getValueByKey(event->_content, "Respond-To");
            if(messCache.find(messageID) != messCache.end()){
                // we find the cache, get the socketfd from its originator
                int socketfd = messCache.find(messageID)->second;
                // decrement TTL by 1 and write to that socket
                string message = Util::decreTTL(event->_content);
                write(socketfd, message.c_str(), message.length());
                // write into log file
                Util::writeLog(logFile, 'u', message, getIdByFd(socketfd));
            }
            // 2. Check if this message is sent for me
            bool sentForMe = false;
            for(string s: sentMessageID){
                if(s == messageID) sentForMe = true;
            }
            if(sentForMe){
                // this message is for me
                string fromNodeID = Util::getValueByKey(event->_content, "From");
                // update adjacnecy list
                string reqCont = Util::getContent(event->_content);
                vector<string> vec = Util::splitString(reqCont, '\n');
                vector<string> list;
                for(size_t i = 0; i < vec.size()-1; i++){
                    list.push_back(vec[i]);
                }
                // Update adjacnecy list
                vector<string> oldList = adjacencyList[fromNodeID];
                for(string s: oldList){
                    if(find(list.begin(), list.end(), s) == list.end()){
                        // cannot find that node id, which means that node already lost
                        removeFromAdjList(s, fromNodeID);
                    }
                }
                adjacencyList[fromNodeID] = list;
            }
        }else if(event->_content.substr(0, 8) == "LINKSUPD" && event->_send == true){
            // Link State Update event
            // Receive Link State Update from the other node
            // 0. Check Cache
            string messageID = Util::getValueByKey(event->_content, "MessageID");
            if(messCache.find(messageID) != messCache.end()){
                // drop the message
                continue;
            }
            messCache.insert(make_pair(messageID, event->_socketfd));
            // 1. Add to adjacency list
            string fromNodeID = Util::getValueByKey(event->_content, "From");
            string reqCont = Util::getContent(event->_content);
            vector<string> vec = Util::splitString(reqCont, '\n');
            vector<string> list;
            for(size_t i = 0; i < vec.size()-1; i++){
                list.push_back(vec[i]);
            }
            // Update adjacnecy list
            vector<string> oldList = adjacencyList[fromNodeID];
            for(string s: oldList){
                if(find(list.begin(), list.end(), s) == list.end()){
                    // cannot find that node id, which means that node already lost
                    removeFromAdjList(s, fromNodeID);
                }
            }
            adjacencyList[fromNodeID] = list;
            // 2. Flood the update message to all neighbors except for sender
            for(map<string, int>::iterator it = nodeidToSockfd.begin();
                it != nodeidToSockfd.end(); ++it)
            {
                // do not send update message again to the sender
                if(it->second == event->_socketfd) continue;
                string message = Util::decreTTL(event->_content);
                write(it->second, message.c_str(), message.length());
                // write into log file
                Util::writeLog(logFile, 'f', message, it->first);
            }
        }else if(event->_content.substr(0, 8) == "LINKSUPD" && event->_send == false){
            // Link State Update event
            // Need to send link state update to the other nodes
            // 1. Send the Link State Update Message to all the other nodes
            for(map<string, int>::iterator it = nodeidToSockfd.begin();
                it != nodeidToSockfd.end(); ++it)
            {
                write(it->second, event->_content.c_str(), event->_content.length());
                // write into log file
                Util::writeLog(logFile, 'i', event->_content, it->first);
            }
            
        }else if(event->_content.substr(0, 8) == "KEEPALIV"){
            // Keep Alive message received event
            // 1. reset keep alive timer
            //Util::Delog(selfNodeID(), "Reset keep alive timer"); 
            reset_timeout_keepAliveTimer(event->_socketfd);
        }else if(event->_content.substr(0, 8) == "CHECKREQ" && event->_send == false){
            // Check Request Event, false (initiated by myself)
            string messageID = Util::getValueByKey(event->_content, "MessageID");
            // 1. Send Check Request to all neighbors
            for(map<string, int>::iterator it = nodeidToSockfd.begin();
                it != nodeidToSockfd.end(); ++it)
            {
                write(it->second, event->_content.c_str(), event->_content.length());
                // write into log file
                Util::writeLog(logFile, 'i', event->_content, it->first);
            }
            sentMessageID.push_back(messageID); // used for check response later
            // 2. Start Check Timeout timer
            checkTimeoutLabel = getCurrLabel();
            Timeout* checkTimeout = new Timeout("CHECK", 
                                                -1, 
                                                stoi(config["check_timeout"]), 
                                                checkTimeoutLabel);
            add_timeout(checkTimeout);
        }else if(event->_content.substr(0, 8) == "CHECKREQ" && event->_send == true){
            // 0. check the message cache
            string messageID = Util::getValueByKey(event->_content, "MessageID");
            if(messCache.find(messageID) != messCache.end()){
                // drop the message
                continue;
            }
            messCache.insert(make_pair(messageID, event->_socketfd));

            // flood the message
            // 1. send the message to all neighbors except for the sender
            for(map<string, int>::iterator it = nodeidToSockfd.begin();
                it != nodeidToSockfd.end(); ++it)
            {
                // do not send update message again to the sender
                if(it->second == event->_socketfd) continue;
                string message = Util::decreTTL(event->_content);
                write(it->second, message.c_str(), message.length());
                // write into log file
                Util::writeLog(logFile, 'f', message, it->first);
            }

            // 2. Check if I am the famous node, if so, route response back
            if(isFamous()){
                // generate check response
                string checkResp = Util::genCheckResp(stoi(config["ttl"]), 
                                                    messageID);
                // send it back to event orginator
                write(event->_socketfd, checkResp.c_str(), checkResp.length());
                // write into log file
                Util::writeLog(logFile, 'i', checkResp, getIdByFd(event->_socketfd));
            }
    
        }else if(event->_content.substr(0, 8) == "CHECKRSP"){
            // 1. Check the message cache
            string messageID = Util::getValueByKey(event->_content, "Respond-To");
            if(messCache.find(messageID) != messCache.end()){
                // we find the cache, get the socketfd from its originator
                int socketfd = messCache.find(messageID)->second;
                // decrement TTL by 1 and write to that socket
                string message = Util::decreTTL(event->_content);
                write(socketfd, message.c_str(), message.length());
                // write into log file
                Util::writeLog(logFile, 'u', message, getIdByFd(socketfd));
            }
            // 2. If the message is sent for me
            bool sentForMe = false;
            for(string s: sentMessageID){
                if(s == messageID) sentForMe = true;
            }
            if(sentForMe){
                // 2.1 Cancel timer
                remove_timeout(checkTimeoutLabel);
                //Util::Delog(selfNodeID(), "Cancel Check Timer");
            }

        }else if(event->_content.substr(0, 8) == "DEBUGREQ" && event->_send == false){
            // Debug Request Event, false (initiated by myself)
            string messageID = Util::getValueByKey(event->_content, "MessageID");
            // 1. Send Debug Request to all neighbors
            for(map<string, int>::iterator it = nodeidToSockfd.begin();
                it != nodeidToSockfd.end(); ++it)
            {
                write(it->second, event->_content.c_str(), event->_content.length());
                // write into log file
                Util::writeLog(logFile, 'i', event->_content, it->first);
            }
            sentMessageID.push_back(messageID); // used for check response later
            // 2. Start Debug Response Timeout timer
            Timeout* debugTimeout = new Timeout("DEBUG", 
                                                -1, 
                                                stoi(config["check_timeout"]), 
                                                getCurrLabel());
            add_timeout(debugTimeout);
        }else if(event->_content.substr(0, 8) == "DEBUGREQ" && event->_send == true){
            // Debug Request Event, true(received from other nodes)
            string messageID = Util::getValueByKey(event->_content, "MessageID");
            if(messCache.find(messageID) != messCache.end()){
                // drop the message
                continue;
            }
            messCache.insert(make_pair(messageID, event->_socketfd));

            // flood the message
            // 1. send the message to all neighbors except for the sender
            for(map<string, int>::iterator it = nodeidToSockfd.begin();
                it != nodeidToSockfd.end(); ++it)
            {
                // do not send update message again to the sender
                if(it->second == event->_socketfd) continue;
                string message = Util::decreTTL(event->_content);
                write(it->second, message.c_str(), message.length());
                // write into log file
                Util::writeLog(logFile, 'f', message, it->first);
                
            }

            // 3. generate Link State Response Message
            string respContent = "";
            for(map<string, int>::iterator it = nodeidToSockfd.begin(); 
                it != nodeidToSockfd.end(); ++it)
            {
                respContent += it->first + "\n";
            }
            string debugResp = Util::genDebugResp(stoi(config["ttl"]),
                                                selfNodeID(),
                                                messageID,
                                                respContent);
            
            // 4. Send Debug Response Message to orginator
            write(event->_socketfd, debugResp.c_str(), debugResp.length());
            // write into log file
            Util::writeLog(logFile, 'i', debugResp, getIdByFd(event->_socketfd));
        }else if(event->_content.substr(0, 8) == "DEBUGRSP"){
            // Debug Response Event
            // 1. Check message Cache
            string messageID = Util::getValueByKey(event->_content, "Respond-To");
            if(messCache.find(messageID) != messCache.end()){
                // we find the cache, get the socketfd from its originator
                int socketfd = messCache.find(messageID)->second;
                // decrement TTL by 1 and write to that socket
                string message = Util::decreTTL(event->_content);
                write(socketfd, message.c_str(), message.length());
                // write into log file
                Util::writeLog(logFile, 'u', message, getIdByFd(socketfd));
            }
            // 2. Check if this message is sent for me
            bool sentForMe = false;
            for(string s: sentMessageID){
                if(s == messageID) sentForMe = true;
            }
            if(sentForMe){
                // this message is for me
                string fromNodeID = Util::getValueByKey(event->_content, "From");
                // update adjacnecy list
                string reqCont = Util::getContent(event->_content);
                vector<string> vec = Util::splitString(reqCont, '\n');
                vector<string> list;
                for(size_t i = 0; i < vec.size()-1; i++){
                    list.push_back(vec[i]);
                }
                // Update adjacnecy list
                vector<string> oldList = adjacencyList[fromNodeID];
                for(string s: oldList){
                    if(find(list.begin(), list.end(), s) == list.end()){
                        // cannot find that node id, which means that node already lost
                        removeFromAdjList(s, fromNodeID);
                    }
                }
                adjacencyList[fromNodeID] = list;
            }
        }
        // To-do

        // handle timeout event
        if(event->_content == "TIMEOUT_DISCOREQ"){
            // discovery request time out
            // 1. pick the neighbor
            int startNeiNum = stoi(config["num_startup_neighbors"]);
            if(startNeiNum > (int)locToNodeID.size()){
                //Util::Delog(selfNodeID(), "Not enough start up neighbors");
                // sleep for discovery_retry_interval
                this_thread::sleep_for(chrono::seconds(stoi(config["discovery_retry_interval"])));
            }else{
                string outFilePath = config["root"] + "/startup_neighbors.txt";
                ofstream out(outFilePath);
                map<int, string>::iterator it = locToNodeID.begin();
                for(int i = 0; i < startNeiNum; i++){
                    out << it->second << endl;
                    ++it;
                }
            }
            // prepare for restart, finish discorvery mode
            Event* e = new Event("RESTART", -1, false);
            add_event(e);
        }
        else if(event->_content == "TIMEOUT_SAYHELLO"){
            // say hello time out
            // 1. Clean up all stuff
            //Util::Delog(selfNodeID(), "Say Hello Time Out");
            CleanUp();
            // 2. terminate the program
            exit(0);
        }
        else if(event->_content == "RESTART"){
            //Util::Delog(selfNodeID(), "Restart");
            // send say goodbye message to the other node
            string goodbye = Util::genGoodbye(1);
            if(inDisMode && temporary_socket_fd != -1){
                //Util::Delog(selfNodeID(), "Discovery Goodbye");
                write(temporary_socket_fd, goodbye.c_str(), goodbye.length());
                // write into log file
                Util::writeLog(logFile, 'i', goodbye, disNodeID);
            }
            else if(!inDisMode){
                for(map<string, int>::iterator it = nodeidToSockfd.begin();
                    it != nodeidToSockfd.end(); ++it)
                {
                    write(it->second, goodbye.c_str(), goodbye.length());
                    // write into log file
                    Util::writeLog(logFile, 'i', goodbye, it->first);
                }
            }
            delete event;
            // restart signal
            CleanUp();
            restart = true;
            break;
        }
        else if(event->_content == "SHUTDOWN"){
            // send goodbye message
            string goodbye = Util::genGoodbye(0);
            for(map<string, int>::iterator it = nodeidToSockfd.begin();
                it != nodeidToSockfd.end(); ++it)
            {
                write(it->second, goodbye.c_str(), goodbye.length());
                // write into log file
                Util::writeLog(logFile, 'i', goodbye, it->first);
            }
            CleanUp();
            restart = false;
            termin = true;
            delete event;
            break;
        }
        else if(event->_content == "TIMEOUT_KEEPALIVESEND"){
            // time to send keep alive message
            //Util::Delog(selfNodeID(), "Time to send keep alive message");
            string keepAliveMessage = Util::genKeepAlive();
            if(write(event->_socketfd, keepAliveMessage.c_str(), keepAliveMessage.length()) == -1) continue;
            // write into log file
            Util::writeLog(logFile, 'i', keepAliveMessage, getIdByFd(event->_socketfd));

            // Create Keep Alive Send Timeout
            Timeout* keepAliveSend = new Timeout("KEEPALIVESEND", 
                                                event->_socketfd,
                                                stoi(config["keep_alive_timeout"])/2,
                                                getCurrLabel());
            add_timeout(keepAliveSend);
        }else if(event->_content == "TIMEOUT_KEEPALIVEREC"){
            // keep alive message received time out
            // connection lost
            //Util::Delog(selfNodeID(), "Keep Alive Received Time out");
            //cerr << event->_socketfd << endl;
            // Need to close that connection and fload link state update message
            // 1. Close that connection
            // send say goodbye message
            string goodbye = Util::genGoodbye(2);
            write(event->_socketfd, goodbye.c_str(), goodbye.length());
            // write into log file
            Util::writeLog(logFile, 'i', goodbye, getIdByFd(event->_socketfd));
            close(event->_socketfd);
            // 2. Remove from neighbor map: nodeIDTOScoket
            string nodeLost;
            for(map<string, int>::iterator it = nodeidToSockfd.begin();
                it != nodeidToSockfd.end(); ++it)
            {
                //cerr << it->first << ": " << it->second << endl;
                if(it->second == event->_socketfd){
                    nodeLost = it->first;
                    nodeidToSockfd.erase(it);
                    break;
                }
            }
            
            // 3. Remove selfNode from lost node in adjancey list
            // find the nodeLost, remove fromNodeID from that node
            //cerr << "Node lost: " << nodeLost << endl;
            removeFromAdjList(nodeLost, selfNodeID());


            // 4. Generate neighbor content 
            string neiContent = "";
            for(map<string, int>::iterator it = nodeidToSockfd.begin(); 
                it != nodeidToSockfd.end(); ++it)
            {
                neiContent += it->first + "\n";
            }
            string linkUpdate = Util::genLinkUpdate(stoi(config["ttl"]),
                                                    selfNodeID(),
                                                    neiContent);
            Event* linkUpdEvent = new Event(linkUpdate, 0, false);
            add_event(linkUpdEvent);

            // 5. Check Phase
            // 5.1 check if has famous neighbor
            if(!hasFamousNeighbor()){
                // 5.2 not have famous neighbor, create send request events
                string checkReq = Util::genCheckReq(stoi(config["ttl"]),
                                                    selfNodeID());
                Event* checkReqEvent = new Event(checkReq, 0, false);
                add_event(checkReqEvent);
            }

        }else if(event->_content == "TIMEOUT_CHECK"){
            // not in primary partition
            // 1. delete startup_neighbors.txt
            string path = config["root"] + "/startup_neighbors.txt";
            remove(path.c_str());
            // 2. restart
            Event* e = new Event("RESTART", -1, false);
            add_event(e);
        }

        // pa6 handle unicast message event
        if(event->_content.substr(0, 8) == "UNICAMSG"){
            //cerr << endl << event->_content << endl;
            string content = Util::getContent(event->_content);
            // unicast request, false: initiated by self
            if(content.find("Traceroute-Request") != string::npos && event->_send == false){
                string dest_node = Util::getValueByKey(event->_content, "To");
                //Util::Delog(selfNodeID() + "->" + dest_node, "UNICAMSG Send, ttl = " + Util::getValueByKey(event->_content, "TTL"));
                string nextHopNode = nextHopTo(dest_node);
                if(nextHopNode == ""){
                    cerr << dest_node << " is not in the forwarding table\n";
                    continue;
                }
                write(nodeidToSockfd[nextHopNode], event->_content.c_str(), event->_content.length());
                Util::writeLog(logFile, 'i', event->_content, nextHopNode);
                // start the timeout
                Timeout* uniTimeout = new Timeout("UNICAMSG", 
                                                  nodeidToSockfd[nextHopNode],
                                                  stoi(config["msg_life_time"]),
                                                  UNI_TIMEOUT_LABEL);
                uniTimeout->_content = event->_content;
                add_timeout(uniTimeout);
            }
            else if(content.find("Traceroute-Request") != string::npos && event->_send == true){
                // Traceroute Request Event, true(received from other nodes)
                string messageID = content.substr(content.find("=") + 1);
                if(messCache.find(messageID) != messCache.end()){
                    // drop the message
                    continue;
                }
                messCache.insert(make_pair(messageID, event->_socketfd));

                // 1. decrease ttl
                string message = Util::decreUniTTL(event->_content);
                
                // 2. check if this message is sent to me
                if(Util::getValueByKey(message, "To") == selfNodeID()){
                    // This message is sent to me
                    // generate uni response message
                    // find msgid
                    size_t found = content.find("=");
                    string msgid = content.substr(found+1);
                    string uniResp = Util::genUniResp(stoi(config["ttl"]),
                                                      selfNodeID(),
                                                      Util::getValueByKey(message, "From"),
                                                      msgid);
                    // send response back
                    write(event->_socketfd, uniResp.c_str(), uniResp.length());
                    Util::writeLog(logFile, 'i', uniResp, getIdByFd(event->_socketfd));
                    //cerr << "Send response back\n";
                }else{
                    // 3. Not sent to me
                    // 3.1 check ttl
                    if(stoi(Util::getValueByKey(message, "TTL")) <= 0){
                        // generate zero ttl message
                        size_t found = content.find("=");
                        string msgid = content.substr(found+1);
                        string uniZero = Util::genUniZero(stoi(config["ttl"]),
                                                          selfNodeID(),
                                                          Util::getValueByKey(message, "From"),
                                                          msgid);
                        // send zero ttl message back
                        write(event->_socketfd, uniZero.c_str(), uniZero.length());
                        Util::writeLog(logFile, 'i', uniZero, getIdByFd(event->_socketfd));
                    }else{
                        // 3.2 ttl > 0
                        // send to the next hop node
                        string dest_node = Util::getValueByKey(event->_content, "To");
                        string nextHopNode = nextHopTo(dest_node);
                        //cerr << endl << message << endl;
                        write(nodeidToSockfd[nextHopNode], message.c_str(), message.length());
                        Util::writeLog(logFile, 'u', message, nextHopNode);
                        
                    }
                } 
            }
            else if(content.find("Traceroute-Response") != string::npos){
                // receive response message
                // 1. if sent for me
                string dest_node = Util::getValueByKey(event->_content, "To");
                if(dest_node == selfNodeID()){
                    // 1.1 cancel timer
                    remove_timeout(UNI_TIMEOUT_LABEL);
                    // 1.2 calculate time passed
                    timeval curr_tim;
                    gettimeofday(&curr_tim, NULL);
                    double timePassed = Util::difftime(start_tim, curr_tim);
                    cerr << uni_ttl << " " << Util::getValueByKey(event->_content, "From") << " " << timePassed << "s" << endl;
                    continue;
                }

                // find message id
                size_t found = content.find("=");
                string msgid = content.substr(found+1);
                if(messCache.find(msgid) != messCache.end()){
                    // we find the cache, get the socketfd from its originator
                    int socketfd = messCache.find(msgid)->second;
                    // decrement TTL by 1 and write to that socket
                    string message = Util::decreUniTTL(event->_content);
                    write(socketfd, message.c_str(), message.length());
                    // write into log file
                    //Util::writeLog(logFile, 'u', message, getIdByFd(socketfd));
                    Util::writeLog(logFile, 'u', message, getIdByFd(socketfd));
                    
                }

            }
            else if(content.find("Traceroute-ZeroTTL") != string::npos){
                // receive zero ttl message
                // 1. if sent for me
                string dest_node = Util::getValueByKey(event->_content, "To");
                if(dest_node == selfNodeID()){
                    // 1.1 cancel timer
                    remove_timeout(UNI_TIMEOUT_LABEL);
                    // 1.2 calculate time passed
                    timeval curr_tim;
                    gettimeofday(&curr_tim, NULL);
                    double timePassed = Util::difftime(start_tim, curr_tim);
                    cerr << uni_ttl << " " << Util::getValueByKey(event->_content, "From") << " " << timePassed << "s" << endl;

                    // 1.3 re-send traceroute request with higher ttl
                    uni_ttl++; // increase uni_ttl
                    string uniReq = Util::genUniReq(uni_ttl, selfNodeID(), DEST_NODE);
                    Event* uniReqEvent = new Event(uniReq, 0, false);
                    add_event(uniReqEvent);
                    gettimeofday(&start_tim, NULL);
                    continue;
                }

                // 2. not sent for me, route back
                // find message id
                size_t found = content.find("=");
                string msgid = content.substr(found+1);
                if(messCache.find(msgid) != messCache.end()){
                    // we find the cache, get the socketfd from its originator
                    int socketfd = messCache.find(msgid)->second;
                    // decrement TTL by 1 and write to that socket
                    string message = Util::decreUniTTL(event->_content);
                    write(socketfd, message.c_str(), message.length());
                    // write into log file
                    Util::writeLog(logFile, 'u', message, getIdByFd(socketfd));
                }
            }


        }

        delete event;
    }
    return NULL;
}

string readMessage(int socketfd){
    string message = "";
    char buf[1];
    while(read(socketfd, buf, sizeof(buf)) > 0){
        message += buf[0];
        // check if reach the end of header
        int length = message.length();  
        if(length >= 4 && message.substr(length-4) == "\r\n\r\n"){
            // reach the end of the header
            // need to get the content length
            string value = Util::getValueByKey(message, "Content-Length");
            if(value == "") cerr << "Malformed Message\n";
            else if(stoi(value) == 0) break; // no content
            else{
                // read the left content
                for(int i = 0; i < stoi(value); i++){
                    read(socketfd, buf, sizeof(buf));
                    message += buf[0];
                }
            }
            break;
        }
    }
    return message;
}

void addNeighbor(string nodeID){
    for(size_t i = 0; i < neighbor.size(); i++){
        if(strcmp(nodeID.c_str(), neighbor[i].c_str()) < 0){
            neighbor.insert(neighbor.begin() + i, nodeID);
            return;
        }
    }
    neighbor.push_back(nodeID);
}

void printNeighbors(){
    if(nodeidToSockfd.size() == 0){
        cerr << selfNodeID() << " has no neighbors\n";
        return;
    }
    cerr << "Neighbors of " << selfNodeID() << " are:\n";
    for(map<string, int>::iterator it = nodeidToSockfd.begin();
        it != nodeidToSockfd.end(); ++it){
        cerr << "\t" << it->first << "\n";
    }
}

void *regular_connect_discover(void *arg){
    inDisMode = true;
    string hostname = "127.0.1.1";
    string port = to_string((int)(unsigned long)arg);
    string str = hostname + "_" + port;
    disNodeID = str;
    
    // preparation
    char server_name[256];
    int client_socket_fd;
    struct addrinfo hints;
    struct addrinfo* res;
    struct sockaddr_in soc_address;
    strncpy(server_name, hostname.c_str(), sizeof(hostname));
    memset(&hints,0,sizeof(hints));
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = 0;
    hints.ai_flags = AI_NUMERICSERV|AI_ADDRCONFIG;
    getaddrinfo(server_name, port.c_str(), &hints, &res);
    client_socket_fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    memset(&soc_address, 0, sizeof(soc_address));
    if (*server_name >= '0' && *server_name <= '9') {
        soc_address.sin_addr.s_addr = inet_addr(server_name);
    } else {
        struct hostent *p_hostent;

        p_hostent = gethostbyname(server_name);
        memcpy(&soc_address.sin_addr, p_hostent->h_addr, p_hostent->h_length);
    }
    soc_address.sin_family = AF_INET;
    soc_address.sin_port = htons((unsigned short)stoi(port));
    // preparation finish
    while(connect(client_socket_fd, (struct sockaddr*)&soc_address, sizeof(soc_address)) < 0)
    {
        // connection fail, retry after interval
        //Util::Delog(str, "Discorvery Retry");
        //this_thread::sleep_for(chrono::seconds(stoi(config["discovery_retry_interval"])));
        return NULL;
    }
    temporary_socket_fd = client_socket_fd;
    //Util::Delog(str, "Connect to famous node");
    // send discory request
    string disRequest = Util::genDisReq(stoi(config["ttl"]), selfNodeID());
    write(client_socket_fd, disRequest.c_str(), disRequest.length());
    // write into log file
    Util::writeLog(logFile, 'i', disRequest, str);
        // add timeout to time out list
    Timeout* timeout = new Timeout("DISCOREQ", 
                                    client_socket_fd, 
                                    stoi(config["discovery_timeout"]), 
                                    getCurrLabel());
    add_timeout(timeout);
    string requestID = Util::getValueByKey(disRequest, "MessageID");

    

    // TO-DO
    while(true){
        string disResp = readMessage(client_socket_fd);
        if(disResp == ""){
            closeConnection(client_socket_fd);
            //Util::Delog(str, "Server socket shutdown");
            break;
        }
        // TO-DO
        // write into log file
        Util::writeLog(logFile, 'r', disResp, str);

        if(requestID ==  Util::getValueByKey(disResp, "Respond-To")){
            string nodeID = Util::getValueByKey(disResp, "From");
            int location = abs(stoi(Util::getLocation(disResp)) - stoi(config["location"]));
            locToNodeID.insert(make_pair(location, nodeID));
        }  
    }
    // process by time out and event queue
    return NULL;
}

int getCurrLabel(){
    {
        lock_guard<mutex> guard(main_mutex);
        int ret = nextLabel;
        nextLabel.fetch_add(1);
        return ret;
    }
}

void CleanUp(){
    // clean event queue
    
    // 1. close all socket, remove from map
    for(map<string, int>::iterator it = nodeidToSockfd.begin();
        it != nodeidToSockfd.end();
        ++it)
    {
        close(it->second);
        // nodeidToSockfd.erase(it);
    }
    nodeidToSockfd.clear();
    if(temporary_socket_fd != -1){
        close(temporary_socket_fd);
        temporary_socket_fd = -1;
    }
    // if(master_socket_fd != -1){
    //     close(master_socket_fd);
    //     master_socket_fd = -1;
    // }
    close(master_socket_fd);

    // 2. Clear time out list
    pthread_mutex_lock(&timeout_m);
    for(Timeout* t: timeoutList){
        delete t;
        t = nullptr;
    }
    timeoutList.clear();
    pthread_mutex_unlock(&timeout_m);

    // 3. Clear the event queue
    pthread_mutex_lock(&m);
    while(!q.empty()){
        Event* temp = q.front();
        delete temp;
        temp = nullptr;
        q.pop();
    }
    pthread_mutex_unlock(&m);

    // 4. clear message cache
    messCache.clear();

    // 5. Clear locToNodeID
    locToNodeID.clear();

    // 6. reset label counter
    nextLabel = 1;

    // 7. clear neighbor vector
    neighbor.clear();

    // 8. Clear sent message id vector
    sentMessageID.clear();

    // 9. Cancel handle_timeout thread
    pthread_cancel(handle_timeouts_id);
    // 10. Cancel handle_event thread
    pthread_cancel(handle_events_id);
    // 11. Reset the label
    checkTimeoutLabel = 0;
    // 12. Cancel open_socket
    pthread_cancel(open_socket_id);

}

void* regular_connect_participate(void* arg){
    inDisMode = false;
    string hostname = config["host"];
    string port = to_string((int)(unsigned long)arg);
    string str = hostname + "_" + port;

    // preparation
    char server_name[256];
    int client_socket_fd;
    struct addrinfo hints;
    struct addrinfo* res;
    struct sockaddr_in soc_address;
    strncpy(server_name, hostname.c_str(), sizeof(hostname));
    memset(&hints,0,sizeof(hints));
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = 0;
    hints.ai_flags = AI_NUMERICSERV|AI_ADDRCONFIG;
    getaddrinfo(server_name, port.c_str(), &hints, &res);
    client_socket_fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    memset(&soc_address, 0, sizeof(soc_address));
    if (*server_name >= '0' && *server_name <= '9') {
        soc_address.sin_addr.s_addr = inet_addr(server_name);
    } else {
        struct hostent *p_hostent;

        p_hostent = gethostbyname(server_name);
        memcpy(&soc_address.sin_addr, p_hostent->h_addr, p_hostent->h_length);
    }
    soc_address.sin_family = AF_INET;
    soc_address.sin_port = htons((unsigned short)stoi(port));
    // preparation finish
    
    while(connect(client_socket_fd, (struct sockaddr*)&soc_address, sizeof(soc_address)) < 0)
    {
        // connection fail, retry after interval
        //Util::Delog(str, "Participate Retry");
        this_thread::sleep_for(chrono::seconds(stoi(config["discovery_retry_interval"])));
    }
    //Util::Delog(str, "Participate Mode");
    // already connected, send hello message
    string hello = Util::genHello(selfNodeID(), config["keep_alive_timeout"]);
    write(client_socket_fd, hello.c_str(), hello.length());
    // write into log file
    Util::writeLog(logFile, 'i', hello, str);
    
    // generate say hello time out
    int timeoutLabel = getCurrLabel();
    Timeout* helloTimeout = new Timeout("SAYHELLO", 
                                    client_socket_fd, 
                                    stoi(config["keep_alive_timeout"]), 
                                    timeoutLabel);
    add_timeout(helloTimeout);
    string response = readMessage(client_socket_fd); // get immediate response
    Util::writeLog(logFile, 'r', response, str);
    if(response.substr(0, 8) == "SAYHELLO"){
        remove_timeout(timeoutLabel);
        //Util::Delog(str, "Connection established");
        nodeidToSockfd.insert(make_pair(str, client_socket_fd));
        //addNeighbor(str);
    }else{
        // response not SYAHELLO
        closeConnection(client_socket_fd);
        return NULL;
    }

    // Create Keep Alive Send Timeout
    Timeout* keepAliveSend = new Timeout("KEEPALIVESEND", 
                                         client_socket_fd,
                                         stoi(config["keep_alive_timeout"])/2,
                                         getCurrLabel());
    add_timeout(keepAliveSend);
    // Create Keep Aive Receive Timeout
    int KEEPALIVE_LABEL = getCurrLabel(); // KEEPALIVE_LABEL is used for this connection to 
                                          // check keep alive receive timeout, const
    Timeout* keepAliveRec = new Timeout("KEEPALIVEREC", 
                                         client_socket_fd,
                                         stoi(config["keep_alive_timeout"]),
                                         KEEPALIVE_LABEL);
    add_timeout(keepAliveRec);
    
    // send Link State Request Message
    // 1. generate Link State Request Message
    string neiContent = "";
    for(map<string, int>::iterator it = nodeidToSockfd.begin(); 
        it != nodeidToSockfd.end(); ++it)
    {
        neiContent += it->first + "\n";
    }
    string linkReq = Util::genLinkReq(stoi(config["ttl"]), selfNodeID(), neiContent);
    // 2. flood the message to this connection
    write(client_socket_fd, linkReq.c_str(), linkReq.length());
    // write into log file
    Util::writeLog(logFile, 'i', linkReq, getIdByFd(client_socket_fd));

    // 3. Add the message id to sentMessageID, later check for response
    sentMessageID.push_back(Util::getValueByKey(linkReq, "MessageID"));
    
    // start continuous reading from this socket
    while(true){
        string message = readMessage(client_socket_fd);
        // cerr << message << endl;
        if(message == ""){
            close(client_socket_fd);
            break;
        }
        // write into log file
        Util::writeLog(logFile, 'r', message, getIdByFd(client_socket_fd));

        // package into Event
        if(message.substr(0, 8) == "DISCORSP"){
            // discover response message
            //Util::Delog(str, "Discover Response");
            Event* event = new Event(message, client_socket_fd, true);
            add_event(event);
        }else if(message.substr(0, 8) == "DISCOREQ"){
            // discover requset message
            //Util::Delog(str, "Discover Request");
            Event* event = new Event(message, client_socket_fd, true);
            add_event(event);
        }else if(message.substr(0, 8) == "LINKSRSP"){
            // Linke State Response Message
            //Util::Delog(str, "Link State Response Message");
            Event* event = new Event(message, client_socket_fd, true);
            add_event(event);
        }
        else if(message.substr(0, 8) == "LINKSUPD"){
            // Link State Update message
            //Util::Delog(Util::getValueByKey(message, "From"), "Link State Update");
            Event* event = new Event(message, client_socket_fd, true);
            add_event(event);
        }
        else if(message.substr(0, 8) == "LINKSREQ"){
            // Link State Request message
            //Util::Delog(Util::getValueByKey(message, "From"), "Link State Requset");
            Event* event = new Event(message, client_socket_fd, true);
            add_event(event);
        }
        else if(message.substr(0, 8) == "KEEPALIV"){
            // Keep Alive message
            Event* event = new Event(message, client_socket_fd, true);
            add_event(event);
        }else if(message.substr(0, 8) == "CHECKREQ"){
            // Check Request
            //Util::Delog(selfNodeID(), "Check Request");
            Event* event = new Event(message, client_socket_fd, true);
            add_event(event);
        }else if(message.substr(0, 8) == "CHECKRSP"){
            // Check Response
            //Util::Delog(selfNodeID(), "Check Response");
            Event* event = new Event(message, client_socket_fd, true);
            add_event(event);
        }else if(message.substr(0, 8) == "DEBUGREQ"){
            // Link State Request message
            //Util::Delog(Util::getValueByKey(message, "From"), "Debug Request");
            Event* event = new Event(message, client_socket_fd, true);
            add_event(event);
        }else if(message.substr(0, 8) == "DEBUGRSP"){
            // Link State Request message
            //Util::Delog(Util::getValueByKey(message, "From"), "Debug Response");
            Event* event = new Event(message, client_socket_fd, true);
            add_event(event);
        }else if(message.substr(0, 8) == "SGOODBYE"){
            // SAY GOODBYE message
            //Util::Delog(selfNodeID(), "Say Goodbye: " + Util::getValueByKey(message, "Reason"));
            bool active = !(getIdByFd(client_socket_fd) == "");
            closeConnection(client_socket_fd);
            if(active){
                // 4. Generate neighbor content 
                string neiContent = "";
                for(map<string, int>::iterator it = nodeidToSockfd.begin(); 
                    it != nodeidToSockfd.end(); ++it)
                {
                    neiContent += it->first + "\n";
                }
                string linkUpdate = Util::genLinkUpdate(stoi(config["ttl"]),
                                                        selfNodeID(),
                                                        neiContent);
                Event* linkUpdEvent = new Event(linkUpdate, 0, false);
                add_event(linkUpdEvent);
            }

            // 5. Check Phase
            // 5.1 check if has famous neighbor
            if(!hasFamousNeighbor()){
                // 5.2 not have famous neighbor, create send request events
                string checkReq = Util::genCheckReq(stoi(config["ttl"]),
                                                    selfNodeID());
                Event* checkReqEvent = new Event(checkReq, 0, false);
                add_event(checkReqEvent);
            }
        }else if(message.substr(0, 8) == "UNICAMSG"){
            // pa6
            //Util::Delog(selfNodeID(), "TraceRoute Message");
            //cerr << endl << message << endl;
            Event* event = new Event(message, client_socket_fd, true);
            add_event(event);
        }
        // To-do
    }

    return NULL;
}

void closeConnection(int socketfd){
    string nodeID;
    for(map<string,int>::iterator it = nodeidToSockfd.begin();
        it != nodeidToSockfd.end(); ++it)
    {
        if(it->second == socketfd){
            nodeID = it->first;
            nodeidToSockfd.erase(it);
            break;
        }
    }
    // [modified] remove timeout event related to that node
    pthread_mutex_lock(&timeout_m);
    for(size_t i = 0; i < timeoutList.size(); i++){
        if(timeoutList[i] != nullptr && timeoutList[i]->_socketfd == socketfd){
            delete timeoutList[i];
            timeoutList[i] = nullptr;
        }
    }
    pthread_mutex_unlock(&timeout_m);
    close(socketfd);

    // remove from adjancecy list
    if(adjacencyList.find(nodeID) != adjacencyList.end()){
        adjacencyList[nodeID].erase(find(adjacencyList[nodeID].begin(),
                                adjacencyList[nodeID].end(),
                                selfNodeID()));
    }
    
}
void closeConnection(string nodeID){
    if(nodeidToSockfd.find(nodeID) == nodeidToSockfd.end()) return;
    int socketfd = nodeidToSockfd[nodeID];
    nodeidToSockfd.erase(nodeidToSockfd.find(nodeID));
    close(socketfd);
}

void printNetgraph(){
    if(inDisMode){
        cerr << selfNodeID() << " is in discovery mode\n";
        return;
    }
    if(nodeidToSockfd.size() == 0){
        cerr << selfNodeID() << " has no neighbors\n";
        return;
    }
    map<string, vector<string>> temp = adjacencyList;
    vector<string> vec;
    for(map<string, int>::iterator it = nodeidToSockfd.begin(); 
        it != nodeidToSockfd.end(); ++it)
    {
        vec.push_back(it->first);
    }
    temp[selfNodeID()] = vec;
    for(map<string, vector<string>>::iterator it = temp.begin();
        it != temp.end(); ++it)
    {
        if(it->second.size() == 0) continue;
        cerr << it->first << ": ";
        for(size_t i = 0; i < it->second.size(); i++){
            if(i == it->second.size() - 1){
                cerr << it->second[i] << endl;
            }else{
                cerr << it->second[i] << ", ";
            }
        }
    }


    // bool printSelf = nodeidToSockfd.size() == 0;
    // for(map<string, vector<string>>::iterator it = adjacencyList.begin();
    //     it != adjacencyList.end(); ++it)
    // {   
    //     if(it->second.size() == 0) continue;
    //     if(!printSelf && (strcmp(selfNodeID().c_str(), it->first.c_str()) < 0)){
    //         cerr << selfNodeID() << ":";
    //         for(map<string, int>::iterator itMy = nodeidToSockfd.begin();
    //             itMy != nodeidToSockfd.end(); ++itMy)
    //         {
    //             if(itMy == nodeidToSockfd.begin()){
    //                 cerr << " " << it->first;
    //             }else{
    //                 cerr << ", " << it->first; 
    //             }
    //         }
    //         cerr << endl;
    //         printSelf = true;

    //         cerr << it->first << ": ";
    //         for(size_t i = 0; i < it->second.size(); i++){
    //             if(i == it->second.size() - 1){
    //                 cerr << it->second[i] << endl;
    //             }else{
    //                 cerr << it->second[i] << ", ";
    //             }
    //         }
    //         continue;
    //     }
    //     cerr << it->first << ": ";
    //     for(size_t i = 0; i < it->second.size(); i++){
    //         if(i == it->second.size() - 1){
    //             cerr << it->second[i] << endl;
    //         }else{
    //             cerr << it->second[i] << ", ";
    //         }
    //     }
        
    // }
    // if(!printSelf){
    //     cerr << selfNodeID() << ":";
    //     for(map<string, int>::iterator it = nodeidToSockfd.begin();
    //         it != nodeidToSockfd.end(); ++it)
    //     {
    //         if(it == nodeidToSockfd.begin()){
    //             cerr << " " << it->first;
    //         }else{
    //             cerr << ", " << it->first;
    //         }
    //     }
    //     cerr << endl;
    // }
}

// A as key, B as remove target
void removeFromAdjList(string nodeA, string nodeB){
    for(size_t i = 0; i < adjacencyList[nodeA].size(); i++){
        if(adjacencyList[nodeA][i] == nodeB){
            // remove this node
            adjacencyList[nodeA].erase(adjacencyList[nodeA].begin() + i);
            return;
        }
    }
} 

bool hasFamousNeighbor(){  
    if(isFamous()) return true;
    vector<string> vec;
    for(int i = 0; i < stoi(config["count"]); i++){
        string famousNodeID = config[to_string(i)];
        vec.push_back(famousNodeID);
    }
    
    for(map<string, int>::iterator it = nodeidToSockfd.begin();
        it != nodeidToSockfd.end(); ++it)
    {
        if(find(vec.begin(), vec.end(), it->first) != vec.end()) return true;
    }
    return false;
}

string getIdByFd(int socketfd){
    for(map<string, int>::iterator it = nodeidToSockfd.begin();
        it != nodeidToSockfd.end(); ++it)
    {
        if(it->second == socketfd) return it->first;
    }
    return "";
}

string nextHopTo(string dest_node){
    // do bfs on the net graph
    queue<string> bfs_q;
    map<string, string> nodeToParent;
    set<string> visited;
    visited.insert(selfNodeID());
    for(map<string, int>::iterator it = nodeidToSockfd.begin();
        it != nodeidToSockfd.end(); ++it)
    {
        bfs_q.push(it->first);
        nodeToParent.insert(make_pair(it->first, selfNodeID()));
    }

    bool hasPath = false;

    while(!bfs_q.empty()){
        string node = bfs_q.front();
        bfs_q.pop();
        visited.insert(node);
        if(node == dest_node) {
            hasPath = true;
            break;
        }
        if(adjacencyList.find(node) != adjacencyList.end()){
            for(string s: adjacencyList[node]){
                if(visited.find(s) != visited.end()) continue;
                bfs_q.push(s);
                nodeToParent.insert(make_pair(s, node));
            }
        }
    }

    if(!hasPath) return "";

    // trace back to the src node
    string nextNode = dest_node;
    string currNode;
    while(nextNode != selfNodeID()){
        currNode = nextNode;
        nextNode = nodeToParent[currNode];
    }
    return currNode;
}

void printForwardingTable(){
    if(nodeidToSockfd.empty()){
        cerr << selfNodeID() << " has no neighbors\n";
    }

    cerr << "Forwarding table of " << selfNodeID() << ":\n";

    for(map<string, vector<string>>::iterator it = adjacencyList.begin();
        it != adjacencyList.end(); ++it)
    {
        if(it->second.empty()) continue;
        cerr << "\t" << it->first << " " << nextHopTo(it->first) << endl;
    }
}