#include "main.h"
#include "container.h"
#include "backend.h"
#include "CompositorResource.h"
#include "compositor.h"
#include "config.h"
#include <X11/keysym.h>
#include <wordexp.h>

namespace Config{

ContainerInterface::ContainerInterface() :
canvasOffset(boost::python::make_tuple(0.0f,0.0f)),
canvasExtent(boost::python::make_tuple(0.0f,0.0f)),
margin(boost::python::make_tuple(0.0f,0.0f)),
size(boost::python::make_tuple(1.0f,1.0f)),
minSize(boost::python::make_tuple(0.0f,0.0f)),
maxSize(boost::python::make_tuple(1.0f,1.0f)),
floatingMode(FLOAT_AUTOMATIC),
titleBar(WManager::Container::TITLEBAR_NONE),
titleStackOnly(false),
shaderUserFlags(0),
pcontainer(0){
	//
}

ContainerInterface::~ContainerInterface(){
	//erase from update queues
	shaderUpdateQueue.erase(this);
}

//Used to copy tuples and other python object values from the interface to setup struct. The user may set some values before the internal Container instance is created, and they must be propagated forward.
//-CopySettingsSetup: propagate container specific properties
//-DeferredPropertyTransfer: propagate client specific properties
void ContainerInterface::CopySettingsSetup(WManager::Container::Setup &setup){
	setup.pname = name.c_str();
	setup.canvasOffset.x = boost::python::extract<float>(canvasOffset[0])();
	setup.canvasOffset.y = boost::python::extract<float>(canvasOffset[1])();
	setup.canvasExtent.x = boost::python::extract<float>(canvasExtent[0])();
	setup.canvasExtent.y = boost::python::extract<float>(canvasExtent[1])();
	setup.margin.x = boost::python::extract<float>(margin[0])();
	setup.margin.y = boost::python::extract<float>(margin[1])();
	setup.size.x = boost::python::extract<float>(size[0])();
	setup.size.y = boost::python::extract<float>(size[1])();
	setup.minSize.x = boost::python::extract<float>(minSize[0])();
	setup.minSize.y = boost::python::extract<float>(minSize[1])();
	setup.maxSize.x = boost::python::extract<float>(maxSize[0])();
	setup.maxSize.y = boost::python::extract<float>(maxSize[1])();
	setup.titleBar = titleBar;
	setup.titleStackOnly = titleStackOnly;
}

void ContainerInterface::DeferredPropertyTransfer(){
	Compositor::ClientFrame *pclientFrame = dynamic_cast<Compositor::ClientFrame *>(pcontainer->pclient);
	if(!pclientFrame)
		return;
	pclientFrame->shaderUserFlags = shaderUserFlags;
}

void ContainerInterface::OnSetupContainer(){
	//
}

void ContainerInterface::OnSetupClient(){
	//
}

boost::python::object ContainerInterface::OnParent(){
	//
	return boost::python::object();
}

void ContainerInterface::OnCreate(){
	//TODO: focus on this
}

bool ContainerInterface::OnFullscreen(bool toggle){
	//
	return true;
}

void ContainerInterface::OnStack(bool toggle){
	//
}

void ContainerInterface::OnFloat(bool toggle){
	//
}

bool ContainerInterface::OnFocus(){
	//
	return true;
}

void ContainerInterface::OnEnter(){
	//
}

void ContainerInterface::OnPropertyChange(PROPERTY_ID id){
	//
}

boost::python::object ContainerInterface::GetNext() const{
	if(!pcontainer){
		PyErr_SetString(PyExc_ValueError,"Invalid or expired container.");
		return boost::python::object();
	}
	WManager::Container *pnext = pcontainer->GetNext();
	ContainerConfig *pcontainer1 = dynamic_cast<ContainerConfig *>(pnext);
	if(pcontainer1)
		return pcontainer1->pcontainerInt->self;
	return boost::python::object();
}

boost::python::object ContainerInterface::GetPrev() const{
	if(!pcontainer){
		PyErr_SetString(PyExc_ValueError,"Invalid or expired container.");
		return boost::python::object();
	}
	WManager::Container *pPrev = pcontainer->GetPrev();
	ContainerConfig *pcontainer1 = dynamic_cast<ContainerConfig *>(pPrev);
	if(pcontainer1)
		return pcontainer1->pcontainerInt->self;
	return boost::python::object();
}

boost::python::object ContainerInterface::GetParent() const{
	if(!pcontainer){
		PyErr_SetString(PyExc_ValueError,"Invalid or expired container.");
		return boost::python::object();
	}
	WManager::Container *pParent = pcontainer->GetParent();
	if(!pParent)
		return boost::python::object();
	ContainerConfig *pcontainer1 = dynamic_cast<ContainerConfig *>(pParent);
	if(pcontainer1)
		return pcontainer1->pcontainerInt->self;
	return boost::python::object();
}

boost::python::object ContainerInterface::GetFocus() const{
	if(!pcontainer){
		PyErr_SetString(PyExc_ValueError,"Invalid or expired container.");
		return boost::python::object();
	}
	WManager::Container *pfocus = pcontainer->GetFocus();
	ContainerConfig *pcontainer1 = dynamic_cast<ContainerConfig *>(pfocus);
	if(pcontainer1)
		return pcontainer1->pcontainerInt->self;
	return boost::python::object();
}

boost::python::object ContainerInterface::GetTiledFocus() const{
	if(!pcontainer){
		PyErr_SetString(PyExc_ValueError,"Invalid or expired container.");
		return boost::python::object();
	}
	if(WManager::Container::tiledFocusQueue.size() == 0)
		return boost::python::object();
	
	auto m = std::find_if(WManager::Container::tiledFocusQueue.begin(),WManager::Container::tiledFocusQueue.end(),[&](auto &p)->bool{
		return p.first == pcontainer;
	});
	if(m == WManager::Container::tiledFocusQueue.begin() || m == WManager::Container::tiledFocusQueue.end()){
		ContainerConfig *pcontainer1 = dynamic_cast<ContainerConfig *>(WManager::Container::tiledFocusQueue.back().first);
		if(pcontainer1)
			return pcontainer1->pcontainerInt->self;
		return boost::python::object();
	}

	ContainerConfig *pcontainer1 = dynamic_cast<ContainerConfig *>((*(m-1)).first);
	if(pcontainer1)
		return pcontainer1->pcontainerInt->self;
	return boost::python::object();
}

boost::python::object ContainerInterface::GetFloatFocus() const{
	if(!pcontainer){
		PyErr_SetString(PyExc_ValueError,"Invalid or expired container.");
		return boost::python::object();
	}
	if(WManager::Container::floatFocusQueue.size() == 0)
		return boost::python::object();
	
	auto m = std::find(WManager::Container::floatFocusQueue.begin(),WManager::Container::floatFocusQueue.end(),pcontainer);
	if(m == WManager::Container::floatFocusQueue.begin() || m == WManager::Container::floatFocusQueue.end()){
		ContainerConfig *pcontainer1 = dynamic_cast<ContainerConfig *>(WManager::Container::floatFocusQueue.back());
		if(pcontainer1)
			return pcontainer1->pcontainerInt->self;
		return boost::python::object();
	}

	ContainerConfig *pcontainer1 = dynamic_cast<ContainerConfig *>(*(m-1));
	if(pcontainer1)
		return pcontainer1->pcontainerInt->self;
	return boost::python::object();
}

void ContainerInterface::Move(boost::python::object containerObject){
	if(!pcontainer){
		PyErr_SetString(PyExc_ValueError,"Invalid or expired container.");
		return;
	}
	boost::python::extract<Config::ContainerInterface &> containerExtract(containerObject);
	if(!containerExtract.check()){
		DebugPrintf(stderr,"Move(): Invalid container parameter.\n");
		return;
	}

	Config::ContainerInterface &containerInt = containerExtract();
	if(!containerInt.pcontainer)
		return;
	ContainerConfig *pcontainer1 = dynamic_cast<ContainerConfig *>(containerInt.pcontainer);
	if(pcontainer1)
		pcontainer1->pbackend->MoveContainer(pcontainer,containerInt.pcontainer);
}

void ContainerInterface::UpdateShaders(){
	for(auto m = shaderUpdateQueue.begin(); m != shaderUpdateQueue.end();){
		if(!(*m)->pcontainer){
			m = shaderUpdateQueue.erase(m);
			continue;
		}
		Compositor::ClientFrame *pclientFrame = dynamic_cast<Compositor::ClientFrame *>((*m)->pcontainer->pclient);
		if(!pclientFrame || !pclientFrame->enabled){
			++m;
			continue;
		}
		const char *pshaderName[Compositor::Pipeline::SHADER_MODULE_COUNT] = {
			(*m)->vertexShader.size() > 0?(*m)->vertexShader.c_str():0,
			(*m)->geometryShader.size() > 0?(*m)->geometryShader.c_str():0,
			(*m)->fragmentShader.size() > 0?(*m)->fragmentShader.c_str():0
		};
		pclientFrame->SetShaders(pshaderName);

		m = shaderUpdateQueue.erase(m);
	}
}

std::set<ContainerInterface *> ContainerInterface::shaderUpdateQueue;

ContainerProxy::ContainerProxy() : ContainerInterface(){
	//
}

ContainerProxy::~ContainerProxy(){
	//
}

void ContainerProxy::OnSetupContainer(){
	boost::python::override ovr = this->get_override("OnSetupContainer");
	if(ovr){
		try{
			ovr();
		}catch(boost::python::error_already_set &){
			PyErr_Print();
			//
			boost::python::handle_exception();
			PyErr_Clear();
		}
	}else ContainerInterface::OnSetupContainer();
}

void ContainerProxy::OnSetupClient(){
	boost::python::override ovr = this->get_override("OnSetupClient");
	if(ovr){
		try{
			ovr();
		}catch(boost::python::error_already_set &){
			PyErr_Print();
			//
			boost::python::handle_exception();
			PyErr_Clear();
		}
	}else ContainerInterface::OnSetupClient();
}

boost::python::object ContainerProxy::OnParent(){
	boost::python::override ovr = this->get_override("OnParent");
	if(ovr){
		try{
			return ovr();
		}catch(boost::python::error_already_set &){
			PyErr_Print();
			//
			boost::python::handle_exception();
			PyErr_Clear();
		}
	}
	return ContainerInterface::OnParent();
}

void ContainerProxy::OnCreate(){
	boost::python::override ovr = this->get_override("OnCreate");
	if(ovr){
		try{
			ovr();
		}catch(boost::python::error_already_set &){
			PyErr_Print();
			//
			boost::python::handle_exception();
			PyErr_Clear();
		}
	}else ContainerInterface::OnCreate();
}

bool ContainerProxy::OnFullscreen(bool toggle){
	boost::python::override ovr = this->get_override("OnFullscreen");
	if(ovr){
		try{
			return ovr(toggle);
		}catch(boost::python::error_already_set &){
			PyErr_Print();
			//
			boost::python::handle_exception();
			PyErr_Clear();
		}
	}
	return ContainerInterface::OnFullscreen(toggle);
}

void ContainerProxy::OnStack(bool toggle){
	boost::python::override ovr = this->get_override("OnStack");
	if(ovr){
		try{
			ovr(toggle);
		}catch(boost::python::error_already_set &){
			PyErr_Print();
			//
			boost::python::handle_exception();
			PyErr_Clear();
		}
	}else ContainerInterface::OnStack(toggle);
}

void ContainerProxy::OnFloat(bool toggle){
	boost::python::override ovr = this->get_override("OnFloat");
	if(ovr){
		try{
			ovr(toggle);
		}catch(boost::python::error_already_set &){
			PyErr_Print();
			//
			boost::python::handle_exception();
			PyErr_Clear();
		}
	}else ContainerInterface::OnFloat(toggle);
}

bool ContainerProxy::OnFocus(){
	boost::python::override ovr = this->get_override("OnFocus");
	if(ovr){
		try{
			return ovr();
		}catch(boost::python::error_already_set &){
			PyErr_Print();
			//
			boost::python::handle_exception();
			PyErr_Clear();
		}
	}
	return ContainerInterface::OnFocus();
}

void ContainerProxy::OnEnter(){
	boost::python::override ovr = this->get_override("OnEnter");
	if(ovr){
		try{
			ovr();
		}catch(boost::python::error_already_set &){
			PyErr_Print();
			//
			boost::python::handle_exception();
			PyErr_Clear();
		}
	}else ContainerInterface::OnEnter();
}

void ContainerProxy::OnPropertyChange(PROPERTY_ID id){
	boost::python::override ovr = this->get_override("OnPropertyChange");
	if(ovr){
		try{
			ovr(id);
		}catch(boost::python::error_already_set &){
			PyErr_Print();
			//
			boost::python::handle_exception();
			PyErr_Clear();
		}
	}else ContainerInterface::OnPropertyChange(id);
}

ContainerConfig::ContainerConfig(ContainerInterface *_pcontainerInt, Backend::X11Backend *_pbackend) : pcontainerInt(_pcontainerInt), pbackend(_pbackend){
	//
}

ContainerConfig::ContainerConfig(Backend::X11Backend *_pbackend) : pbackend(_pbackend){
	//TODO: setting root node properties (e.g. maxSize) should be allowed, this can be used for example
	//to make space for docks and other widgets
	try{
		//https://code.activestate.com/lists/python-cplusplus-sig/17025/
		const char *prootsrc =
			"import chamfer\n"
			"class RootContainer(chamfer.Container):\n"
			"    pass;\n";
		boost::python::object main = boost::python::import("__main__");
		boost::python::object global(main.attr("__dict__"));

		boost::python::object result = boost::python::exec(prootsrc,global,global);
		boost::python::object type = global["RootContainer"];

		boost::python::object object = type();
		boost::python::incref(object.ptr());
		pcontainerInt = &boost::python::extract<Config::ContainerInterface &>(object)();
		pcontainerInt->self = object;

	}catch(boost::python::error_already_set &){

		PyErr_Print();
	}
}

ContainerConfig::~ContainerConfig(){
	//
	pcontainerInt->pcontainer = 0;
}

X11ContainerConfig::X11ContainerConfig(ContainerInterface *_pcontainerInt, WManager::Container *_pParent, const WManager::Container::Setup &_setup, Backend::X11Backend *_pbackend) : Backend::X11Container(_pParent,_setup,_pbackend), ContainerConfig(_pcontainerInt,_pbackend){
	//
}

X11ContainerConfig::X11ContainerConfig(Backend::X11Backend *_pbackend, bool _noComp) : Backend::X11Container(_pbackend,_noComp), ContainerConfig(_pbackend){
	//
	pcontainerInt->pcontainer = this;
}

X11ContainerConfig::~X11ContainerConfig(){
	//
}

DebugContainerConfig::DebugContainerConfig(ContainerInterface *_pcontainerInt, WManager::Container *_pParent, const WManager::Container::Setup &_setup, Backend::X11Backend *_pbackend) : Backend::DebugContainer(_pParent,_setup,_pbackend), ContainerConfig(_pcontainerInt,_pbackend){
	//
}

DebugContainerConfig::DebugContainerConfig(Backend::X11Backend *_pbackend) : Backend::DebugContainer(_pbackend), ContainerConfig(_pbackend){
	//
	pcontainerInt->pcontainer = this;
}

DebugContainerConfig::~DebugContainerConfig(){
	//
}

/*WorkspaceInterface::WorkspaceInterface(){
	//
}

WorkspaceInterface::~WorkspaceInterface(){
	//
}

WorkspaceProxy::WorkspaceProxy() : WorkspaceInterface(){
	//
}

WorkspaceProxy::~WorkspaceProxy(){
	//
}*/

BackendInterface::BackendInterface() : standaloneComp(Loader::standaloneComp), pbackend(0){
	//
}

BackendInterface::~BackendInterface(){
	//
}

void BackendInterface::OnSetupKeys(bool debug){
	//
	DebugPrintf(stdout,"No KeyConfig interface, skipping configuration.\n");
}

boost::python::object BackendInterface::OnCreateContainer(){
	//TODO: should create a default instance here
	return boost::python::object();
}

/*boost::python::object BackendInterface::OnCreateWorkspace(){
	//TODO: should create a default instance here
	return boost::python::object();
}*/

void BackendInterface::OnKeyPress(uint keyId){
	//
}

void BackendInterface::OnKeyRelease(uint keyId){
	//
}

void BackendInterface::OnTimer(){
	//
}

void BackendInterface::OnExit(){
	//
}

boost::python::object BackendInterface::GetFocus(){
	ContainerConfig *pcontainer1 = dynamic_cast<ContainerConfig *>(WManager::Container::ptreeFocus);
	if(pcontainer1)
		return pcontainer1->pcontainerInt->self;
	return boost::python::object();
}

boost::python::object BackendInterface::GetRoot(boost::python::object nameObj){
	WManager::Container *pParent = WManager::Container::ptreeFocus;
	for(WManager::Container *pcontainer = pParent->pParent; pcontainer; pParent = pcontainer, pcontainer = pcontainer->pParent);

	boost::python::extract<std::string> nameExtract(nameObj);
	if(!nameExtract.check()){
		ContainerConfig *pcontainer1 = dynamic_cast<ContainerConfig *>(pParent);
		if(pcontainer1)
			return pcontainer1->pcontainerInt->self;
		return boost::python::object();
	}
	
	const std::string &name = nameExtract();
	WManager::Container *pcontainer = pParent;
	do{
		if(pcontainer->pname && strcmp(name.c_str(),pcontainer->pname) == 0){
			//found, return
			ContainerConfig *pcontainer1 = dynamic_cast<ContainerConfig *>(pcontainer);
			if(pcontainer1)
				return pcontainer1->pcontainerInt->self;
			return boost::python::object();
		}
		pcontainer = pcontainer->pRootNext;
	}while(pcontainer != pParent);

	//not found, create a new one
	if(!pbackend){
		DebugPrintf(stderr,"error: no backend while executing GetRoot() (create container)");
		return boost::python::object();
	}
	Backend::X11Backend *pbackend11 = dynamic_cast<Backend::X11Backend *>(pbackend);
	Config::X11ContainerConfig *pnewRoot = new Config::X11ContainerConfig(pbackend11,name == "nocomp");
	pnewRoot->SetName(name.c_str());
	pParent->AppendRoot(pnewRoot);

	return pnewRoot->pcontainerInt->self;
}

void BackendInterface::BindKey(uint symbol, uint mask, uint keyId){
	if(!pbackend)
		return;
	Backend::X11Backend *pbackend11 = dynamic_cast<Backend::X11Backend *>(pbackend);
	pbackend11->BindKey(symbol,mask,keyId);
}

void BackendInterface::MapKey(uint symbol, uint mask, uint keyId){
	if(!pbackend)
		return;
	Backend::X11Backend *pbackend11 = dynamic_cast<Backend::X11Backend *>(pbackend);
	pbackend11->MapKey(symbol,mask,keyId);
}

void BackendInterface::GrabKeyboard(bool enable){
	if(!pbackend)
		return;
	Backend::X11Backend *pbackend11 = dynamic_cast<Backend::X11Backend *>(pbackend);
	pbackend11->GrabKeyboard(enable);
}

void BackendInterface::Bind(boost::python::object obj){
	BackendInterface &backendInt1 = boost::python::extract<BackendInterface&>(obj)();
	pbackendInt = &backendInt1;
	boost::python::import("chamfer").attr("backend") = obj;
}

BackendInterface BackendInterface::defaultInt;
BackendInterface *BackendInterface::pbackendInt = &BackendInterface::defaultInt;

BackendProxy::BackendProxy(){
	//
}

BackendProxy::~BackendProxy(){
	//
}

void BackendProxy::OnSetupKeys(bool debug){
	boost::python::override ovr = this->get_override("OnSetupKeys");
	if(ovr){
		try{
			ovr(debug);
		}catch(boost::python::error_already_set &){
			PyErr_Print();
			//
			boost::python::handle_exception();
			PyErr_Clear();
		}
	}else BackendInterface::OnSetupKeys(debug);
}

boost::python::object BackendProxy::OnCreateContainer(){
	boost::python::override ovr = this->get_override("OnCreateContainer");
	if(ovr){
		try{
			return ovr();
		}catch(boost::python::error_already_set &){
			PyErr_Print();
			//
			boost::python::handle_exception();
			PyErr_Clear();
		}
	}
	return BackendInterface::OnCreateContainer();
}

void BackendProxy::OnKeyPress(uint keyId){
	boost::python::override ovr = this->get_override("OnKeyPress");
	if(ovr){
		try{
			ovr(keyId);
		}catch(boost::python::error_already_set &){
			PyErr_Print();
			//
			boost::python::handle_exception();
			PyErr_Clear();
		}
	}else BackendInterface::OnKeyPress(keyId);
}

void BackendProxy::OnKeyRelease(uint keyId){
	boost::python::override ovr = this->get_override("OnKeyRelease");
	if(ovr){
		try{
			ovr(keyId);
		}catch(boost::python::error_already_set &){
			PyErr_Print();
			//
			boost::python::handle_exception();
			PyErr_Clear();
		}
	}else BackendInterface::OnKeyRelease(keyId);
}

void BackendProxy::OnTimer(){
	boost::python::override ovr = this->get_override("OnTimer");
	if(ovr){
		try{
			ovr();
		}catch(boost::python::error_already_set &){
			PyErr_Print();
			//
			boost::python::handle_exception();
			PyErr_Clear();
		}
	}else BackendInterface::OnTimer();
}

void BackendProxy::OnExit(){
	boost::python::override ovr = this->get_override("OnExit");
	if(ovr){
		try{
			ovr();
		}catch(boost::python::error_already_set &){
			PyErr_Print();
			//
			boost::python::handle_exception();
			PyErr_Clear();
		}
	}else BackendInterface::OnExit();
}

BackendConfig::BackendConfig(BackendInterface *_pbackendInt) : pbackendInt(_pbackendInt){
	pbackendInt->pbackend = this;
}

BackendConfig::~BackendConfig(){
	//
	pbackendInt->pbackend = 0;
}

CompositorInterface::CompositorInterface() : noCompositor(Loader::noCompositor), deviceIndex(Loader::deviceIndex), debugLayers(Loader::debugLayers), scissoring(Loader::scissoring), incrementalPresent(Loader::incrementalPresent), memoryImportMode(Loader::memoryImportMode), unredirOnFullscreen(Loader::unredirOnFullscreen), enableAnimation(true), animationDuration(0.3f), fontName("Monospace"), fontSize(18), pcompositor(0){
	//
}

CompositorInterface::~CompositorInterface(){
	//
}

bool CompositorInterface::OnRedirectExternal(std::string title, std::string className){
	return true;
}

void CompositorInterface::Bind(boost::python::object obj){
	CompositorInterface &compositorInt = boost::python::extract<CompositorInterface&>(obj)();
	pcompositorInt = &compositorInt;
	boost::python::import("chamfer").attr("compositor") = obj;
}

CompositorInterface CompositorInterface::defaultInt;
CompositorInterface *CompositorInterface::pcompositorInt = &CompositorInterface::defaultInt;

CompositorProxy::CompositorProxy(){
	//
}

CompositorProxy::~CompositorProxy(){
	//
}

bool CompositorProxy::OnRedirectExternal(std::string title, std::string className){
	boost::python::override ovr = this->get_override("OnRedirectExternal");
	if(ovr){
		try{
			return ovr(title,className);
		}catch(boost::python::error_already_set &){
			PyErr_Print();
			//
			boost::python::handle_exception();
			PyErr_Clear();
		}
	}
	return CompositorInterface::OnRedirectExternal(title,className);
}

CompositorConfig::CompositorConfig(CompositorInterface *_pcompositorInt) : pcompositorInt(_pcompositorInt){
	pcompositorInt->pcompositor = this;
}

CompositorConfig::~CompositorConfig(){
	//
	pcompositorInt->pcompositor = 0;
}

BOOST_PYTHON_MEMBER_FUNCTION_OVERLOADS(GetRootOvr,GetRoot,0,1)

BOOST_PYTHON_MODULE(chamfer){
	boost::python::scope().attr("MOD_MASK_1") = uint(XCB_MOD_MASK_1);
	boost::python::scope().attr("MOD_MASK_2") = uint(XCB_MOD_MASK_2);
	boost::python::scope().attr("MOD_MASK_3") = uint(XCB_MOD_MASK_3);
	boost::python::scope().attr("MOD_MASK_4") = uint(XCB_MOD_MASK_4);
	boost::python::scope().attr("MOD_MASK_5") = uint(XCB_MOD_MASK_5);
	boost::python::scope().attr("MOD_MASK_SHIFT") = uint(XCB_MOD_MASK_SHIFT);
	boost::python::scope().attr("MOD_MASK_LOCK") = uint(XCB_MOD_MASK_LOCK);
	boost::python::scope().attr("MOD_MASK_CONTROL") = uint(XCB_MOD_MASK_CONTROL);
	boost::python::scope().attr("MOD_MASK_ANY") = uint(XCB_MOD_MASK_ANY);

	//boost::python::class_<Backend::X11KeyBinder>("KeyBinder",boost::python::no_init)
	//	.def("BindKey",&Backend::X11KeyBinder::BindKey)
	//	;
	
	boost::python::enum_<ContainerInterface::FLOAT>("floatingMode")
		.value("AUTOMATIC",ContainerInterface::FLOAT_AUTOMATIC)
		.value("ALWAYS",ContainerInterface::FLOAT_ALWAYS)
		.value("NEVER",ContainerInterface::FLOAT_NEVER);

	boost::python::enum_<ContainerInterface::PROPERTY_ID>("property")
		.value("NAME",ContainerInterface::PROPERTY_ID_NAME)
		.value("CLASS",ContainerInterface::PROPERTY_ID_CLASS);

	boost::python::enum_<WManager::Container::LAYOUT>("layout")
		.value("VSPLIT",WManager::Container::LAYOUT_VSPLIT)
		.value("HSPLIT",WManager::Container::LAYOUT_HSPLIT);

	boost::python::enum_<WManager::Container::TITLEBAR>("titleBar")
		.value("NONE",WManager::Container::TITLEBAR_NONE)
		.value("LEFT",WManager::Container::TITLEBAR_LEFT)
		.value("TOP",WManager::Container::TITLEBAR_TOP)
		.value("RIGHT",WManager::Container::TITLEBAR_RIGHT)
		.value("BOTTOM",WManager::Container::TITLEBAR_BOTTOM);
		//.value("AUTO_TOP_LEFT",WManager::Container::TITLEBAR_AUTO_TOP_LEFT)
		//.value("AUTO_BOTTOM_RIGHT",WManager::Container::TITLEBAR_AUTO_BOTTOM_RIGHT);

	boost::python::class_<ContainerProxy,boost::noncopyable>("Container")
		.def("OnSetupContainer",&ContainerInterface::OnSetupContainer)
		.def("OnSetupClient",&ContainerInterface::OnSetupClient)
		.def("OnParent",&ContainerInterface::OnParent)
		.def("OnCreate",&ContainerInterface::OnCreate)
		.def("OnFullscreen",&ContainerInterface::OnFullscreen)
		.def("OnStack",&ContainerInterface::OnStack)
		.def("OnFloat",&ContainerInterface::OnFloat)
		.def("OnFocus",&ContainerInterface::OnFocus)
		.def("OnEnter",&ContainerInterface::OnEnter)
		.def("OnPropertyChange",&ContainerInterface::OnPropertyChange)
		.def("GetNext",&ContainerInterface::GetNext)
		.def("GetPrev",&ContainerInterface::GetPrev)
		.def("GetParent",&ContainerInterface::GetParent)
		.def("GetFocus",&ContainerInterface::GetFocus)
		.def("GetTiledFocus",&ContainerInterface::GetTiledFocus)
		.def("GetFloatFocus",&ContainerInterface::GetFloatFocus)
		.def("MoveNext",boost::python::make_function(
			[](ContainerInterface &container){
				if(!container.pcontainer)
					return;
				container.pcontainer->MoveNext();
			},boost::python::default_call_policies(),boost::mpl::vector2<void, ContainerInterface &>()))
		.def("MovePrev",boost::python::make_function(
			[](ContainerInterface &container){
				if(!container.pcontainer)
					return;
				container.pcontainer->MovePrev();
			},boost::python::default_call_policies(),boost::mpl::vector2<void, ContainerInterface &>()))
		.def("Move",&ContainerInterface::Move)
		.def("Focus",boost::python::make_function(
			[](ContainerInterface &container){
				if(!container.pcontainer)
					return;
				if(container.OnFocus())
					container.pcontainer->Focus();
			},boost::python::default_call_policies(),boost::mpl::vector2<void, ContainerInterface &>()))
		.def("Kill",boost::python::make_function(
			[](ContainerInterface &container){
				if(!container.pcontainer)
					return;
				if(container.pcontainer->pclient)
					container.pcontainer->pclient->Kill();
			},boost::python::default_call_policies(),boost::mpl::vector2<void, ContainerInterface &>()))
		.def("ShiftLayout",boost::python::make_function(
			[](ContainerInterface &container, WManager::Container::LAYOUT layout){
				if(!container.pcontainer)
					return;
				container.pcontainer->SetLayout(layout);
			},boost::python::default_call_policies(),boost::mpl::vector3<void, ContainerInterface &, WManager::Container::LAYOUT>()))
		.def("SetFullscreen",boost::python::make_function(
			[](ContainerInterface &container, bool toggle){
				if(!container.pcontainer)
					return;
				if(container.OnFullscreen(toggle))
					container.pcontainer->SetFullscreen(toggle);
			},boost::python::default_call_policies(),boost::mpl::vector3<void, ContainerInterface &, bool>()))
		.def("SetStacked",boost::python::make_function(
			[](ContainerInterface &container, bool toggle){
				if(!container.pcontainer)
					return;
				container.OnStack(toggle);
				container.pcontainer->SetStacked(toggle);
			},boost::python::default_call_policies(),boost::mpl::vector3<void, ContainerInterface &, bool>()))
		.def("SetFloating",boost::python::make_function(
			[](ContainerInterface &container, bool toggle){ //XXX toggle does nothing here!
				if(!container.pcontainer)
					return;
				container.OnFloat(toggle);
				ContainerConfig *pcontainer1 = dynamic_cast<ContainerConfig *>(container.pcontainer);
				if(pcontainer1)
					pcontainer1->pbackend->FloatContainer(container.pcontainer);
				//container.pcontainer->SetStacked(toggle);
			},boost::python::default_call_policies(),boost::mpl::vector3<void, ContainerInterface &, bool>()))
		.def("IsFloating",boost::python::make_function(
			[](ContainerInterface &container){
				if(!container.pcontainer){
					PyErr_SetString(PyExc_ValueError,"Invalid or expired container.");
					return false;
				}
				return (container.pcontainer->flags & WManager::Container::FLAG_FLOATING) != 0;
			},boost::python::default_call_policies(),boost::mpl::vector2<bool, ContainerInterface &>()))
		.def("IsAlive",boost::python::make_function(
			[](ContainerInterface &container){
				return container.pcontainer != 0;
			},boost::python::default_call_policies(),boost::mpl::vector2<bool, ContainerInterface &>()))
		//.def_readwrite("canvasOffset",&ContainerInterface::canvasOffset)
		.add_property("name",
			boost::python::make_function(
			[](ContainerInterface &container){
				if(!container.pcontainer || !container.pcontainer->pname)
					return container.name;
				return std::string(container.pcontainer->pname);
			},boost::python::default_call_policies(),boost::mpl::vector2<std::string, ContainerInterface &>()),
			boost::python::make_function(
			[](ContainerInterface &container, std::string name){
				container.name = name;
				if(container.pcontainer){
					container.pcontainer->SetName(name.c_str());
					return;
				}
			},boost::python::default_call_policies(),boost::mpl::vector3<void, ContainerInterface &, std::string>()))
		.add_property("canvasOffset",
			boost::python::make_function(
			[](ContainerInterface &container){
				if(!container.pcontainer)
					return container.canvasOffset;
				return boost::python::make_tuple(container.pcontainer->canvasOffset.x,container.pcontainer->canvasOffset.y);
			},boost::python::default_call_policies(),boost::mpl::vector2<boost::python::tuple, ContainerInterface &>()),
			boost::python::make_function(
			[](ContainerInterface &container, boost::python::tuple tuple){
				if(!container.pcontainer){
					container.canvasOffset = tuple;
					return;
				}
				container.pcontainer->canvasOffset.x = boost::python::extract<float>(tuple[0])();
				container.pcontainer->canvasOffset.y = boost::python::extract<float>(tuple[1])();
			},boost::python::default_call_policies(),boost::mpl::vector3<void, ContainerInterface &, boost::python::tuple>()))
		//.def_readwrite("canvasExtent",&ContainerInterface::canvasExtent)
		.add_property("canvasExtent",
			boost::python::make_function(
			[](ContainerInterface &container){
				if(!container.pcontainer)
					return container.canvasExtent;
				return boost::python::make_tuple(container.pcontainer->canvasExtent.x,container.pcontainer->canvasExtent.y);
			},boost::python::default_call_policies(),boost::mpl::vector2<boost::python::tuple, ContainerInterface &>()),
			boost::python::make_function(
			[](ContainerInterface &container, boost::python::tuple tuple){
				if(!container.pcontainer){
					container.canvasExtent = tuple;
					return;
				}
				container.pcontainer->canvasExtent.x = boost::python::extract<float>(tuple[0])();
				container.pcontainer->canvasExtent.y = boost::python::extract<float>(tuple[1])();
			},boost::python::default_call_policies(),boost::mpl::vector3<void, ContainerInterface &, boost::python::tuple>()))
		//.def_readwrite("margin",&ContainerInterface::margin)
		.add_property("margin",
			boost::python::make_function(
			[](ContainerInterface &container){
				if(!container.pcontainer)
					return container.margin;
				return boost::python::make_tuple(container.pcontainer->margin.x,container.pcontainer->margin.y);
			},boost::python::default_call_policies(),boost::mpl::vector2<boost::python::tuple, ContainerInterface &>()),
			boost::python::make_function(
			[](ContainerInterface &container, boost::python::tuple tuple){
				if(!container.pcontainer){
					container.margin = tuple;
					return;
				}
				container.pcontainer->margin.x = boost::python::extract<float>(tuple[0])();
				container.pcontainer->margin.y = boost::python::extract<float>(tuple[1])();
			},boost::python::default_call_policies(),boost::mpl::vector3<void, ContainerInterface &, boost::python::tuple>()))
		.add_property("borderWidth",
			boost::python::make_function(
			[](ContainerInterface &container){
				DebugPrintf(stdout,"depcrecation warning: borderWidth - use 'margin'.\n");
				if(!container.pcontainer)
					return container.margin;
				return boost::python::make_tuple(container.pcontainer->margin.x,container.pcontainer->margin.y);
			},boost::python::default_call_policies(),boost::mpl::vector2<boost::python::tuple, ContainerInterface &>()),
			boost::python::make_function(
			[](ContainerInterface &container, boost::python::tuple tuple){
				DebugPrintf(stdout,"depcrecation warning: borderWidth - use 'margin'.\n");
				if(!container.pcontainer){
					container.margin = tuple;
					return;
				}
				container.pcontainer->margin.x = boost::python::extract<float>(tuple[0])();
				container.pcontainer->margin.y = boost::python::extract<float>(tuple[1])();
			},boost::python::default_call_policies(),boost::mpl::vector3<void, ContainerInterface &, boost::python::tuple>()))
		.add_property("size",
			boost::python::make_function(
			[](ContainerInterface &container){
				if(!container.pcontainer)
					return container.size;
				return boost::python::make_tuple(container.pcontainer->size.x,container.pcontainer->size.y);
			},boost::python::default_call_policies(),boost::mpl::vector2<boost::python::tuple, ContainerInterface &>()),
			boost::python::make_function(
			[](ContainerInterface &container, boost::python::tuple tuple){
				if(!container.pcontainer){
					container.size = tuple;
					return;
				}
				glm::vec2 size = glm::vec2(
					boost::python::extract<float>(tuple[0])(),
					boost::python::extract<float>(tuple[1])());
				container.pcontainer->SetSize(size);
			},boost::python::default_call_policies(),boost::mpl::vector3<void, ContainerInterface &, boost::python::tuple>()))
		.add_property("minSize",
			boost::python::make_function(
			[](ContainerInterface &container){
				if(!container.pcontainer)
					return container.minSize;
				return boost::python::make_tuple(container.pcontainer->minSize.x,container.pcontainer->minSize.y);
			},boost::python::default_call_policies(),boost::mpl::vector2<boost::python::tuple, ContainerInterface &>()),
			boost::python::make_function(
			[](ContainerInterface &container, boost::python::tuple tuple){
				if(!container.pcontainer){
					container.minSize = tuple;
					return;
				}
				container.pcontainer->minSize.x = boost::python::extract<float>(tuple[0])();
				container.pcontainer->minSize.y = boost::python::extract<float>(tuple[1])();
			},boost::python::default_call_policies(),boost::mpl::vector3<void, ContainerInterface &, boost::python::tuple>()))
		//.def_readwrite("maxSize",&ContainerInterface::maxSize)
		.add_property("maxSize",
			boost::python::make_function(
			[](ContainerInterface &container){
				if(!container.pcontainer)
					return container.maxSize;
				return boost::python::make_tuple(container.pcontainer->maxSize.x,container.pcontainer->maxSize.y);
			},boost::python::default_call_policies(),boost::mpl::vector2<boost::python::tuple, ContainerInterface &>()),
			boost::python::make_function(
			[](ContainerInterface &container, boost::python::tuple tuple){
				if(!container.pcontainer){
					container.maxSize = tuple;
					return;
				}
				container.pcontainer->maxSize.x = boost::python::extract<float>(tuple[0])();
				container.pcontainer->maxSize.y = boost::python::extract<float>(tuple[1])();
			},boost::python::default_call_policies(),boost::mpl::vector3<void, ContainerInterface &, boost::python::tuple>()))
		.add_property("fullscreen",boost::python::make_function(
			[](ContainerInterface &container){
				if(!container.pcontainer){
					PyErr_SetString(PyExc_ValueError,"Invalid or expired container.");
					return false;
				}
				return (container.pcontainer->flags & WManager::Container::FLAG_FULLSCREEN) != 0;
			},boost::python::default_call_policies(),boost::mpl::vector2<bool, ContainerInterface &>()))
		.add_property("stacked",boost::python::make_function(
			[](ContainerInterface &container){
				if(!container.pcontainer){
					PyErr_SetString(PyExc_ValueError,"Invalid or expired container.");
					return false;
				}
				return (container.pcontainer->flags & WManager::Container::FLAG_STACKED) != 0;
			},boost::python::default_call_policies(),boost::mpl::vector2<bool, ContainerInterface &>()))
		.add_property("titleBar",boost::python::make_function(
			// = NONE, LEFT, TOP, RIGHT, BOTTOM
			//location, size (or use common font size)?
			[](ContainerInterface &container){
				if(!container.pcontainer)
					return WManager::Container::TITLEBAR_NONE;
				return container.pcontainer->titleBar;
			},boost::python::default_call_policies(),boost::mpl::vector2<WManager::Container::TITLEBAR, ContainerInterface &>()),
			boost::python::make_function(
			[](ContainerInterface &container, WManager::Container::TITLEBAR titleBar){
				if(!container.pcontainer){
					container.titleBar = titleBar;
					return;
				}
				container.pcontainer->SetTitlebar(titleBar);
			},boost::python::default_call_policies(),boost::mpl::vector3<void, ContainerInterface &, WManager::Container::TITLEBAR>()))
		.add_property("titleStackOnly",boost::python::make_function(
			[](ContainerInterface &container){
				if(!container.pcontainer)
					return false;
				return container.pcontainer->titleStackOnly;
			},boost::python::default_call_policies(),boost::mpl::vector2<bool, ContainerInterface &>()),
			boost::python::make_function(
			[](ContainerInterface &container, bool titleStackOnly){
				if(!container.pcontainer){
					container.titleStackOnly = titleStackOnly;
					return;
				}
				container.pcontainer->titleStackOnly = titleStackOnly;
			},boost::python::default_call_policies(),boost::mpl::vector3<void, ContainerInterface &, bool>()))
		.add_property("shaderFlags",
			boost::python::make_function(
			[](ContainerInterface &container){
				if(!container.pcontainer)
					return container.shaderUserFlags;
				Compositor::ClientFrame *pclientFrame = dynamic_cast<Compositor::ClientFrame *>(container.pcontainer->pclient);
				if(!pclientFrame)
					return container.shaderUserFlags;
				return pclientFrame->shaderUserFlags;
			},boost::python::default_call_policies(),boost::mpl::vector2<uint, ContainerInterface &>()),
			boost::python::make_function(
			[](ContainerInterface &container, uint flags){
				if(!container.pcontainer){
					container.shaderUserFlags = flags;
					return;
				}
				Compositor::ClientFrame *pclientFrame = dynamic_cast<Compositor::ClientFrame *>(container.pcontainer->pclient);
				if(!pclientFrame){
					container.shaderUserFlags = flags;
					return;
				}
				pclientFrame->shaderUserFlags = flags;
			},boost::python::default_call_policies(),boost::mpl::vector3<void, ContainerInterface &, uint>()))
		.def_readonly("wm_name",&ContainerInterface::wm_name)
		.def_readonly("wm_class",&ContainerInterface::wm_class)
		//.def_readwrite("vertexShader",&ContainerInterface::vertexShader)
		.add_property("vertexShader",
			boost::python::make_function(
			[](ContainerInterface &container){
				return container.vertexShader;
			},boost::python::default_call_policies(),boost::mpl::vector2<std::string, ContainerInterface &>()),
			boost::python::make_function(
			[](ContainerInterface &container, std::string vertexShader){
				container.vertexShader = vertexShader;
				if(container.pcontainer && container.pcontainer->pclient){
					ContainerInterface::shaderUpdateQueue.insert(&container);}
			},boost::python::default_call_policies(),boost::mpl::vector3<void, ContainerInterface &, std::string>()))
		//.def_readwrite("geometryShader",&ContainerInterface::geometryShader)
		.add_property("geometryShader",
			boost::python::make_function(
			[](ContainerInterface &container){
				return container.geometryShader;
			},boost::python::default_call_policies(),boost::mpl::vector2<std::string, ContainerInterface &>()),
			boost::python::make_function(
			[](ContainerInterface &container, std::string geometryShader){
				container.geometryShader = geometryShader;
				if(container.pcontainer && container.pcontainer->pclient)
					ContainerInterface::shaderUpdateQueue.insert(&container);
			},boost::python::default_call_policies(),boost::mpl::vector3<void, ContainerInterface &, std::string>()))
		//.def_readwrite("fragmentShader",&ContainerInterface::fragmentShader)
		.add_property("fragmentShader",
			boost::python::make_function(
			[](ContainerInterface &container){
				return container.fragmentShader;
			},boost::python::default_call_policies(),boost::mpl::vector2<std::string, ContainerInterface &>()),
			boost::python::make_function(
			[](ContainerInterface &container, std::string fragmentShader){
				container.fragmentShader = fragmentShader;
				if(container.pcontainer && container.pcontainer->pclient)
					ContainerInterface::shaderUpdateQueue.insert(&container);
			},boost::python::default_call_policies(),boost::mpl::vector3<void, ContainerInterface &, std::string>()))
		.add_property("layout",boost::python::make_function(
			[](ContainerInterface &container){
				if(!container.pcontainer){
					PyErr_SetString(PyExc_ValueError,"Invalid or expired container.");
					return WManager::Container::LAYOUT_VSPLIT;
				}
				return container.pcontainer->layout;
			},boost::python::default_call_policies(),boost::mpl::vector2<WManager::Container::LAYOUT, ContainerInterface &>()))
		.def_readwrite("floatingMode",&ContainerInterface::floatingMode)
		;
	
	/*boost::python::class_<WorkspaceProxy,boost::noncopyable>("Workspace")
		//.def("OnSetupContainer",&ContainerInterface::OnSetupContainer)
		;*/
	
	boost::python::class_<BackendProxy,boost::noncopyable>("Backend")
		.def("OnSetupKeys",&BackendInterface::OnSetupKeys)
		.def("OnCreateContainer",&BackendInterface::OnCreateContainer)
		//.def("OnCreateWorkspace",&BackendInterface::OnCreateWorkspace)
		.def("OnKeyPress",&BackendInterface::OnKeyPress)
		.def("OnKeyRelease",&BackendInterface::OnKeyRelease)
		.def("OnTimer",&BackendInterface::OnTimer)
		.def("OnExit",&BackendInterface::OnExit)
		.def("GetFocus",&BackendInterface::GetFocus)
		//.def("GetRoot",&BackendInterface::GetRoot)
		.def("GetRoot",&BackendInterface::GetRoot,GetRootOvr(boost::python::args("name")))
		.def("BindKey",&BackendInterface::BindKey)
		.def("MapKey",&BackendInterface::MapKey)
		.def("GrabKeyboard",&BackendInterface::GrabKeyboard)
		.def_readwrite("standaloneCompositor",&BackendInterface::standaloneComp)
		;
	boost::python::def("BindBackend",BackendInterface::Bind);

	boost::python::enum_<Compositor::ClientFrame::SHADER_FLAG>("shaderFlag")
		.value("FOCUS",Compositor::ClientFrame::SHADER_FLAG_FOCUS)
		.value("FLOATING",Compositor::ClientFrame::SHADER_FLAG_FLOATING)
		.value("STACKED",Compositor::ClientFrame::SHADER_FLAG_STACKED)
		.value("USER_BIT",Compositor::ClientFrame::SHADER_FLAG_USER_BIT);

	boost::python::enum_<Compositor::CompositorInterface::IMPORT_MODE>("importMode")
		.value("CPU_COPY",Compositor::CompositorInterface::IMPORT_MODE_CPU_COPY)
		.value("HOST_MEMORY",Compositor::CompositorInterface::IMPORT_MODE_HOST_MEMORY)
		.value("DMABUF",Compositor::CompositorInterface::IMPORT_MODE_DMABUF);
	
	boost::python::class_<CompositorProxy,boost::noncopyable>("Compositor")
		.def("OnRedirectExternal",&CompositorInterface::OnRedirectExternal)
		.def_readwrite("noCompositor",&CompositorInterface::noCompositor)
		.def_readwrite("deviceIndex",&CompositorInterface::deviceIndex)
		.def_readwrite("debugLayers",&CompositorInterface::debugLayers)
		.def_readwrite("scissoring",&CompositorInterface::scissoring)
		.def_readwrite("incrementalPresent",&CompositorInterface::incrementalPresent)
		//.def_readwrite("hostMemoryImport",&CompositorInterface::hostMemoryImport)
		.def_readwrite("memoryImportMode",&CompositorInterface::memoryImportMode)
		.def_readwrite("unredirOnFullscreen",&CompositorInterface::unredirOnFullscreen)
		.add_property("enableAnimation",
			boost::python::make_function(
			[](CompositorInterface &compositor){
				if(!compositor.pcompositor)
					return compositor.enableAnimation;
				return dynamic_cast<Compositor::CompositorInterface *>(compositor.pcompositor)->enableAnimation;
			},boost::python::default_call_policies(),boost::mpl::vector2<bool, CompositorInterface &>()),
			boost::python::make_function(
			[](CompositorInterface &compositor, bool enableAnimation){
				if(!compositor.pcompositor){
					compositor.enableAnimation = enableAnimation;
					return;
				}
				dynamic_cast<Compositor::CompositorInterface *>(compositor.pcompositor)->enableAnimation = enableAnimation;
			},boost::python::default_call_policies(),boost::mpl::vector3<void, CompositorInterface &, bool>()))
		.add_property("animationDuration",
			boost::python::make_function(
			[](CompositorInterface &compositor){
				if(!compositor.pcompositor)
					return compositor.animationDuration;
				return dynamic_cast<Compositor::CompositorInterface *>(compositor.pcompositor)->animationDuration;
			},boost::python::default_call_policies(),boost::mpl::vector2<float, CompositorInterface &>()),
			boost::python::make_function(
			[](CompositorInterface &compositor, float animationDuration){
				if(!compositor.pcompositor){
					compositor.animationDuration = animationDuration;
					return;
				}
				dynamic_cast<Compositor::CompositorInterface *>(compositor.pcompositor)->animationDuration = animationDuration;
			},boost::python::default_call_policies(),boost::mpl::vector3<void, CompositorInterface &, float>()))
		.def_readwrite("fontName",&CompositorInterface::fontName)
		.def_readwrite("fontSize",&CompositorInterface::fontSize)
		;
	boost::python::def("BindCompositor",CompositorInterface::Bind);
}

Loader::Loader(const char *pargv0){
	wchar_t wargv0[1024];
	mbtowc(wargv0,pargv0,sizeof(wargv0)/sizeof(wargv0[0]));
	Py_SetProgramName(wargv0);

	PyImport_AppendInittab("chamfer",PyInit_chamfer);
	Py_Initialize();
}

Loader::~Loader(){
	Py_Finalize();
}

bool Loader::Run(const char *pfilePath, const char *pfileLabel){
	FILE *pf = 0;
	if(!pfilePath){
		const char *pconfigPaths[] = {
			"~/.config/chamfer/config.py",
			"~/.chamfer/config.py",
			"/usr/share/chamfer/config/config.py"
		};
		wordexp_t expResult;
		for(uint i = 0; i < sizeof(pconfigPaths)/sizeof(pconfigPaths[0]); ++i){
			wordexp(pconfigPaths[i],&expResult,WRDE_NOCMD);
			if((pf = fopen(expResult.we_wordv[0],"rb"))){
				DebugPrintf(stdout,"Found config %s\n",expResult.we_wordv[0]);
				break;
			}
			wordfree(&expResult);
		}
	}else{
		wordexp_t expResult;
		wordexp(pfilePath,&expResult,WRDE_NOCMD);
		DebugPrintf(stdout,"Loading config %s...\n",expResult.we_wordv[0]);
		pf = fopen(expResult.we_wordv[0],"rb");
		wordfree(&expResult);
	}
	if(!pf){
		DebugPrintf(stderr,"Unable to find configuration file.\n");
		return false;
	}
	try{
		PyRun_SimpleFile(pf,pfileLabel);

	}catch(boost::python::error_already_set &){
		boost::python::handle_exception();
		PyErr_Clear();
		fclose(pf);
		return false;
	}
	fclose(pf);

	return true;
}

bool Loader::standaloneComp;

bool Loader::noCompositor;
sint Loader::deviceIndex;
bool Loader::debugLayers;
bool Loader::scissoring;
bool Loader::incrementalPresent;
//bool Loader::hostMemoryImport;
Compositor::CompositorInterface::IMPORT_MODE Loader::memoryImportMode;
bool Loader::unredirOnFullscreen;

}

