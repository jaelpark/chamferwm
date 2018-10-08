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
	scale(1.0f), p(0.0f), e(1.0f), borderWidth(0.0f), minSize(0.0f), maxSize(1.0f),
	layout(LAYOUT_VSPLIT){
	//
}

Container::Container(Container *_pParent) : pParent(_pParent), pch(0), pnext(0),
	pclient(0),
	scale(1.0f), borderWidth(0.0f), minSize(0.4f), maxSize(1.0f),
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

	//pParent->SetTranslation(pParent->p,pParent->e);
	pParent->Translate();
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
	
	//pParent->SetTranslation(pParent->p,pParent->e);
	pParent->Translate();
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

glm::vec2 Container::GetMinSize() const{
	//Return the minimum size requirement for the container
	glm::vec2 chSize(0.0f);
	for(Container *pcontainer = pch; pcontainer; chSize = glm::max(chSize,pcontainer->GetMinSize()), pcontainer = pcontainer->pnext);
	return glm::max(chSize,minSize);
}

void Container::TranslateRecursive(glm::vec2 p, glm::vec2 e){
	uint count = 0;

	glm::vec2 minSizeSum(0.0f);
	for(Container *pcontainer = pch; pcontainer; ++count, pcontainer = pcontainer->pnext){
		pcontainer->e1 = pcontainer->GetMinSize();
		minSizeSum += pcontainer->e1;
	}
	
	/*
	minimize overlap1(e1)/e1+overlap2(e2)/e2+...
	st. e1+e2+...=e
	e1 > m1
	e2 > m2
	...
	//use steepest descent algorithm with boundary walls or penalty function
	//-need initial starting point within the boundaries, use it for testing first

	initial guess
	-assign min to everyone
	-assign the remaining equally
	*/

	glm::vec2 position(0.0f);
	switch(layout){
	default:
	case LAYOUT_VSPLIT:
		if(e.x-minSizeSum.x < 0.0f){
			//overlap required, everything has been minimized to the limit
			for(Container *pcontainer = pch; pcontainer; pcontainer = pcontainer->pnext){
				glm::vec2 e1 = glm::vec2(pcontainer->e1.x,e.y);
				glm::vec2 p1 = position;

				position.x += e1.x-(minSizeSum.x-e.x)/(float)(count-1);
				pcontainer->TranslateRecursive(p1,e1);
			}

		}else{
			//optimize the containers, minimize interior overlap
			for(Container *pcontainer = pch; pcontainer; pcontainer = pcontainer->pnext){
				glm::vec2 e1 = glm::vec2(pcontainer->e1.x+(e.x-minSizeSum.x)/(float)count,e.y);
				glm::vec2 p1 = position;

				position.x += e1.x;
				pcontainer->TranslateRecursive(p1,e1);
			}
		}
		break;
	case LAYOUT_HSPLIT:
		if(e.y-minSizeSum.y < 0.0f){
			//overlap required, everything has been minimized to the limit
			for(Container *pcontainer = pch; pcontainer; pcontainer = pcontainer->pnext){
				glm::vec2 e1 = glm::vec2(e.x,pcontainer->e1.y);
				glm::vec2 p1 = position;

				position.y += e1.y-(minSizeSum.y-e.y)/(float)(count-1);
				pcontainer->TranslateRecursive(p1,e1);
			}

		}else{
			//optimize the containers, minimize interior overlap
			for(Container *pcontainer = pch; pcontainer; pcontainer = pcontainer->pnext){
				glm::vec2 e1 = glm::vec2(e.x,pcontainer->e1.y+(e.y-minSizeSum.y)/(float)count);
				glm::vec2 p1 = position;

				position.y += e1.y;
				pcontainer->TranslateRecursive(p1,e1);
			}
		}
		//
		break;
	}

	this->p = p;
	this->e = e;
	if(pclient)
		pclient->UpdateTranslation();
}

void Container::Translate(){
	glm::vec2 size = GetMinSize();

	//Check the parent hierarchy, up to which point they won't have to be updated (condition overlappedSize <= e).
	//Find the first ancestor large enough to contain everything without modifiying its size.
	Container *pcontainer;
	for(pcontainer = pParent; pcontainer && glm::any(glm::lessThan(pcontainer->e,size-1e-5f)); pcontainer = pcontainer->pParent);
	if(!pcontainer)
		pcontainer = this; //reached the root

	pcontainer->TranslateRecursive(pcontainer->p,pcontainer->e);
}

void Container::SetLayout(LAYOUT layout){
	this->layout = layout;
	//SetTranslation(p,e);
	Translate();
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
			e1.x = pcontainer->scale.x/scaleSum.x; //note: fail with subcontainers
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

