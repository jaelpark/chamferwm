#include "main.h"
#include "container.h"
#include "backend.h"
#include "config.h"
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

	//pf = fopen("/tmp/log1","a+");
	char tbuf[256];
	strftime(tbuf,sizeof(tbuf),"[chamferwm %F %T]",pti);
	fprintf(pf,"%s ",tbuf);

	va_list args;
	va_start(args,pfmt);
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

	template<class T, class U>
	Config::ContainerInterface & SetupContainer(WManager::Container *pParent){
		boost::python::object containerObject = Config::BackendInterface::pbackendInt->OnCreateContainer();
		boost::python::extract<Config::ContainerInterface &> containerExtract(containerObject);
		if(!containerExtract.check())
			throw Exception("OnCreateContainer(): Invalid container object returned.\n"); //TODO: create default

		Config::ContainerInterface &containerInt = containerExtract();
		containerInt.self = containerObject;

		containerInt.OnSetup();

		WManager::Container::Setup setup;
		setup.mode = WManager::Container::MODE_TILED;
		containerInt.CopySettingsSetup(setup);
	
		if(!pParent){
			boost::python::object parentObject = containerInt.OnParent();
			boost::python::extract<Config::ContainerInterface &> parentExtract(parentObject);
			if(!parentExtract.check())
				throw Exception("OnParent(): Invalid parent container object returned.\n");

			Config::ContainerInterface &parentInt = parentExtract();
			if(parentInt.pcontainer == containerInt.pcontainer)
				throw Exception("OnParent(): Cannot parent to itself.\n");

			if(parentInt.pcontainer->pclient){
				Config::ContainerInterface &parentInt1 = SetupContainer<T,U>(parentInt.pcontainer);
				pParent = parentInt1.pcontainer;

			}else pParent = parentInt.pcontainer;
		}

		T *pcontainer = new T(&containerInt,pParent,setup,static_cast<U*>(this));
		containerInt.pcontainer = pcontainer;

		return containerInt;
	}


	template<class T, class U>
	Config::ContainerInterface & SetupFloating(){
		boost::python::object containerObject = Config::BackendInterface::pbackendInt->OnCreateContainer();
		boost::python::extract<Config::ContainerInterface &> containerExtract(containerObject);
		if(!containerExtract.check())
			throw Exception("OnCreateContainer(): Invalid container object returned.\n"); //TODO: create default

		Config::ContainerInterface &containerInt = containerExtract();
		containerInt.self = containerObject;

		containerInt.OnSetup();

		WManager::Container::Setup setup;
		setup.mode = WManager::Container::MODE_FLOATING;
		containerInt.CopySettingsSetup(setup);
	
		T *pcontainer = new T(&containerInt,proot,setup,static_cast<U*>(this));
		containerInt.pcontainer = pcontainer;

		return containerInt;
	}

	void ReleaseContainersRecursive(const WManager::Container *pcontainer){
		for(const WManager::Container *pcont = pcontainer; pcont;){
			if(pcont->pclient)
				delete pcont->pclient;
			else
			if(pcont->pch)
				ReleaseContainersRecursive(pcont->pch);

			const WManager::Container *pdiscard = pcont;
			pcont = pcont->pnext;
			delete pdiscard;
		}
	}

	void ReleaseContainers(){
		//TODO: cleanup clients that are not part of the hierarchy!
		if(proot->pch)
			ReleaseContainersRecursive(proot->pch);
		for(auto &p : stackAppendix){
			if(p.second->pcontainer)
				delete p.second->pcontainer;
			delete p.second;
		}
	}

//protected:
	WManager::Container *proot;
	std::vector<std::pair<const WManager::Container *, WManager::Client *>> stackAppendix;
	class RunCompositor *pcomp;
};

class RunCompositor{
public:
	RunCompositor(WManager::Container *_proot, std::vector<std::pair<const WManager::Container *, WManager::Client *>> *_pstackAppendix) : proot(_proot), pstackAppendix(_pstackAppendix){}
	virtual ~RunCompositor(){}
	virtual void Present() = 0;
	virtual void WaitIdle() = 0;
protected:
	WManager::Container *proot;
	std::vector<std::pair<const WManager::Container *, WManager::Client *>> *pstackAppendix;
};

class DefaultBackend : public Backend::Default, public RunBackend{
public:
	DefaultBackend() : Default(), RunBackend(new Config::X11ContainerConfig(this)){
		Start();
		DebugPrintf(stdout,"Backend initialized.\n");
	}

	~DefaultBackend(){
		delete proot;
	}

	void DefineBindings(Backend::BackendKeyBinder *pkeyBinder){
		Backend::X11KeyBinder *pkeyBinder11 = dynamic_cast<Backend::X11KeyBinder*>(pkeyBinder);
		Config::BackendInterface::pbackendInt->OnSetupKeys(pkeyBinder11,false);
	}

