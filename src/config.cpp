#include "main.h"
#include "config.h"
#include "container.h"
#include "backend.h"
#include <xcb/xcb_keysyms.h> //todo: should not depend on xcb here

namespace Config{

KeyConfigInterface::KeyConfigInterface(){
	//
}

KeyConfigInterface::~KeyConfigInterface(){
	//
}

void KeyConfigInterface::SetKeyBinder(Backend::BackendKeyBinder *_pkeyBinder){
	pkeyBinder = _pkeyBinder;
}

void KeyConfigInterface::SetupKeys(){
	//
	DebugPrintf(stdout,"No KeyConfig interface, skipping configuration.\n");
}

void KeyConfigInterface::Bind(boost::python::object obj){
	KeyConfigInterface &keyConfigInt1 = boost::python::extract<KeyConfigInterface&>(obj)();
	pkeyConfigInt = &keyConfigInt1;
}

KeyConfigInterface KeyConfigInterface::defaultInt;
KeyConfigInterface *KeyConfigInterface::pkeyConfigInt = &KeyConfigInterface::defaultInt;

KeyConfigProxy::KeyConfigProxy(){
	//
}

KeyConfigProxy::~KeyConfigProxy(){
	//
}

void KeyConfigProxy::BindKey(uint symbol, uint mask, uint keyId){
	pkeyBinder->BindKey(symbol,mask,keyId);
}

void KeyConfigProxy::SetupKeys(){
	boost::python::override ovr = this->get_override("SetupKeys");
	if(ovr)
		ovr();
	else KeyConfigInterface::SetupKeys();
}

BackendInterface::BackendInterface(){
	//
}

BackendInterface::~BackendInterface(){
	//
}

void BackendInterface::OnCreateClient(WManager::Container *pcontainer){
	//
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

//void BackendInterface::SetFocus(boost::python::object obj){
void BackendInterface::SetFocus(WManager::Container *pcontainer){
	//WManager::Container *pcontainer = &boost::python::extract<WManager::Container&>(obj)();
	printf("SetFocus: %x\n",pcontainer);
	if(!pcontainer)
		return;
	pcontainer->Focus();
	pfocus = pcontainer;
}

WManager::Container * BackendInterface::GetFocus(){
	printf("GetFocus: %x\n",pfocus);
	return pfocus;
//boost::python::object BackendInterface::GetFocus(){
	//printf("GetFocus: %x\n",pfocus);
	//return boost::python::object(pfocus);
	/*typename boost::python::reference_existing_object::apply<WManager::Container*>::type converter;
	boost::python::handle handle(converter(pfocus));
	return boost::python::object(handle);*/
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

void BackendProxy::OnCreateClient(WManager::Container *pcontainer){
	boost::python::override ovr = this->get_override("OnCreateClient");
	if(ovr){
		ovr(pcontainer);
		//ovr.operator()<boost::python::return_value_policy::reference>(pcontainer).cast<WManager::Container*>();
		/*typename boost::python::reference_existing_object::apply<WManager::Container*>::type converter;
		boost::python::handle handle(converter(pfocus));
		ovr(boost::python::object(handle));*/
	}
	else BackendInterface::OnCreateClient(pcontainer);
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

	boost::python::class_<KeyConfigProxy,boost::noncopyable>("KeyConfig")
		.def("BindKey",&KeyConfigProxy::BindKey)
		.def("SetupKeys",&KeyConfigInterface::SetupKeys)
		;
	boost::python::def("bind_KeyConfig",KeyConfigInterface::Bind);
	boost::python::class_<BackendProxy,boost::noncopyable>("Backend")
		.def("OnCreateClient",&BackendInterface::OnCreateClient)
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
		.value("HSPLIT",WManager::Container::LAYOUT_HSPLIT)
		.value("STACKED",WManager::Container::LAYOUT_STACKED)
		.value("TABBED",WManager::Container::LAYOUT_TABBED);

	boost::python::class_<WManager::Container>("Container",boost::python::no_init)
		//.def("GetNext",&WManager::Container::GetNext,boost::python::return_value_policy<boost::python::reference_existing_object>())
		//.def("GetPrev",&WManager::Container::GetPrev,boost::python::return_value_policy<boost::python::reference_existing_object>())
		//.def("GetParent",&WManager::Container::GetParent,boost::python::return_value_policy<boost::python::reference_existing_object>())
		//.def("GetFocus",&WManager::Container::GetFocus,boost::python::return_value_policy<boost::python::reference_existing_object>())
		.def("GetNext",&WManager::Container::GetNext,boost::python::return_internal_reference<>())
		.def("GetPrev",&WManager::Container::GetPrev,boost::python::return_internal_reference<>())
		.def("GetParent",&WManager::Container::GetParent,boost::python::return_internal_reference<>())
		.def("GetFocus",&WManager::Container::GetFocus,boost::python::return_internal_reference<>())
		.def_readonly("layout",&WManager::Container::layout)
		;
	
	//boost::python::def("GetFocus",&BackendInterface::GetFocus,boost::python::return_value_policy<boost::python::reference_existing_object>());
	boost::python::def("GetFocus",&BackendInterface::GetFocus,boost::python::return_internal_reference<>());
	//boost::python::def("GetFocus",&BackendInterface::GetFocus);
	boost::python::def("SetFocus",&BackendInterface::SetFocus);
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

