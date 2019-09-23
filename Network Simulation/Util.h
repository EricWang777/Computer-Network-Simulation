#include <string>
#include <vector>
#include <fstream>
using namespace std;

class Util{
public:
    static string genDisReq(int ttl, string nodeID);
    static string genDisResp(int ttl, string msgid, string nodeID, int location);
    static string genHello(string nodeID, string keepAlive);
    static string genKeepAlive();
    static string genCheckReq(int ttl, string nodeID);
    static string genCheckResp(int ttl, string msgid);
    static string genGoodbye(int reason);
    static string genLinkReq(int ttl, string nodeID, string content);
    static string genLinkResp(int ttl, string nodeID, string msgid, string content);
    static string genLinkUpdate(int ttl, string nodeID, string content);
    static string genDebugReq(int ttl, string nodeID, string content);
    static string genDebugResp(int ttl, string nodeID, string msgid, string content);
    static string timeStamp();
    static string getValueByKey(string message, string key);
    static void Delog(string nodeID, string message);
    static string decreTTL(string message);
    static string decreUniTTL(string message);
    static vector<string> splitString(string str, char delimiter);
    static string getLocation(string message);
    static string getContent(string message);
    static void writeLog(ofstream& log, char category, string message, string nodeID);
    static string genUniReq(int ttl, string src_nodeID, string dest_nodeID);
    static string genUniResp(int ttl, string src_nodeID, string dest_nodeID, string msgid);
    static string genUniZero(int ttl, string src_nodeID, string dest_nodeID, string msgid);
    static double difftime(timeval& start, timeval& end);

};

