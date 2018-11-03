
import chamfer
from enum import Enum,auto

import psutil
from Xlib.keysymdef import latin1,miscellany,xf86

import sys
try:
	import pulsectl
except ModuleNotFoundError:
	print("No pulsectl module.");

class Key(Enum):
	FOCUS_RIGHT = auto()
	FOCUS_LEFT = auto()
	FOCUS_UP = auto()
	FOCUS_DOWN = auto()
	FOCUS_PARENT = auto()
	FOCUS_CHILD = auto()

	FOCUS_PARENT_RIGHT = auto()
	FOCUS_PARENT_LEFT = auto()

	YANK_CONTAINER = auto()
	PASTE_CONTAINER = auto()

	MOVE_LEFT = auto()
	MOVE_RIGHT = auto()

	LAYOUT = auto()
	MAXIMIZE = auto()
	SPLIT_V = auto()

	KILL = auto()
	LAUNCH_TERMINAL = auto()
	LAUNCH_BROWSER = auto()
	LAUNCH_BROWSER_PRIVATE = auto()

	AUDIO_VOLUME_UP = auto()
	AUDIO_VOLUME_DOWN = auto()

	MONITOR_BRIGHTNESS_UP = auto()
	MONITOR_BRIGHTNESS_DOWN = auto()

class Container(chamfer.Container):
	def OnSetup(self):
		self.borderWidth = (0.015,0.015);
		self.minSize = (0.4,0.3);

		self.splitArmed = False;

	def OnParent(self):
		focus = chamfer.GetFocus();
		if hasattr(focus,'splitArmed') and focus.splitArmed:
			focus.splitArmed = False;
			return focus;

		parent = focus.GetParent();
		if parent is None:
			return focus; #root container

		return parent;
	
	def OnCreate(self):
		print("created client, {} ({})".format(self.wm_name,self.wm_class));
		self.Focus();
	
	def OnPropertyChange(self, propId):
		print(self.wm_name);
		
