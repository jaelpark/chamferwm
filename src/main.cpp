#include "main.h"
#include "config.h"
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
	strftime(tbuf,sizeof(tbuf),"[chamferwm %F %T]",pti);
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
	
//protected:
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
};

class DefaultBackend : public Backend::Default, public RunBackend{
public:
	DefaultBackend() : Default(), RunBackend(new Backend::X11Container(this)){
		Start();
		DebugPrintf(stdout,"Backend initialized.\n");
	}

	~DefaultBackend(){
		delete proot;
	}

	void DefineBindings(Backend::BackendKeyBinder *pkeyBinder){
		Backend::X11KeyBinder *pkeyBinder11 = dynamic_cast<Backend::X11KeyBinder*>(pkeyBinder);
		Config::BackendInterface::pbackendInt->SetupKeys(pkeyBinder11);
		DebugPrintf(stdout,"DefineKeybindings()\n");
	}

	Backend::X11Client * SetupClient(const Backend::X11Client::CreateInfo *pcreateInfo){
		//Containers should be created always under the parent of the current focus.
		//config script should manage this (point to container which should be the parent of the
		//new one), while also setting some of the parameters like border width and such.
		WManager::Container *pcontainer = new Backend::X11Container(proot,this);
		pcontainer->borderWidth = glm::vec2(0.02f);
		//pcontainer->minSize = glm::vec2(0.4f); //needs to be set as constructor params
		Compositor::X11Compositor *pcomp11 = dynamic_cast<Compositor::X11Compositor *>(pcomp);
		//TODO: All the dimension related parameters should be set before the container is created and the siblings adjusted.

		Backend::X11Client *pclient11;
		if(!pcomp11)
			pclient11 = new Backend::X11Client(pcontainer,pcreateInfo);
		else pclient11 = new Compositor::X11ClientFrame(pcontainer,pcreateInfo,pcomp11);
		pcontainer->pclient = pclient11;

		Config::ClientProxy client(pclient11);
		Config::BackendInterface::pbackendInt->OnCreateClient(client);

		return pclient11;
	}

	void DestroyClient(Backend::X11Client *pclient){
		pclient->pcontainer->Remove();
		Config::BackendInterface::pfocus = proot;
		//find the first parent which has clients available to be focused
		for(WManager::Container *pcontainer = pclient->pcontainer->pParent; pcontainer; pcontainer = pcontainer->pParent)
			if(pcontainer->focusQueue.size() > 0){
				Config::BackendInterface::SetFocus(pcontainer->focusQueue.back());
				break;
			}
		delete pclient->pcontainer;
		delete pclient;
	}

	void EventNotify(const Backend::BackendEvent *pevent){
		Compositor::X11Compositor *pcomp11 = dynamic_cast<Compositor::X11Compositor *>(pcomp);
		if(!pcomp11)
			return;
		const Backend::X11Event *pevent11 = dynamic_cast<const Backend::X11Event *>(pevent);
		pcomp11->FilterEvent(pevent11);
	}

	void KeyPress(uint keyId, bool down){
		if(down)
			Config::BackendInterface::pbackendInt->OnKeyPress(keyId);
		else Config::BackendInterface::pbackendInt->OnKeyRelease(keyId);
	}
};

class DebugBackend : public Backend::Debug, public RunBackend{
public:
	DebugBackend() : Debug(), RunBackend(new Backend::DebugContainer(this)){
		Start();
		DebugPrintf(stdout,"Backend initialized.\n");
	}

	~DebugBackend(){
		delete proot;
	}

	Backend::DebugClient * SetupClient(const Backend::DebugClient::CreateInfo *pcreateInfo){
		WManager::Container *pcontainer = new Backend::DebugContainer(proot,this);
		pcontainer->borderWidth = glm::vec2(0.02f);
		Compositor::X11DebugCompositor *pcomp11 = dynamic_cast<Compositor::X11DebugCompositor *>(pcomp);
		Backend::DebugClient *pclient;
		if(!pcomp11)
			pclient = new Backend::DebugClient(pcontainer,pcreateInfo);
		else pclient = new Compositor::X11DebugClientFrame(pcontainer,pcreateInfo,pcomp11);
		pcontainer->pclient = pclient;

		Config::ClientProxy client(pclient);
		Config::BackendInterface::pbackendInt->OnCreateClient(client);

		return pclient;
	}

	void DestroyClient(Backend::DebugClient *pclient){
		pclient->pcontainer->Remove();
		Config::BackendInterface::pfocus = proot;
		for(WManager::Container *pcontainer = pclient->pcontainer->pParent; pcontainer; pcontainer = pcontainer->pParent)
			if(pcontainer->focusQueue.size() > 0){
				Config::BackendInterface::SetFocus(pcontainer->focusQueue.back());
				break;
			}
		delete pclient->pcontainer;
		delete pclient;
	}

	void DefineBindings(Backend::BackendKeyBinder *pkeyBinder){
		Backend::X11KeyBinder *pkeyBinder11 = dynamic_cast<Backend::X11KeyBinder*>(pkeyBinder);
		Config::BackendInterface::pbackendInt->SetupKeys(pkeyBinder11);
		DebugPrintf(stdout,"DefineKeybindings()\n");
	}

	void EventNotify(const Backend::BackendEvent *pevent){
		//nothing to process
	}

	void KeyPress(uint keyId, bool down){
		if(down)
			Config::BackendInterface::pbackendInt->OnKeyPress(keyId);
		else Config::BackendInterface::pbackendInt->OnKeyRelease(keyId);
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
	args::ArgumentParser parser("chamferwm - A compositing window manager","");
	args::HelpFlag help(parser,"help","Display this help menu",{'h',"help"});

	args::ValueFlag<std::string> configPath(parser,"path","Configuration Python script",{"config",'c'},"config.py");

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

/*#define MAX_EVENTS 1024
	struct epoll_event event1, events[MAX_EVENTS];
	sint efd = epoll_create1(0);
	if(efd == -1){
		DebugPrintf(stderr,"epoll efd\n");
		return 1;
	}*/

	Config::Loader *pconfigLoader = new Config::Loader(pargv[0]);
	pconfigLoader->Run(configPath.Get().c_str(),"config.py");

	RunBackend *pbackend;
	try{
		if(debugBackend.Get())
			pbackend = new DebugBackend();
		else pbackend = new DefaultBackend();
	}catch(Exception e){
		DebugPrintf(stderr,"%s\n",e.what());
		return 1;
	}

	Config::BackendInterface::pfocus = pbackend->proot;

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
			pcomp = new DebugCompositor(gpuIndex.Get(),pbackend->proot,pbackend11);
		else pcomp = new DefaultCompositor(gpuIndex.Get(),pbackend->proot,pbackend11);
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
	delete pconfigLoader;

	return 0;
}

