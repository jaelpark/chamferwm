
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

class KeyConfig(chamfer.KeyConfig):
	def SetupKeys(self):
		print("setting up keys...");

		self.BindKey(ord('l'),chamfer.MOD_MASK_1,Key.FOCUS_RIGHT.value);
		self.BindKey(ord('h'),chamfer.MOD_MASK_1,Key.FOCUS_LEFT.value);
		self.BindKey(ord('k'),chamfer.MOD_MASK_1,Key.FOCUS_UP.value);
		self.BindKey(ord('j'),chamfer.MOD_MASK_1,Key.FOCUS_DOWN.value);
		self.BindKey(ord('a'),chamfer.MOD_MASK_1,Key.FOCUS_PARENT.value);
		self.BindKey(ord('x'),chamfer.MOD_MASK_1,Key.FOCUS_CHILD.value);

		self.BindKey(ord('e'),chamfer.MOD_MASK_1,Key.LAYOUT.value);

		#debug only
		self.BindKey(ord('h'),chamfer.MOD_MASK_SHIFT,Key.FOCUS_LEFT.value);
		self.BindKey(ord('k'),chamfer.MOD_MASK_SHIFT,Key.FOCUS_UP.value);
		self.BindKey(ord('l'),chamfer.MOD_MASK_SHIFT,Key.FOCUS_RIGHT.value);
		self.BindKey(ord('j'),chamfer.MOD_MASK_SHIFT,Key.FOCUS_DOWN.value);

		self.BindKey(ord('e'),chamfer.MOD_MASK_SHIFT,Key.LAYOUT.value);

class Backend(chamfer.Backend):
	def OnCreateClient(self, client):
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
			parent.ShiftLayout(layout);
	
	def OnKeyRelease(self, keyId):
		print("key release: {}".format(keyId));

class Compositor(chamfer.Compositor):
	def __init__(self):
		#self.shaderPath = "../";
		print(self.shaderPath);

	def OnPropertyChange(self, prop):
		pass;

keyConfig = KeyConfig();
chamfer.bind_KeyConfig(keyConfig);

backend = Backend();
chamfer.bind_Backend(backend);

#compositor = Compositor();
#chamfer.bind_Compositor(compositor);

#startup applications here?

