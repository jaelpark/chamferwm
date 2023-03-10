#include "main.h"
#include "container.h"
#include "backend.h"
#include "CompositorResource.h"
#include "compositor.h"
#include "CompositorFont.h" //font size
#include "config.h"

#include <cstdlib>
#include <stdarg.h>
#include <time.h>

#include <args.hxx>
#include <iostream>
#include <csignal>

#include <boost/filesystem.hpp>
#include <wordexp.h>

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
			throw Exception("readlink() failed for /proc/self/exe.");
		path[len] = 0;
		pf = fopen((boost::filesystem::path(path).parent_path()/pfileName).string().c_str(),"rb");
		if(pf)
			break;

		//try /usr/share
		pf = fopen((std::string("/usr/share/chamfer/")+pfileName).c_str(),"rb");
		if(pf)
			break;

		snprintf(Exception::buffer,sizeof(Exception::buffer),"Unable to open file: %s",pfileName);
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

static bool sigTerm = false;
void SignalHandler(int sig){
	if(sig == 15)
		sigTerm = true;
}

typedef std::pair<const WManager::Client *, WManager::Client *> StackAppendixElement;
class RunCompositor : public Config::CompositorConfig{
public:
	//RunCompositor(std::vector<StackAppendixElement> *_pstackAppendix, Config::CompositorInterface *_pcompositorInt) : pstackAppendix(_pstackAppendix), Config::CompositorConfig(_pcompositorInt){}
	RunCompositor(std::vector<StackAppendixElement> *_pstackAppendix, Config::CompositorInterface *_pcompositorInt) : Config::CompositorConfig(_pcompositorInt){}
	virtual ~RunCompositor(){}
	virtual void Present() = 0;
	virtual bool IsAnimating() const = 0;
	virtual void WaitIdle() = 0;
//protected:
//	std::vector<StackAppendixElement> *pstackAppendix;
};

class RunBackend : public Config::BackendConfig{
public:
	RunBackend(Config::BackendInterface *_pbackendInt) : pcomp(0), Config::BackendConfig(_pbackendInt){
		//
	}

	virtual ~RunBackend(){}

	void SetCompositor(class RunCompositor *pcomp){
		this->pcomp = pcomp;
		pcompInt = dynamic_cast<Compositor::CompositorInterface *>(pcomp);
	}

	struct ContainerCreateInfo{
		std::string wm_name;
		std::string wm_class;
		std::string shaderName[Compositor::Pipeline::SHADER_MODULE_COUNT];
		bool floating;
	};

	template<class T, class U>
	Config::ContainerInterface & SetupContainer(const ContainerCreateInfo *pcreateInfo){
		boost::python::object containerObject = Config::BackendInterface::pbackendInt->OnCreateContainer();
		boost::python::extract<Config::ContainerInterface &> containerExtract(containerObject);
		if(!containerExtract.check())
			throw Exception("OnCreateContainer(): Invalid container object returned."); //TODO: create default

		Config::ContainerInterface &containerInt = containerExtract();
		containerInt.self = containerObject;

		if(pcreateInfo){
			containerInt.wm_name = pcreateInfo->wm_name;
			containerInt.wm_class = pcreateInfo->wm_class;
			containerInt.vertexShader = pcreateInfo->shaderName[Compositor::Pipeline::SHADER_MODULE_VERTEX];
			containerInt.geometryShader = pcreateInfo->shaderName[Compositor::Pipeline::SHADER_MODULE_GEOMETRY];
			containerInt.fragmentShader = pcreateInfo->shaderName[Compositor::Pipeline::SHADER_MODULE_FRAGMENT];
		}

		return containerInt;
	}

	template<class T, class U>
	Config::ContainerInterface & SetupContainer(WManager::Container *pParent, const ContainerCreateInfo *pcreateInfo){
		Config::ContainerInterface &containerInt = SetupContainer<T,U>(pcreateInfo);
		containerInt.OnSetupContainer();

		WManager::Container::Setup setup;
		if(containerInt.floatingMode == Config::ContainerInterface::FLOAT_ALWAYS ||
			(containerInt.floatingMode != Config::ContainerInterface::FLOAT_NEVER && pcreateInfo && pcreateInfo->floating))
			setup.flags = WManager::Container::FLAG_FLOATING;
		if(pcompInt->ptextEngine)
			setup.titlePad = pcompInt->ptextEngine->GetFontSize();
		containerInt.CopySettingsSetup(setup);

		if(!(setup.flags & WManager::Container::FLAG_FLOATING)){
			if(!pParent){
				boost::python::object parentObject = containerInt.OnParent();
				boost::python::extract<Config::ContainerInterface &> parentExtract(parentObject);
				if(!parentExtract.check())
					throw Exception("OnParent(): Invalid parent container object returned.");

				Config::ContainerInterface &parentInt = parentExtract();
				if(parentInt.pcontainer == containerInt.pcontainer)
					throw Exception("OnParent(): Cannot parent to itself.");

				if(parentInt.pcontainer->pclient){
					Config::ContainerInterface &parentInt1 = SetupContainer<T,U>(parentInt.pcontainer,0);
					pParent = parentInt1.pcontainer;

				}else pParent = parentInt.pcontainer;
			}

			T *pcontainer = new T(&containerInt,pParent,setup,static_cast<U*>(this));
			containerInt.pcontainer = pcontainer;

		}else{
			WManager::Container *proot = WManager::Container::ptreeFocus->GetRoot();
			T *pcontainer = new T(&containerInt,proot,setup,static_cast<U*>(this));
			containerInt.pcontainer = pcontainer;
		}

		return containerInt;
	}

