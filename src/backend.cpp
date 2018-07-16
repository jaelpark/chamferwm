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
#include <xcb/xproto.h>
#include <xcb/xcb_util.h>
#include <xcb/composite.h>

#include <X11/keysym.h>

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

/*static xcb_atom_t GetAtom(xcb_connection_t *pcon, const char *pname){
	xcb_intern_atom_cookie_t atomCookie = xcb_intern_atom(pcon,0,strlen(pname),pname);
	xcb_intern_atom_reply_t *patomReply = xcb_intern_atom_reply(pcon,atomCookie,0);
	if(!patomReply)
		return 0;
	xcb_atom_t atom = patomReply->atom;
	free(patomReply);

	return atom;
}*/

namespace Backend{

BackendInterface::BackendInterface(){
	//
}

BackendInterface::~BackendInterface(){
	//
}

X11Backend::X11Backend(){
	//
}

X11Backend::~X11Backend(){
	//
}

Default::Default() : X11Backend(){
	//
}

Default::~Default(){
	xcb_xfixes_set_window_shape_region(pcon,overlay,XCB_SHAPE_SK_BOUNDING,0,0,XCB_XFIXES_REGION_NONE);
	xcb_xfixes_set_window_shape_region(pcon,overlay,XCB_SHAPE_SK_INPUT,0,0,XCB_XFIXES_REGION_NONE);

	xcb_composite_release_overlay_window(pcon,overlay);

	//sigprocmask(SIG_UNBLOCK,&signals,0);

	//cleanup
	xcb_set_input_focus(pcon,XCB_NONE,XCB_INPUT_FOCUS_POINTER_ROOT,XCB_CURRENT_TIME);
	xcb_flush(pcon);
	xcb_disconnect(pcon);
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
	DefineBindings();

	xcb_flush(pcon);

	xcb_key_symbols_free(psymbols);

	xcb_generic_error_t *perr;

	uint mask = XCB_CW_EVENT_MASK;
	uint values[1] = {XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT
		|XCB_EVENT_MASK_STRUCTURE_NOTIFY
		|XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY};

	xcb_void_cookie_t cookie = xcb_change_window_attributes_checked(pcon,pscr->root,mask,values);
	perr = xcb_request_check(pcon,cookie);

	xcb_flush(pcon);

	if(perr != 0){
		snprintf(Exception::buffer,sizeof(Exception::buffer),"Substructure redirection failed (%d). WM already present.\n",perr->error_code);
		throw Exception();
	}

	//compositor
	xcb_composite_query_version_cookie_t compCookie = xcb_composite_query_version(pcon,XCB_COMPOSITE_MAJOR_VERSION,XCB_COMPOSITE_MINOR_VERSION);
	xcb_composite_query_version_reply_t *pcompReply = xcb_composite_query_version_reply(pcon,compCookie,0);
	if(!pcompReply)
		throw Exception("XCompositor unavailable.\n");
	DebugPrintf(stdout,"XComposite %u.%u\n",pcompReply->major_version,pcompReply->minor_version);
	free(pcompReply);

	//overlay
	xcb_composite_get_overlay_window_cookie_t overlayCookie = xcb_composite_get_overlay_window(pcon,pscr->root);
	xcb_composite_get_overlay_window_reply_t *poverlayReply = xcb_composite_get_overlay_window_reply(pcon,overlayCookie,0);
	if(!poverlayReply)
		throw Exception("Unable to get overlay window.\n");
	overlay = poverlayReply->overlay_win;
	free(poverlayReply);

	//xfixes
	xcb_xfixes_query_version_cookie_t fixesCookie = xcb_xfixes_query_version(pcon,XCB_XFIXES_MAJOR_VERSION,XCB_XFIXES_MINOR_VERSION);
	xcb_xfixes_query_version_reply_t *pfixesReply = xcb_xfixes_query_version_reply(pcon,fixesCookie,0);
	if(!pfixesReply)
		throw Exception("XFixes unavailable.\n");
	DebugPrintf(stdout,"XFixes %u.%u\n",pfixesReply->major_version,pfixesReply->minor_version);

	//allow overlay input passthrough
	xcb_xfixes_region_t region = xcb_generate_id(pcon);
	xcb_void_cookie_t regionCookie = xcb_xfixes_create_region_checked(pcon,region,0,0);
	perr = xcb_request_check(pcon,regionCookie);
	if(perr != 0){
		snprintf(Exception::buffer,sizeof(Exception::buffer),"Unable to create overlay region (%d).",perr->error_code);
		throw Exception();
	}
	xcb_discard_reply(pcon,regionCookie.sequence);
	xcb_xfixes_set_window_shape_region(pcon,overlay,XCB_SHAPE_SK_BOUNDING,0,0,XCB_XFIXES_REGION_NONE);
	xcb_xfixes_set_window_shape_region(pcon,overlay,XCB_SHAPE_SK_INPUT,0,0,region);
	xcb_xfixes_destroy_region(pcon,region);

	xcb_flush(pcon);

	//const xcb_atom_t atomWmState = GetAtom(pcon,"WM_STATE");
}

sint Default::GetEventFileDescriptor(){
	sint fd = xcb_get_file_descriptor(pcon);
	return fd;
}

bool Default::HandleEvent(){
	xcb_generic_event_t *pevent = xcb_poll_for_event(pcon);
	if(!pevent){
		if(xcb_connection_has_error(pcon)){
			DebugPrintf(stderr,"X server connection lost\n");
			return false;
		}
	}

	//switch(pevent->response_type & ~0x80){
	switch(pevent->response_type & 0x7F){
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
		xcb_flush(pcon);
		DebugPrintf(stdout,"configure request: %u, %u, %u, %u\n",pev->x,pev->y,pev->width,pev->height);
		}
		break;
	case XCB_MAP_REQUEST:{
		xcb_map_request_event_t *pev = (xcb_map_request_event_t*)pevent;

		uint values[1] = {XCB_EVENT_MASK_ENTER_WINDOW};
		xcb_change_window_attributes_checked(pcon,pev->window,XCB_CW_EVENT_MASK,values);
		xcb_map_window(pcon,pev->window);

		//uint data[] = {XCB_ICCCM_WM_STATE_NORMAL,XCB_NONE};
		//xcb_change_property(pcon,XCB_PROP_MODE_REPLACE,pev->window,atomWmState,atomWmState,32,2,data);
		xcb_flush(pcon);
		DebugPrintf(stdout,"map request\n");
		}
		break;
	case XCB_KEY_PRESS:{
		xcb_key_press_event_t *pev = (xcb_key_press_event_t*)pevent;
		xcb_flush(pcon);
		DebugPrintf(stdout,"key: %u(%u), state: %x & %x\n",pev->detail,exitKeycode,pev->state,XCB_MOD_MASK_1);

		if(pev->state & XCB_MOD_MASK_1 && pev->detail == exitKeycode)
			return false;
		//
		}
		break;
	case XCB_KEY_RELEASE:{
		xcb_key_release_event_t *pev = (xcb_key_release_event_t*)pevent;
		//xcb_send_event(pcon,false,XCB_SEND_EVENT_DEST_ITEM_FOCUS,XCB_EVENT_MASK_NO_EVENT,(const char*)pev);
		xcb_flush(pcon);
		DebugPrintf(stdout,"release\n");
		}
		break;
	case XCB_CONFIGURE_NOTIFY:{
		xcb_configure_notify_event_t *pev = (xcb_configure_notify_event_t*)pevent;
		if(pev->window != pscr->root)
			break;

		pscr->width_in_pixels = pev->width;
		pscr->height_in_pixels = pev->height;
		DebugPrintf(stdout,"configure\n");
		}
		break;
	case XCB_MAPPING_NOTIFY:
		DebugPrintf(stdout,"mapping\n");
		break;
	case XCB_UNMAP_NOTIFY:
		DebugPrintf(stdout,"unmapping\n");
		break;
	default:
		DebugPrintf(stdout,"default event\n");
		break;
	}

