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
	//scale(1.0f), p(0.0f), e(1.0f), margin(0.0f), minSize(0.0f), maxSize(1.0f), mode(MODE_TILED),
	p(0.0f), posFullCanvas(0.0f), e(1.0f), extFullCanvas(1.0f), canvasOffset(0.0f), canvasExtent(0.0f),
	margin(0.015f), size(1.0f), minSize(0.015f), maxSize(1.0f),
	flags(0), layout(LAYOUT_VSPLIT){//, flags(0){
	//
}

Container::Container(Container *_pParent, const Setup &setup) :
	pParent(_pParent), pch(0), pnext(0),
	pclient(0),
	canvasOffset(setup.canvasOffset), canvasExtent(setup.canvasExtent),
	margin(setup.margin), size(setup.size), minSize(setup.minSize), maxSize(setup.maxSize),// mode(setup.mode),
	flags(setup.flags), layout(LAYOUT_VSPLIT){//, flags(setup.flags){

	if(flags & FLAG_FLOATING)
		return;

	//TODO: allow reparenting containers without client
	if(pParent->pclient){
	//if(pParent->pclient || setup.insert == Setup::INSERT_REPARENT){
		size = pParent->size;
		pParent->size = setup.size;
		//reparent
		Container *pbase = pParent->pParent;
		//replace the original parent with this new container
		if(std::find(pbase->focusQueue.begin(),pbase->focusQueue.end(),pParent) != pbase->focusQueue.end()){
			std::replace(pbase->focusQueue.begin(),pbase->focusQueue.end(),pParent,this);
			focusQueue.push_back(pParent); //carry on the focus queue if it exists
		}
		for(Container *pcontainer = pbase->pch, *pPrev = 0; pcontainer; pPrev = pcontainer, pcontainer = pcontainer->pnext)
			if(pcontainer == pParent){
				if(pPrev)
					pPrev->pnext = this;
				else pbase->pch = this;
				pnext = pcontainer->pnext;
				break;
			}

		//add the original parent under the new one
		pParent->pParent = this;
		pParent->pnext = 0;

		pch = pParent;
		pParent = pbase;

	}else{
		if(pParent->focusQueue.size() > 0){
			//place next to the focused container
			Container *pfocus = pParent->focusQueue.back();
			pnext = pfocus->pnext;
			pfocus->pnext = this;

		}else{
			Container **pn = &pParent->pch;
			while(*pn)
				pn = &(*pn)->pnext;
			*pn = this;
		}

		uint containerCount = 0;
		for(Container *pcontainer = pParent->pch; pcontainer; ++containerCount, pcontainer = pcontainer->pnext);
		size = glm::vec2(1.0f/(float)containerCount);

		for(Container *pcontainer = pParent->pch; pcontainer; pcontainer = pcontainer->pnext)
			if(pcontainer != this)
				pcontainer->size *= glm::vec2(1.0f)-size;

	}

	pParent->Translate();
}

Container::~Container(){
	//
}

void Container::Place(Container *pParent1){
	//TODO: reparent the floating container
	if(flags & FLAG_FLOATING)
		return;
	pParent = pParent1;
	if(pParent->focusQueue.size() > 0){
		//place next to the focused container
		Container *pfocus = pParent->focusQueue.back();
		pnext = pfocus->pnext;
		pfocus->pnext = this;

	}else{
		Container **pn = &pParent->pch;
		while(*pn)
			pn = &(*pn)->pnext;
		*pn = this;
	}

	uint containerCount = 0;
	for(Container *pcontainer = pParent->pch; pcontainer; ++containerCount, pcontainer = pcontainer->pnext);
	size = glm::vec2(1.0f/(float)containerCount);

	for(Container *pcontainer = pParent->pch; pcontainer; pcontainer = pcontainer->pnext)
		if(pcontainer != this)
			pcontainer->size *= glm::vec2(1.0f)-size;
	
	GetRoot()->Stack();
	pParent->Translate();
}