	template<class T, class U>
	void MoveContainer(WManager::Container *pcontainer, WManager::Container *pdst){
		//
		WManager::Container *proot = pcontainer->GetRoot();
		PrintTree(proot,0);
		printf("-----------\n");

		if(pcontainer == pdst || pdst->flags & WManager::Container::FLAG_FLOATING)
			return;

		WManager::Container *premoved = pcontainer->Remove();
		WManager::Container *pOrigParent = premoved->pParent;

		if(pdst->pclient){
			Config::ContainerInterface &parentInt1 = SetupContainer<T,U>(pdst,0);
			pdst = parentInt1.pcontainer;
		}
		pcontainer->Place(pdst);

		printf("----------- removed %p\n",premoved);
		PrintTree(proot,0);
		printf("-----------\n");

		WManager::Container *pNewParent = premoved->pParent;

		WManager::Container *pcollapsed = 0;
		//if(pNewParent != proot)
		if(pNewParent->pParent) //new parent might also be a root
			pcollapsed = pNewParent->Collapse();
		if(!pcollapsed && pNewParent->pch)
			pcollapsed = pNewParent->pch->Collapse();

		if(WManager::Container::ptreeFocus == pcollapsed){
			WManager::Container *pNewFocus = proot;
			for(WManager::Container *pcontainer = pNewFocus; pcontainer; pNewFocus = pcontainer, pcontainer = pcontainer->GetFocus());
			Config::X11ContainerConfig *pNewFocus1 = static_cast<Config::X11ContainerConfig *>(pNewFocus);
			if(pNewFocus1->pcontainerInt->OnFocus())
				pNewFocus1->Focus();
		}

		if(pcollapsed){
			if(pcollapsed->pch)
				ReleaseContainersRecursive(pcollapsed->pch);
			delete pcollapsed;
		}

		pcollapsed = 0;
		//if(pOrigParent != proot)
		if(pOrigParent->pParent)
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

		Config::X11ContainerConfig *pcontainer1 = static_cast<Config::X11ContainerConfig *>(pcontainer);
		if(pcontainer1->pcontainerInt->OnFocus())
			pcontainer1->Focus();

		PrintTree(proot,0);
		printf("-----------\n");

		//if(premoved->pch)
			//ReleaseContainersRecursive(premoved->pch); //should this be here??? once container trees can be moved
		//PrintTree(proot,0);
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
		for(auto &p : stackAppendix){
			if(p.second->pcontainer->GetParent() != 0){ //check that it's not a root container
				const Config::ContainerConfig *pcontainer1 = dynamic_cast<const Config::ContainerConfig *>(p.second->pcontainer);
				if(pcontainer1 && pcontainer1->pcontainerInt->pcontainer == p.second->pcontainer)
					pcontainer1->pcontainerInt->pcontainer = 0;

				if(p.second->pcontainer)
					delete p.second->pcontainer;
			}
			delete p.second;
		}
		stackAppendix.clear();
		//
		if(!WManager::Container::rootQueue.empty()){
			WManager::Container *proot1 = WManager::Container::rootQueue.front();
			WManager::Container *pRootNext = proot1->pRootNext;
			do{
				if(pRootNext->pch)
					ReleaseContainersRecursive(pRootNext->pch);

				pRootNext = pRootNext->pRootNext;
			}while(pRootNext != proot1);
		}
	}

	void PrintTree(WManager::Container *pcontainer, uint level) const{
		for(uint i = 0; i < level; ++i)
			printf("  ");
		printf("%p: ",pcontainer);
		if(pcontainer->pclient)
			printf("(client), ");
		if(pcontainer->pParent)
			printf("(parent: %p), ",pcontainer->pParent);
		if(WManager::Container::ptreeFocus == pcontainer)
			printf("(focus), ");
		if(pcontainer->flags & WManager::Container::FLAG_STACKED)
			printf("(+stacked), ");
		if(pcontainer->flags & WManager::Container::FLAG_FULLSCREEN)
			printf("(+fullscr.), ");
		Config::ContainerConfig *pcontainerConfig = dynamic_cast<Config::ContainerConfig *>(pcontainer);
		printf("[ContainerConfig: %p, (->container: %p)], ",pcontainerConfig,pcontainerConfig->pcontainerInt->pcontainer);
		//printf("focusQueue: %lu (%p), stackQueue: %lu (%p)\n",pcontainer->focusQueue.size(),pcontainer->focusQueue.size() > 0?pcontainer->focusQueue.back():0,pcontainer->stackQueue.size(),pcontainer->stackQueue.size() > 0?pcontainer->stackQueue.back():0);
		printf("focusQueue: %lu (%p), stackQueue: %lu (%p) {",pcontainer->focusQueue.size(),pcontainer->focusQueue.size() > 0?pcontainer->focusQueue.back():0,pcontainer->stackQueue.size(),pcontainer->stackQueue.size() > 0?pcontainer->stackQueue.back():0);
		for(WManager::Container *pcont : pcontainer->stackQueue)
			printf("%p,",pcont);
		printf("}\n");
		for(WManager::Container *pcontainer1 = pcontainer->pch; pcontainer1; pcontainer1 = pcontainer1->pnext)
			PrintTree(pcontainer1,level+1);
		if(level == 0)
			for(auto &p : stackAppendix)
				printf("FLOAT %p, flags: %x\n",p.second->pcontainer,p.second->pcontainer->flags);
	}

//protected:
	std::vector<StackAppendixElement> stackAppendix;
	class RunCompositor *pcomp;
	Compositor::CompositorInterface *pcompInt; //dynamic casted from pcomp on initialization
};

