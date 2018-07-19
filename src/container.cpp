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

Container::Container() : pParent(0), pch(0), pnext(0), pfocus(this), pPrevFocus(this), pclient(0){
	//
}

Container::Container(Container *pParent) : pch(0), pnext(0), pfocus(this), pPrevFocus(this), pclient(0){
	Container **pn = &pParent->pch;
	while(*pn)
		pn = &(*pn)->pnext;
	*pn = this;
	this->pParent = pParent;
	pParent->pPrevFocus = pParent->pfocus;
	pParent->pfocus = this;
}

Container::~Container(){
	//
}

}