Container * Container::Remove(){
	if(flags & FLAG_FLOATING)
		return this;
	Container *pbase = pParent, *pch1 = this; //remove all the split containers (although there should be only one)
	for(; pbase->pParent; pch1 = pbase, pbase = pbase->pParent){
		if(pbase->pch->pnext)
			break; //more than one container
	}
	pbase->focusQueue.erase(std::remove(pbase->focusQueue.begin(),pbase->focusQueue.end(),pch1),pbase->focusQueue.end());
	for(Container *pcontainer = pbase->pch, *pPrev = 0; pcontainer; pPrev = pcontainer, pcontainer = pcontainer->pnext)
		if(pcontainer == pch1){
			if(pPrev)
				pPrev->pnext = pcontainer->pnext;
			else pbase->pch = pcontainer->pnext;
			break;
		}
	
	for(Container *pcontainer = pParent->pch; pcontainer; pcontainer = pcontainer->pnext)
		pcontainer->size /= 1.0f-size;
	
	GetRoot()->Stack();
	pbase->Translate();

	return pch1;
}

Container * Container::Collapse(){
	if(flags & FLAG_FLOATING)
		return 0;
	if(!pParent || !pch)
		return 0;
	
	if(pParent->pch->pnext){
		if(pch->pnext)
			return 0; //should not collapse

		pch->size = size;

		//only one container to collapse
		//printf("** type 1 collapse\n");
		std::replace(pParent->focusQueue.begin(),pParent->focusQueue.end(),this,pch);

		pch->pParent = pParent;

		for(Container *pcontainer = pParent->pch, *pPrev = 0; pcontainer; pPrev = pcontainer, pcontainer = pcontainer->pnext){
			if(pcontainer == this){
				if(pPrev)
					pPrev->pnext = pch;
				else pParent->pch = pch;
				pch->pnext = pcontainer->pnext;
				break;
			}
		}

		pch = 0;
		focusQueue.clear();

		return this;

	}else
	if(pch->pnext){ //if there are more than one container
		if(pParent->pch->pnext) //and there's a container next to this one
			return 0; //should not collapse

		//more than one container to deal with
		//printf("** type 2 collapse\n");
		Container *pfocus = focusQueue.size() > 0?focusQueue.back():pch;
		std::replace(pParent->focusQueue.begin(),pParent->focusQueue.end(),this,pfocus);

		for(Container *pcontainer = pch; pcontainer; pcontainer = pcontainer->pnext)
			pcontainer->pParent = pParent;

		pParent->pch = pch;

		pch = 0; //dereference the leftover container
		focusQueue.clear();

		return this;
	}

	GetRoot()->Stack();

	return 0;
}

void Container::Focus(){
	if(flags & FLAG_NO_FOCUS)
		return;
	if(flags & FLAG_FLOATING){
		floatFocusQueue.erase(std::remove(floatFocusQueue.begin(),floatFocusQueue.end(),this),floatFocusQueue.end());
		floatFocusQueue.push_back(this);

		Stack1();

	}else{
		tiledFocusQueue.erase(std::remove_if(tiledFocusQueue.begin(),tiledFocusQueue.end(),[&](auto &p)->bool{
			return p.first == this;
		}),tiledFocusQueue.end());
		struct timespec focusTime;
		clock_gettime(CLOCK_MONOTONIC,&focusTime);
		if(tiledFocusQueue.size() > 1 && //keep the previous container if it's the only one
			timespec_diff(focusTime,tiledFocusQueue.back().second) < 0.5)
			tiledFocusQueue.back() = std::pair<WManager::Container *, struct timespec>(this,focusTime);
		else tiledFocusQueue.push_back(std::pair<WManager::Container *, struct timespec>(this,focusTime));

		for(Container *pcontainer = this; pcontainer->pParent; pcontainer = pcontainer->pParent){
			pcontainer->pParent->focusQueue.erase(std::remove(pcontainer->pParent->focusQueue.begin(),pcontainer->pParent->focusQueue.end(),pcontainer),pcontainer->pParent->focusQueue.end());
			pcontainer->pParent->focusQueue.push_back(pcontainer);
		}

		GetRoot()->Stack();
	}

	ptreeFocus = this;
	Focus1();
}

void Container::SetFullscreen(bool toggle){
	if(toggle)
		flags |= FLAG_FULLSCREEN;
	else flags &= ~FLAG_FULLSCREEN;

	Fullscreen1();
	Translate();
}

Container * Container::GetNext(){
	if(flags & FLAG_FLOATING)
		return this;
	if(!pParent)
		return this; //root container
	if(!pnext)
		return pParent->pch;
	return pnext;
}

