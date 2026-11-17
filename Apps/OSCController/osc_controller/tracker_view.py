"""Receive-only multi-view tracker. No rendering or network work is done on worker threads."""
import math
import json
import time
import tkinter as tk
from tkinter import ttk, filedialog
from .tracker import ObjectTracker, project

BG,PANEL,TEXT,MUTED,CYAN,PURPLE,ORANGE='#0b1426','#14243c','#eef5ff','#a9c1d9','#54d8f5','#bf9aff','#ffbf69'


class TrackerPanel(ttk.Frame):
    def __init__(self,parent,start,stop,query,import_cues):
        super().__init__(parent,padding=12)
        self.model=ObjectTracker();self.last_draw=self.last_table=0
        self.ip=tk.StringVar(value='127.0.0.1');self.ports=tk.StringVar(value='50018, 9002')
        self.extent=tk.StringVar(value='1.0');self.trails=tk.BooleanVar(value=True)
        self.filter=tk.StringVar(value='All')
        self.query_ip=tk.StringVar(value='127.0.0.1');self.query_port=tk.StringVar(value='8100');self.reply_port=tk.StringVar(value='9011')
        self.scene=None;self.scene_status=tk.StringVar(value='No scene inventory queried. Query is read-only; importing cue configuration requires confirmation.')
        self.status=tk.StringVar(value='Stopped. No packets fabricated: views show only received data.')
        row=ttk.Frame(self);row.pack(fill='x')
        for label,var,width in [('Listen IP',self.ip,16),('Spatial ports',self.ports,17)]:
            ttk.Label(row,text=label).pack(side='left');ttk.Entry(row,textvariable=var,width=width).pack(side='left',padx=6)
        ttk.Button(row,text='Start tracking',command=start).pack(side='left',padx=4)
        ttk.Button(row,text='Stop tracking',command=stop).pack(side='left',padx=4)
        ttk.Button(row,text='Clear data',command=self.clear).pack(side='right')
        query_row=ttk.Frame(self);query_row.pack(fill='x',pady=(6,0))
        for label,var,width in [('Query IP',self.query_ip,16),('Port',self.query_port,7),('Reply port',self.reply_port,7)]:
            ttk.Label(query_row,text=label).pack(side='left');ttk.Entry(query_row,textvariable=var,width=width).pack(side='left',padx=5)
        ttk.Button(query_row,text='Query scene',command=query).pack(side='left',padx=4)
        ttk.Button(query_row,text='Import discovered cues…',command=import_cues).pack(side='left',padx=4)
        ttk.Button(query_row,text='Export JSON…',command=self.export_scene).pack(side='left',padx=4)
        ttk.Label(self,textvariable=self.status,foreground=CYAN).pack(anchor='w',pady=6)
        options=ttk.Frame(self);options.pack(fill='x',pady=(0,6))
        ttk.Label(options,text='View extent ±').pack(side='left');ttk.Entry(options,textvariable=self.extent,width=8).pack(side='left',padx=5)
        ttk.Button(options,text='Fit received',command=self.fit).pack(side='left',padx=5)
        ttk.Checkbutton(options,text='Trails',variable=self.trails).pack(side='left',padx=8)
        ttk.Combobox(options,textvariable=self.filter,values=('All','ADM','Custom','Scene'),state='readonly',width=8).pack(side='left',padx=5)
        ttk.Label(options,text='Receive only · age ≠ disconnect',foreground=MUTED).pack(side='left',padx=6)
        split=ttk.Panedwindow(self,orient='horizontal');split.pack(fill='both',expand=True)
        views=ttk.Notebook(split);split.add(views,weight=3)
        multi=ttk.Frame(views);views.add(multi,text='Top / Front / Side / 3D')
        multi.columnconfigure((0,1),weight=1);multi.rowconfigure((0,1),weight=1)
        self.canvases={}
        for i,mode in enumerate(('Top','Front','Side','3D')):
            canvas=tk.Canvas(multi,bg=PANEL,highlightthickness=1,highlightbackground='#304b69',width=260,height=180)
            canvas.grid(row=i//2,column=i%2,sticky='nsew',padx=3,pady=3);self.canvases[mode]=canvas
        for mode,label in [('Polar','Polar / AED'),('History','XYZ history')]:
            canvas=tk.Canvas(views,bg=PANEL,highlightthickness=0);views.add(canvas,text=label);self.canvases[mode]=canvas
        inventory=ttk.Frame(views);views.add(inventory,text='Scene inventory')
        ttk.Label(inventory,textvariable=self.scene_status,foreground=CYAN,wraplength=700).pack(fill='x',pady=5)
        self.inventory_text=tk.Text(inventory,wrap='none',bg=PANEL,fg=TEXT,insertbackground=TEXT,font=('Consolas',10),width=40,height=8,state='disabled')
        vs=ttk.Scrollbar(inventory,orient='vertical',command=self.inventory_text.yview)
        hs=ttk.Scrollbar(inventory,orient='horizontal',command=self.inventory_text.xview)
        self.inventory_text.configure(yscrollcommand=vs.set,xscrollcommand=hs.set)
        hs.pack(side='bottom',fill='x');vs.pack(side='right',fill='y');self.inventory_text.pack(fill='both',expand=True)
        side=ttk.Frame(split);split.add(side,weight=1)
        ttk.Label(side,text='Received objects (select to isolate)',foreground=CYAN).pack(anchor='w',pady=4)
        list_area=ttk.Frame(side);list_area.pack(fill='both',expand=True)
        self.table=ttk.Treeview(list_area,columns=('object','front','right','up','age'),show='headings',selectmode='browse',height=4)
        for key,label,width in [('object','Object / source',135),('front','Front',55),('right','Right',55),('up','Up',55),('age','Age',50)]:
            self.table.heading(key,text=label);self.table.column(key,width=width,minwidth=45)
        scroll=ttk.Scrollbar(list_area,orient='vertical',command=self.table.yview);self.table.configure(yscrollcommand=scroll.set)
        scroll.pack(side='right',fill='y');self.table.pack(fill='both',expand=True)
        ttk.Button(side,text='Show all objects',command=lambda:self.table.selection_remove(*self.table.selection())).pack(fill='x',pady=5)
        self.detail=tk.StringVar(value='Waiting for ADM /xyz or /aed, or example /demo/{zone}/{id}/xyz.\nPositions use front / right / up internally.')
        ttk.Label(side,textvariable=self.detail,wraplength=340,foreground=MUTED,justify='left').pack(anchor='w',pady=5)
        ttk.Label(self,text='ADM wire XYZ: right, front, up. Example Custom: front, right, up. Sources stay separate; no inferred object pairing. Views cap at 32 objects; table holds up to 256.',foreground=MUTED,wraplength=1150).pack(anchor='w',pady=(5,0))

    def set_scene(self,scene,sender_ip,now):
        self.scene=scene;self.model.apply_scene(scene,sender_ip,now)
        self.scene_status.set(f'Schema v{scene["schema_version"]} · {scene["total"]} records · world {scene["world"]} · Scene dots are query snapshots, not OSC motion')
        text=json.dumps(scene,indent=2,ensure_ascii=False,allow_nan=False)
        preview=text[:262144]+ ('\n[Preview capped at 256 KiB; Export JSON contains the full inventory.]' if len(text)>262144 else '')
        self.inventory_text.configure(state='normal');self.inventory_text.delete('1.0','end');self.inventory_text.insert('1.0',preview);self.inventory_text.configure(state='disabled')

    def export_scene(self):
        if self.scene is None:return
        path=filedialog.asksaveasfilename(parent=self,title='Export complete scene inventory',defaultextension='.json',initialfile='spatialfabric-scene.json',filetypes=[('JSON','*.json')])
        if not path:return
        try:
            from pathlib import Path
            Path(path).write_text(json.dumps(self.scene,indent=2,ensure_ascii=False,allow_nan=False)+'\n',encoding='utf-8')
            self.scene_status.set('Full inventory exported: '+path)
        except (OSError,ValueError) as error:self.scene_status.set(str(error))

    def clear(self):
        self.model.clear();self.table.delete(*self.table.get_children());self.last_draw=self.last_table=0

    def fit(self):
        points=[o.position for o in self.model.objects.values() if o.position is not None]
        extent=max([1.0]+[math.hypot(*p) for p in points])
        self.extent.set(f'{extent*1.1:.3g}')

    def port_numbers(self):
        try:ports=[int(p.strip()) for p in self.ports.get().split(',') if p.strip()]
        except ValueError:raise ValueError('Spatial ports must be comma-separated integers') from None
        if not ports or len(ports)>8 or any(not 1<=p<=65535 for p in ports):raise ValueError('Choose 1..8 spatial ports in 1..65535')
        return sorted(set(ports))

    def refresh(self,now):
        if not self.winfo_ismapped() or now-self.last_draw<.05:return
        self.last_draw=now
        objects=[o for o in self.model.objects.values() if o.position is not None]
        if now-self.last_table>.25:
            self.last_table=now;keys={o.key for o in objects}
            for key in self.table.get_children():
                if key not in keys:self.table.delete(key)
            for obj in objects:
                values=(f'{obj.protocol} {obj.object_id} {obj.name}',*[f'{v:.3f}' for v in obj.position],f'{now-obj.position_time:.1f}s')
                if self.table.exists(obj.key):self.table.item(obj.key,values=values)
                else:self.table.insert('','end',iid=obj.key,values=values)
        selected=self.table.selection()
        visible=[o for o in objects if (not selected or o.key==selected[0]) and (self.filter.get()=='All' or o.protocol==self.filter.get())][:32]
        try:extent=float(self.extent.get())
        except ValueError:extent=1.0
        if not math.isfinite(extent) or not .001<=extent<=1e7:extent=1.0
        for mode,canvas in self.canvases.items():
            if canvas.winfo_ismapped():self._draw(canvas,mode,visible,extent,now)
        if visible:
            obj=visible[0];a,e,r=obj.aed
            source = f'Scene query snapshot · {obj.peer}' if obj.source=='scene_query' else f'OSC {obj.peer} → local :{obj.port}'
            self.detail.set(f'{obj.protocol} object {obj.object_id} · {obj.name or "unnamed"}\n{source}\nAz {a:.1f}° · El {e:.1f}° · R {r:.3f}\nGain {obj.gain:.3f} · width {obj.width:.3f} · mute {obj.mute}\nPosition rate {obj.rate(now):.1f} updates/s\nLast position {now-obj.position_time:.1f}s ago\nHistory is bounded; stale dots are dimmed.')
        else:self.detail.set('No spatial positions received. Check Start tracking, Unreal output destinations and PIE/realtime updates.')

    def _draw(self,c,mode,objects,extent,now):
        c.delete('all');w,h=max(c.winfo_width(),20),max(c.winfo_height(),20)
        c.create_text(10,10,anchor='nw',text=mode+'  '+({'Top':'right / front','Front':'right / up','Side':'front / up','3D':'isometric','Polar':'azimuth / radius','History':'last 15 seconds'}.get(mode,'')),fill=TEXT)
        if mode=='History':
            if not objects:return
            obj=objects[0];left,right,top,bottom=45,w-20,35,h-35;mid=(top+bottom)/2
            c.create_line(left,mid,right,mid,fill='#365678');c.create_text(left,bottom+15,text='−15 s',fill=MUTED);c.create_text(right,bottom+15,text='now',fill=MUTED)
            for axis,color,label in [(0,CYAN,'front'),(1,ORANGE,'right'),(2,PURPLE,'up')]:
                points=[]
                for sample in obj.trail:
                    if now-sample[0]<=15:points.extend((right-(now-sample[0])/15*(right-left),mid-sample[axis+1]/extent*(bottom-top)/2))
                if len(points)>=4:c.create_line(*points,fill=color,width=2)
                c.create_text(90+axis*100,30,text=label,fill=color)
            return
        cx,cy=w/2,h/2+10;scale=min(w,h)*.34/extent
        c.create_line(cx-extent*scale,cy,cx+extent*scale,cy,fill='#365678');c.create_line(cx,cy-extent*scale,cx,cy+extent*scale,fill='#365678')
        if mode in ('Top','Polar'):
            for ratio in (.5,1):
                r=extent*scale*ratio;c.create_oval(cx-r,cy-r,cx+r,cy+r,outline='#365678')
            if mode=='Polar':
                c.create_text(cx,cy-extent*scale-14,text='0° FRONT',fill=MUTED);c.create_text(cx-extent*scale-15,cy,text='+90°',fill=MUTED);c.create_text(cx+extent*scale+15,cy,text='−90°',fill=MUTED)
        if mode=='3D':
            for p,color,label in [((extent,0,0),ORANGE,'front'),((0,extent,0),CYAN,'right'),((0,0,extent),PURPLE,'up')]:
                x,y=project(p,mode);c.create_line(cx,cy,cx+x*scale,cy-y*scale,fill=color);c.create_text(cx+x*scale,cy-y*scale-12,text=label,fill=color)
        def xy(point):
            if mode=='Polar':
                f,r,u=point;az=math.atan2(-r,f);radius=math.hypot(f,r,u);return -math.sin(az)*radius,math.cos(az)*radius
            return project(point,mode)
        for obj in objects:
            color=obj.color if now-obj.position_time<3 else MUTED
            if self.trails.get():
                points=[]
                for _,f,r,u in list(obj.trail)[-60:]:
                    x,y=xy((f,r,u));points.extend((cx+x*scale,cy-y*scale))
                if len(points)>=4:c.create_line(*points,fill=color,width=1)
            x,y=xy(obj.position);x,y=cx+x*scale,cy-y*scale
            c.create_oval(x-5,y-5,x+5,y+5,fill='' if obj.mute else color,outline=color,width=2)
            c.create_text(x+8,y-9,anchor='w',text=f'{obj.protocol} {obj.object_id}',fill=color)
