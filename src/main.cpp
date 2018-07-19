#include "main.h"
#include "container.h"
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

class RunBackend{
public:
	RunBackend(){}
	virtual ~RunBackend(){}
};

class DefaultBackend : public Backend::Default, public RunBackend{
public:
	DefaultBackend() : Default(), RunBackend(){
		Start();
		DebugPrintf(stdout,"Backend initialized.\n");
	}
	
	~DefaultBackend(){}

	void DefineBindings(){
		DebugPrintf(stdout,"DefineKeybindings()\n");
	}
};

class FakeBackend : public Backend::Fake, public RunBackend{
public:
	FakeBackend() : Fake(), RunBackend(){
		Start();
		DebugPrintf(stdout,"Backend initialized.\n");
	}

	~FakeBackend(){}

	void DefineBindings(){
		DebugPrintf(stdout,"DefineKeybindings()\n");
	}
};

class RunCompositor{
public:
	RunCompositor(){}
	virtual ~RunCompositor(){}
	virtual void Present() = 0;
};

class DefaultCompositor : public Compositor::X11Compositor, public RunCompositor{
public:
	DefaultCompositor(uint gpuIndex, WManager::Container *proot, Backend::X11Backend *pbackend) : X11Compositor(gpuIndex,pbackend), RunCompositor(){
		Start();
		GenerateCommandBuffers(proot); //TODO: move to present
		DebugPrintf(stdout,"Compositor enabled.\n");
	}

	~DefaultCompositor(){}

	void Present(){
		Compositor::X11Compositor::Present();
	}
};

class DebugCompositor : public Compositor::X11DebugCompositor, public RunCompositor{
public:
	DebugCompositor(uint gpuIndex, WManager::Container *proot, Backend::X11Backend *pbackend) : X11DebugCompositor(gpuIndex,pbackend), RunCompositor(){
		Compositor::X11DebugCompositor::Start();
		GenerateCommandBuffers(proot);
		DebugPrintf(stdout,"Compositor enabled.\n");
	}

	~DebugCompositor(){}

	void Present(){
		Compositor::X11DebugCompositor::Present();
	}
};

int main(sint argc, const char **pargv){	
	args::ArgumentParser parser("xwm - A compositing window manager","");
	args::HelpFlag help(parser,"help","Display this help menu",{'h',"help"});
	args::Group group_backend(parser,"Backend",args::Group::Validators::DontCare);
	args::Flag debugBackend(group_backend,"debugBackend","Create a test environment for the compositor engine without redirection.",{'d',"debug-backend"});
	args::Group group_comp(parser,"Compositor",args::Group::Validators::DontCare);
	args::ValueFlag<uint> gpuIndex(group_comp,"id","GPU to use by its index. By default the first enumerated GPU will be used.",{"gpu-index"},0);

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

	WManager::Container *proot = new WManager::Container();
	WManager::Container *pna = new WManager::Container(proot); //temp: testing
	pna->pclient = new Backend::FakeClient(400,10,400,200);
	WManager::Container *pnb = new WManager::Container(proot); //temp: testing
	pnb->pclient = new Backend::FakeClient(10,10,400,800);

	RunBackend *pbackend;
	try{
		if(debugBackend.Get())
			pbackend = new FakeBackend();
		else pbackend = new DefaultBackend();
	}catch(Exception e){
		DebugPrintf(stderr,"%s\n",e.what());
		return 1;
	}

	Backend::X11Backend *pbackend11 = dynamic_cast<Backend::X11Backend *>(pbackend);
	sint fd = pbackend11->GetEventFileDescriptor();
	if(fd == -1){
		DebugPrintf(stderr,"XCB fd\n");
		return 1;
	}
	event1.data.ptr = &fd;
	event1.events = EPOLLIN;
	epoll_ctl(efd,EPOLL_CTL_ADD,fd,&event1);

	RunCompositor *pcomp;
	try{
		if(debugBackend.Get())
			pcomp = new DebugCompositor(gpuIndex.Get(),proot,pbackend11);
		else pcomp = new DefaultCompositor(gpuIndex.Get(),proot,pbackend11);
		pcomp->Present();
	}catch(Exception e){
		DebugPrintf(stderr,"%s\n",e.what());
		delete pbackend;
		return 1;
	}

	for(bool r = true; r;){
		for(sint n = epoll_wait(efd,events,MAX_EVENTS,-1), i = 0; i < n; ++i){
			if(events[i].data.ptr == &fd){
				r = pbackend11->HandleEvent();
				if(!r)
					break;
				try{
					pcomp->Present();

				}catch(Exception e){
					DebugPrintf(stderr,"%s\n",e.what());
					break;
				}
			}
		}
		//xcb_flush(pcon);
	}

	DebugPrintf(stdout,"Exit\n");

	delete pcomp;
	delete pbackend;
	delete proot;

	return 0;
}

