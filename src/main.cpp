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

#include <boost/filesystem.hpp>

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
	FILE *pf;
	do{
		//try with filename directly
		pf = fopen(pfileName,"rb");
		if(pf)
			break;

		//search in the binary directory
		char path[256];
		ssize_t len = readlink("/proc/self/exe",path,sizeof(path));
		if(len == -1)
			throw Exception("readlink() failed for /proc/self/exe.\n");
		path[len] = 0;
		pf = fopen((boost::filesystem::path(path).parent_path()/pfileName).string().c_str(),"rb");
		if(pf)
			break;

		//try /usr/share
		pf = fopen((std::string("/usr/share/chamfer/")+pfileName).c_str(),"rb");
		if(pf)
			break;

		//TODO: config directory

		snprintf(Exception::buffer,sizeof(Exception::buffer),"Unable to open file: %s\n",pfileName);
		throw Exception();
	}while(0);

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

		containerInt.OnSetupContainer();

		WManager::Container::Setup setup;
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

		containerInt.OnSetupContainer();

		WManager::Container::Setup setup;
		setup.flags = WManager::Container::FLAG_FLOATING;
		containerInt.CopySettingsSetup(setup);
	
		T *pcontainer = new T(&containerInt,proot,setup,static_cast<U*>(this));
		containerInt.pcontainer = pcontainer;

		return containerInt;
	}

	template<class T, class U>
	void MoveContainer(WManager::Container *pcontainer, WManager::Container *pdst){
		//
		PrintTree(proot,0);
		printf("-----------\n");

		WManager::Container *premoved = pcontainer->Remove();
		WManager::Container *pOrigParent = premoved->pParent;

		if(pdst->pclient){
			Config::ContainerInterface &parentInt1 = SetupContainer<T,U>(pdst);
			pdst = parentInt1.pcontainer;
		}
		pcontainer->Place(pdst);

		PrintTree(proot,0);
		printf("-----------\n");

		WManager::Container *pNewParent = premoved->pParent;

		WManager::Container *pcollapsed = 0;
		if(pNewParent != proot)
			pcollapsed = pNewParent->Collapse();
		if(!pcollapsed && pNewParent->pch)
			pcollapsed = pNewParent->pch->Collapse();
		if(pcollapsed){
			if(pcollapsed->pch)
				ReleaseContainersRecursive(pcollapsed->pch);
			delete pcollapsed;
		}

		pcollapsed = 0;
		if(pOrigParent != proot)
			pcollapsed = pOrigParent->Collapse();
		if(!pcollapsed && pOrigParent->pch)
			pcollapsed = pOrigParent->pch->Collapse();
		if(pcollapsed){
			if(pcollapsed->pch)
				ReleaseContainersRecursive(pcollapsed->pch);
			delete pcollapsed;
		}

		if(premoved != pcontainer)
			pcontainer->pParent->pch = 0;

		Config::BackendInterface::SetFocus(pcontainer);

		PrintTree(proot,0);
		printf("-----------\n");

		if(premoved->pch)
			ReleaseContainersRecursive(premoved->pch); //should this be here??? once container trees can be moved
		PrintTree(proot,0);
	}

	void ReleaseContainersRecursive(const WManager::Container *pcontainer){
		for(const WManager::Container *pcont = pcontainer; pcont;){
			if(pcont->pclient)
				delete pcont->pclient;
			else
			if(pcont->pch)
				ReleaseContainersRecursive(pcont->pch);

			const Config::ContainerConfig *pcontainer1 = dynamic_cast<const Config::ContainerConfig *>(pcont);
			if(pcontainer1 && pcontainer1->pcontainerInt->pcontainer == pcont)
				pcontainer1->pcontainerInt->pcontainer = 0;

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
			const Config::ContainerConfig *pcontainer1 = dynamic_cast<const Config::ContainerConfig *>(p.second->pcontainer);
			if(pcontainer1 && pcontainer1->pcontainerInt->pcontainer == p.second->pcontainer)
				pcontainer1->pcontainerInt->pcontainer = 0;

			if(p.second->pcontainer)
				delete p.second->pcontainer;
			delete p.second;
		}
		stackAppendix.clear();
	}

	void PrintTree(WManager::Container *pcontainer, uint level){
		for(uint i = 0; i < level; ++i)
			printf(" ");
		printf("%x: ",pcontainer);
		if(pcontainer->pclient)
			printf("(client), ");
		if(pcontainer->pParent)
			printf("(parent: %x), ",pcontainer->pParent);
		if(Config::BackendInterface::pfocus == pcontainer)
			printf("(focus), ");
		printf("focusQueue: %u (%x)\n",pcontainer->focusQueue.size(),pcontainer->focusQueue.size() > 0?pcontainer->focusQueue.back():0);
		for(WManager::Container *pcontainer1 = pcontainer->pch; pcontainer1; pcontainer1 = pcontainer1->pnext)
			PrintTree(pcontainer1,level+1);
	}

