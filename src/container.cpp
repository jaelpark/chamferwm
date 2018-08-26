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

Container::Container() : pParent(0), pch(0), pnext(0),
	pfocus(this), pPrevFocus(this),
	pclient(0),
	scale(1.0f), p(0.0f), e(1.0f){
	//
}

Container::Container(Container *pParent) : pch(0), pnext(0),
	pfocus(this), pPrevFocus(this),
	pclient(0),
	scale(1.0f){
	//uint totalSiblings = 0;
	//for(Container *p = pParent->pch; p; ++totalSiblings, p = p->pnext);

	Container **pn = &pParent->pch;
	while(*pn)
		pn = &(*pn)->pnext;
	*pn = this;
	this->pParent = pParent;
	pParent->pPrevFocus = pParent->pfocus;
	pParent->pfocus = this;

	//If parent has a client, this container is a result of a new split.
	//Move the client to this new container.
	if(pParent->pclient){
		pclient = pParent->pclient;
		pParent->pclient = 0;
	}

	//TODO: Resize recursive, all child containers of the parent
	//1. get screen size (or work with normalized scales, supply them to SetTranslation)
	//2. normalize the container scales
	//3. resize the clients, apply recursive total scale

	pParent->SetTranslation(pParent->p,pParent->e);
}

Container::~Container(){
	//
}

void Container::SetTranslation(glm::vec2 p, glm::vec2 e){
	glm::vec2 scaleSum(0.0f);
	for(Container *pcontainer = pch; pcontainer; scaleSum += pcontainer->scale, pcontainer = pcontainer->pnext);

	glm::vec2 position(0.0f);
	for(Container *pcontainer = pch; pcontainer; pcontainer = pcontainer->pnext){
		glm::vec2 e1 = mode != MODE_STACKED?pcontainer->scale/scaleSum:e;
		pcontainer->SetTranslation(position,e1);
		switch(mode){
		case MODE_VSPLIT:
			position.x += e1.x;
			break;
		case MODE_HSPLIT:
			position.y += e1.y;
			break;
		};
	}

	this->p = p;
	this->e = e;
	if(pclient)
		pclient->SetTranslation(p,e);
}

void Container::Assign(Client *pclient){
	this->pclient = pclient;
	pclient->SetTranslation(p,e);
}

}

