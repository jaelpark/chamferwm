#include "main.h"
#include "container.h"
#include "backend.h"
#include "CompositorResource.h"
#include "compositor.h"

#include <cstdlib>
#include <stdarg.h>
#include <time.h>

#include <args.hxx>
#include <iostream>

//#include <sys/epoll.h>
//#include <signal.h>
//#include <sys/signalfd.h>

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

/*Clock::Clock(){
	clock_gettime(CLOCK_MONOTONIC,&step0);
	step1 = step0;
}

Clock::~Clock(){
	//
}

void Clock::Step(){
	step0 = step1;
	clock_gettime(CLOCK_MONOTONIC,&step1);
}

float Clock::GetTimeDelta() const{
	return (float)(step1.tv_sec-step0.tv_sec)+(float)((step1.tv_nsec-step0.tv_nsec)/1e9);
}*/

Blob::Blob(const char *pfileName){
	FILE *pf = fopen(pfileName,"rb");
	if(!pf){
		snprintf(Exception::buffer,sizeof(Exception::buffer),"Unable to open file: %s\n",pfileName);
		throw Exception();
	}

	fseek(pf,0,SEEK_END);
	buflen = ftell(pf);
	fseek(pf,0,SEEK_SET);
	
	pbuffer = new char[buflen];
	fread(pbuffer,1,buflen,pf);
	fclose(pf);
}

Blob::~Blob(){
	delete []pbuffer;
}

const char * Blob::GetBufferPointer() const{
	return pbuffer;
}

size_t Blob::GetBufferLength() const{
	return buflen;
}

void DebugPrintf(FILE *pf, const char *pfmt, ...){
	time_t rt;
	time(&rt);
	const struct tm *pti = localtime(&rt);

	char tbuf[256];
	strftime(tbuf,sizeof(tbuf),"[xwm %F %T]",pti);
	fprintf(pf,"%s ",tbuf);

	va_list args;
	va_start(args,pfmt);
	//pf = fopen("/tmp/log1","a+");
	if(pf == stderr)
		fprintf(pf,"Error: ");
	vfprintf(pf,pfmt,args);
	//fclose(pf);
	va_end(args);
}

class RunBackend{
public:
	RunBackend(WManager::Container *_proot) : proot(_proot), pcomp(0){}
	virtual ~RunBackend(){}

	void SetCompositor(class RunCompositor *pcomp){
		this->pcomp = pcomp;
	}

	void ReleaseContainersRecursive(const WManager::Container *pcontainer){
		for(const WManager::Container *pcont = pcontainer; pcont;){
			if(pcont->pclient)
				delete pcont->pclient;
			if(pcont->pch){
				ReleaseContainersRecursive(pcont->pch);
				delete pcont->pch;
			}
			
			const WManager::Container *pdiscard = pcont;
			pcont = pcont->pnext;
			delete pdiscard;
		}
	}

	void ReleaseContainers(){
		if(proot->pch)
			ReleaseContainersRecursive(proot->pch);
	}
	
protected:
	WManager::Container *proot;
	class RunCompositor *pcomp;
};

class RunCompositor{
public:
	RunCompositor(WManager::Container *_proot) : proot(_proot){}
	virtual ~RunCompositor(){}
	virtual void Present() = 0;
	virtual void WaitIdle() = 0;
protected:
	WManager::Container *proot;
	//Clock clock;
};

class DefaultBackend : public Backend::Default, public RunBackend{
public:
	DefaultBackend(WManager::Container *_proot) : Default(), RunBackend(_proot){
		Start();
		DebugPrintf(stdout,"Backend initialized.\n");
	}

	~DefaultBackend(){
		//
	}

	void DefineBindings(){
		DebugPrintf(stdout,"DefineKeybindings()\n");
	}

	Backend::X11Client * SetupClient(const Backend::X11Client::CreateInfo *pcreateInfo){
		WManager::Container *pcontainer = new WManager::Container(proot);
		Compositor::X11Compositor *pcomp11 = dynamic_cast<Compositor::X11Compositor *>(pcomp);
		if(!pcomp11){
			Backend::X11Client *pclient11 = new Backend::X11Client(pcreateInfo);
			pcontainer->pclient = pclient11;
			return pclient11;
		}
		Compositor::CompositorInterface *pcompInterface = dynamic_cast<Compositor::CompositorInterface *>(pcomp);
		Compositor::X11ClientFrame *pclientFrame = new Compositor::X11ClientFrame(pcreateInfo,pcompInterface);
		pcontainer->pclient = pclientFrame;
		return pclientFrame;
	}

	void EventNotify(const Backend::BackendEvent *pevent){
		Compositor::X11Compositor *pcomp11 = dynamic_cast<Compositor::X11Compositor *>(pcomp);
		if(!pcomp11)
			return;
		const Backend::X11Event *pevent11 = dynamic_cast<const Backend::X11Event *>(pevent);
		pcomp11->FilterEvent(pevent11);
	}
};

class DebugBackend : public Backend::Debug, public RunBackend{
public:
	DebugBackend(WManager::Container *_proot) : Debug(), RunBackend(_proot){
		Start();
		DebugPrintf(stdout,"Backend initialized.\n");
	}

	~DebugBackend(){}