Container * Container::GetPrev(){
	if(flags & FLAG_FLOATING)
		return this;
	if(!pParent)
		return this; //root container
	Container *pcontainer = pParent->pch;
	if(pcontainer == this)
		for(; pcontainer->pnext; pcontainer = pcontainer->pnext);
	else for(; pcontainer->pnext != this; pcontainer = pcontainer->pnext);
	return pcontainer;
}

Container * Container::GetParent() const{
	return pParent;
}

Container * Container::GetFocus() const{
	return focusQueue.size() > 0?focusQueue.back():pch;
}

Container * Container::GetRoot(){
	Container *proot = this;
	for(Container *pcontainer = pParent; pcontainer; proot = pcontainer, pcontainer = pcontainer->pParent);
	return proot;
}

void Container::SetSize(glm::vec2 size1){
	if(flags & FLAG_FLOATING){
		p += 0.5f*(size-size1);
		e = size1;
		size = size1;
		if(pclient)
			pclient->UpdateTranslation();
		return;
	}
	glm::vec2 sizeSum = glm::vec2(1.0f)-size;
	size = size1;
	for(Container *pcontainer = pParent->pch; pcontainer; pcontainer = pcontainer->pnext)
		if(pcontainer != this)
			pcontainer->size *= (glm::vec2(1.0f)-size)/sizeSum;

	pParent->Translate();
}

void Container::MoveNext(){
	if(flags & FLAG_FLOATING)
		return;
	if(!pParent)
		return;
	Container *pcontainer = GetNext();
	Container *pPrev = GetPrev();
	Container *pPrevNext = pPrev->pnext;

	Container *pnext1 = pcontainer->pnext;
	pcontainer->pnext = pnext?this:0;
	pnext = pnext1 != this?pnext1:pcontainer;

	if(pParent->pch == this)
		pParent->pch = pcontainer;
	else
	if(pParent->pch == pcontainer)
		pParent->pch = this;
	
	if(pPrevNext != 0 && pPrev != pcontainer)
		pPrev->pnext = pcontainer;
	
	pParent->Stack();
	pParent->Translate();
}

void Container::MovePrev(){
	GetPrev()->MoveNext();
}

glm::vec2 Container::GetMinSize() const{
	//Return the minimum size requirement for the container
	glm::vec2 chSize(0.0f);
	for(Container *pcontainer = pch; pcontainer; chSize = glm::max(chSize,pcontainer->GetMinSize()), pcontainer = pcontainer->pnext);
	return glm::max(chSize,minSize);
}

