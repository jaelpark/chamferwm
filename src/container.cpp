#include "main.h"
#include "container.h"

namespace WManager{

Client::Client(Container *_pcontainer) : pcontainer(_pcontainer){
	//
}

Client::~Client(){
	//
}

Container::Container() : pParent(0), pch(0), pnext(0),
	pfocus(this), pPrevFocus(this),
	pclient(0),
	scale(1.0f), p(0.0f), e(1.0f),
	mode(MODE_VSPLIT){
	//
}

Container::Container(Container *pParent) : pch(0), pnext(0),
	pfocus(this), pPrevFocus(this),
	pclient(0),
	scale(1.0f),
	mode(MODE_VSPLIT){

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

		pclient->pcontainer = this;
	}

	pParent->SetTranslation(pParent->p,pParent->e);
}

Container::~Container(){
	//
	for(Container *pcontainer = pParent->pch, *pPrev = 0; pcontainer; pPrev = pcontainer, pcontainer = pcontainer->pnext)
		if(pcontainer == this){
			if(pPrev)
				pPrev->pnext = pcontainer->pnext;
			else pParent->pch = pcontainer->pnext;
			break;
		}
	
	pParent->SetTranslation(pParent->p,pParent->e);
}

void Container::SetTranslation(glm::vec2 p, glm::vec2 e){
	glm::vec2 scaleSum(0.0f);
	for(Container *pcontainer = pch; pcontainer; scaleSum += pcontainer->scale, pcontainer = pcontainer->pnext);

	glm::vec2 position(0.0f);
	for(Container *pcontainer = pch; pcontainer; pcontainer = pcontainer->pnext){
		glm::vec2 e1 = e;//mode != MODE_STACKED?pcontainer->scale/scaleSum:e;
		glm::vec2 p1 = position;
		switch(mode){
		case MODE_VSPLIT:
			e1.x = pcontainer->scale.x/scaleSum.x;
			position.x += e1.x;
			break;
		case MODE_HSPLIT:
			e1.y = pcontainer->scale.y/scaleSum.y;
			position.y += e1.y;
			break;
		};
		pcontainer->SetTranslation(p1,e1);
	}

	this->p = p;
	this->e = e;
	if(pclient)
		pclient->SetTranslation(p,e);
}

}

