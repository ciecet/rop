#include <string>
#include <vector>
#include "Echo.h"
string EchoImpl::echo (string msg)
{
    return msg;
}

string EchoImpl::concat (vector<string> msgs)
{
    string ret;
    for (vector<string>::iterator i = msgs.begin(); i != msg.end(); i++) {
        ret += *i;
    }
    return ret;
}
