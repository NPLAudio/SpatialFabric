"""Add/Edit cue dialog: name and OSC base are typed, the cue TYPE is chosen from a fixed list (never typed)."""
import tkinter as tk
from tkinter import ttk
from .model import CUE_KINDS, Cue

KIND_LABELS = {'playback': 'Playback (sound file cue)', 'live': 'Live input (microphone / line in)'}
LABEL_TO_KIND = {label: kind for kind, label in KIND_LABELS.items()}
DEFAULT_BASES = {'playback': '/demo/playback/new', 'live': '/audioloom/Stage/live/1'}


class CueDialog(tk.Toplevel):
    """Modal. result is a validated Cue, or None when cancelled. Pass wait=False to build it without blocking (tests)."""
    def __init__(self, parent, cue=None, wait=True):
        super().__init__(parent)
        self.title('Edit cue' if cue else 'Add cue'); self.transient(parent); self.resizable(False, False)
        self.result = None; self.error = tk.StringVar()
        self.name = tk.StringVar(value=cue.name if cue else ''); self.base = tk.StringVar(value=cue.base if cue else DEFAULT_BASES['playback'])
        self.kind = tk.StringVar(value=KIND_LABELS[cue.kind if cue else 'playback'])
        frame = ttk.Frame(self, padding=14); frame.pack(fill='both', expand=True)
        ttk.Label(frame, text='Display name').grid(row=0, column=0, sticky='w'); ttk.Entry(frame, textvariable=self.name, width=42).grid(row=0, column=1, pady=3)
        ttk.Label(frame, text='Type').grid(row=1, column=0, sticky='w')
        self.kind_box = ttk.Combobox(frame, textvariable=self.kind, values=list(KIND_LABELS.values()), state='readonly', width=39); self.kind_box.grid(row=1, column=1, pady=3)
        self.kind_box.bind('<<ComboboxSelected>>', self._kind_changed)
        ttk.Label(frame, text='OSC base (no /play suffix)').grid(row=2, column=0, sticky='w'); ttk.Entry(frame, textvariable=self.base, width=42).grid(row=2, column=1, pady=3)
        ttk.Label(frame, textvariable=self.error, foreground='#ffbf69').grid(row=3, column=0, columnspan=2, sticky='w')
        buttons = ttk.Frame(frame); buttons.grid(row=4, column=0, columnspan=2, sticky='e', pady=(10, 0))
        ttk.Button(buttons, text='OK', command=self.accept).pack(side='right', padx=(6, 0)); ttk.Button(buttons, text='Cancel', command=self.destroy).pack(side='right')
        self.bind('<Return>', lambda _e: self.accept()); self.bind('<Escape>', lambda _e: self.destroy())
        if wait:
            self.grab_set(); self.wait_window(self)

    def _kind_changed(self, _event=None):
        # Offer a sensible base for the chosen type, but never overwrite something the user already typed.
        if self.base.get() in DEFAULT_BASES.values(): self.base.set(DEFAULT_BASES[self.selected_kind()])

    def selected_kind(self): return LABEL_TO_KIND.get(self.kind.get(), 'playback')

    def accept(self):
        try:
            cue = Cue(self.name.get().strip(), self.base.get().strip(), self.selected_kind()); cue.validate()
        except ValueError as error:
            self.error.set(str(error)); return None
        self.result = cue; self.destroy(); return cue
