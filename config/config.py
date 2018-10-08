
import chamfer
from enum import Enum,auto

class Key(Enum):
	FOCUS_RIGHT = auto()
	FOCUS_LEFT = auto()
	FOCUS_UP = auto()
	FOCUS_DOWN = auto()
	FOCUS_PARENT = auto()
	FOCUS_CHILD = auto()

	LAYOUT = auto()

class Backend(chamfer.Backend):
	def SetupKeys(self, binder):
		print("setting up keys...");

		binder.BindKey(ord('l'),chamfer.MOD_MASK_1,Key.FOCUS_RIGHT.value);
		binder.BindKey(ord('h'),chamfer.MOD_MASK_1,Key.FOCUS_LEFT.value);
		binder.BindKey(ord('k'),chamfer.MOD_MASK_1,Key.FOCUS_UP.value);
		binder.BindKey(ord('j'),chamfer.MOD_MASK_1,Key.FOCUS_DOWN.value);
		binder.BindKey(ord('a'),chamfer.MOD_MASK_1,Key.FOCUS_PARENT.value);
		binder.BindKey(ord('x'),chamfer.MOD_MASK_1,Key.FOCUS_CHILD.value);

		binder.BindKey(ord('e'),chamfer.MOD_MASK_1,Key.LAYOUT.value);

		#/ - search for a window
		#n - next match
		#N - previous match

		#debug only
		binder.BindKey(ord('h'),chamfer.MOD_MASK_SHIFT,Key.FOCUS_LEFT.value);
		binder.BindKey(ord('k'),chamfer.MOD_MASK_SHIFT,Key.FOCUS_UP.value);
		binder.BindKey(ord('l'),chamfer.MOD_MASK_SHIFT,Key.FOCUS_RIGHT.value);
		binder.BindKey(ord('j'),chamfer.MOD_MASK_SHIFT,Key.FOCUS_DOWN.value);

		binder.BindKey(ord('e'),chamfer.MOD_MASK_SHIFT,Key.LAYOUT.value);
	
	def SetupClient(self, client):
		#
		pass;

	def OnCreateClient(self, client):
		#TODO: define minimum and maximum dimensions. Minimum size clients will start overlapping,
		#while clients exceeding the maximum won't stretch any further.
		container = client.GetContainer();
		chamfer.SetFocus(container);

	def OnKeyPress(self, keyId):
		print("key press: {}".format(keyId));
		focus = chamfer.GetFocus();
		parent = focus.GetParent();

		if keyId == Key.FOCUS_RIGHT.value and parent.layout == chamfer.layout.VSPLIT:
			focus = focus.GetNext();
			chamfer.SetFocus(focus);

		elif keyId == Key.FOCUS_LEFT.value and parent.layout == chamfer.layout.VSPLIT:
			focus = focus.GetPrev();
			chamfer.SetFocus(focus);

		elif keyId == Key.FOCUS_DOWN.value and parent.layout == chamfer.layout.HSPLIT:
			focus = focus.GetNext();
			chamfer.SetFocus(focus);

		elif keyId == Key.FOCUS_UP.value and parent.layout == chamfer.layout.HSPLIT:
			focus = focus.GetPrev();
			chamfer.SetFocus(focus);
			
		elif keyId == Key.LAYOUT.value:
			layout = {
				chamfer.layout.VSPLIT:chamfer.layout.HSPLIT,
				chamfer.layout.HSPLIT:chamfer.layout.VSPLIT
			}[parent.layout];
			parent.ShiftLayout(layout); #TODO: layout = ..., use UpdateLayout() to update all changes?
	
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

#compositor = Compositor();
#chamfer.bind_Compositor(compositor);

#startup applications here?