void Container::TranslateRecursive(glm::vec2 posFullCanvas, glm::vec2 extFullCanvas, glm::vec2 p, glm::vec2 e){
	if(flags & FLAG_FULLSCREEN){
		p = glm::vec2(0.0f);
		e = glm::vec2(1.0f);
	}
	uint count = 0;

	glm::vec2 minSizeSum(0.0f);
	glm::vec2 maxMinSize(0.0f);
	for(Container *pcontainer = pch; pcontainer; ++count, pcontainer = pcontainer->pnext){
		glm::vec2 e1 = pcontainer->GetMinSize();
		minSizeSum += e1;
		maxMinSize = glm::max(e1,maxMinSize);
	}

	glm::vec2 position(p);
	switch(layout){
	default:
	case LAYOUT_VSPLIT:
		if(e.x-minSizeSum.x < 0.0f){
			//overlap required, everything has been minimized to the limit
			for(Container *pcontainer = pch; pcontainer; pcontainer = pcontainer->pnext){
				glm::vec2 e1 = glm::vec2(maxMinSize.x,e.y);
				glm::vec2 p1 = glm::vec2(position.x,p.y);//position;

				/*if(pcontainer->pnext){
					if(p1.x+e1.x+pcontainer->pnext->e1.x > 1.0f)
						e1.x = 1.0f;
					else position += ...;
				*/
					
				//----
				/*if(pcontainer->pnext){
					if(p1.x+e1.x+pcontainer->pnext->e1.x <= 1.0)
						pcontainer->pnext->p1.x = p1.x+e1.x;
					else{
						pcontainer->pnext->p1.x = position.x;
						//TODO: resize the previous row
					}
					position.x += maxMinSize.x-((float)count*maxMinSize.x-e.x)/(float)(count-1);
				}*/

				//position.x += e1.x-(minSizeSum.x-e.x)/(float)(count-1);
				position.x += e1.x-((float)count*maxMinSize.x-e.x)/(float)(count-1);
				pcontainer->TranslateRecursive(p1,e1,p1+pcontainer->canvasOffset,e1-pcontainer->canvasExtent);
			}

		}else{
			//optimize the containers, minimize interior overlap
			/*for(Container *pcontainer = pch; pcontainer; pcontainer = pcontainer->pnext){
				glm::vec2 e1 = glm::vec2(pcontainer->e1.x+(e.x-minSizeSum.x)/(float)count,e.y);
				glm::vec2 p1 = position;

				position.x += e1.x;
				pcontainer->TranslateRecursive(p1,e1,p1+pcontainer->canvasOffset,e1-pcontainer->canvasExtent);
			}*/
			for(Container *pcontainer = pch; pcontainer; pcontainer = pcontainer->pnext){
				float s = pcontainer->size.x*e.x;
				//glm::vec2 e1 = glm::vec2(std::max(s,pcontainer->GetMinSize().x),e.y);
				glm::vec2 e1 = glm::vec2(s,e.y);
				glm::vec2 p1 = position;

				position.x += s;
				pcontainer->TranslateRecursive(p1,e1,p1+pcontainer->canvasOffset,e1-pcontainer->canvasExtent);
			}
		}
		break;
	case LAYOUT_HSPLIT:
		if(e.y-minSizeSum.y < 0.0f){
			//overlap required, everything has been minimized to the limit
			for(Container *pcontainer = pch; pcontainer; pcontainer = pcontainer->pnext){
				glm::vec2 e1 = glm::vec2(e.x,maxMinSize.y);
				glm::vec2 p1 = glm::vec2(p.x,position.y);//position;

				position.y += e1.y-((float)count*maxMinSize.y-e.y)/(float)(count-1);
				pcontainer->TranslateRecursive(p1,e1,p1+pcontainer->canvasOffset,e1-pcontainer->canvasExtent);
			}

		}else{
			/*for(Container *pcontainer = pch; pcontainer; pcontainer = pcontainer->pnext){
				glm::vec2 e1 = glm::vec2(e.x,pcontainer->e1.y+(e.y-minSizeSum.y)/(float)count);
				glm::vec2 p1 = position;

				position.y += e1.y;
				pcontainer->TranslateRecursive(p1,e1,p1+pcontainer->canvasOffset,e1-pcontainer->canvasExtent);
			}*/
			for(Container *pcontainer = pch; pcontainer; pcontainer = pcontainer->pnext){
				float s = pcontainer->size.y*e.y;
				glm::vec2 e1 = glm::vec2(e.x,s);
				//glm::vec2 e1 = glm::vec2(e.x,std::max(s,pcontainer->GetMinSize().y));
				glm::vec2 p1 = position;

				position.y += s;
				pcontainer->TranslateRecursive(p1,e1,p1+pcontainer->canvasOffset,e1-pcontainer->canvasExtent);
			}
		}
		break;
	}

	this->posFullCanvas = posFullCanvas;
	this->extFullCanvas = extFullCanvas;
	glm::vec2 pcorr = glm::min(p,glm::vec2(0.0f));
	p -= pcorr; //make sure the containers stay within the screen limits
	glm::vec2 ecorr = glm::min(1.0f-(p+e),glm::vec2(0.0f));
	this->p = glm::max(p+ecorr,0.0f);
	this->e = glm::min(e,1.0f);
	if(pclient)
		pclient->UpdateTranslation();
}

void Container::Translate(){
	glm::vec2 size1 = GetMinSize();

	//Check the parent hierarchy, up to which point they won't have to be updated (condition overlappedSize <= e).
	//Find the first ancestor large enough to contain everything without modifiying its size.
	Container *pcontainer;
	for(pcontainer = pParent; pcontainer && glm::any(glm::lessThan(pcontainer->e,size1-1e-5f)); pcontainer = pcontainer->pParent);
	if(!pcontainer)
		pcontainer = this; //reached the root

	//pcontainer->posFullCanvas-canvasOffset,e,posFullCanvas,extFullCanvas
	//pcontainer->TranslateRecursive(pcontainer->p,pcontainer->e);
	pcontainer->TranslateRecursive(pcontainer->posFullCanvas,pcontainer->extFullCanvas,
		pcontainer->posFullCanvas+pcontainer->canvasOffset,pcontainer->extFullCanvas-pcontainer->canvasExtent);
}

