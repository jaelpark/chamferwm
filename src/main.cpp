#include "main.h"
#include "backend.h"
#include "compositor.h"

#include <cstdlib>
#include <stdarg.h>
#include <time.h>

#include <args.hxx>
#include <iostream>

#include <sys/epoll.h>
#include <signal.h>
#include <sys/signalfd.h>

typedef unsigned int uint;
typedef int sint;

Exception::Exception(){
	this->pmsg = buffer;
}

Exception::Exception(const char *pmsg){
	this->pmsg = pmsg;
}

Exception::~Exception(){
	//
}

const char * Exception::what(){
	return pmsg;
}

char Exception::buffer[4096];

void DebugPrintf(FILE *pf, const char *pfmt, ...){
	time_t rt;
	time(&rt);
	const struct tm *pti = localtime(&rt);

	char tbuf[256];
	strftime(tbuf,sizeof(tbuf),"[xwm %F %T]",pti);
	fprintf(pf,"%s ",tbuf);

	va_list args;
	va_start(args,pfmt);
	if(pf == stderr)
		fprintf(pf,"Error: ");
	vfprintf(pf,pfmt,args);
	va_end(args);
}

class RunBackend : public Backend::Default{
public:
	RunBackend() : Default(){
		Start();
		DebugPrintf(stdout,"Backend initialized.\n");
	}
	
	~RunBackend(){}

	void DefineBindings(){
		DebugPrintf(stdout,"DefineKeybindings()\n");
	}
};

class FakeBackend : public Backend::Fake{
public:
	FakeBackend() : Fake(){
		Start();
		DebugPrintf(stdout,"Backend initialized.\n");
	}

	~FakeBackend(){}

	void DefineBindings(){
		DebugPrintf(stdout,"DefineKeybindings()\n");
	}
};

class RunCompositor : public Compositor::X11Compositor{
public:
	RunCompositor(uint gpuIndex, Backend::X11Backend *pbackend) : X11Compositor(gpuIndex,pbackend){
		Start();
		DebugPrintf(stdout,"Compositor enabled.\n");
	}

	~RunCompositor(){}
};

int main(sint argc, const char **pargv){	
	args::ArgumentParser parser("xwm - A compositing window manager","");
	args::HelpFlag help(parser,"help","Display this help menu",{'h',"help"});
	args::Group group_comp(parser,"Compositor",args::Group::Validators::DontCare);
	args::ValueFlag<uint> gpuIndex(group_comp,"id","GPU to use by its index",{"gpu-index"},0);

	try{
		parser.ParseCLI(argc,pargv);

	}catch(args::Help){
		std::cout<<parser;
		return 0;

	}catch(args::ParseError e){
		std::cout<<e.what()<<std::endl<<parser;
		return 1;
	}

#define MAX_EVENTS 1024
	struct epoll_event event1, events[MAX_EVENTS];
	sint efd = epoll_create1(0);
	if(efd == -1){
		DebugPrintf(stderr,"epoll efd\n");
		return 1;
	}

	//https://stackoverflow.com/questions/43212106/handle-signals-with-epoll-wait
	/*signal(SIGINT,SIG_IGN);

	sigset_t signals;
	sigemptyset(&signals);
	sigaddset(&signals,SIGINT);
	sint sfd = signalfd(-1,&signals,SFD_NONBLOCK);
	if(sfd == -1){
		DebugPrintf(stderr,"signal fd\n");
		return 1;
	}
	sigprocmask(SIG_BLOCK,&signals,0);
	event1.data.ptr = &sfd;
	event1.events = EPOLLIN;
	epoll_ctl(sfd,EPOLL_CTL_ADD,sfd,&event1);*/
#if 0
	RunBackend *pbackend;
	try{
		pbackend = new RunBackend();
	}catch(Exception e){
		DebugPrintf(stderr,"%s\n",e.what());
		return 1;
	}
#else
	Backend::X11Backend *pbackend;
	try{
		pbackend = new FakeBackend();
	}catch(Exception e){
		DebugPrintf(stderr,"%s\n",e.what());
		return 1;
	}
#endif

	sint fd = pbackend->GetEventFileDescriptor();
	if(fd == -1){
		DebugPrintf(stderr,"XCB fd\n");
		return 1;
	}
	event1.data.ptr = &fd;
	event1.events = EPOLLIN;
	epoll_ctl(efd,EPOLL_CTL_ADD,fd,&event1);

	RunCompositor *pcomp;
	try{
		pcomp = new RunCompositor(gpuIndex.Get(),pbackend);
	}catch(Exception e){
		DebugPrintf(stderr,"%s\n",e.what());
		delete pbackend;
		return 1;
	}

	//https://harrylovescode.gitbooks.io/vulkan-api/content/chap04/chap04-linux.html
	//https://harrylovescode.gitbooks.io/vulkan-api/content/chap05/chap05-linux.html

	for(bool r = true; r;){
		for(sint n = epoll_wait(efd,events,MAX_EVENTS,-1), i = 0; i < n; ++i){
			if(events[i].data.ptr == &fd){
				r = pbackend->HandleEvent();
				if(!r)
					break;
			}
		}
		//xcb_flush(pcon);
	}

	DebugPrintf(stdout,"Exit\n");

	delete pcomp;
	delete pbackend;

	return 0;
}