	Backend::X11Client * SetupClient(const Backend::X11Client::CreateInfo *pcreateInfo){
		//Containers should be created always under the parent of the current focus.
		//config script should manage this (point to container which should be the parent of the
		//new one), while also setting some of the parameters like border width and such.
		if(pcreateInfo->mode == Backend::X11Client::CreateInfo::CREATE_AUTOMATIC){
			Backend::X11Client *pclient11 = SetupClient(proot,pcreateInfo);
			if(pclient11)
				stackAppendix.push_back(std::pair<const WManager::Container *, WManager::Client *>(pcreateInfo->pstackContainer,pclient11));
			return pclient11;

		}else{
			Config::ContainerInterface &containerInt = !pcreateInfo->pstackContainer?
				SetupContainer<Config::X11ContainerConfig,DefaultBackend>(0):
				SetupFloating<Config::X11ContainerConfig,DefaultBackend>();

			Backend::X11Client *pclient11 = SetupClient(containerInt.pcontainer,pcreateInfo);
			containerInt.pcontainer->pclient = pclient11;
			if(pcreateInfo->pstackContainer)
				stackAppendix.push_back(std::pair<const WManager::Container *, WManager::Client *>(pcreateInfo->pstackContainer,pclient11));

			containerInt.OnCreate();

			return pclient11;
		}
	}

	Backend::X11Client * SetupClient(WManager::Container *pcontainer, const Backend::X11Client::CreateInfo *pcreateInfo){
		Backend::X11Client *pclient11;
		Compositor::X11Compositor *pcomp11 = dynamic_cast<Compositor::X11Compositor *>(pcomp);
		if(!pcomp11)
			pclient11 = new Backend::X11Client(pcontainer,pcreateInfo);
		else{
			if(pcreateInfo->window == pcomp11->overlay)
				return 0;
			pclient11 = new Compositor::X11ClientFrame(pcontainer,pcreateInfo,pcomp11);
		}

		return pclient11;
	}

	void DestroyClient(Backend::X11Client *pclient){
		WManager::Client *pbase = pclient;
		auto m = std::find_if(stackAppendix.begin(),stackAppendix.end(),[&](auto &p)->bool{
			return pbase == p.second;
		});
		if(m != stackAppendix.end())
			stackAppendix.erase(m);
		if(pclient->pcontainer == proot){
			/*if(Config::BackendInterface::pfocus == pbase->pcontainer)
				Config::BackendInterface::SetFocus(m != stackAppendix.end()?
					(WManager::Container*)(*m).first:proot); //hack cast, TODO: remove const from the vector*/
			delete pclient;
			return;
		}
		WManager::Container *premoved = pclient->pcontainer->Remove();
		WManager::Container *pcollapsed = premoved->pParent->Collapse();
		if(Config::BackendInterface::pfocus == pclient->pcontainer){
			Config::BackendInterface::pfocus = proot;
			//find the first parent which has clients available to be focused
			for(WManager::Container *pcontainer = premoved->pParent; pcontainer; pcontainer = pcontainer->pParent)
				if(pcontainer->focusQueue.size() > 0){
					Config::BackendInterface::SetFocus(pcontainer->focusQueue.back());
					break;
				}else
				if(pcontainer->pch){
					//should always have at least one container available
					Config::BackendInterface::SetFocus(pcontainer->pch);
					break;
				}
		}
		if(premoved->pch)
			ReleaseContainersRecursive(premoved->pch);
		if(premoved->pclient)
			delete premoved->pclient;
		delete premoved;
		if(pcollapsed){
			if(pcollapsed->pch)
				ReleaseContainersRecursive(pcollapsed->pch);
			delete pcollapsed;
		}
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

	const WManager::Container * GetRoot() const{
		return proot;
	}

	const std::vector<std::pair<const WManager::Container *, WManager::Client *>> * GetStackAppendix() const{
		return &stackAppendix;
	}
};

//TODO: some of these functions can be templated and shared with the DefaultBackend
class DebugBackend : public Backend::Debug, public RunBackend{
public:
	//DebugBackend() : Debug(), RunBackend(new Backend::DebugContainer(this)){
	DebugBackend() : Debug(), RunBackend(new Config::DebugContainerConfig(this)){
		Start();
		DebugPrintf(stdout,"Backend initialized.\n");
	}

	~DebugBackend(){
		delete proot;
	}

	Backend::DebugClient * SetupClient(const Backend::DebugClient::CreateInfo *pcreateInfo){
		Config::ContainerInterface &containerInt = SetupContainer<Config::DebugContainerConfig,DebugBackend>(0);

		Backend::DebugClient *pclient;
		Compositor::X11DebugCompositor *pcomp11 = dynamic_cast<Compositor::X11DebugCompositor *>(pcomp);
		if(!pcomp11)
			pclient = new Backend::DebugClient(containerInt.pcontainer,pcreateInfo);
		else pclient = new Compositor::X11DebugClientFrame(containerInt.pcontainer,pcreateInfo,pcomp11);
		containerInt.pcontainer->pclient = pclient;

		containerInt.OnCreate();

		return pclient;
	}

