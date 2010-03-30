#include "protocol/al_OSC.hpp"

//namespace al{
namespace osc{

void RecvPacket::print(FILE * f) const {
	fprintf(f,"[%15s %d] %s %s\n", remoteIP().c_str(), remotePort(), addr().c_str(), tags().c_str());
}

void RecvPacket::printVerbose(FILE * f) const {
	fprintf(f,"sender:    %s %d\n", remoteIP().c_str(), remotePort());
	fprintf(f,"time:      %lld\n", time);
	fprintf(f,"arguments: %ld\n", argc());
	fprintf(f,"message:   %s %s\n", addr().c_str(), tags().c_str());
}



OSCRecv::OSCRecv(networkRecvCB cb, void * userData)
:	callback(cb), user(userData), mPort(0), mTime(1), mStarted(false)
{}

OSCRecv::OSCRecv(unsigned int port, networkRecvCB cb, void * userData)
:	callback(cb), user(userData), mPort(port), mTime(1), mStarted(false)
{}

OSCRecv::~OSCRecv(){ stop(); }

OSCRecv& OSCRecv::port(unsigned int p){
	mPort = p;
	if(started()){ stop(); start(); }
	return *this;
}

void OSCRecv::ProcessMessage(const OSCMsg& m, const NetAddr& remote){
	if(callback) callback(RecvPacket(m, remote, mTime), user);
}

void OSCRecv::ProcessBundle(const OSCBundle& b, const NetAddr& remote){
	mTime = b.TimeTag();
	OscPacketListener::ProcessBundle(b, remote);
}

void OSCRecv::start(){
	if(started()) return;
	mStarted = true;
	mThread.start(threadFunc, this);
}

void OSCRecv::stop(){
	if(started() && mSocket){
		mSocket->AsynchronousBreak();
		mThread.wait();
	}
	mStarted=false;
}

THREAD_FUNCTION(OSCRecv::threadFunc){
	OSCRecv * oscRecv = (OSCRecv *)user;
	UdpListeningReceiveSocket listeningSocket(
		NetAddr(NetAddr::ANY_ADDRESS, oscRecv->port()), oscRecv);
	oscRecv->mSocket = &listeningSocket;
	listeningSocket.Run();
	oscRecv->mSocket = 0;
	return 0;
}




OSCSend::OSCSend(int maxPacketSizeBytes)
:	mBuffer(0), mStream(0)
{
	maxPacketSize(maxPacketSizeBytes);
}

OSCSend::OSCSend(const char * remoteIP, int port, int maxPacketSizeBytes)
:	mBuffer(0), mStream(0)
{
	maxPacketSize(maxPacketSizeBytes);
	add(remoteIP, port);
}

OSCSend::~OSCSend(){ delete[] mBuffer; delete mStream; }

OSCSend& OSCSend::add(const char * remoteIP, int port){
	mEndpoints.push_back(NetAddr(remoteIP, port));
	return *this;
}

OSCSend& OSCSend::remove(const char * remoteIP, int port){

	NetAddr v(remoteIP, port);
	
	std::vector<NetAddr>::iterator it = mEndpoints.begin();
	for(; it<mEndpoints.end(); it++){
		if(v == *it) mEndpoints.erase(it);
	}
	
	return *this;
}

void OSCSend::maxPacketSize(int bytes){
	if(mBuffer) delete[] mBuffer;
	mBuffer = new char[bytes];
	
	if(mStream) delete mStream;
	mStream = new osc::OutboundPacketStream(mBuffer, bytes);

	mStream->Clear();
}

/// Sends the current outbound packet and then clears it.
void OSCSend::send(){
	//for(int i=0; i<mStream->Size(); ++i) printf("%c", mStream->Data()[i]); printf("\n");
	
//	mSocket.Connect(IpEndpointName("127.0.0.1", 12000));
//	mSocket.Send(mStream->Data(), mStream->Size());
	
	for(unsigned i=0; i<mEndpoints.size(); ++i){
		//printf("%d %d\n", mEndpoints[i].address, mEndpoints[i].port);
		mSocket.SendTo(mEndpoints[i], mStream->Data(), mStream->Size());
	}
	mStream->Clear();
}

} // osc::
//} // al::
