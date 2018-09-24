#include "main.h"
#include "container.h"

#include <algorithm>

namespace WManager{

Client::Client(Container *_pcontainer) : pcontainer(_pcontainer){
	//
}

Client::~Client(){
	//
}

Container::Container() : pParent(0), pch(0), pnext(0),
	pclient(0),
	scale(1.0f), p(0.0f), e(1.0f), borderWidth(0.0f),
	layout(LAYOUT_VSPLIT){
	//
}

Container::Container(Container *_pParent) : pParent(_pParent), pch(0), pnext(0),
	pclient(0),
	scale(1.0f), borderWidth(0.0f),
	layout(LAYOUT_VSPLIT){
	
	if(pParent->focusQueue.size() > 0){
		Container *pfocus = pParent->focusQueue.back();
		pnext = pfocus->pnext;
		pfocus->pnext = this;

	}else{
		Container **pn = &pParent->pch;
		while(*pn)
			pn = &(*pn)->pnext;
		*pn = this;
	}

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
}

void Container::Remove(){
	if(pParent)
		pParent->focusQueue.erase(std::remove(pParent->focusQueue.begin(),pParent->focusQueue.end(),this),pParent->focusQueue.end());
	for(Container *pcontainer = pParent->pch, *pPrev = 0; pcontainer; pPrev = pcontainer, pcontainer = pcontainer->pnext)
		if(pcontainer == this){
			if(pPrev)
				pPrev->pnext = pcontainer->pnext;
			else pParent->pch = pcontainer->pnext;
			break;
		}
	
	pParent->SetTranslation(pParent->p,pParent->e);
}

void Container::Focus(){
	if(pParent){
		pParent->focusQueue.erase(std::remove(pParent->focusQueue.begin(),pParent->focusQueue.end(),this),pParent->focusQueue.end());
		pParent->focusQueue.push_back(this);
	}
	if(pclient)
		pclient->Focus();
}

Container * Container::GetNext(){
	if(!pParent)
		return 0; //root container
	if(!pnext)
		return pParent->pch;
	return pnext;
}

Container * Container::GetPrev(){
	if(!pParent)
		return 0; //root container
	Container *pcontainer = pParent->pch;
	if(pcontainer == this)
		for(; pcontainer->pnext; pcontainer = pcontainer->pnext);
	else for(; pcontainer->pnext != this; pcontainer = pcontainer->pnext);
	return pcontainer;
}

Container * Container::GetParent(){
	return pParent;
}

Container * Container::GetFocus(){
	return focusQueue.size() > 0?focusQueue.back():0;
}

void Container::SetLayout(LAYOUT layout){
	this->layout = layout;
	SetTranslation(p,e);
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
	if(pclient)
		pclient->UpdateTranslation();
}

}