//protected:
	WManager::Container *proot;
	std::vector<std::pair<const WManager::Client *, WManager::Client *>> stackAppendix;
	//std::vector<WManager::Client *> desktopStack;
	class RunCompositor *pcomp;
};

class RunCompositor{
public:
	RunCompositor(WManager::Container *_proot, std::vector<std::pair<const WManager::Client *, WManager::Client *>> *_pstackAppendix) : proot(_proot), pstackAppendix(_pstackAppendix){}
	virtual ~RunCompositor(){}
	virtual void Present() = 0;
	virtual void WaitIdle() = 0;
protected:
	WManager::Container *proot;
	std::vector<std::pair<const WManager::Client *, WManager::Client *>> *pstackAppendix;
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
		static const char *pshaderName[Compositor::Pipeline::SHADER_MODULE_COUNT] = {
			"frame_vertex.spv","frame_geometry.spv","frame_fragment.spv"
		};
		if(pcreateInfo->mode == Backend::X11Client::CreateInfo::CREATE_AUTOMATIC){
			Backend::X11Client *pclient11 = SetupClient(proot,pcreateInfo,pshaderName);
			if(pclient11) //in some cases the window is found to be unmanageable
				stackAppendix.push_back(std::pair<const WManager::Client *, WManager::Client *>(pcreateInfo->pstackClient,pclient11));
			return pclient11;

		}else{
			Config::ContainerInterface &containerInt = !pcreateInfo->prect?
				SetupContainer<Config::X11ContainerConfig,DefaultBackend>(0):
				SetupFloating<Config::X11ContainerConfig,DefaultBackend>();

			containerInt.wm_name = pcreateInfo->pwmName->pstr;
			containerInt.wm_class = pcreateInfo->pwmClass->pstr;

			containerInt.vertexShader = pshaderName[Compositor::Pipeline::SHADER_MODULE_VERTEX];
			containerInt.geometryShader = pshaderName[Compositor::Pipeline::SHADER_MODULE_GEOMETRY];
			containerInt.fragmentShader = pshaderName[Compositor::Pipeline::SHADER_MODULE_FRAGMENT];

			if(pcreateInfo->hints & Backend::X11Client::CreateInfo::HINT_NO_INPUT)
				containerInt.pcontainer->flags |= WManager::Container::FLAG_NO_FOCUS;

			containerInt.OnSetupClient();

			const char *pshaderName[Compositor::Pipeline::SHADER_MODULE_COUNT] = {
				containerInt.vertexShader.c_str(),containerInt.geometryShader.c_str(),containerInt.fragmentShader.c_str()
			};
			Backend::X11Client *pclient11 = SetupClient(containerInt.pcontainer,pcreateInfo,pshaderName);
			containerInt.pcontainer->pclient = pclient11;
			//if(pcreateInfo->prect && (pcreateInfo->pstackClient || pcreateInfo->hints & Backend::X11Client::CreateInfo::HINT_DESKTOP))
			if(pcreateInfo->prect)
				stackAppendix.push_back(std::pair<const WManager::Client *, WManager::Client *>(pcreateInfo->pstackClient,pclient11));

			containerInt.OnCreate();

			return pclient11;
		}
	}

	Backend::X11Client * SetupClient(WManager::Container *pcontainer, const Backend::X11Client::CreateInfo *pcreateInfo, const char *pshaderName[Compositor::Pipeline::SHADER_MODULE_COUNT]){
		Backend::X11Client *pclient11;
		Compositor::X11Compositor *pcomp11 = dynamic_cast<Compositor::X11Compositor *>(pcomp);
		if(!pcomp11)
			pclient11 = new Backend::X11Client(pcontainer,pcreateInfo);
		else{
			if(pcreateInfo->window == pcomp11->overlay)
				return 0;
			pclient11 = new Compositor::X11ClientFrame(pcontainer,pcreateInfo,pshaderName,pcomp11);
		}

		return pclient11;
	}

	void MoveContainer(WManager::Container *pcontainer, WManager::Container *pdst){
		//
		RunBackend::MoveContainer<Config::X11ContainerConfig,DefaultBackend>(pcontainer,pdst);
	}

	void PropertyChange(Backend::X11Client *pclient, PROPERTY_ID id, const Backend::BackendProperty *pProperty){
		if(!pclient){
			//root window
			const Backend::BackendPixmapProperty *pPixmapProperty = dynamic_cast<const Backend::BackendPixmapProperty *>(pProperty);
			Compositor::X11Compositor *pcomp11 = dynamic_cast<Compositor::X11Compositor *>(pcomp);
			if(pcomp11)
				pcomp11->SetBackgroundPixmap(pPixmapProperty);
			return;
		}
		if(pclient->pcontainer == proot)
			return;
		Config::X11ContainerConfig *pcontainer1 = dynamic_cast<Config::X11ContainerConfig *>(pclient->pcontainer);
		if(id == PROPERTY_ID_WM_NAME){
			pcontainer1->pcontainerInt->wm_name = dynamic_cast<const Backend::BackendStringProperty *>(pProperty)->pstr;
			pcontainer1->pcontainerInt->OnPropertyChange(Config::ContainerInterface::PROPERTY_ID_NAME);
		}else
		if(id == PROPERTY_ID_WM_CLASS){
			pcontainer1->pcontainerInt->wm_name = dynamic_cast<const Backend::BackendStringProperty *>(pProperty)->pstr;
			pcontainer1->pcontainerInt->OnPropertyChange(Config::ContainerInterface::PROPERTY_ID_CLASS);
		}else
		if(id == PROPERTY_ID_TRANSIENT_FOR){
			//
			//TODO!!
			/*WManager::Container *pstackContainer = dynamic_cast<const Backend::BackendContainerProperty *>(pProperty)->pcontainer;
			auto m = std::find_if(stackAppendix.begin(),stackAppendix.end(),[&](auto &p)->bool{
				return pclient == p.second;
			});
			if(m != stackAppendix.end())
				(*m).first = pstackContainer;*/
			//else: TODO: should we remove it from the tiling tree and make it floating according to its rect?
		}
	}

	void DestroyClient(Backend::X11Client *pclient){
		auto n = std::find(Config::BackendInterface::floatFocusQueue.begin(),Config::BackendInterface::floatFocusQueue.end(),pclient->pcontainer);
		if(n != Config::BackendInterface::floatFocusQueue.end())
			Config::BackendInterface::floatFocusQueue.erase(n);

		PrintTree(proot,0);

		WManager::Client *pbase = pclient;
		auto m = std::find_if(stackAppendix.begin(),stackAppendix.end(),[&](auto &p)->bool{
			return pbase == p.second;
		});
		if(m != stackAppendix.end())
			stackAppendix.erase(m);
		if(pclient->pcontainer == proot){
			delete pclient;
			return;
		}
		WManager::Container *premoved = pclient->pcontainer->Remove();
		WManager::Container *pOrigParent = premoved->pParent;

		printf("----------- removed %x\n",premoved);
		PrintTree(proot,0);

		WManager::Container *pcollapsed = 0;
		if(pOrigParent != proot)
			pcollapsed = pOrigParent->Collapse();
		//check if pch is alive, in this case this wasn't the last container
		if(!pcollapsed && pOrigParent->pch)
			pcollapsed = pOrigParent->pch->Collapse();

		if(Config::BackendInterface::pfocus == pclient->pcontainer){
			WManager::Container *pNewFocus = proot;
			//for(WManager::Container *pcontainer = pNewFocus; pcontainer; pNewFocus = pcontainer, pcontainer = pcontainer->focusQueue.size() > 0?pcontainer->focusQueue.back():pcontainer->pch);
			for(WManager::Container *pcontainer = pNewFocus; pcontainer; pNewFocus = pcontainer, pcontainer = pcontainer->GetFocus());
			Config::BackendInterface::SetFocus(pNewFocus);
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

		printf("----------- collapsed %x\n",pcollapsed);
		PrintTree(proot,0);
		printf("stackAppendix: %u\n",stackAppendix.size());
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

	const std::vector<std::pair<const WManager::Client *, WManager::Client *>> * GetStackAppendix() const{
		return &stackAppendix;
	}

	void TimerEvent(){
		//
		Config::BackendInterface::pbackendInt->OnTimer();
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

		static const char *pshaderName[Compositor::Pipeline::SHADER_MODULE_COUNT] = {
			"frame_vertex.spv","frame_geometry.spv","frame_fragment.spv"
		};

		Backend::DebugClient *pclient;
		Compositor::X11DebugCompositor *pcomp11 = dynamic_cast<Compositor::X11DebugCompositor *>(pcomp);
		if(!pcomp11)
			pclient = new Backend::DebugClient(containerInt.pcontainer,pcreateInfo);
		else pclient = new Compositor::X11DebugClientFrame(containerInt.pcontainer,pcreateInfo,pshaderName,pcomp11);
		containerInt.pcontainer->pclient = pclient;

		containerInt.OnCreate();

		return pclient;
	}

	void MoveContainer(WManager::Container *pcontainer, WManager::Container *pdst){
		//
		RunBackend::MoveContainer<Config::DebugContainerConfig,DebugBackend>(pcontainer,pdst);
	}

	void DestroyClient(Backend::DebugClient *pclient){
		WManager::Client *pbase = pclient;
		auto m = std::find_if(stackAppendix.begin(),stackAppendix.end(),[&](auto &p)->bool{
			return pbase == p.second;
		});
		if(m != stackAppendix.end())
			stackAppendix.erase(m);
		if(pclient->pcontainer == proot){
			delete pclient;
			return;
		}
		WManager::Container *premoved = pclient->pcontainer->Remove();

		WManager::Container *pcollapsed = 0;
		if(premoved->pParent != proot)
			pcollapsed = premoved->pParent->Collapse();
		if(!pcollapsed && premoved->pParent->pch) //check if pch is alive, in this case this wasn't the last container
			pcollapsed = premoved->pParent->pch->Collapse();

		if(Config::BackendInterface::pfocus == pclient->pcontainer){
			WManager::Container *pNewFocus = proot;
			for(WManager::Container *pcontainer = pNewFocus; pcontainer; pNewFocus = pcontainer, pcontainer = pcontainer->GetFocus());
			Config::BackendInterface::SetFocus(pNewFocus);
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

	const std::vector<std::pair<const WManager::Client *, WManager::Client *>> * GetStackAppendix() const{
		return &stackAppendix;
	}

	void TimerEvent(){
		//
	}
};

class DefaultCompositor : public Compositor::X11Compositor, public RunCompositor{
public:
	DefaultCompositor(uint gpuIndex, WManager::Container *_proot, std::vector<std::pair<const WManager::Client *, WManager::Client *>> *_pstackAppendix, Backend::X11Backend *pbackend, args::ValueFlagList<std::string> &shaderPaths) : X11Compositor(gpuIndex,pbackend), RunCompositor(_proot,_pstackAppendix){
		Start();

		for(auto &m : args::get(shaderPaths)){
			boost::filesystem::directory_iterator end;
			for(boost::filesystem::directory_iterator di(m); di != end; ++di){
				if(boost::filesystem::is_regular_file(di->status()) &&
					boost::filesystem::extension(di->path()) == ".spv"){
					Blob blob(di->path().string().c_str());
					AddShader(di->path().filename().string().c_str(),&blob);
				}
			}
		}

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
	DebugCompositor(uint gpuIndex, WManager::Container *_proot, std::vector<std::pair<const WManager::Client *, WManager::Client *>> *_pstackAppendix, Backend::X11Backend *pbackend, args::ValueFlagList<std::string> &shaderPaths) : X11DebugCompositor(gpuIndex,pbackend), RunCompositor(_proot,_pstackAppendix){
		Compositor::X11DebugCompositor::Start();

		for(auto &m : args::get(shaderPaths)){
			boost::filesystem::directory_iterator end;
			for(boost::filesystem::directory_iterator di(m); di != end; ++di){
				if(boost::filesystem::is_regular_file(di->status()) &&
					boost::filesystem::extension(di->path()) == ".spv"){
					Blob blob(di->path().string().c_str());
					AddShader(di->path().filename().string().c_str(),&blob);
				}
			}
		}

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
	args::Flag debugBackend(group_backend,"debugBackend","Create a test environment for the compositor engine without redirection. The application will not act as a window manager.",{'d',"debug-backend"});

	args::Group group_comp(parser,"Compositor",args::Group::Validators::DontCare);
	args::Flag noComp(group_comp,"noComp","Disable compositor.",{"no-compositor",'n'});
	args::ValueFlag<uint> gpuIndex(group_comp,"id","GPU to use by its index. By default the first device in the list of enumerated GPUs will be used.",{"device-index"},0);
	args::ValueFlagList<std::string> shaderPaths(group_comp,"path","Shader lookup path. SPIR-V shader objects are identified by an '.spv' extension.",{"shader-path"});
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

	RunCompositor *pcomp;
	try{
		if(noComp.Get())
			pcomp = new NullCompositor();
		else
		if(debugBackend.Get())
			pcomp = new DebugCompositor(gpuIndex.Get(),pbackend->proot,&pbackend->stackAppendix,pbackend11,shaderPaths);
		else pcomp = new DefaultCompositor(gpuIndex.Get(),pbackend->proot,&pbackend->stackAppendix,pbackend11,shaderPaths);

	}catch(Exception e){
		DebugPrintf(stderr,"%s\n",e.what());
		delete pbackend;
		return 1;
	}

	pbackend->SetCompositor(pcomp);
	//if(pbackend11)
		//pbackend11->SetupEnvironment();

	for(;;){
		//TODO: can we wait for vsync before handling the event? Might help with the stuttering
		sint result = pbackend11->HandleEvent();
		if(result == -1)
			break;
		else
		if(result == 0)
			continue;

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