class Backend(chamfer.Backend):
	def OnSetupKeys(self, binder, debug):
		print("setting up keys...");

		if not debug:
			binder.BindKey(ord('l'),chamfer.MOD_MASK_1,Key.FOCUS_RIGHT.value);
			binder.BindKey(ord('l'),chamfer.MOD_MASK_1|chamfer.MOD_MASK_SHIFT,Key.MOVE_RIGHT.value);
			binder.BindKey(ord('h'),chamfer.MOD_MASK_1,Key.FOCUS_LEFT.value);
			binder.BindKey(ord('h'),chamfer.MOD_MASK_1|chamfer.MOD_MASK_SHIFT,Key.MOVE_LEFT.value);
			binder.BindKey(ord('k'),chamfer.MOD_MASK_1,Key.FOCUS_UP.value);
			binder.BindKey(ord('j'),chamfer.MOD_MASK_1,Key.FOCUS_DOWN.value);
			binder.BindKey(ord('a'),chamfer.MOD_MASK_1,Key.FOCUS_PARENT.value);
			binder.BindKey(ord('s'),chamfer.MOD_MASK_1,Key.FOCUS_CHILD.value);
			#binder.BindKey(ord('x'),chamfer.MOD_MASK_1,Key.FOCUS_CHILD.value);

			binder.BindKey(ord('l'),chamfer.MOD_MASK_4,Key.FOCUS_PARENT_RIGHT.value);
			binder.BindKey(ord('h'),chamfer.MOD_MASK_4,Key.FOCUS_PARENT_LEFT.value);

			binder.BindKey(ord('y'),chamfer.MOD_MASK_1,Key.YANK_CONTAINER.value);
			binder.BindKey(ord('p'),chamfer.MOD_MASK_1,Key.PASTE_CONTAINER.value);

			binder.BindKey(ord('e'),chamfer.MOD_MASK_1,Key.LAYOUT.value);
			binder.BindKey(ord('m'),chamfer.MOD_MASK_1,Key.MAXIMIZE.value);
			binder.BindKey(miscellany.XK_Tab,chamfer.MOD_MASK_1,Key.SPLIT_V.value);
			
			binder.BindKey(ord('q'),chamfer.MOD_MASK_1|chamfer.MOD_MASK_SHIFT,Key.KILL.value);
			binder.BindKey(miscellany.XK_Return,chamfer.MOD_MASK_1,Key.LAUNCH_TERMINAL.value);
			binder.BindKey(ord('1'),chamfer.MOD_MASK_4,Key.LAUNCH_BROWSER.value);
			binder.BindKey(ord('2'),chamfer.MOD_MASK_4,Key.LAUNCH_BROWSER_PRIVATE.value);

			binder.BindKey(xf86.XK_XF86_AudioRaiseVolume,0,Key.AUDIO_VOLUME_UP.value);
			binder.BindKey(xf86.XK_XF86_AudioLowerVolume,0,Key.AUDIO_VOLUME_DOWN.value);

			binder.BindKey(xf86.XK_XF86_MonBrightnessUp,0,Key.MONITOR_BRIGHTNESS_UP.value);
			binder.BindKey(xf86.XK_XF86_MonBrightnessDown,0,Key.MONITOR_BRIGHTNESS_DOWN.value);
			#/ - search for a window
			#n - next match
			#N - previous match
			#[num] m - set window min size to [num]0%
			#... M - set max
			#f - highlight GTK buttons etc to be activated with keyboard

			#alt+shift+a, select the top most base container under root

			#alt+tab, cycle between floating containers?

			#moving windows between containers: a cut/paste mechanism
			#not only indivudal clients can be replaced, but whole container hierarchies
		else:
			#debug only
			binder.BindKey(ord('h'),chamfer.MOD_MASK_SHIFT,Key.FOCUS_LEFT.value);
			binder.BindKey(ord('k'),chamfer.MOD_MASK_SHIFT,Key.FOCUS_UP.value);
			binder.BindKey(ord('l'),chamfer.MOD_MASK_SHIFT,Key.FOCUS_RIGHT.value);
			binder.BindKey(ord('j'),chamfer.MOD_MASK_SHIFT,Key.FOCUS_DOWN.value);
			binder.BindKey(ord('u'),chamfer.MOD_MASK_SHIFT,Key.MOVE_LEFT.value);
			binder.BindKey(ord('i'),chamfer.MOD_MASK_SHIFT,Key.MOVE_RIGHT.value);
			binder.BindKey(ord('a'),chamfer.MOD_MASK_SHIFT,Key.FOCUS_PARENT.value);
			binder.BindKey(ord('s'),chamfer.MOD_MASK_SHIFT,Key.FOCUS_CHILD.value);
			binder.BindKey(ord('x'),chamfer.MOD_MASK_SHIFT,Key.FOCUS_CHILD.value);
			binder.BindKey(ord('y'),chamfer.MOD_MASK_SHIFT,Key.YANK_CONTAINER.value);
			binder.BindKey(ord('p'),chamfer.MOD_MASK_SHIFT,Key.PASTE_CONTAINER.value);

			binder.BindKey(ord('e'),chamfer.MOD_MASK_SHIFT,Key.LAYOUT.value);
			binder.BindKey(ord('m'),chamfer.MOD_MASK_SHIFT,Key.MAXIMIZE.value);
			binder.BindKey(miscellany.XK_Tab,chamfer.MOD_MASK_SHIFT,Key.SPLIT_V.value);
	
	def OnCreateContainer(self):
		print("OnCreateContainer()");
		return Container();

	def OnKeyPress(self, keyId):
		print("key press: {}".format(keyId));
		focus = chamfer.GetFocus();
		parent = focus.GetParent();

		if keyId == Key.FOCUS_RIGHT.value:
			focus = focus.GetNext();
			#focus = focus.GetAdjacent(chamfer.adjacent.RIGHT);
			focus.Focus();

		elif keyId == Key.FOCUS_LEFT.value:
			focus = focus.GetPrev();
			#focus = focus.GetAdjacent(chamfer.adjacent.LEFT);
			focus.Focus();

		elif keyId == Key.FOCUS_DOWN.value:
			focus = focus.GetNext();
			#focus = focus.GetAdjacent(chamfer.adjacent.DOWN);
			focus.Focus();

		elif keyId == Key.FOCUS_UP.value:
			focus = focus.GetPrev();
			#focus = focus.GetAdjacent(chamfer.adjacent.UP);
			focus.Focus();

		elif keyId == Key.MOVE_RIGHT.value:
			focus.MoveNext();

		elif keyId == Key.MOVE_LEFT.value:
			focus.MovePrev();

		elif keyId == Key.FOCUS_PARENT.value:
			if parent is None:
				return;
			parent.Focus();

		elif keyId == Key.FOCUS_CHILD.value:
			focus = focus.GetFocus();
			if focus is None:
				return;
			focus.Focus();

		elif keyId == Key.FOCUS_PARENT_RIGHT.value:
			if parent is None:
				return;
			parent1 = parent.GetNext();
			focus = parent1.GetFocus();
			if focus is None:
				return;
			focus.Focus();
			
		elif keyId == Key.FOCUS_PARENT_LEFT.value:
			if parent is None:
				return;
			parent1 = parent.GetPrev();
			focus = parent1.GetFocus();
			if focus is None:
				return;
			focus.Focus();
			
		elif keyId == Key.YANK_CONTAINER.value:
			print("yanking container...");
			self.yank = focus;

		elif keyId == Key.PASTE_CONTAINER.value:
			print("pasting container...");
			try:
				self.yank.Move(focus); #warning! need to know wether yank is still alive
			except AttributeError:
				pass;

		elif keyId == Key.LAYOUT.value:
			if parent is None:
				return;
			layout = {
				chamfer.layout.VSPLIT:chamfer.layout.HSPLIT,
				chamfer.layout.HSPLIT:chamfer.layout.VSPLIT
			}[parent.layout];
			parent.ShiftLayout(layout);

		elif keyId == Key.MAXIMIZE.value:
			focus.minSize = (0.98,0.98);
			focus.ShiftLayout(focus.layout);
		
		elif keyId == Key.SPLIT_V.value:
			#arm the container split
			#TODO: add render flags property, bitwise or them
			print("split armed.");
			focus.splitArmed = not focus.splitArmed;

		elif keyId == Key.KILL.value:
			print("killing client...");
			focus.Kill();

		elif keyId == Key.LAUNCH_TERMINAL.value:
			psutil.Popen(["termite"],stdout=None,stderr=None);

		elif keyId == Key.LAUNCH_BROWSER.value:
			psutil.Popen(["firefox"],stdout=None,stderr=None);

		elif keyId == Key.LAUNCH_BROWSER_PRIVATE.value:
			psutil.Popen(["firefox","--private-window"],stdout=None,stderr=None);

		elif keyId == Key.AUDIO_VOLUME_UP.value:
			if "pulsectl" in sys.modules:
				with pulsectl.Pulse('volume-increaser') as pulse:
					for sink in pulse.sink_list():
						pulse.volume_change_all_chans(sink,0.05);

		elif keyId == Key.AUDIO_VOLUME_DOWN.value:
			if "pulsectl" in sys.modules:
				with pulsectl.Pulse('volume-increaser') as pulse:
					for sink in pulse.sink_list():
						pulse.volume_change_all_chans(sink,-0.05);

		elif keyId == Key.MONITOR_BRIGHTNESS_UP.value:
			psutil.Popen(["xbacklight","-inc","20"]);
			pass;

		elif keyId == Key.MONITOR_BRIGHTNESS_DOWN.value:
			psutil.Popen(["xbacklight","-dec","20"]);
			pass;

	
	def OnKeyRelease(self, keyId):
		print("key release: {}".format(keyId));

