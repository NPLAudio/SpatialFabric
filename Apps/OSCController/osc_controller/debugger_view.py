"""Debugger / test-bed tab. Opening it binds nothing; sockets exist only between Start and Stop."""
import json
import time
import tkinter as tk
from tkinter import filedialog, messagebox, ttk
from .debugger import Debugger

CYAN, MUTED, ORANGE = '#54d8f5', '#a9c1d9', '#ffbf69'
COLORS = {'ok': '#7ee2a8', 'info': '#a9c1d9', 'warn': '#ffbf69', 'error': '#ff7a7a'}


class DebuggerPanel(ttk.Frame):
    def __init__(self, parent):
        super().__init__(parent, padding=12)
        self.debugger = Debugger(); self.token = None; self.rows = []
        self.listen_ip = tk.StringVar(value='127.0.0.1'); self.listen_port = tk.StringVar(value='9400')
        self.monitor_port = tk.StringVar(value='9001')
        self.engine_ip = tk.StringVar(value='127.0.0.1'); self.engine_port = tk.StringVar(value='9000'); self.reply_port = tk.StringVar(value='9012')
        self.forward = tk.BooleanVar(value=False); self.only_problems = tk.BooleanVar(value=False)
        self.status = tk.StringVar(value='Stopped. Nothing is bound or sent. Point your sender app at the listen port, then Start.')
        self.contract_text = tk.StringVar(value='Contract: wire rules only (no engine inventory loaded).')
        ttk.Label(self, text='Sender-app test bed: see what an app sends, whether it conforms, and (optionally) what the running engine actually does with it.', foreground=MUTED).pack(anchor='w')
        row = ttk.Frame(self); row.pack(fill='x', pady=(8, 0))
        for label, var, width in [('Listen IP', self.listen_ip, 14), ('port', self.listen_port, 7), ('Engine IP', self.engine_ip, 14), ('Engine input port', self.engine_port, 7), ('State port', self.monitor_port, 7), ('Query reply port', self.reply_port, 7)]:
            ttk.Label(row, text=label).pack(side='left', padx=(8, 0)); ttk.Entry(row, textvariable=var, width=width).pack(side='left', padx=4)
        row = ttk.Frame(self); row.pack(fill='x', pady=6)
        ttk.Button(row, text='Start debugger', command=self.start).pack(side='left', padx=(0, 6))
        ttk.Button(row, text='Stop', command=self.stop).pack(side='left', padx=(0, 6))
        ttk.Button(row, text='Load engine contract', command=self.load_contract).pack(side='left', padx=(0, 6))
        ttk.Checkbutton(row, text='Forward + verify (may start playback / mic)', variable=self.forward).pack(side='left', padx=10)
        ttk.Checkbutton(row, text='Problems only', variable=self.only_problems, command=self.refilter).pack(side='left', padx=6)
        ttk.Button(row, text='Export report…', command=self.export).pack(side='right', padx=(6, 0))
        ttk.Button(row, text='Clear', command=self.clear).pack(side='right')
        ttk.Label(self, textvariable=self.contract_text, foreground=CYAN).pack(anchor='w')
        ttk.Label(self, textvariable=self.status, foreground=MUTED).pack(anchor='w', pady=(0, 6))
        columns = ('time', 'sev', 'kind', 'address', 'args', 'verdict')
        self.tree = ttk.Treeview(self, columns=columns, show='headings', height=14, selectmode='browse')
        for key, label, width in [('time', 'Time', 70), ('sev', 'Result', 60), ('kind', 'Kind', 130), ('address', 'Address', 250), ('args', 'Arguments', 170), ('verdict', 'Verdict (what the engine does / what is wrong)', 480)]:
            self.tree.heading(key, text=label); self.tree.column(key, width=width, anchor='w')
        for severity, color in COLORS.items(): self.tree.tag_configure(severity, foreground=color)
        scroll = ttk.Scrollbar(self, orient='vertical', command=self.tree.yview); self.tree.configure(yscrollcommand=scroll.set)
        scroll.pack(side='right', fill='y'); self.tree.pack(fill='both', expand=True)
        self.tree.bind('<<TreeviewSelect>>', self.show_detail)
        self.detail = tk.Text(self, height=5, wrap='word', bg='#14243c', fg='#eef5ff', relief='flat'); self.detail.pack(side='bottom', fill='x', pady=(6, 0))
        self.detail.insert('1.0', 'Select a row for the full explanation and, for malformed packets, the raw bytes.'); self.detail.configure(state='disabled')

    def _endpoint(self, ip, port): return ip.get().strip(), int(port.get())

    def start(self):
        try:
            forward = self.forward.get()
            if forward and not messagebox.askyesno('Forward to the engine?', 'Every valid message from your sender app will be forwarded to the engine input, so a /play can start REAL playback and a live-input /play can open a microphone there. Use headphones or low gain.\n\nForward and verify?', parent=self): return
            engine = self._endpoint(self.engine_ip, self.engine_port)
            monitor = (self.listen_ip.get().strip(), int(self.monitor_port.get())) if self.monitor_port.get().strip() else None
            self.debugger.start(self._endpoint(self.listen_ip, self.listen_port), monitor=monitor, query=engine, reply_port=int(self.reply_port.get()), forward=engine if forward else None)
            self.status.set(f'Listening on {self.listen_ip.get()}:{self.listen_port.get()}' + (f' · FORWARDING to {engine[0]}:{engine[1]}' if forward else ' · forwarding OFF (nothing reaches the engine)') + (f' · engine state on :{monitor[1]}' if monitor else ''))
            self.schedule()
        except (ValueError, OSError) as error: self.status.set(f'Could not start: {error}')

    def stop(self):
        self.debugger.stop()
        if self.token is not None: self.after_cancel(self.token); self.token = None
        self.status.set('Stopped. Sockets released; recorded rows are historical.')

    def load_contract(self):
        try:
            if not self.debugger.listening: raise ValueError('Start the debugger first (the read-only scene query needs its reply socket)')
            self.debugger.refresh_contract('manual'); self.status.set('Querying the engine scene (read-only; never starts audio)…')
        except (ValueError, OSError) as error: self.status.set(str(error))

    def schedule(self):
        if self.token is None: self.token = self.after(20, self.tick)

    def tick(self):
        self.token = None
        fresh = self.debugger.poll(time.monotonic())
        for record in fresh: self.add_row(record)
        contract = self.debugger.validator.contract
        self.contract_text.set(f'Contract: {contract.source} · {len(contract.cues)} cue(s) · objects {sorted(contract.objects)[:8] if contract.objects else "unknown"} · {contract.world}' if contract.source == 'engine' else 'Contract: wire rules only (no engine inventory loaded). Load the engine contract to check real cues and IDs.')
        c = self.debugger.counts
        if self.debugger.listening: self.schedule()
        self.title_counts = f'{self.debugger.total} records · {c["error"]} errors · {c["warn"]} warnings · {self.debugger.malformed} malformed · {self.debugger.forwarded} forwarded'
        self.status.set(self.status.get().split(' | ')[0] + ' | ' + self.title_counts)

    def add_row(self, record):
        self.rows.append(record)
        if len(self.rows) > 2000: self.rows.pop(0)
        if self.only_problems.get() and record.severity in ('ok', 'info'): return
        self._insert(record)

    def _insert(self, record):
        verdict = ' | '.join(f.text for f in record.findings)
        label = record.address or ('(malformed)' if record.kind == 'malformed' else '')
        item = self.tree.insert('', 'end', values=(time.strftime('%H:%M:%S'), record.severity.upper(), record.kind + (' →engine' if record.forwarded else ''), label, record.args, verdict), tags=(record.severity,))
        self.tree.record_map = getattr(self.tree, 'record_map', {}); self.tree.record_map[item] = record
        children = self.tree.get_children()
        if len(children) > 1500: self.tree.delete(children[0])
        self.tree.yview_moveto(1)

    def refilter(self):
        self.tree.delete(*self.tree.get_children())
        for record in self.rows:
            if not (self.only_problems.get() and record.severity in ('ok', 'info')): self._insert(record)

    def show_detail(self, _event=None):
        selected = self.tree.selection()
        record = getattr(self.tree, 'record_map', {}).get(selected[0]) if selected else None
        if record is None: return
        lines = [f'{record.kind}  {record.address}  {record.args}', *[f'[{f.severity.upper()}] {f.code}: {f.text}' for f in record.findings]]
        if record.raw: lines.append('raw bytes (first 96): ' + record.raw)
        self.detail.configure(state='normal'); self.detail.delete('1.0', 'end'); self.detail.insert('1.0', '\n'.join(lines)); self.detail.configure(state='disabled')

    def clear(self):
        self.rows.clear(); self.tree.delete(*self.tree.get_children()); self.debugger.records.clear()
        self.debugger.counts = {s: 0 for s in self.debugger.counts}; self.debugger.total = self.debugger.malformed = self.debugger.forwarded = 0; self.debugger.effects.clear(); self.debugger._pending.clear()

    def export(self):
        path = filedialog.asksaveasfilename(parent=self, defaultextension='.json', filetypes=[('JSON report', '*.json')], title='Export debugger report')
        if path:
            with open(path, 'w', encoding='utf-8') as stream: json.dump(self.debugger.report(), stream, indent=2)
            self.status.set(f'Report written to {path}')

    def close(self):
        if self.token is not None: self.after_cancel(self.token); self.token = None
        self.debugger.stop()