void Container::StackRecursive(){
	stackQueue.clear();

	Container *pfocus = focusQueue.size() > 0?focusQueue.back():pch;
	if(!pfocus)
		return;

	for(Container *pcontainer = pch; pcontainer != pfocus; pcontainer = pcontainer->pnext)
		stackQueue.push_back(pcontainer);
	for(Container *pcontainer = pch->GetPrev(); pcontainer != pfocus; pcontainer = pcontainer->GetPrev())
		stackQueue.push_back(pcontainer);
	
	stackQueue.push_back(pfocus);

	for(Container *pcontainer = pch; pcontainer; pcontainer = pcontainer->pnext)
		pcontainer->StackRecursive();
}

void Container::Stack(){
	/*stackQueue.clear();
	Container *pfocus = focusQueue.size() > 0?focusQueue.back():pch;
	if(!pfocus)
		return;*/
	//stacking scheme 1: prefer to show left side of the window
	//problem: may hide some windows completely
	/*for(Container *pcontainer = pch; pcontainer != pfocus; pcontainer = pcontainer->pnext)
		stackQueue.push_back(pcontainer);
	Container *pstack = 0;
	for(Container *pcontainer = pch->GetPrev(); pcontainer != pfocus; pcontainer = pcontainer->GetPrev()){
		if((layout == LAYOUT_VSPLIT && pcontainer->p.x < pfocus->p.x+pfocus->e.x) ||
			(layout == LAYOUT_HSPLIT && pcontainer->p.y < pfocus->p.y+pfocus->e.y))
			pstack = pcontainer;
		if(pstack)
			stackQueue.push_back(pcontainer);
	}
	for(Container *pcontainer = pstack; pcontainer; pcontainer = pcontainer->pnext)
		stackQueue.push_back(pcontainer);*/

	//TODO: stacking scheme 3: stack from left to right, except that all the windows before the
	//one that would be completely visible after focus (see 1), will be more tightly spaced to
	//avoid waste. This needs modifications in Translate(), otherwise same as 2.

	//1. check if any of the windows would be visible completely after the focus
	//2. stack the windows normally until the next after focus
	//3. for a length of one stack spacing, tighly place all the windows before the one found in 1.
	//-if no such window was found, continue stacking according to scheme 3.
	//4. after this, stack the remaining windows according to 2. The first of the remaining should
	//be in the same place as it would be in normal left to right stacking.

	//stacking scheme 3: for every window always keep at least small part visible
	//problem(?): may waste screen space if there are many windows with left-aligned text (terminals)
	//-----------
	/*for(Container *pcontainer = pch; pcontainer != pfocus; pcontainer = pcontainer->pnext)
		stackQueue.push_back(pcontainer);
	for(Container *pcontainer = pch->GetPrev(); pcontainer != pfocus; pcontainer = pcontainer->GetPrev())
		stackQueue.push_back(pcontainer);
	
	stackQueue.push_back(pfocus);*/
	//-----------
	/*for(Container *pcontainer = pch; pcontainer; pcontainer = pcontainer->pnext)
		stackQueue.push_back(pcontainer);

	if(focusQueue.size() == 0)
		return;
	Container *pfocus = focusQueue.back();
	auto m = std::find(stackQueue.begin(),stackQueue.end(),pfocus);
	std::iter_swap(m,stackQueue.end()-1);*/

	/*std::sort(stackQueue.begin(),stackQueue.end(),[&](Container *pa, Container *pb)->bool{
		return pa != pfocus && (pb == pfocus || pb->p[layout] > pfocus->p[layout]+pfocus->e[layout] || pb->p[layout]+pb->e[layout] < pfocus->p[layout]);
	});*/
	/*std::sort(stackQueue.begin(),stackQueue.end(),[&](Container *pa, Container *pb)->bool{
		return pa != pfocus && (pb == pfocus || pb->p[layout] > pfocus->p[layout]+pfocus->e[layout] || pb->p[layout]+pb->e[layout] < pfocus->p[layout]);
	});*/
	StackRecursive();
	Stack1();
}

void Container::SetLayout(LAYOUT layout){
	this->layout = layout;
	Translate();
}

WManager::Container *Container::ptreeFocus = 0; //initially set to root container as soon as it's created
std::deque<std::pair<WManager::Container *, struct timespec>> Container::tiledFocusQueue;
std::deque<WManager::Container *> Container::floatFocusQueue;

}