class DefaultBackend : public Backend::Default, public RunBackend{
public:
	DefaultBackend(Config::BackendInterface *_pbackendInt) : Default(_pbackendInt->standaloneComp), RunBackend(_pbackendInt){
		Start();
		DebugPrintf(stdout,"Backend initialized.\n");
	}

	~DefaultBackend(){
		Stop();
		if(!WManager::Container::rootQueue.empty()){
			WManager::Container *proot1 = WManager::Container::rootQueue.front();
			WManager::Container *pRootNext = proot1->pRootNext;
			do{
				WManager::Container *pRootNext2 = pRootNext->pRootNext;
				delete pRootNext;

				pRootNext = pRootNext2;
			}while(pRootNext != proot1);
		}
	}

	void DefineBindings(){
		Config::BackendInterface::pbackendInt->OnSetupKeys(false);
	}

	Backend::X11Client * SetupClient(const Backend::X11Client::CreateInfo *pcreateInfo){
		//Containers should be created always under the parent of the current focus.
		//config script should manage this (point to container which should be the
		//parent of the new one), while also setting some of the parameters like border
		//width and such.
		//Compositor::X11Compositor *pcomp11 = dynamic_cast<Compositor::X11Compositor *>(pcomp);

		ContainerCreateInfo containerCreateInfo;
		containerCreateInfo.wm_name = pcreateInfo->pwmName->pstr;
		containerCreateInfo.wm_class = pcreateInfo->pwmClass->pstr;
		containerCreateInfo.shaderName[Compositor::Pipeline::SHADER_MODULE_VERTEX] = "frame_vertex.spv";
		containerCreateInfo.shaderName[Compositor::Pipeline::SHADER_MODULE_GEOMETRY] = "frame_geometry.spv";
		containerCreateInfo.shaderName[Compositor::Pipeline::SHADER_MODULE_FRAGMENT] = "frame_fragment.spv";
		containerCreateInfo.floating = (pcreateInfo->hints & Backend::X11Client::CreateInfo::HINT_FLOATING) != 0;

		if(pcreateInfo->mode == Backend::X11Client::CreateInfo::CREATE_AUTOMATIC){
			//create a temporary container through which (py) OnSetupClient() can be called. The container is discarded as soon as the call has been made. 
			Config::ContainerInterface &containerInt = SetupContainer<Config::X11ContainerConfig,DefaultBackend>(&containerCreateInfo);

			containerInt.OnSetupClient();

			const char *pshaderName[Compositor::Pipeline::SHADER_MODULE_COUNT] = {
				containerInt.vertexShader.c_str(),containerInt.geometryShader.c_str(),containerInt.fragmentShader.c_str()
			};
			//WManager::Container *proot = WManager::Container::ptreeFocus->GetRoot(); //TODO: stack's root?
			Backend::X11Container *proot = static_cast<Backend::X11Container *>(WManager::Container::ptreeFocus->GetRoot());
			Backend::X11Client *pclient11 = SetupClient(proot,pcreateInfo,pshaderName);

			if(pclient11) //in some cases the window is found to be unmanageable
				stackAppendix.push_back(StackAppendixElement(pcreateInfo->pstackClient,pclient11));
			return pclient11;

		}else{
			Config::ContainerInterface &containerInt = SetupContainer<Config::X11ContainerConfig,DefaultBackend>(0,&containerCreateInfo); //null parent = obtain parent from config script

			if(pcreateInfo->hints & Backend::X11Client::CreateInfo::HINT_NO_INPUT)
				containerInt.pcontainer->flags |= WManager::Container::FLAG_NO_FOCUS;

			containerInt.OnSetupClient();

			const char *pshaderName[Compositor::Pipeline::SHADER_MODULE_COUNT] = {
				containerInt.vertexShader.c_str(),containerInt.geometryShader.c_str(),containerInt.fragmentShader.c_str()
			};
			Backend::X11Container *pcontainer11 = static_cast<Backend::X11Container *>(containerInt.pcontainer);
			Backend::X11Client *pclient11 = SetupClient(pcontainer11,pcreateInfo,pshaderName);
			pcontainer11->SetClient(pclient11);
			if(pclient11 && containerInt.pcontainer->flags & WManager::Container::FLAG_FLOATING)
				stackAppendix.push_back(StackAppendixElement(pcreateInfo->pstackClient,pclient11));

			containerInt.DeferredPropertyTransfer();
			containerInt.OnCreate();

			return pclient11;
		}
	}

