#include "main.h"
#include "container.h"
#include "backend.h"
#include <cstdlib>
#include <algorithm>

#include <xcb/xcb.h>
#include <xcb/randr.h>
#include <xcb/xcb_keysyms.h>
#include <xcb/xcb_atom.h>
#include <xcb/xcb_icccm.h>
#include <xcb/xcb_util.h>

#include <X11/keysym.h>

namespace Backend{

/*inline uint GetModMask1(uint keysym, xcb_key_symbols_t *psymbols, xcb_get_modifier_mapping_reply_t *pmodmapReply){
	xcb_keycode_t *pcodes = xcb_key_symbols_get_keycode(psymbols,keysym);
	if(!pcodes)
		return 0;

	xcb_keycode_t *pmodmap = xcb_get_modifier_mapping_keycodes(pmodmapReply);

	for(uint m = 0; m < 8; ++m)
		for(uint i = 0; i < pmodmapReply->keycodes_per_modifier; ++i){
			xcb_keycode_t mcode = pmodmap[m*pmodmapReply->keycodes_per_modifier+i];
			for(xcb_keycode_t *pcode = pcodes; *pcode; ++pcode){
				if(*pcode != mcode)
					continue;

				free(pcodes);
				return (1<<m);
			}
		}

	return 0;
}

static uint GetModMask(xcb_connection_t *pcon, uint keysym, xcb_key_symbols_t *psymbols){
	xcb_flush(pcon);

	xcb_get_modifier_mapping_cookie_t cookie = xcb_get_modifier_mapping(pcon);
	xcb_get_modifier_mapping_reply_t *pmodmapReply = xcb_get_modifier_mapping_reply(pcon,cookie,0);
	if(!pmodmapReply)
		return 0;

	uint mask = GetModMask1(keysym,psymbols,pmodmapReply);
	free(pmodmapReply);

	return mask;
}*/

static xcb_keycode_t SymbolToKeycode(xcb_keysym_t symbol, xcb_key_symbols_t *psymbols){
	xcb_keycode_t *pkey = xcb_key_symbols_get_keycode(psymbols,symbol);
	if(!pkey)
		return 0;

	xcb_keycode_t key = *pkey;
	free(pkey);

	return key;
}

BackendEvent::BackendEvent(){
	//
}

BackendEvent::~BackendEvent(){
	//
}

BackendKeyBinder::BackendKeyBinder(){
	//
}

BackendKeyBinder::~BackendKeyBinder(){
	//
}

BackendInterface::BackendInterface(){
	//
}

BackendInterface::~BackendInterface(){
	//
}

X11Event::X11Event(xcb_generic_event_t *_pevent, const X11Backend *_pbackend) : BackendEvent(), pevent(_pevent), pbackend(_pbackend){
	//
}

X11Event::~X11Event(){
	//
}

X11KeyBinder::X11KeyBinder(xcb_key_symbols_t *_psymbols, X11Backend *_pbackend) : BackendKeyBinder(), psymbols(_psymbols), pbackend(_pbackend){
	//
}

X11KeyBinder::~X11KeyBinder(){
	//
}

void X11KeyBinder::BindKey(uint symbol, uint mask, uint keyId){
	xcb_keycode_t keycode = SymbolToKeycode(symbol,psymbols);
	xcb_grab_key(pbackend->pcon,1,pbackend->pscr->root,mask,keycode,
		XCB_GRAB_MODE_ASYNC,XCB_GRAB_MODE_ASYNC);
	X11Backend::KeyBinding binding;
	binding.keycode = keycode;
	binding.mask = mask;
	binding.keyId = keyId;
	pbackend->keycodes.push_back(binding);
}

X11Client::X11Client(WManager::Container *pcontainer, const CreateInfo *pcreateInfo) : Client(pcontainer), window(pcreateInfo->window), pbackend(pcreateInfo->pbackend){
	uint values[1] = {XCB_EVENT_MASK_ENTER_WINDOW|XCB_EVENT_MASK_EXPOSURE};
	xcb_change_window_attributes(pbackend->pcon,window,XCB_CW_EVENT_MASK,values);

	UpdateTranslation();

	xcb_map_window(pbackend->pcon,window);
}

X11Client::~X11Client(){
	//
}

void X11Client::UpdateTranslation(){
	glm::vec4 screen(pbackend->pscr->width_in_pixels,pbackend->pscr->height_in_pixels,pbackend->pscr->width_in_pixels,pbackend->pscr->height_in_pixels);
	glm::vec4 coord = glm::vec4(pcontainer->p,pcontainer->e)*screen;
	rect = (WManager::Rectangle){coord.x,coord.y,coord.z,coord.w};

	uint values[4] = {rect.x,rect.y,rect.w,rect.h};
	xcb_configure_window(pbackend->pcon,window,XCB_CONFIG_WINDOW_X|XCB_CONFIG_WINDOW_Y|XCB_CONFIG_WINDOW_WIDTH|XCB_CONFIG_WINDOW_HEIGHT,values);
	xcb_set_input_focus(pbackend->pcon,XCB_INPUT_FOCUS_POINTER_ROOT,window,XCB_CURRENT_TIME);

	xcb_flush(pbackend->pcon);

	AdjustSurface1(); //virtual function gets called only if the compositor client class has been initialized
}

X11Backend::X11Backend(){
	//
}

X11Backend::~X11Backend(){
	//
}

bool X11Backend::QueryExtension(const char *pname, sint *pfirstEvent, sint *pfirstError) const{
	xcb_generic_error_t *perr;
	xcb_query_extension_cookie_t queryExtCookie = xcb_query_extension(pcon,strlen(pname),pname);
	xcb_query_extension_reply_t *pqueryExtReply = xcb_query_extension_reply(pcon,queryExtCookie,&perr);
	if(!pqueryExtReply || perr)
		return false;
	*pfirstEvent = pqueryExtReply->first_event;
	*pfirstError = pqueryExtReply->first_error;
	free(pqueryExtReply);
	return true;
}

Default::Default() : X11Backend(){
	//
}

Default::~Default(){
	//sigprocmask(SIG_UNBLOCK,&signals,0);

	//cleanup
	xcb_set_input_focus(pcon,XCB_NONE,XCB_INPUT_FOCUS_POINTER_ROOT,XCB_CURRENT_TIME);
	xcb_disconnect(pcon);
	xcb_flush(pcon);
}

void Default::Start(){
	sint scount;
	pcon = xcb_connect(0,&scount);
	if(xcb_connection_has_error(pcon))
		throw Exception("Failed to connect to X server.\n");

	const xcb_setup_t *psetup = xcb_get_setup(pcon);
	xcb_screen_iterator_t sm = xcb_setup_roots_iterator(psetup);
	for(sint i = 0; i < scount; ++i)
		xcb_screen_next(&sm);

	pscr = sm.data;
	if(!pscr)
		throw Exception("Screen unavailable.\n");

	DebugPrintf(stdout,"Screen size: %ux%u\n",pscr->width_in_pixels,pscr->height_in_pixels);

	xcb_key_symbols_t *psymbols = xcb_key_symbols_alloc(pcon);

	//xcb_keycode_t kk = SymbolToKeycode(XK_Return,psymbols);
	//xcb_grab_key(pcon,1,pscr->root,XCB_MOD_MASK_1,kk,
		//XCB_GRAB_MODE_ASYNC,XCB_GRAB_MODE_ASYNC);
	//xcb_grab_key(pcon,1,pscr->root,XCB_MOD_MASK_SHIFT|XCB_MOD_MASK_1,kk,
		//XCB_GRAB_MODE_ASYNC,XCB_GRAB_MODE_ASYNC);
	exitKeycode = SymbolToKeycode(XK_Q,psymbols);
	xcb_grab_key(pcon,1,pscr->root,XCB_MOD_MASK_1,exitKeycode,
		XCB_GRAB_MODE_ASYNC,XCB_GRAB_MODE_ASYNC);
	launchKeycode = SymbolToKeycode(XK_space,psymbols);
	xcb_grab_key(pcon,1,pscr->root,XCB_MOD_MASK_1,launchKeycode,
		XCB_GRAB_MODE_ASYNC,XCB_GRAB_MODE_ASYNC);
	
	X11KeyBinder binder(psymbols,this);
	DefineBindings(&binder); //TODO: an interface to define bindings? This is passed to config before calling SetupKeys

	xcb_flush(pcon);

	xcb_key_symbols_free(psymbols);

	uint mask = XCB_CW_EVENT_MASK;
	uint values[1] = {XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT
		|XCB_EVENT_MASK_STRUCTURE_NOTIFY
		|XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY
		|XCB_EVENT_MASK_EXPOSURE};

	xcb_void_cookie_t cookie = xcb_change_window_attributes_checked(pcon,pscr->root,mask,values);
	xcb_generic_error_t *perr = xcb_request_check(pcon,cookie);

	xcb_flush(pcon);

	window = pscr->root;

	if(perr != 0){
		snprintf(Exception::buffer,sizeof(Exception::buffer),"Substructure redirection failed (%d). WM already present.\n",perr->error_code);
		throw Exception();
	}
}

/*sint Default::GetEventFileDescriptor(){
	sint fd = xcb_get_file_descriptor(pcon);
	return fd;
}*/

bool Default::HandleEvent(){
	//xcb_generic_event_t *pevent = xcb_poll_for_event(pcon);
	//for(xcb_generic_event_t *pevent = xcb_poll_for_event(pcon); pevent; pevent = xcb_poll_for_event(pcon)){
	for(xcb_generic_event_t *pevent = xcb_wait_for_event(pcon); pevent; pevent = xcb_poll_for_event(pcon)){
		if(!pevent){
			if(xcb_connection_has_error(pcon)){
				DebugPrintf(stderr,"X server connection lost\n");
				return false;
			}

			return true;
		}

		//switch(pevent->response_type & ~0x80){
		switch(pevent->response_type & 0x7f){
		case XCB_CONFIGURE_REQUEST:{
			xcb_configure_request_event_t *pev = (xcb_configure_request_event_t*)pevent;
			struct{
				uint16_t m;
				uint32_t v;
			} maskm[] = {
				{XCB_CONFIG_WINDOW_X,pev->x},
				{XCB_CONFIG_WINDOW_Y,pev->y},
				{XCB_CONFIG_WINDOW_WIDTH,pev->width},
				{XCB_CONFIG_WINDOW_HEIGHT,pev->height},
				{XCB_CONFIG_WINDOW_BORDER_WIDTH,pev->border_width},
				{XCB_CONFIG_WINDOW_SIBLING,pev->sibling},
				{XCB_CONFIG_WINDOW_STACK_MODE,pev->stack_mode}
			};
			uint values[7];
			uint mask = 0;
			for(uint j = 0, mi = 0; j < 7; ++j)
				if(maskm[j].m & pev->value_mask){
					mask |= maskm[j].m;
					values[mi++] = maskm[j].v;
				}

			xcb_configure_window(pcon,pev->window,mask,values);
			DebugPrintf(stdout,"configure request: %u, %u, %u, %u\n",pev->x,pev->y,pev->width,pev->height);
			}
			break;
		case XCB_MAP_REQUEST:{
			xcb_map_request_event_t *pev = (xcb_map_request_event_t*)pevent;
			
			//check if window already exists
			if(std::find_if(clients.begin(),clients.end(),[&](X11Client *pclient)->bool{
				return pclient->window == pev->window;
			}) != clients.end())
				break;

			X11Client::CreateInfo createInfo;
			createInfo.window = pev->window;
			createInfo.pbackend = this;
			X11Client *pclient = SetupClient(&createInfo);
			clients.push_back(pclient);

			DebugPrintf(stdout,"map request, %u\n",pev->window);
			}
			break;
		case XCB_CLIENT_MESSAGE:{
			//xcb_client_message_event_t *pev = (xcb_client_message_event_t*)pevent;
			//if(pev->data.data32[0] == wmD
			//_NET_WM_STATE
			//_NET_WM_STATE_FULLSCREEN, _NET_WM_STATE_DEMANDS_ATTENTION
			}
			break;
		case XCB_KEY_PRESS:{
			xcb_key_press_event_t *pev = (xcb_key_press_event_t*)pevent;
			if(pev->state & XCB_MOD_MASK_1 && pev->detail == exitKeycode){
				free(pevent);
				return false;
			//
			}else
			if(pev->state & XCB_MOD_MASK_1 && pev->detail == launchKeycode){
				//create test client
				DebugPrintf(stdout,"Spawning test client...\n");
				system("termite & >/tmp/chamferwm-test");
			}else
			for(KeyBinding &binding : keycodes){
				if(pev->state == binding.mask && pev->detail == binding.keycode){
					KeyPress(binding.keyId,true);
					break;
				}
			}
			}
			break;
		case XCB_KEY_RELEASE:{
			xcb_key_release_event_t *pev = (xcb_key_release_event_t*)pevent;
			for(KeyBinding &binding : keycodes){
				if(pev->state == binding.mask && pev->detail == binding.keycode){
					KeyPress(binding.keyId,false);
					break;
				}
			}
			}
			break;
		case XCB_CONFIGURE_NOTIFY:{
			xcb_configure_notify_event_t *pev = (xcb_configure_notify_event_t*)pevent;
			/*if(pev->window != pscr->root)
				break;

			pscr->width_in_pixels = pev->width;
			pscr->height_in_pixels = pev->height;*/
			DebugPrintf(stdout,"configure to %ux%u\n",pev->width,pev->height);
			}
			break;
		case XCB_MAPPING_NOTIFY:
			DebugPrintf(stdout,"mapping\n");
			break;
		case XCB_UNMAP_NOTIFY:{
			//
			}
			break;
		case XCB_DESTROY_NOTIFY:{
			DebugPrintf(stdout,"destroy notify (%lu)\n",clients.size());
			xcb_destroy_notify_event_t *pev = (xcb_destroy_notify_event_t*)pevent;

			auto m = std::find_if(clients.begin(),clients.end(),[&](X11Client *pclient)->bool{
				return pclient->window == pev->window;
			});
			if(m == clients.end())
				break;
			//delete *m;
			DestroyClient(*m);
			
			std::iter_swap(m,clients.end()-1);
			clients.pop_back();
			}
			break;
		case 0:
			DebugPrintf(stdout,"Invalid event\n");
			break;
		default:
			DebugPrintf(stdout,"default event: %u\n",pevent->response_type & 0x7f);

			X11Event event11(pevent,this);
			EventNotify(&event11);
			break;
		}
		
		free(pevent);
		xcb_flush(pcon);
	}

	return true;
}

X11Client * Default::FindClient(xcb_window_t window) const{
	auto m = std::find_if(clients.begin(),clients.end(),[&](X11Client *pclient)->bool{
		/*printf("%x, %x\n",pclient->window,window);
		xcb_query_tree_cookie_t queryTreeCookie = xcb_query_tree(pcon,pclient->window);
		xcb_query_tree_reply_t *pqueryTreeReply = xcb_query_tree_reply(pcon,queryTreeCookie,0);
		if(!pqueryTreeReply)
			DebugPrintf(stdout,"failed to query\n");
		xcb_window_t *pchs = xcb_query_tree_children(pqueryTreeReply);
		for(uint i = 0; i < pqueryTreeReply->children_len; ++i)
			printf("-- %x (ch)\n",pchs[i]);
		free(pqueryTreeReply);*/
		return pclient->window == window;
	});
	if(m == clients.end())
		return 0;
	return *m;
}

/*DebugClient::DebugClient(WManager::Rectangle _rect, const X11Backend *_pbackend) : pbackend(_pbackend){
	rect = _rect;
}*/

DebugClient::DebugClient(WManager::Container *pcontainer, const DebugClient::CreateInfo *pcreateInfo) : Client(pcontainer), pbackend(pcreateInfo->pbackend){
	UpdateTranslation();
}

DebugClient::~DebugClient(){
	//
}

void DebugClient::UpdateTranslation(){
	printf("UpdateTranslation()\n");
	xcb_get_geometry_cookie_t geometryCookie = xcb_get_geometry(pbackend->pcon,pbackend->window);
	xcb_get_geometry_reply_t *pgeometryReply = xcb_get_geometry_reply(pbackend->pcon,geometryCookie,0);
	if(!pgeometryReply)
		throw("Invalid geometry size - unable to retrieve.");
	glm::uvec2 se(pgeometryReply->width,pgeometryReply->height);
	free(pgeometryReply);

	//glm::vec4 screen(pbackend->pscr->width_in_pixels,pbackend->pscr->height_in_pixels,pbackend->pscr->width_in_pixels,pbackend->pscr->height_in_pixels);
	glm::vec4 screen(se.x,se.y,se.x,se.y);
	glm::vec4 coord = glm::vec4(pcontainer->p,pcontainer->e)*screen;
	rect = (WManager::Rectangle){coord.x,coord.y,coord.z,coord.w};

	AdjustSurface1();
}

Debug::Debug() : X11Backend(){
	//
}

Debug::~Debug(){
	//
	xcb_destroy_window(pcon,window);
	xcb_flush(pcon);
}

void Debug::Start(){
	sint scount;
	pcon = xcb_connect(0,&scount);
	if(xcb_connection_has_error(pcon))
		throw Exception("Failed to connect to X server.\n");

	const xcb_setup_t *psetup = xcb_get_setup(pcon);
	xcb_screen_iterator_t sm = xcb_setup_roots_iterator(psetup);
	for(sint i = 0; i < scount; ++i)
		xcb_screen_next(&sm);

	pscr = sm.data;
	if(!pscr)
		throw Exception("Screen unavailable.\n");

	DebugPrintf(stdout,"Screen size: %ux%u\n",pscr->width_in_pixels,pscr->height_in_pixels);

	xcb_key_symbols_t *psymbols = xcb_key_symbols_alloc(pcon);

	exitKeycode = SymbolToKeycode(XK_Q,psymbols);
	xcb_grab_key(pcon,1,pscr->root,XCB_MOD_MASK_1,exitKeycode,
		XCB_GRAB_MODE_ASYNC,XCB_GRAB_MODE_ASYNC);
	launchKeycode = SymbolToKeycode(XK_space,psymbols);
	xcb_grab_key(pcon,1,pscr->root,0,launchKeycode,
		XCB_GRAB_MODE_ASYNC,XCB_GRAB_MODE_ASYNC);
	closeKeycode = SymbolToKeycode(XK_X,psymbols);
	xcb_grab_key(pcon,1,pscr->root,0,closeKeycode,
		XCB_GRAB_MODE_ASYNC,XCB_GRAB_MODE_ASYNC);
	
	X11KeyBinder binder(psymbols,this);
	DefineBindings(&binder);

	xcb_flush(pcon);

	window = xcb_generate_id(pcon);

	uint mask = XCB_CW_EVENT_MASK;
	uint values[1] = {XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT
		|XCB_EVENT_MASK_STRUCTURE_NOTIFY
		|XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY};
	
	xcb_create_window(pcon,XCB_COPY_FROM_PARENT,window,pscr->root,100,100,800,600,0,XCB_WINDOW_CLASS_INPUT_OUTPUT,pscr->root_visual,mask,values);
	const char title[] = "xwm compositor debug mode";
	xcb_change_property(pcon,XCB_PROP_MODE_REPLACE,window,XCB_ATOM_WM_NAME,XCB_ATOM_STRING,8,strlen(title),title);
	xcb_map_window(pcon,window);

	xcb_flush(pcon);

	xcb_key_symbols_free(psymbols);
}

/*sint Debug::GetEventFileDescriptor(){
	sint fd = xcb_get_file_descriptor(pcon);
	return fd;
}*/

bool Debug::HandleEvent(){
	//xcb_generic_event_t *pevent = xcb_poll_for_event(pcon);
	for(xcb_generic_event_t *pevent = xcb_poll_for_event(pcon); pevent; pevent = xcb_poll_for_event(pcon)){
		if(!pevent){
			if(xcb_connection_has_error(pcon)){
				DebugPrintf(stderr,"X server connection lost\n");
				return false;
			}

			return true;
		}

		//switch(pevent->response_type & ~0x80){
		switch(pevent->response_type & 0x7f){
		/*case XCB_EXPOSE:{
			printf("expose\n");
			}
			break;*/
		case XCB_CLIENT_MESSAGE:{
			DebugPrintf(stdout,"message\n");
			//xcb_client_message_event_t *pev = (xcb_client_message_event_t*)pevent;
			//if(pev->data.data32[0] == wmD
			}
			break;
		case XCB_KEY_PRESS:{
			xcb_key_press_event_t *pev = (xcb_key_press_event_t*)pevent;
			//DebugPrintf(stdout,"key: %u(%u), state: %x & %x\n",pev->detail,exitKeycode,pev->state,XCB_MOD_MASK_1);
			if(pev->state & XCB_MOD_MASK_1 && pev->detail == exitKeycode){
				free(pevent);
				return false;
			}else
			if(pev->detail == launchKeycode){
				//create test client
				DebugClient::CreateInfo createInfo = {};
				createInfo.pbackend = this;
				DebugClient *pclient = SetupClient(&createInfo);
				clients.push_back(pclient);
			}else
			if(pev->detail == closeKeycode){
				if(clients.size() == 0)
					break;
				DebugClient *pclient = clients.back();
				DestroyClient(pclient);
				
				clients.pop_back();
			}else
			for(KeyBinding &binding : keycodes){
				if(pev->state == binding.mask && pev->detail == binding.keycode){
					KeyPress(binding.keyId,true);
					break;
				}
			}
			}
			break;
		case XCB_KEY_RELEASE:{
			xcb_key_release_event_t *pev = (xcb_key_release_event_t*)pevent;
			for(KeyBinding &binding : keycodes){
				if(pev->state == binding.mask && pev->detail == binding.keycode){
					KeyPress(binding.keyId,false);
					break;
				}
			}
			}
			break;
		default:{
			DebugPrintf(stdout,"default event: %u\n",pevent->response_type & 0x7f);
			//TODO: find client here
			//X11Event event11(pevent); //DebugEvent
			//EventNotify(&event11);
			}
			break;
		}

		free(pevent);
		xcb_flush(pcon);
	}

	return true;
}

X11Client * Debug::FindClient(xcb_window_t window) const{
	return 0;
}

};