	Backend::DebugClient * SetupClient(const Backend::DebugClient::CreateInfo *pcreateInfo){
		WManager::Container *pcontainer = new WManager::Container(proot);
		Compositor::X11DebugCompositor *pcomp11 = dynamic_cast<Compositor::X11DebugCompositor *>(pcomp);
		if(!pcomp11){
			Backend::DebugClient *pclient = new Backend::DebugClient(pcreateInfo);
			//pcontainer->Assign(pclient);
			pcontainer->pclient = pclient;
			return pclient;
		}
		Compositor::CompositorInterface *pcompInterface = dynamic_cast<Compositor::CompositorInterface *>(pcomp);
		Compositor::X11DebugClientFrame *pclientFrame = new Compositor::X11DebugClientFrame(pcreateInfo,pcompInterface);
		//pcontainer->Assign(pclientFrame);
		pcontainer->pclient = pclientFrame;
		return pclientFrame;
	}

	void DefineBindings(){
		DebugPrintf(stdout,"DefineKeybindings()\n");
	}

	void EventNotify(const Backend::BackendEvent *pevent){
		//nothing to process
	}
};

class DefaultCompositor : public Compositor::X11Compositor, public RunCompositor{
public:
	DefaultCompositor(uint gpuIndex, WManager::Container *_proot, Backend::X11Backend *pbackend) : X11Compositor(gpuIndex,pbackend), RunCompositor(_proot){
		Start();
		DebugPrintf(stdout,"Compositor enabled.\n");
	}

	~DefaultCompositor(){
		Stop();
	}

	void Present(){
		if(!PollFrameFence())
			return;
		GenerateCommandBuffers(proot);
		Compositor::X11Compositor::Present();
	}

	void WaitIdle(){
		Compositor::X11Compositor::WaitIdle();
	}
};

class DebugCompositor : public Compositor::X11DebugCompositor, public RunCompositor{
public:
	DebugCompositor(uint gpuIndex, WManager::Container *_proot, Backend::X11Backend *pbackend) : X11DebugCompositor(gpuIndex,pbackend), RunCompositor(_proot){
		Compositor::X11DebugCompositor::Start();
		DebugPrintf(stdout,"Compositor enabled.\n");
	}

	~DebugCompositor(){
		Compositor::X11DebugCompositor::Stop();
	}

	void Present(){
		if(!PollFrameFence())
			return;
		GenerateCommandBuffers(proot);
		Compositor::X11DebugCompositor::Present();
	}

	void WaitIdle(){
		Compositor::X11DebugCompositor::WaitIdle();
	}
};

class NullCompositor : public Compositor::NullCompositor, public RunCompositor{
public:
	NullCompositor() : Compositor::NullCompositor(), RunCompositor(0){
		Start();
	}

	~NullCompositor(){
		Stop();
	}

	void Present(){
		//
	}

	void WaitIdle(){
		//
	}
};

int main(sint argc, const char **pargv){	
	args::ArgumentParser parser("xwm - A compositing window manager","");
	args::HelpFlag help(parser,"help","Display this help menu",{'h',"help"});

	args::Group group_backend(parser,"Backend",args::Group::Validators::DontCare);
	args::Flag debugBackend(group_backend,"debugBackend","Create a test environment for the compositor engine without redirection. The application will not as a window manager.",{'d',"debug-backend"});

	args::Group group_comp(parser,"Compositor",args::Group::Validators::DontCare);
	args::Flag noComp(group_comp,"noComp","Disable compositor.",{"no-compositor",'n'});
	args::ValueFlag<uint> gpuIndex(group_comp,"id","GPU to use by its index. By default the first device in the list of enumerated GPUs will be used.",{"device-index"},0);
	//args::ValueFlag<std::string> shaderPath(group_comp,"path","Path to SPIR-V shader binary blobs",{"shader-path"},".");

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
	/*struct epoll_event event1, events[MAX_EVENTS];
	sint efd = epoll_create1(0);
	if(efd == -1){
		DebugPrintf(stderr,"epoll efd\n");
		return 1;
	}*/

	WManager::Container *proot = new WManager::Container();

	RunBackend *pbackend;
	try{
		if(debugBackend.Get())
			pbackend = new DebugBackend(proot);
		else pbackend = new DefaultBackend(proot);
	}catch(Exception e){
		DebugPrintf(stderr,"%s\n",e.what());
		return 1;
	}

	Backend::X11Backend *pbackend11 = dynamic_cast<Backend::X11Backend *>(pbackend);
	/*sint fd = pbackend11->GetEventFileDescriptor();
	if(fd == -1){
		DebugPrintf(stderr,"XCB fd\n");
		return 1;
	}*/
	/*event1.data.ptr = &fd;
	event1.events = EPOLLIN;
	epoll_ctl(efd,EPOLL_CTL_ADD,fd,&event1);*/

	RunCompositor *pcomp;
	try{
		if(noComp.Get())
			pcomp = new NullCompositor();
		else
		if(debugBackend.Get())
			pcomp = new DebugCompositor(gpuIndex.Get(),proot,pbackend11);
		else pcomp = new DefaultCompositor(gpuIndex.Get(),proot,pbackend11);
	}catch(Exception e){
		DebugPrintf(stderr,"%s\n",e.what());
		delete pbackend;
		return 1;
	}

	pbackend->SetCompositor(pcomp);

	for(;;){
		if(!pbackend11->HandleEvent())
			break;

		try{
			pcomp->Present();

		}catch(Exception e){
			DebugPrintf(stderr,"%s\n",e.what());
			break;
		}
	}

	DebugPrintf(stdout,"Exit\n");

	pcomp->WaitIdle();
	pbackend->ReleaseContainers();

	delete pcomp;
	delete pbackend;
	delete proot;

	return 0;
}

