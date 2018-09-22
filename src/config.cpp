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

KeyActionInterface::KeyActionInterface(){
	//
}

KeyActionInterface::~KeyActionInterface(){
	//
}

void KeyActionInterface::OnKeyPress(uint keyId){
	//
}

void KeyActionInterface::OnKeyRelease(uint keyId){
	//
}

void KeyActionInterface::Bind(boost::python::object obj){
	KeyActionInterface &keyActionInt1 = boost::python::extract<KeyActionInterface&>(obj)();
	pkeyActionInt = &keyActionInt1;
}

KeyActionInterface KeyActionInterface::defaultInt;
KeyActionInterface *KeyActionInterface::pkeyActionInt = &KeyActionInterface::defaultInt;

KeyActionProxy::KeyActionProxy(){
	//
}

KeyActionProxy::~KeyActionProxy(){
	//
}

void KeyActionProxy::OnKeyPress(uint keyId){
	boost::python::override ovr = this->get_override("OnKeyPress");
	if(ovr)
		ovr(keyId);
	else KeyActionInterface::OnKeyPress(keyId);
}

void KeyActionProxy::OnKeyRelease(uint keyId){
	boost::python::override ovr = this->get_override("OnKeyRelease");
	if(ovr)
		ovr(keyId);
	else KeyActionInterface::OnKeyRelease(keyId);
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
	boost::python::class_<KeyActionProxy,boost::noncopyable>("KeyAction")
		.def("OnKeyPress",&KeyActionInterface::OnKeyPress)
		.def("OnKeyRelease",&KeyActionInterface::OnKeyRelease)
		;
	boost::python::def("bind_KeyAction",KeyActionInterface::Bind);
	boost::python::class_<CompositorProxy,boost::noncopyable>("Compositor")
		.add_property("shaderPath",&CompositorInterface::shaderPath)
		;
	boost::python::def("bind_Compositor",CompositorInterface::Bind);
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
		printf("failure\n");
		boost::python::handle_exception();
		PyErr_Clear();
	}
	fclose(pf);
}

}