	Backend::X11Client * SetupClient(Backend::X11Container *pcontainer, const Backend::X11Client::CreateInfo *pcreateInfo, const char *pshaderName[Compositor::Pipeline::SHADER_MODULE_COUNT]){
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

	void FloatContainer(WManager::Container *pcontainer){
		if(!pcontainer->pclient){
			DebugPrintf(stderr,"Only client containers can be floated.\n");
			return; //who knows we'll fix this limitation someday
		}
		auto n1 = std::find_if(WManager::Container::tiledFocusQueue.begin(),WManager::Container::tiledFocusQueue.end(),[&](auto &p)->bool{
			return p.first == pcontainer;
		});
		if(n1 != WManager::Container::tiledFocusQueue.end())
			WManager::Container::tiledFocusQueue.erase(n1);
		auto n2 = std::find(WManager::Container::floatFocusQueue.begin(),WManager::Container::floatFocusQueue.end(),pcontainer);
		if(n2 != WManager::Container::floatFocusQueue.end())
			WManager::Container::floatFocusQueue.erase(n2);

		WManager::Container *proot = pcontainer->GetRoot();
		PrintTree(proot,0);
		printf("-----------\n");

		WManager::Client *pbase = pcontainer->pclient;
		auto m = std::find_if(stackAppendix.begin(),stackAppendix.end(),[&](auto &p)->bool{
			return pbase == p.second;
		});
		if(m != stackAppendix.end()){
			stackAppendix.erase(m);
			//TODO: call OnParent
			pcontainer->flags &= ~WManager::Container::FLAG_FLOATING;
			pcontainer->Place(proot);

			printf("----------- placed %p\n",pcontainer);
			PrintTree(proot,0);

			return;
		}
		WManager::Container *premoved = pcontainer->Remove();
		WManager::Container *pOrigParent = premoved->pParent;

		printf("----------- removed %p\n",premoved);
		PrintTree(proot,0);

		WManager::Container *pcollapsed = 0;
		if(pOrigParent != proot)
			pcollapsed = pOrigParent->Collapse();
		//check if pch is alive, in this case this wasn't the last container
		if(!pcollapsed && pOrigParent->pch)
			pcollapsed = pOrigParent->pch->Collapse();

		if(WManager::Container::ptreeFocus == pcontainer || WManager::Container::ptreeFocus == pcollapsed){
			WManager::Container *pNewFocus = proot;
			for(WManager::Container *pcontainer = pNewFocus; pcontainer; pNewFocus = pcontainer, pcontainer = pcontainer->GetFocus());
			Config::X11ContainerConfig *pNewFocus1 = static_cast<Config::X11ContainerConfig *>(pNewFocus);
			if(pNewFocus1->pcontainerInt->OnFocus())
				pNewFocus1->Focus();
		}

		printf("----------- collapsed %p\n",pcollapsed);
		PrintTree(proot,0);

		/*if(premoved->pch) //in this case nothing gets removed
			ReleaseContainersRecursive(premoved->pch);
		delete premoved;*/
		if(pcollapsed){
			if(pcollapsed->pch) //this should not exist
				ReleaseContainersRecursive(pcollapsed->pch);
			delete pcollapsed;
		}
		
		static WManager::Client dummyClient(0); //base client being unavailable means that the client is stacked on top of everything else
	
		pcontainer->flags |= WManager::Container::FLAG_FLOATING;
		pcontainer->p = pcontainer->p+0.5f*(pcontainer->e-glm::vec2(0.3f,0.4f));
		pcontainer->SetSize(glm::vec2(0.3f,0.4f)); //TODO: this should be determined better or memorized
		if(pcontainer->pclient)
			stackAppendix.push_back(StackAppendixElement(&dummyClient,pcontainer->pclient));

		pcontainer->pParent = proot;
		pcontainer->Focus();
	}

	WManager::Container * CreateWorkspace(const char *pname){
		WManager::Container *plastRoot = WManager::Container::rootQueue.back();
		Config::X11ContainerConfig *pnewRoot = new Config::X11ContainerConfig(this);
		pnewRoot->SetName(pname);
		plastRoot->AppendRoot(pnewRoot);
		return pnewRoot;
	}

	//Called for spontaneous (client requested) fullscreen triggered by an event
	void SetFullscreen(Backend::X11Client *pclient, bool toggle){
		if(!pclient || pclient->pcontainer == pclient->pcontainer->GetRoot())
			return;
		Config::X11ContainerConfig *pcontainer1 = static_cast<Config::X11ContainerConfig *>(pclient->pcontainer);
		if(pcontainer1->pcontainerInt->OnFullscreen(toggle))
			pcontainer1->SetFullscreen(toggle);
	}

	//Called for spontaneous (client requested) focus triggered by an event
	void SetFocus(Backend::X11Client *pclient){
		if(!pclient || pclient->pcontainer == pclient->pcontainer->GetRoot())
			return;
		Config::X11ContainerConfig *pcontainer1 = static_cast<Config::X11ContainerConfig *>(pclient->pcontainer);
		if(pcontainer1->pcontainerInt->OnFocus())
			pcontainer1->Focus();
	}

	void Enter(Backend::X11Client *pclient){
		if(!pclient || pclient->pcontainer == pclient->pcontainer->GetRoot())
			return;
		Config::X11ContainerConfig *pcontainer1 = static_cast<Config::X11ContainerConfig *>(pclient->pcontainer);
		pcontainer1->pcontainerInt->OnEnter();
	}

	void PropertyChange(Backend::X11Client *pclient, PROPERTY_ID id, const Backend::BackendProperty *pProperty){
		if(!pclient){
			//root window
			const Backend::BackendPixmapProperty *pPixmapProperty = static_cast<const Backend::BackendPixmapProperty *>(pProperty);
			Compositor::X11Compositor *pcomp11 = dynamic_cast<Compositor::X11Compositor *>(pcomp);
			if(pcomp11)
				pcomp11->SetBackgroundPixmap(pPixmapProperty);
			return;
		}
		if(pclient->pcontainer == pclient->pcontainer->GetRoot())
			return;
		Config::X11ContainerConfig *pcontainer1 = static_cast<Config::X11ContainerConfig *>(pclient->pcontainer);
		if(id == PROPERTY_ID_WM_NAME){
			pcontainer1->pcontainerInt->wm_name = static_cast<const Backend::BackendStringProperty *>(pProperty)->pstr;
			pcontainer1->pcontainerInt->OnPropertyChange(Config::ContainerInterface::PROPERTY_ID_NAME);
		}else
		if(id == PROPERTY_ID_WM_CLASS){
			pcontainer1->pcontainerInt->wm_name = static_cast<const Backend::BackendStringProperty *>(pProperty)->pstr;
			pcontainer1->pcontainerInt->OnPropertyChange(Config::ContainerInterface::PROPERTY_ID_CLASS);
		}else
		if(id == PROPERTY_ID_TRANSIENT_FOR){
			//
			//TODO!!
			/*WManager::Container *pstackContainer = static_cast<const Backend::BackendContainerProperty *>(pProperty)->pcontainer;
			auto m = std::find_if(stackAppendix.begin(),stackAppendix.end(),[&](auto &p)->bool{
				return pclient == p.second;
			});
			if(m != stackAppendix.end())
				(*m).first = pstackContainer;*/
			//else: TODO: should we remove it from the tiling tree and make it floating according to its rect?
		}
	}

	void DestroyClient(Backend::X11Client *pclient){
		auto n1 = std::find_if(WManager::Container::tiledFocusQueue.begin(),WManager::Container::tiledFocusQueue.end(),[&](auto &p)->bool{
			return p.first == pclient->pcontainer;
		});
		if(n1 != WManager::Container::tiledFocusQueue.end())
			WManager::Container::tiledFocusQueue.erase(n1);
		auto n2 = std::find(WManager::Container::floatFocusQueue.begin(),WManager::Container::floatFocusQueue.end(),pclient->pcontainer);
		if(n2 != WManager::Container::floatFocusQueue.end())
			WManager::Container::floatFocusQueue.erase(n2);

		WManager::Container *proot = pclient->pcontainer->GetRoot();
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

		printf("----------- removed %p\n",premoved);
		PrintTree(proot,0);

		WManager::Container *pcollapsed = 0;
		if(pOrigParent != proot)
			pcollapsed = pOrigParent->Collapse();
		//check if pch is alive, in this case this wasn't the last container
		if(!pcollapsed && pOrigParent->pch)
			pcollapsed = pOrigParent->pch->Collapse();

		if(WManager::Container::ptreeFocus == pclient->pcontainer || WManager::Container::ptreeFocus == pcollapsed){
			WManager::Container *pNewFocus = proot;
			//for(WManager::Container *pcontainer = pNewFocus; pcontainer; pNewFocus = pcontainer, pcontainer = pcontainer->focusQueue.size() > 0?pcontainer->focusQueue.back():pcontainer->pch);
			for(WManager::Container *pcontainer = pNewFocus; pcontainer; pNewFocus = pcontainer, pcontainer = pcontainer->GetFocus());
			Config::X11ContainerConfig *pNewFocus1 = static_cast<Config::X11ContainerConfig *>(pNewFocus);
			if(pNewFocus1->pcontainerInt->OnFocus())
				pNewFocus1->Focus();
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

		printf("----------- collapsed %p\n",pcollapsed);
		PrintTree(proot,0);
		printf("stackAppendix: %lu\n",stackAppendix.size());
	}

	bool EventNotify(const Backend::BackendEvent *pevent){
		Compositor::X11Compositor *pcomp11 = dynamic_cast<Compositor::X11Compositor *>(pcomp);
		if(!pcomp11)
			return false;
		const Backend::X11Event *pevent11 = static_cast<const Backend::X11Event *>(pevent);
		return pcomp11->FilterEvent(pevent11);
	}

	void WakeNotify(){
		pcompInt->FullDamageRegion();
	}

	void KeyPress(uint keyId, bool down){
		if(down)
			Config::BackendInterface::pbackendInt->OnKeyPress(keyId);
		else Config::BackendInterface::pbackendInt->OnKeyRelease(keyId);
	}

	const std::vector<StackAppendixElement> * GetStackAppendix() const{
		return &stackAppendix;
	}

	void SortStackAppendix(){
		std::sort(stackAppendix.begin(),stackAppendix.end(),[&](StackAppendixElement &a, StackAppendixElement &b)->bool{
			for(auto *p : WManager::Container::floatFocusQueue){
				if(p == a.second->pcontainer)
					return true;
				if(p == b.second->pcontainer)
					return false;
			}
			return false;
		});
	}

	void TimerEvent(){
		//
		Config::BackendInterface::pbackendInt->OnTimer();
	}

	bool ApproveExternal(const Backend::BackendStringProperty *pwmName, const Backend::BackendStringProperty *pwmClass){
		return Config::CompositorInterface::pcompositorInt->OnRedirectExternal(pwmName->pstr,pwmClass->pstr);
	}
};

//TODO: some of these functions can be templated and shared with the DefaultBackend
class DebugBackend : public Backend::Debug, public RunBackend{
public:
	DebugBackend(Config::BackendInterface *_pbackendInt) : Debug(), RunBackend(_pbackendInt){
		Start();
		DebugPrintf(stdout,"Backend initialized.\n");
	}

	~DebugBackend(){
		Stop();
		if(!WManager::Container::rootQueue.empty()){
			WManager::Container *proot1 = WManager::Container::rootQueue.front();
			WManager::Container *pRootNext = proot1->pRootNext;
			do{
				WManager::Container *pRootNext2 = pRootNext->pRootNext;
				delete pRootNext;

				pRootNext = pRootNext2;
			}while(pRootNext != proot1);
		}
	}

	Backend::DebugClient * SetupClient(const Backend::DebugClient::CreateInfo *pcreateInfo){
		Config::ContainerInterface &containerInt = SetupContainer<Config::DebugContainerConfig,DebugBackend>(0,0);

		static const char *pshaderName[Compositor::Pipeline::SHADER_MODULE_COUNT] = {
			"frame_vertex.spv","frame_geometry.spv","frame_fragment.spv"
		};

		Backend::DebugClient *pclient;
		Compositor::X11DebugCompositor *pcomp11 = dynamic_cast<Compositor::X11DebugCompositor *>(pcomp);
		Backend::DebugContainer *pdebugContainer = static_cast<Backend::DebugContainer *>(containerInt.pcontainer);
		if(!pcomp11)
			pclient = new Backend::DebugClient(pdebugContainer,pcreateInfo);
		else pclient = new Compositor::X11DebugClientFrame(pdebugContainer,pcreateInfo,pshaderName,pcomp11);
		pdebugContainer->SetClient(pclient);
		//containerInt.pcontainer->pclient = pclient;
		//

		containerInt.OnCreate();

		return pclient;
	}

	void MoveContainer(WManager::Container *pcontainer, WManager::Container *pdst){
		//
		RunBackend::MoveContainer<Config::DebugContainerConfig,DebugBackend>(pcontainer,pdst);
	}

	void FloatContainer(WManager::Container *pcontainer){
		//
	}

	void DestroyClient(Backend::DebugClient *pclient){
		WManager::Client *pbase = pclient;
		auto m = std::find_if(stackAppendix.begin(),stackAppendix.end(),[&](auto &p)->bool{
			return pbase == p.second;
		});
		if(m != stackAppendix.end())
			stackAppendix.erase(m);
		WManager::Container *proot = pclient->pcontainer->GetRoot();
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

		if(WManager::Container::ptreeFocus == pclient->pcontainer || WManager::Container::ptreeFocus == pcollapsed){
			WManager::Container *pNewFocus = proot;
			for(WManager::Container *pcontainer = pNewFocus; pcontainer; pNewFocus = pcontainer, pcontainer = pcontainer->GetFocus());
			Config::DebugContainerConfig *pNewFocus1 = dynamic_cast<Config::DebugContainerConfig *>(pNewFocus);
			if(pNewFocus1->pcontainerInt->OnFocus())
				pNewFocus1->Focus();
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

	void DefineBindings(){
		Config::BackendInterface::pbackendInt->OnSetupKeys(true);
	}

	bool EventNotify(const Backend::BackendEvent *pevent){
		//nothing to process
		return false;
	}

	void WakeNotify(){
		pcompInt->FullDamageRegion();
	}

	void KeyPress(uint keyId, bool down){
		if(down)
			Config::BackendInterface::pbackendInt->OnKeyPress(keyId);
		else Config::BackendInterface::pbackendInt->OnKeyRelease(keyId);
	}

	const std::vector<StackAppendixElement> * GetStackAppendix() const{
		return &stackAppendix;
	}

	void SortStackAppendix(){
		//
	}

	void TimerEvent(){
		//
	}

	bool ApproveExternal(const Backend::BackendStringProperty *pwmName, const Backend::BackendStringProperty *pwmClass){
		return true;
	}
};

class DefaultCompositor : public Compositor::X11Compositor, public RunCompositor{
public:
	DefaultCompositor(std::vector<StackAppendixElement> *_pstackAppendix, Backend::X11Backend *pbackend, args::ValueFlagList<std::string> &shaderPaths, Config::CompositorInterface *_pcompositorInt) : X11Compositor(pconfig = new Configuration{_pcompositorInt->deviceIndex,_pcompositorInt->debugLayers,_pcompositorInt->scissoring,_pcompositorInt->incrementalPresent,_pcompositorInt->memoryImportMode,_pcompositorInt->unredirOnFullscreen,_pcompositorInt->enableAnimation,_pcompositorInt->animationDuration,_pcompositorInt->fontName.c_str(),_pcompositorInt->fontSize},pbackend), RunCompositor(_pstackAppendix,_pcompositorInt){
		Start();

		wordexp_t expResult;
		for(auto &m : args::get(shaderPaths)){
			wordexp(m.c_str(),&expResult,WRDE_NOCMD);
			try{
				boost::filesystem::directory_iterator end;
				for(boost::filesystem::directory_iterator di(expResult.we_wordv[0]); di != end; ++di){
					if(boost::filesystem::is_regular_file(di->status()) &&
						boost::filesystem::extension(di->path()) == ".spv"){
						Blob blob(di->path().string().c_str());
						AddShader(di->path().filename().string().c_str(),&blob);
					}
				}
			}catch(boost::filesystem::filesystem_error &e){
				DebugPrintf(stderr,"%s\n",e.what());
			}
			wordfree(&expResult);
		}

		ClearBackground();

		DebugPrintf(stdout,"Compositor enabled.\n");
	}

	~DefaultCompositor(){
		delete pconfig;
		Stop();
	}

	void Present(){
		Config::ContainerInterface::UpdateShaders();
		Backend::X11Container *proot = static_cast<Backend::X11Container *>(WManager::Container::ptreeFocus->GetRoot());

		if(!PollFrameFence(proot->noComp))
			return;

		GenerateCommandBuffers(&pbackend->clientStack,WManager::Container::ptreeFocus,pbackend->pfocusInClient);
		Compositor::X11Compositor::Present();
	}

	bool IsAnimating() const{
		return playingAnimation;
	}

	void WaitIdle(){
		Compositor::X11Compositor::WaitIdle();
	}

	Configuration *pconfig;
};

class DebugCompositor : public Compositor::X11DebugCompositor, public RunCompositor{
public:
	DebugCompositor(std::vector<StackAppendixElement> *_pstackAppendix, Backend::X11Backend *pbackend, args::ValueFlagList<std::string> &shaderPaths, Config::CompositorInterface *_pcompositorInt) : X11DebugCompositor(pconfig = new Configuration{_pcompositorInt->deviceIndex,_pcompositorInt->debugLayers,_pcompositorInt->scissoring,_pcompositorInt->incrementalPresent,_pcompositorInt->memoryImportMode,_pcompositorInt->unredirOnFullscreen,_pcompositorInt->enableAnimation,_pcompositorInt->animationDuration,_pcompositorInt->fontName.c_str(),_pcompositorInt->fontSize},pbackend), RunCompositor(_pstackAppendix,_pcompositorInt){
		Compositor::X11DebugCompositor::Start();

		wordexp_t expResult;
		for(auto &m : args::get(shaderPaths)){
			wordexp(m.c_str(),&expResult,WRDE_NOCMD);
			try{
				boost::filesystem::directory_iterator end;
				for(boost::filesystem::directory_iterator di(expResult.we_wordv[0]); di != end; ++di){
					if(boost::filesystem::is_regular_file(di->status()) &&
						boost::filesystem::extension(di->path()) == ".spv"){
						Blob blob(di->path().string().c_str());
						AddShader(di->path().filename().string().c_str(),&blob);
					}
				}
			}catch(boost::filesystem::filesystem_error &e){
				DebugPrintf(stderr,"%s\n",e.what());
			}
			wordfree(&expResult);
		}

		ClearBackground();

		DebugPrintf(stdout,"Compositor enabled.\n");
	}

	~DebugCompositor(){
		Compositor::X11DebugCompositor::Stop();
		delete pconfig;
	}

	void Present(){
		Config::ContainerInterface::UpdateShaders();
		if(!PollFrameFence(false))
			return;
		
		GenerateCommandBuffers(&pbackend->clientStack,WManager::Container::ptreeFocus,pbackend->pfocusInClient);
		Compositor::X11DebugCompositor::Present();
	}

	bool IsAnimating() const{
		return playingAnimation;
	}

	void WaitIdle(){
		Compositor::X11DebugCompositor::WaitIdle();
	}

	Configuration *pconfig;
};

class NullCompositor : public Compositor::NullCompositor, public RunCompositor{
public:
	NullCompositor(Config::CompositorInterface *_pcompositorInt) : Compositor::NullCompositor(), RunCompositor(0,_pcompositorInt){
		Start();
	}

	~NullCompositor(){
		Stop();
	}

	void Present(){
		//
	}

	bool IsAnimating() const{
		return false;
	}

	void WaitIdle(){
		//
	}
};

int main(sint argc, const char **pargv){	
	args::ArgumentParser parser("chamferwm - A compositing window manager","");
	args::HelpFlag help(parser,"help","Display this help menu",{'h',"help"});

	args::ValueFlag<std::string> configPath(parser,"path","Configuration Python script",{"config",'c'});

	args::Group group_backend(parser,"Backend",args::Group::Validators::DontCare);
	args::Flag debugBackend(group_backend,"debugBackend","Create a test environment for the compositor engine without redirection. The application will not act as a window manager.",{'d',"debug-backend"});
	args::Flag staComp(group_backend,"standaloneCompositor","Standalone compositor for external window managers.",{'C',"standalone-compositor"});

	args::Group group_comp(parser,"Compositor",args::Group::Validators::DontCare);
	args::Flag noComp(group_comp,"noComp","Disable compositor.",{"no-compositor",'n'});
	args::ValueFlag<uint> deviceIndexOpt(group_comp,"id","GPU to use by its index. By default the first device in the list of enumerated GPUs will be used.",{"device-index"});
	args::Flag debugLayersOpt(group_comp,"debugLayers","Enable Vulkan debug layers.",{"debug-layers",'l'},false);
	args::Flag noScissoringOpt(group_comp,"noScissoring","Disable scissoring optimization.",{"no-scissoring"},false);
	args::Flag noIncPresentOpt(group_comp,"noIncPresent","Disable incremental present support.",{"no-incremental-present"},false);
	args::ValueFlag<uint> memoryImportMode(group_comp,"memoryImportMode","Memory import mode:\n0: DMA-buf import (fastest, default)\n1: Host memory import (compatibility)\n2: CPU copy (slow, compatibility)",{"memory-import-mode",'m'});
	args::Flag unredirOnFullscreenOpt(group_comp,"unredirOnFullscreen","Unredirect a fullscreen window bypassing the compositor to improve performance.",{"unredir-on-fullscreen"},false);
	args::ValueFlagList<std::string> shaderPaths(group_comp,"path","Shader lookup path. SPIR-V shader objects are identified by an '.spv' extension. Multiple paths may be specified through multiple --shader-path, and the first specified paths will take priority over the later ones.",{"shader-path"},{"/usr/share/chamfer/shaders"});

	try{
		parser.ParseCLI(argc,pargv);

	}catch(args::Help &e){
		std::cout<<parser;
		return 0;

	}catch(args::ParseError &e){
		std::cout<<e.what()<<std::endl<<parser;
		return 1;
	}

	Config::Loader::standaloneComp = staComp.Get();
	Config::Loader::noCompositor = noComp.Get();
	Config::Loader::deviceIndex = deviceIndexOpt?deviceIndexOpt.Get():0;
	Config::Loader::debugLayers = debugLayersOpt.Get();
	Config::Loader::scissoring = !noScissoringOpt.Get();
	Config::Loader::incrementalPresent = !noIncPresentOpt.Get();
	if(memoryImportMode){
		if(memoryImportMode.Get() >= Compositor::CompositorInterface::IMPORT_MODE_COUNT){
			DebugPrintf(stderr,"Invalid memory import mode specified (%u). Mode specifier must be less than %u. See --help for available options.\n",memoryImportMode.Get(),Compositor::CompositorInterface::IMPORT_MODE_COUNT);
			return 1;
		}
		Config::Loader::memoryImportMode = (Compositor::CompositorInterface::IMPORT_MODE)memoryImportMode.Get();
	}else Config::Loader::memoryImportMode = Compositor::CompositorInterface::IMPORT_MODE_DMABUF;
	Config::Loader::unredirOnFullscreen = unredirOnFullscreenOpt.Get();

	Config::Loader *pconfigLoader = new Config::Loader(pargv[0]);
	if(!pconfigLoader->Run(configPath?configPath.Get().c_str():0,"config.py"))
		return 1;

	if(staComp.Get()){
		if(noComp.Get()){
			DebugPrintf(stderr,"Incompatible options: --no-compositor (-n) and --standalone-compositor (-C).\n");
			return 1;
		}
		Config::BackendInterface::pbackendInt->standaloneComp = true;
	}

	if(noComp)
		Config::CompositorInterface::pcompositorInt->noCompositor = noComp.Get();
	if(deviceIndexOpt)
		Config::CompositorInterface::pcompositorInt->deviceIndex = deviceIndexOpt.Get();
	if(debugLayersOpt.Get())
		Config::CompositorInterface::pcompositorInt->debugLayers = true;
	if(noScissoringOpt.Get())
		Config::CompositorInterface::pcompositorInt->scissoring = false;
	if(noIncPresentOpt.Get())
		Config::CompositorInterface::pcompositorInt->incrementalPresent = false;
	if(memoryImportMode)
		Config::CompositorInterface::pcompositorInt->memoryImportMode = (Compositor::CompositorInterface::IMPORT_MODE)memoryImportMode.Get();
	if(unredirOnFullscreenOpt.Get())
		Config::CompositorInterface::pcompositorInt->unredirOnFullscreen = true;
	
	Backend::X11Backend *pbackend11;
	try{
		if(debugBackend.Get()){
			pbackend11 = new DebugBackend(Config::BackendInterface::pbackendInt);
			WManager::Container::ptreeFocus = new Config::DebugContainerConfig(pbackend11); //self registered
		}else{
			pbackend11 = new DefaultBackend(Config::BackendInterface::pbackendInt);
			WManager::Container::ptreeFocus = new Config::X11ContainerConfig(pbackend11); //self registered
		}

	}catch(Exception e){
		DebugPrintf(stderr,"%s\n",e.what());
		return 1;
	}

	WManager::Container::ptreeFocus->SetName("1");

	RunBackend *pbackend = dynamic_cast<RunBackend *>(pbackend11);

	RunCompositor *pcomp;
	try{
		if(Config::CompositorInterface::pcompositorInt->noCompositor)
			pcomp = new NullCompositor(Config::CompositorInterface::pcompositorInt);
		else
		if(debugBackend.Get())
			pcomp = new DebugCompositor(&pbackend->stackAppendix,pbackend11,shaderPaths,Config::CompositorInterface::pcompositorInt);
		else pcomp = new DefaultCompositor(&pbackend->stackAppendix,pbackend11,shaderPaths,Config::CompositorInterface::pcompositorInt);

	}catch(Exception e){
		DebugPrintf(stderr,"%s\n",e.what());
		delete pbackend;
		return 1;
	}

	pbackend->SetCompositor(pcomp);

	signal(SIGTERM,SignalHandler);

	for(;;){
		//TODO: can we wait for vsync before handling the event? Might help with the stuttering
		sint result = pbackend11->HandleEvent(pcomp->IsAnimating());
		if(result == -1 || sigTerm)
			break;
		else
		if(result == 0 && !pcomp->IsAnimating())
			continue;

		try{
			pcomp->Present();

		}catch(Exception e){
			DebugPrintf(stderr,"%s\n",e.what());
			break;
		}
	}

	DebugPrintf(stdout,"Exit\n");

	Config::BackendInterface::pbackendInt->OnExit();

	pcomp->WaitIdle();
	pbackend->ReleaseContainers();

	delete pcomp;
	delete pbackend;
	delete pconfigLoader;

	return 0;
}

