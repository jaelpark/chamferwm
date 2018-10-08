#ifndef CONFIG_H
#define CONFIG_H

#include <boost/python.hpp>

namespace Backend{
class BackendKeyBinder;
class X11KeyBinder;
}

namespace WManager{
class Client;
class Container;
}

namespace Config{

class ClientProxy{
public:
	ClientProxy();
	ClientProxy(WManager::Client *);
	~ClientProxy();
	WManager::Container * GetContainer() const;
	WManager::Client *pclient;
};

class BackendInterface{
public:
	BackendInterface();
	virtual ~BackendInterface();
	virtual void SetupKeys(Backend::X11KeyBinder *);
	virtual void OnCreateClient(const ClientProxy &);
	virtual void OnKeyPress(uint);
	virtual void OnKeyRelease(uint);

	static void Bind(boost::python::object);
	static void SetFocus(WManager::Container *);
	static WManager::Container * GetFocus();

	static BackendInterface defaultInt;
	static BackendInterface *pbackendInt;
	static WManager::Container *pfocus; //client focus, managed by Python
};

class BackendProxy : public BackendInterface, public boost::python::wrapper<BackendInterface>{
public:
	BackendProxy();
	~BackendProxy();
	//
	void SetupKeys(Backend::X11KeyBinder *);
	void OnCreateClient(const ClientProxy &);
	void OnKeyPress(uint);
	void OnKeyRelease(uint);
};

class CompositorInterface{
public:
	CompositorInterface();
	~CompositorInterface();
	std::string shaderPath;
	//virtual void SetupShaders();

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

