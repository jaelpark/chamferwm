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
	pfocus(this),// pPrevFocus(this),
	pclient(0),
	scale(1.0f), p(0.0f), e(1.0f),
	layout(LAYOUT_VSPLIT){
	//
}

Container::Container(Container *pParent) : pch(0), pnext(0),
	pfocus(0),// pPrevFocus(this),
	pclient(0),
	scale(1.0f),
	layout(LAYOUT_VSPLIT){

	Container **pn = &pParent->pch;
	while(*pn)
		pn = &(*pn)->pnext;
	*pn = this;
	this->pParent = pParent;
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

void Container::Remove(){
	for(Container *pcontainer = pParent->pch, *pPrev = 0; pcontainer; pPrev = pcontainer, pcontainer = pcontainer->pnext)
		if(pcontainer == this){
			if(pPrev)
				pPrev->pnext = pcontainer->pnext;
			else pParent->pch = pcontainer->pnext;
			break;
		}
	
	pParent->SetTranslation(pParent->p,pParent->e);
}

/*Container * Container::FindFocus(){
	if(!pfocus)
		return this;
	return pfocus->FindFocus();
}*/

Container * Container::GetNext(){
	return pnext;
}

Container * Container::GetPrev(){
	Container *pcontainer = pParent->pch;
	for(; pcontainer->pnext != this; pcontainer = pcontainer->pnext);
	return pcontainer;
}

Container * Container::GetParent(){
	return pParent;
}

Container * Container::GetFocus(){
	return pfocus;
}

void Container::SetLayout(LAYOUT layout){
	this->layout = layout;
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
		glm::vec2 e1 = e;
		glm::vec2 p1 = position;
		switch(layout){
		default:
		case LAYOUT_VSPLIT:
			e1.x = pcontainer->scale.x/scaleSum.x;
			position.x += e1.x;
			break;
		case LAYOUT_HSPLIT:
			e1.y = pcontainer->scale.y/scaleSum.y;
			position.y += e1.y;
			break;
		};
		pcontainer->SetTranslation(p1,e1);
	}

	this->p = p;
	this->e = e;
	if(pclient){
		pclient->UpdateTranslation();
		printf("pclient->UpdateTranslation()\n");
	}
}

}

