#include "main.h"
#include "container.h"

namespace WManager{

/*Client::Client(BackendInterface *pbackend){
	pbackend->clients.push_back(this);
	this->pbackend = pbackend;
}

Client::~Client(){
	std::vector<Client *>::iterator m = std::find(
		pbackend->clients.begin(),pbackend->clients.end(),this);
	std::iter_swap(m,pbackend->clients.end()-1);
	pbackend->clients.pop_back();
}*/

Client::Client(){
	//
}

Client::~Client(){
	//
}

Container::Container() : pch(0), pnext(0), pclient(0){
	//
}

Container::~Container(){
	//
}

}

