#include "simppl/detail/signalrecipient.h"

#include "simppl/detail/frames.h"
#include "simppl/detail/util.h"


void SignalSender::operator()(const SignalRecipient& info)
{
   SignalEmitFrame f(info.clientsid_);
   f.payloadsize_ = len_;
   
   // FIXME need to remove socket on EPIPE 
   genericSend(info.fd_, f, data_);
}