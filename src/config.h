#ifndef CONFIG_H
#define CONFIG_H

#include "main.h"
#include <boost/python.hpp>

namespace Backend{
class BackendKeyBinder;
}

namespace WManager{
class Container;
}

namespace Config{

class KeyConfigInterface{
public:
	KeyConfigInterface();
	~KeyConfigInterface();
	void SetKeyBinder(class Backend::BackendKeyBinder *);
	virtual void SetupKeys();
	Backend::BackendKeyBinder *pkeyBinder;

	static void Bind(boost::python::object);
	static KeyConfigInterface defaultInt;
	static KeyConfigInterface *pkeyConfigInt;
};

class KeyConfigProxy : public KeyConfigInterface, public boost::python::wrapper<KeyConfigInterface>{
public:
	KeyConfigProxy();
	~KeyConfigProxy();
	void BindKey(uint, uint, uint);
	//
	void SetupKeys();
};

class BackendInterface{
public:
	BackendInterface();
	~BackendInterface();
	virtual void OnCreateClient(WManager::Container *);
	virtual void OnKeyPress(uint);
	virtual void OnKeyRelease(uint);

	static void Bind(boost::python::object);
	static BackendInterface defaultInt;
	static BackendInterface *pbackendInt;
};

class BackendProxy : public BackendInterface, public boost::python::wrapper<BackendInterface>{
public:
	BackendProxy();
	~BackendProxy();
	//
	void OnCreateClient(WManager::Container *);
	void OnKeyPress(uint);
	void OnKeyRelease(uint);
};

class CompositorInterface{
public:
	CompositorInterface();
	~CompositorInterface();
	std::string shaderPath;
	static void Bind(boost::python::object);
	static CompositorInterface defaultInt;
	static CompositorInterface *pcompositorInt;
};

class CompositorProxy : public CompositorInterface, public boost::python::wrapper<CompositorInterface>{
public:
	CompositorProxy();
	~CompositorProxy();
};

class Loader{
public:
	Loader(const char *);
	~Loader();
	void Run(const char *, const char *);
};

}

#endif

