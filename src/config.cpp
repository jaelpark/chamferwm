#include "main.h"
#include "container.h"
#include "backend.h"
#include "config.h"
#include <xcb/xcb_keysyms.h> //todo: should not depend on xcb here
#include <X11/keysym.h>

namespace Config{

ContainerInterface::ContainerInterface() :
borderWidth(boost::python::make_tuple(0.0f,0.0f)),
minSize(boost::python::make_tuple(0.0f,0.0f)),
maxSize(boost::python::make_tuple(1.0f,1.0f)){
	//
}

ContainerInterface::~ContainerInterface(){
	//
}

void ContainerInterface::CopySettings(WManager::Container::Setup &setup){
	setup.borderWidth.x = boost::python::extract<float>(borderWidth[0])();
	setup.borderWidth.y = boost::python::extract<float>(borderWidth[1])();
	setup.minSize.x = boost::python::extract<float>(minSize[0])();
	setup.minSize.y = boost::python::extract<float>(minSize[1])();
	setup.maxSize.x = boost::python::extract<float>(maxSize[0])();
	setup.maxSize.y = boost::python::extract<float>(maxSize[1])();
}

boost::python::object ContainerInterface::OnSetup(){
	//
	return BackendInterface::GetFocus(); //TODO: sensible default
}

void ContainerInterface::OnCreate(){
	//TODO: focus on this
}

boost::python::object ContainerInterface::GetNext() const{
	WManager::Container *pnext = pcontainer->GetNext();
	ContainerConfig *pcontainer1 = dynamic_cast<ContainerConfig *>(pnext);
	if(pcontainer1)
		return pcontainer1->pcontainerInt->self;
	return boost::python::object();
}

boost::python::object ContainerInterface::GetPrev() const{
	WManager::Container *pPrev = pcontainer->GetPrev();
	ContainerConfig *pcontainer1 = dynamic_cast<ContainerConfig *>(pPrev);
	if(pcontainer1)
		return pcontainer1->pcontainerInt->self;
	return boost::python::object();
}

boost::python::object ContainerInterface::GetParent() const{
	WManager::Container *pParent = pcontainer->GetParent();
	if(!pParent)
		return boost::python::object();
	ContainerConfig *pcontainer1 = dynamic_cast<ContainerConfig *>(pParent);
	if(pcontainer1)
		return pcontainer1->pcontainerInt->self;
	return boost::python::object();
}

boost::python::object ContainerInterface::GetFocus() const{
	WManager::Container *pfocus = pcontainer->GetFocus();
	ContainerConfig *pcontainer1 = dynamic_cast<ContainerConfig *>(pfocus);
	if(pcontainer1)
		return pcontainer1->pcontainerInt->self;
	return boost::python::object();
}

ContainerProxy::ContainerProxy() : ContainerInterface(){
	//
}

ContainerProxy::~ContainerProxy(){
	//
}

boost::python::object ContainerProxy::OnSetup(){
	boost::python::override ovr = this->get_override("OnSetup");
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
	return ContainerInterface::OnSetup();
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

ContainerConfig::ContainerConfig(ContainerInterface *_pcontainerInt) : pcontainerInt(_pcontainerInt){
	//
}

ContainerConfig::ContainerConfig(){
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
}

X11ContainerConfig::X11ContainerConfig(ContainerInterface *_pcontainerInt, WManager::Container *_pParent, const WManager::Container::Setup &_setup, Backend::X11Backend *_pbackend) : Backend::X11Container(_pParent,_setup,_pbackend), ContainerConfig(_pcontainerInt){
	//
}

X11ContainerConfig::X11ContainerConfig(Backend::X11Backend *_pbackend) : Backend::X11Container(_pbackend), ContainerConfig(){
	//
	pcontainerInt->pcontainer = this;
}

X11ContainerConfig::~X11ContainerConfig(){
	//
}

DebugContainerConfig::DebugContainerConfig(ContainerInterface *_pcontainerInt, WManager::Container *_pParent, const WManager::Container::Setup &_setup, Backend::X11Backend *_pbackend) : Backend::DebugContainer(_pParent,_setup,_pbackend), ContainerConfig(_pcontainerInt){
	//
}

DebugContainerConfig::DebugContainerConfig(Backend::X11Backend *_pbackend) : Backend::DebugContainer(_pbackend), ContainerConfig(){
	//
	pcontainerInt->pcontainer = this;
}

DebugContainerConfig::~DebugContainerConfig(){
	//
}

BackendInterface::BackendInterface(){
	//
}

BackendInterface::~BackendInterface(){
	//
}

void BackendInterface::OnSetupKeys(Backend::X11KeyBinder *pkeyBinder){
	//
	DebugPrintf(stdout,"No KeyConfig interface, skipping configuration.\n");
}

boost::python::object BackendInterface::OnCreateContainer(){
	//TODO: should create a default instance here
	return boost::python::object();
}

void BackendInterface::OnKeyPress(uint keyId){
	//
}

void BackendInterface::OnKeyRelease(uint keyId){
	//
}

void BackendInterface::Bind(boost::python::object obj){
	BackendInterface &backendInt1 = boost::python::extract<BackendInterface&>(obj)();
	pbackendInt = &backendInt1;
}

void BackendInterface::SetFocus(WManager::Container *pcontainer){
	if(!pcontainer)
		return;
	pfocus = pcontainer;
	pcontainer->Focus();
}

boost::python::object BackendInterface::GetFocus(){
	ContainerConfig *pcontainer1 = dynamic_cast<ContainerConfig *>(pfocus);
	if(pcontainer1)
		return pcontainer1->pcontainerInt->self;
	return boost::python::object();
}

BackendInterface BackendInterface::defaultInt;
BackendInterface *BackendInterface::pbackendInt = &BackendInterface::defaultInt;
WManager::Container *BackendInterface::pfocus = 0; //initially set to root container as soon as it's created

BackendProxy::BackendProxy(){
	//
}

BackendProxy::~BackendProxy(){
	//
}

void BackendProxy::OnSetupKeys(Backend::X11KeyBinder *pkeyBinder){
	boost::python::override ovr = this->get_override("OnSetupKeys");
	if(ovr){
		try{
			ovr(pkeyBinder);
		}catch(boost::python::error_already_set &){
			PyErr_Print();
			//
			boost::python::handle_exception();
			PyErr_Clear();
		}
	}else BackendInterface::OnSetupKeys(pkeyBinder);
}

boost::python::object BackendProxy::OnCreateContainer(){
	boost::python::override ovr = this->get_override("OnCreateContainer");
	if(ovr)
		return ovr();
	else return BackendInterface::OnCreateContainer();
}

void BackendProxy::OnKeyPress(uint keyId){
	boost::python::override ovr = this->get_override("OnKeyPress");
	if(ovr)
		ovr(keyId);
	else BackendInterface::OnKeyPress(keyId);
}

void BackendProxy::OnKeyRelease(uint keyId){
	boost::python::override ovr = this->get_override("OnKeyRelease");
	if(ovr)
		ovr(keyId);
	else BackendInterface::OnKeyRelease(keyId);
}

CompositorInterface::CompositorInterface() : shaderPath("."){
	//
}

CompositorInterface::~CompositorInterface(){
	//
}

void CompositorInterface::Bind(boost::python::object obj){
	CompositorInterface &compositorInt = boost::python::extract<CompositorInterface&>(obj)();
	pcompositorInt = &compositorInt;
}

CompositorInterface CompositorInterface::defaultInt;
CompositorInterface *CompositorInterface::pcompositorInt = &CompositorInterface::defaultInt;

CompositorProxy::CompositorProxy(){
	//
}

CompositorProxy::~CompositorProxy(){
	//
}

BOOST_PYTHON_MODULE(chamfer){
	boost::python::scope().attr("MOD_MASK_1") = uint(XCB_MOD_MASK_1);
	boost::python::scope().attr("MOD_MASK_2") = uint(XCB_MOD_MASK_2);
	boost::python::scope().attr("MOD_MASK_3") = uint(XCB_MOD_MASK_3);
	boost::python::scope().attr("MOD_MASK_4") = uint(XCB_MOD_MASK_4);
	boost::python::scope().attr("MOD_MASK_5") = uint(XCB_MOD_MASK_5);
	boost::python::scope().attr("MOD_MASK_SHIFT") = uint(XCB_MOD_MASK_SHIFT);
	boost::python::scope().attr("MOD_MASK_CONTROL") = uint(XCB_MOD_MASK_CONTROL);

	boost::python::scope().attr("KEY_RETURN") = uint(XK_Return);
	boost::python::scope().attr("KEY_TAB") = uint(XK_Tab);

	boost::python::class_<Backend::X11KeyBinder>("KeyBinder",boost::python::no_init)
		.def("BindKey",&Backend::X11KeyBinder::BindKey)
		;
	
	boost::python::class_<ContainerProxy,boost::noncopyable>("Container")
		.def("OnSetup",&ContainerInterface::OnSetup)
		.def("OnCreate",&ContainerInterface::OnCreate)
		.def("GetNext",&ContainerInterface::GetNext)
		.def("GetPrev",&ContainerInterface::GetPrev)
		.def("GetParent",&ContainerInterface::GetParent)
		.def("GetFocus",&ContainerInterface::GetFocus)
		.def("Focus",boost::python::make_function(
			[](ContainerInterface &container){
				Config::BackendInterface::pfocus = container.pcontainer;
				container.pcontainer->Focus();
			},boost::python::default_call_policies(),boost::mpl::vector<void, ContainerInterface &>()))
		.def("ShiftLayout",boost::python::make_function(
			[](ContainerInterface &container, WManager::Container::LAYOUT layout){
				container.pcontainer->SetLayout(layout);
			},boost::python::default_call_policies(),boost::mpl::vector<void, ContainerInterface &, WManager::Container::LAYOUT>()))
		.def_readwrite("borderWidth",&ContainerInterface::borderWidth)
		.def_readwrite("minSize",&ContainerInterface::minSize)
		.def_readwrite("maxSize",&ContainerInterface::maxSize)
		.add_property("layout",boost::python::make_function(
			[](ContainerInterface &container){
				return container.pcontainer->layout;
			},boost::python::default_call_policies(),boost::mpl::vector<WManager::Container::LAYOUT, ContainerInterface &>()))
		;

	boost::python::class_<BackendProxy,boost::noncopyable>("Backend")
		.def("OnSetupKeys",&BackendInterface::OnSetupKeys)
		.def("OnCreateContainer",&BackendInterface::OnCreateContainer)
		.def("OnKeyPress",&BackendInterface::OnKeyPress)
		.def("OnKeyRelease",&BackendInterface::OnKeyRelease)
		;
	boost::python::def("bind_Backend",BackendInterface::Bind);
	boost::python::class_<CompositorProxy,boost::noncopyable>("Compositor")
		.add_property("shaderPath",&CompositorInterface::shaderPath)
		;
	boost::python::def("bind_Compositor",CompositorInterface::Bind);

	boost::python::enum_<WManager::Container::LAYOUT>("layout")
		.value("VSPLIT",WManager::Container::LAYOUT_VSPLIT)
		.value("HSPLIT",WManager::Container::LAYOUT_HSPLIT);

	boost::python::def("GetFocus",&BackendInterface::GetFocus);
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

void Loader::Run(const char *pfilePath, const char *pfileLabel){
	FILE *pf = fopen(pfilePath,"rb");
	if(!pf){
		DebugPrintf(stderr,"Unable to open configuration file %s\n",pfilePath);
		return;
	}
	try{
		PyRun_SimpleFile(pf,pfileLabel);

	}catch(boost::python::error_already_set &){
		boost::python::handle_exception();
		PyErr_Clear();
	}
	fclose(pf);
}

}