	void DestroyClient(Backend::DebugClient *pclient){
		if(pclient->pcontainer == proot){
			Config::BackendInterface::SetFocus(proot);
			delete pclient;
			return;
		}
		WManager::Container *premoved = pclient->pcontainer->Remove();
		WManager::Container *pcollapsed = premoved->pParent->Collapse();
		if(Config::BackendInterface::pfocus == pclient->pcontainer){
			Config::BackendInterface::pfocus = proot;
			for(WManager::Container *pcontainer = premoved->pParent; pcontainer; pcontainer = pcontainer->pParent)
				if(pcontainer->focusQueue.size() > 0){
					Config::BackendInterface::SetFocus(pcontainer->focusQueue.back());
					break;
				}else
				if(pcontainer->pch){
					//should always have at least one container available
					Config::BackendInterface::SetFocus(pcontainer->pch);
					break;
				}
		}
		if(premoved->pch)
			ReleaseContainersRecursive(premoved->pch);
		if(premoved->pclient)
			delete premoved->pclient;
		delete premoved;
		if(pcollapsed){
			if(pcollapsed->pch)
				ReleaseContainersRecursive(pcollapsed->pch);
			delete pcollapsed;
		}
	}

	void DefineBindings(Backend::BackendKeyBinder *pkeyBinder){
		Backend::X11KeyBinder *pkeyBinder11 = dynamic_cast<Backend::X11KeyBinder*>(pkeyBinder);
		Config::BackendInterface::pbackendInt->OnSetupKeys(pkeyBinder11,true);
	}

	void EventNotify(const Backend::BackendEvent *pevent){
		//nothing to process
	}

	void KeyPress(uint keyId, bool down){
		if(down)
			Config::BackendInterface::pbackendInt->OnKeyPress(keyId);
		else Config::BackendInterface::pbackendInt->OnKeyRelease(keyId);
	}

	const WManager::Container * GetRoot() const{
		return proot;
	}

	const std::vector<std::pair<const WManager::Container *, WManager::Client *>> * GetStackAppendix() const{
		return &stackAppendix;
	}
};

class DefaultCompositor : public Compositor::X11Compositor, public RunCompositor{
public:
	DefaultCompositor(uint gpuIndex, WManager::Container *_proot, std::vector<std::pair<const WManager::Container *, WManager::Client *>> *_pstackAppendix, Backend::X11Backend *pbackend) : X11Compositor(gpuIndex,pbackend), RunCompositor(_proot,_pstackAppendix){
		Start();
		DebugPrintf(stdout,"Compositor enabled.\n");
	}

	~DefaultCompositor(){
		Stop();
	}

	void Present(){
		if(!PollFrameFence())
			return;
		GenerateCommandBuffers(proot,pstackAppendix,Config::BackendInterface::pfocus);
		Compositor::X11Compositor::Present();
	}

	void WaitIdle(){
		Compositor::X11Compositor::WaitIdle();
	}
};

class DebugCompositor : public Compositor::X11DebugCompositor, public RunCompositor{
public:
	DebugCompositor(uint gpuIndex, WManager::Container *_proot, std::vector<std::pair<const WManager::Container *, WManager::Client *>> *_pstackAppendix, Backend::X11Backend *pbackend) : X11DebugCompositor(gpuIndex,pbackend), RunCompositor(_proot,_pstackAppendix){
		Compositor::X11DebugCompositor::Start();
		DebugPrintf(stdout,"Compositor enabled.\n");
	}

	~DebugCompositor(){
		Compositor::X11DebugCompositor::Stop();
	}

	void Present(){
		if(!PollFrameFence())
			return;
		GenerateCommandBuffers(proot,pstackAppendix,Config::BackendInterface::pfocus);
		Compositor::X11DebugCompositor::Present();
	}

	void WaitIdle(){
		Compositor::X11DebugCompositor::WaitIdle();
	}
};

class NullCompositor : public Compositor::NullCompositor, public RunCompositor{
public:
	NullCompositor() : Compositor::NullCompositor(), RunCompositor(0,0){
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
			pcomp = new DebugCompositor(gpuIndex.Get(),pbackend->proot,&pbackend->stackAppendix,pbackend11);
		else pcomp = new DefaultCompositor(gpuIndex.Get(),pbackend->proot,&pbackend->stackAppendix,pbackend11);
	}catch(Exception e){
		DebugPrintf(stderr,"%s\n",e.what());
		delete pbackend;
		return 1;
	}

	pbackend->SetCompositor(pcomp);

	for(;;){
		//TODO: can we wait for vsync before handling the event? Might help with the stuttering
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