class Compositor(chamfer.Compositor):
	def __init__(self):
		#self.shaderPath = "../";
		print(self.shaderPath);
	
	#def SetupGraphics(self):
	#	shader = self.LoadShader("frame_vertex.spv");

	def OnPropertyChange(self, prop):
		pass;

backend = Backend();
chamfer.bind_Backend(backend);

pids = psutil.pids();
pnames = [psutil.Process(pid).name() for pid in pids];
pcmdls = [a for p in [psutil.Process(pid).cmdline() for pid in pids] for a in p];

if not "pulseaudio" in pnames:
	print("starting pulseaudio...");
	#need to wait some time before starting pulseaudio for it initialize successfully
	psutil.Popen(["sh","-c","sleep 5.0; pulseaudio --start"],stdout=None,stderr=None);

if not "dunst" in pnames:
	print("starting dunst..."); #later on, we might have our own notification system
	psutil.Popen(["dunst"],stdout=None,stderr=None);

if not any(["clipster" in p for p in pcmdls]):
	print("starting clipster..."); #clipboard manager
	psutil.Popen(["clipster","-d"],stdout=None,stderr=None);

if not any(["libinput-gestures" in p for p in pcmdls]):
	print("starting libinput-gestures..."); #touchpad gestures
	psutil.Popen(["libinput-gestures-setup","start"],stdout=None,stderr=None);

#compositor = Compositor();
#chamfer.bind_Compositor(compositor);