	free(pevent);
	xcb_flush(pcon);

	return true;
}

FakeClient::FakeClient() : WManager::Client(){
	//
}

FakeClient::~FakeClient(){
	//
}

Fake::Fake() : X11Backend(){
	//
}

Fake::~Fake(){
	//
}

void Fake::Start(){
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
	DefineBindings();

	xcb_flush(pcon);

	xcb_key_symbols_free(psymbols);

	overlay = xcb_generate_id(pcon);

	uint mask = XCB_CW_EVENT_MASK;
	uint values[1] = {XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT
		|XCB_EVENT_MASK_STRUCTURE_NOTIFY
		|XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY};
	
	xcb_create_window(pcon,XCB_COPY_FROM_PARENT,overlay,pscr->root,100,100,800,600,0,XCB_WINDOW_CLASS_INPUT_OUTPUT,pscr->root_visual,mask,values);
	const char title[] = "xwm compositor debug mode";
	xcb_change_property(pcon,XCB_PROP_MODE_REPLACE,overlay,XCB_ATOM_WM_NAME,XCB_ATOM_STRING,8,strlen(title),title);
	
	xcb_map_window(pcon,overlay);
	xcb_flush(pcon);
}

sint Fake::GetEventFileDescriptor(){
	sint fd = xcb_get_file_descriptor(pcon);
	return fd;
}

bool Fake::HandleEvent(){
	xcb_generic_event_t *pevent = xcb_poll_for_event(pcon);
	if(!pevent){
		if(xcb_connection_has_error(pcon)){
			DebugPrintf(stderr,"X server connection lost\n");
			return false;
		}
		return true;
	}

	//switch(pevent->response_type & ~0x80){
	switch(pevent->response_type & 0x7F){
	//case XCB_CONFIGURE_REQUEST:{
	case XCB_CLIENT_MESSAGE:{
		//xcb_client_message_event_t *pev = (xcb_client_message_event_t*)pevent;
		//if(pev->data.data32[0] == wmD
	}
	break;
	case XCB_KEY_PRESS:{
		xcb_key_press_event_t *pev = (xcb_key_press_event_t*)pevent;
		xcb_flush(pcon);
		DebugPrintf(stdout,"key: %u(%u), state: %x & %x\n",pev->detail,exitKeycode,pev->state,XCB_MOD_MASK_1);

		if(pev->state & XCB_MOD_MASK_1 && pev->detail == exitKeycode)
			return false;
		//
		}
		break;
	}

	return true;
}

};

