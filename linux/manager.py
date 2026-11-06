#!/usr/bin/env python3
"""Native GTK3 frontend; no Wine, browser or web server."""
import argparse
import csv
import math
from pathlib import Path
import struct
import threading
import time

import gi
gi.require_version('Gtk','3.0')
gi.require_foreign('cairo')
from gi.repository import Gtk, Gdk, GLib
import cairo
from backend import *

NAMES=['Lambda','Temperature','Heater','Pump PWM','Nernst DC','ESR']
UNITS=['','°C','%','%','V','Ohm']
COLORS=['#3ddceb','#ffb265','#ffd567','#b39cff','#4fdaa6','#ff87bb']

def box(vertical=True,spacing=10):return Gtk.Box(orientation=Gtk.Orientation.VERTICAL if vertical else Gtk.Orientation.HORIZONTAL,spacing=spacing)
def label(text):
    w=Gtk.Label(label=text,xalign=0);w.set_line_wrap(True);return w
def button(text,fn):
    w=Gtk.Button(label=text);w.connect('clicked',lambda *_:fn());return w
def combo(items):
    w=Gtk.ComboBoxText()
    for s in items:w.append_text(s)
    w.set_active(0);return w
def tab(text):return Gtk.Label(label=text)
def scroll(widget):
    s=Gtk.ScrolledWindow();s.set_policy(Gtk.PolicyType.AUTOMATIC,Gtk.PolicyType.AUTOMATIC);s.add(widget);return s
def textview():
    w=Gtk.TextView();w.set_editable(False);w.set_monospace(True);w.set_wrap_mode(Gtk.WrapMode.NONE);return w
def write_text(w,text):w.get_buffer().set_text(text)
def rgb(color):return tuple(int(color[i:i+2],16)/255 for i in (1,3,5))

class Graph(Gtk.Box):
    def __init__(self,app):
        super().__init__(orientation=Gtk.Orientation.VERTICAL,spacing=8);self.app=app
        self.draws=0;self.span=60;self.frozen=None;self.end=0;self.paused_at=0;self.cursor=None;self.drag=None;self.automatic=False
        toolbar=box(False,6);self.pack_start(toolbar,False,False,0);self.toggles=[]
        for n,color in zip(NAMES,COLORS):
            w=Gtk.ToggleButton(label=n);w.set_active(True);w.connect('toggled',lambda *_:self.area.queue_draw());toolbar.pack_start(w,False,False,0);self.toggles.append(w)
        self.pause=button('Pause',self.toggle_pause);toolbar.pack_end(self.pause,False,False,0)
        row=box(False,10);self.pack_start(row,False,False,0)
        self.window=combo(['15 seconds','30 seconds','60 seconds','5 minutes']);self.window.set_active(2);self.window.connect('changed',self.change_span)
        row.pack_start(label('Time'),False,False,0);row.pack_start(self.window,False,False,0)
        w=Gtk.CheckButton(label='Auto scales');w.connect('toggled',lambda b:self.auto(b.get_active()));row.pack_start(w,False,False,0)
        row.pack_start(button('CSV time window',self.export),False,False,0)
        row.pack_start(label('Wheel: zoom  /  Drag: history'),False,False,0)
        self.area=Gtk.DrawingArea();self.area.set_can_focus(True);self.area.set_size_request(640,400)
        self.area.add_events(Gdk.EventMask.POINTER_MOTION_MASK|Gdk.EventMask.BUTTON_PRESS_MASK|Gdk.EventMask.BUTTON_RELEASE_MASK|Gdk.EventMask.SCROLL_MASK|Gdk.EventMask.SMOOTH_SCROLL_MASK)
        self.area.connect('draw',self.draw);self.area.connect('motion-notify-event',self.motion);self.area.connect('button-press-event',self.press);self.area.connect('button-release-event',lambda *_:setattr(self,'drag',None))
        self.area.connect('scroll-event',self.wheel);self.area.connect('key-press-event',self.key)
        self.pack_start(self.area,True,True,0)
    def change_span(self,w):self.span=[15,30,60,300][w.get_active()];self.area.queue_draw()
    def auto(self,value):self.automatic=value;self.area.queue_draw()
    def points(self):return self.frozen if self.frozen is not None else self.app.core.points(self.app.index.get_value_as_int())
    def endpoint(self):return self.end if self.frozen is not None else self.app.ms()
    def visible(self):
        end=self.endpoint();return [p for p in self.points() if 0<=end-p[0]<=self.span*1000]
    def toggle_pause(self):
        if self.frozen is None:
            self.frozen=self.points();self.end=self.paused_at=self.app.ms();self.frozen_index=self.app.index.get_value_as_int();self.pause.set_label('Resume / Live')
        else:self.frozen=None;self.pause.set_label('Pause')
        self.area.queue_draw()
    def pan(self,delta):
        if self.frozen is None:self.toggle_pause()
        oldest=self.frozen[0][0] if self.frozen else self.paused_at
        self.end=max(oldest,min(self.paused_at,self.end+delta));self.area.queue_draw()
    def press(self,w,e):
        w.grab_focus()
        if e.button==1:
            if self.frozen is None:self.toggle_pause()
            self.drag=(e.x,self.end)
        return True
    def motion(self,w,e):
        self.cursor=e.x
        if self.drag:
            self.end=self.drag[1];self.pan(-(e.x-self.drag[0])*self.span*1000/max(1,w.get_allocated_width()-150))
        w.queue_draw();return True
    def wheel(self,w,e):
        up=e.direction==Gdk.ScrollDirection.UP or (e.direction==Gdk.ScrollDirection.SMOOTH and e.delta_y<0)
        if e.state&Gdk.ModifierType.SHIFT_MASK:self.pan((-1 if up else 1)*self.span*100)
        else:self.window.set_active(max(0,min(3,self.window.get_active()+(-1 if up else 1))))
        return True
    def key(self,w,e):
        if e.keyval==Gdk.KEY_space:self.toggle_pause()
        elif e.keyval==Gdk.KEY_Home:
            if self.frozen is not None:self.toggle_pause()
        elif e.keyval in (Gdk.KEY_plus,Gdk.KEY_equal,Gdk.KEY_minus):self.window.set_active(max(0,min(3,self.window.get_active()+ (1 if e.keyval==Gdk.KEY_minus else -1))))
        else:return False
        return True
    def export_to(self,path):
        points=self.visible();index=self.frozen_index if self.frozen is not None else self.app.index.get_value_as_int()
        with open(path,'w',newline='') as f:
            writer=csv.writer(f);writer.writerow(['source','elapsed_s','index','lambda','temp_C','heater_percent','pump_percent','nernst_V','esr_ohm','lambda_valid','std_fresh','diag_fresh'])
            for p in points:writer.writerow(['DEMO' if self.app.demo else 'CAN',f'{p[0]/1000:.3f}',index]+[f'{p[1+i]:.4f}' if p[7+i] else '' for i in range(6)]+[int(p[7]),int(p[13]),int(p[14])])
    def export(self):
        path=self.app.choose('Save visible graph window',True,'wideband-traces.csv')
        if path:
            try:self.export_to(path);self.app.note('Visible graph window exported.')
            except OSError as e:self.app.error(str(e))
    def draw(self,w,c):
        self.draws+=1
        width=w.get_allocated_width();height=w.get_allocated_height();c.set_source_rgb(*rgb('#111e2e'));c.paint()
        def text(x,y,s,color='#a5b4c5',size=13):
            c.set_source_rgb(*rgb(color));c.select_font_face('Sans',cairo.FONT_SLANT_NORMAL,cairo.FONT_WEIGHT_NORMAL);c.set_font_size(size);c.move_to(x,y);c.show_text(s)
        end=self.endpoint();points=self.visible();left,right=76,width-76;top=55;lane=(height-100)/4
        index=self.frozen_index if self.frozen is not None else self.app.index.get_value_as_int()
        text(14,24,f'{"PAUSED" if self.frozen is not None else "LIVE"}  /  Index {index}  /  {self.span} s'+('  /  DEMO - SIMULATED' if self.app.demo else ''))
        enabled=[b.get_active() for b in self.toggles]
        nearest=None
        if self.cursor is not None and points:
            target=end-self.span*1000+(self.cursor-left)/(right-left)*self.span*1000;nearest=min(points,key=lambda p:abs(p[0]-target))
        for row,metrics in enumerate(((0,),(1,5),(2,3),(4,))):
            y=top+row*lane;bottom=y+lane-25
            for j in range(3):
                gy=y+(bottom-y)*j/2;c.set_source_rgb(*rgb('#2b3b4e'));c.set_line_width(1);c.move_to(left,gy);c.line_to(right,gy);c.stroke()
            for j in range(5):
                x=left+(right-left)*j/4;c.set_source_rgb(*rgb('#2b3b4e'));c.move_to(x,y);c.line_to(x,bottom);c.stroke()
                if row==3:text(x-20,bottom+20,f'{(end-self.span*1000+self.span*1000*j/4)/1000:.1f}s',size=11)
            if row==2:common=[p[1+k] for k in metrics if enabled[k] for p in points if p[7+k]]
            for pos,k in enumerate(metrics):
                if not enabled[k]:continue
                values=common if row==2 else [p[1+k] for p in points if p[7+k]]
                lo,hi=self.app.core.bounds(k,self.automatic,values)
                color=COLORS[k];axis=right+8 if pos==1 and row==1 else 8
                if pos==0 or row==1:
                    for j in range(3):text(axis,y+5+(bottom-y)*j/2,f'{hi-(hi-lo)*j/2:.2f}' if k in (0,4) else f'{hi-(hi-lo)*j/2:.0f}',color,11)
                value=f'{nearest[1+k]:.4g} {UNITS[k]}' if nearest is not None and nearest[7+k] else '--'
                text(left+pos*260,y-8,f'{NAMES[k]} {value}',color)
                def py(v):return bottom-max(0,min(1,(v-lo)/(hi-lo)))*(bottom-y)
                if k==0:
                    c.set_source_rgb(.45,.5,.55);c.set_dash([4,4]);c.move_to(left,py(1));c.line_to(right,py(1));c.stroke();c.set_dash([])
                c.set_source_rgb(*rgb(color));c.set_line_width(1.7);previous=None
                for p in points:
                    x=left+(p[0]-(end-self.span*1000))/(self.span*1000)*(right-left)
                    if not p[7+k]:
                        if k==0 and p[13]:
                            c.stroke();c.set_source_rgb(1,.3,.3);c.move_to(x,bottom-4);c.line_to(x,bottom);c.stroke();c.set_source_rgb(*rgb(color))
                        previous=None;continue
                    yy=py(p[1+k])
                    if previous is None or p[0]-previous[0]>350:c.move_to(x,yy)
                    else:c.line_to(x,yy)
                    previous=p
                c.stroke()
        if self.cursor is not None and left<=self.cursor<=right:
            c.set_source_rgb(.75,.8,.85);c.set_dash([3,4]);c.move_to(self.cursor,top);c.line_to(self.cursor,height-70);c.stroke();c.set_dash([])
        footer='Time since program start  /  Gaps: missing or stale data  /  Red: invalid lambda'
        if not points:footer='No measurements yet - connect CAN and select a data index.'
        elif nearest is not None:footer=f'Cursor {nearest[0]/1000:.3f}s / Lambda {"valid" if nearest[7] else "invalid or missing"}'
        text(14,height-14,footer,size=12)
        return False

class Manager(Gtk.Window):
    def __init__(self,demo=False,smoke=None):
        super().__init__(title='rusEFI Wideband Manager 0.5.3 - Linux')
        settings=Gtk.Settings.get_default();settings.set_property('gtk-application-prefer-dark-theme',False);settings.set_property('gtk-theme-name','Adwaita')
        self.set_default_size(1460,1000);self.set_size_request(1060,760)
        self.demo=demo;self.smoke=smoke;self.smoke_failed=False;self.start=time.monotonic();self.offset=120000 if demo else 0
        self.core=Core();self.link=None;self.service=None;self.busy=False;self.heating=False;self.configs={};self.raw=collections.deque(maxlen=3000);self.raw_csv=None
        self.error_seen=False;self.tx_buttons=[];self.demo_step=0;self.monitor_paused=False
        self.connect('delete-event',self.closing)
        css=Gtk.CssProvider();css.load_from_data(b'''window {background:#f1f5fa;color:#1a2c3d;} * {font-family:Sans; font-size:15px;} button {padding:7px;} entry,combobox {min-height:28px;} .header {background:#122438;color:#eef5ff;padding:16px;} .card {background:white;border:1px solid #d7e1ed;border-radius:5px;padding:12px;} .value {font-size:30px;font-weight:bold;color:#008896;} .muted {color:#526b87;} .title {font-size:24px;font-weight:bold;}''')
        Gtk.StyleContext.add_provider_for_screen(Gdk.Screen.get_default(),css,Gtk.STYLE_PROVIDER_PRIORITY_APPLICATION)
        outer=box();self.add(outer)
        header=box(False);header.get_style_context().add_class('header');outer.pack_start(header,False,False,0)
        title=label('rusEFI  /  WIDEBAND');title.get_style_context().add_class('title');header.pack_start(title,False,False,0)
        header.pack_start(label('Linux 0.5.3 / CAN diagnostics & configuration'),True,True,0)
        self.badge=label('DEMO / SIMULATED' if demo else 'OFFLINE');header.pack_end(self.badge,False,False,0)
        body=box(False,14);body.set_border_width(12);outer.pack_start(body,True,True,0)
        self.sidebar=box();self.sidebar.set_border_width(10);self.sidebar.set_size_request(300,-1);self.sidebar.get_style_context().add_class('card')
        side_scroll=scroll(self.sidebar);side_scroll.set_size_request(326,-1);body.pack_start(side_scroll,False,False,0)
        self.transport=combo(['SocketCAN','SLCAN / Lawicel']);self.endpoint=Gtk.Entry(text='can0');self.transport.connect('changed',lambda w:self.endpoint.set_text('can0' if w.get_active()==0 else '/dev/ttyUSB0'))
        self.bitrate=combo(['500 kbit/s','1 Mbit/s'])
        self.add_field(self.sidebar,'CAN adapter',self.transport);self.add_field(self.sidebar,'Interface / serial device',self.endpoint);self.add_field(self.sidebar,'Bus bitrate (SLCAN)',self.bitrate)
        hint=label('SocketCAN bitrate is set on the Linux interface before connecting.');hint.get_style_context().add_class('muted');self.sidebar.pack_start(hint,False,False,0)
        self.connect_button=button('Connect / Disconnect',self.connect_bus);self.sidebar.pack_start(self.connect_button,False,False,0)
        self.add_action(self.sidebar,'Find modules',self.find_modules)
        self.modules=label('No modules discovered.');self.sidebar.pack_start(self.modules,False,False,0);self.discovered={}
        row=box(False);self.sidebar.pack_start(row,False,False,0)
        self.hw=Gtk.SpinButton.new_with_range(0,8,1);self.hw.set_value(4);self.index=Gtk.SpinButton.new_with_range(0,255,1)
        self.add_field(row,'Hardware ID',self.hw);self.add_field(row,'Data index',self.index)
        self.target_index=Gtk.SpinButton.new_with_range(0,255,1)
        self.add_field(self.sidebar,'Target data index',self.target_index)
        self.hw.connect('value-changed',self.target_changed);self.target_index.connect('value-changed',self.target_changed)
        self.sidebar.pack_start(label('Target data index addresses the current module. Data index selects the graph and proposed new index.'),False,False,0)
        self.add_action(self.sidebar,'Detect / read F0',self.detect)
        self.sensor=combo(SENSORS);self.add_field(self.sidebar,'Sensor type',self.sensor)
        self.module_rate=combo(['500 kbit/s','1 Mbit/s']);self.add_field(self.sidebar,'Module bitrate after reset',self.module_rate)
        self.standard=Gtk.CheckButton(label='Send standard data');self.standard.set_active(True)
        self.diag=Gtk.CheckButton(label='Send diagnostic data');self.diag.set_active(True)
        self.sidebar.pack_start(self.standard,False,False,0);self.sidebar.pack_start(self.diag,False,False,0)
        self.add_action(self.sidebar,'Save configuration',self.save_config)
        right=box();body.pack_start(right,True,True,0)
        cards=box(False);right.pack_start(cards,False,False,0);self.cards=[]
        for name in ['LAMBDA','SENSOR TEMPERATURE','HEATER','NERNST DC']:
            card=box();card.get_style_context().add_class('card');card.pack_start(label(name),False,False,0)
            val=label('--');val.get_style_context().add_class('value');card.pack_start(val,False,False,0);cards.pack_start(card,True,True,0);self.cards.append(val)
        self.notebook=Gtk.Notebook();self.notebook.set_scrollable(True);right.pack_start(self.notebook,True,True,0)
        self.graph=Graph(self);self.notebook.append_page(self.graph,tab('Live graph'))
        config=box();config.set_border_width(12);self.notebook.append_page(scroll(config),tab('F0 tools / Rtrim'))
        self.report=textview();write_text(self.report,'Detect / read F0 to load configuration and selectable Rtrim.\nHardware ID and target data index must be unique together on the CAN bus.\nConfiguration requires indexed F0 Manager firmware 0.5.3 or newer.\n'+('DEMO: simulated measurements; all CAN transmission is disabled.' if demo else 'No module configuration has been read yet.'));self.report.set_size_request(-1,120);config.pack_start(scroll(self.report),True,True,0)
        row=box(False);config.pack_start(row,False,False,0)
        for name,fn in [('Save backup',self.save_backup),('Restore backup',self.restore_backup),('Identify for 10s',self.identify),('Restart',self.restart),('Factory defaults',self.defaults)]:self.add_action(row,name,fn)
        row=box(False);config.pack_start(row,False,False,0);self.trim=Gtk.Entry(text='');self.trim.set_width_chars(8)
        row.pack_start(label('Sensor Rtrim [Ohm]'),False,False,0);row.pack_start(self.trim,False,False,0)
        self.add_action(row,'Save Rtrim',self.save_trim);self.add_action(row,'Physical Rtrim',self.physical_trim)
        config.pack_start(label("Select LSU 4.9 Rtrim SW for software correction. Enter your own sensor's labeled value: 30.00..300.00 Ohm. Do not guess. Software mode requires the physical trim resistor to be absent. Sensor and mode must match."),False,False,0)
        self.add_action(config,'Read extended diagnostics',self.extended)
        self.ext_report=label('No extended diagnostic snapshot read.');config.pack_start(self.ext_report,False,False,0)
        row=box(False);config.pack_start(row,False,False,0)
        for name,fn in [('Legacy index',lambda:self.legacy(1)),('Legacy sensor',lambda:self.legacy(2)),('Legacy bitrate',lambda:self.legacy(3))]:self.add_action(row,name,fn)
        config.pack_start(label('Legacy commands support compatible earlier firmware. Legacy ACKs have no sender identity or configuration readback.'),False,False,0)
        self.add_action(config,'Update F0 firmware over CAN…',self.flash)
        self.progress=Gtk.ProgressBar();config.pack_start(self.progress,False,False,0)
        self.events=textview();event_page=box();event_page.pack_start(scroll(self.events),True,True,0);event_page.pack_start(button('Export events CSV',self.export_events),False,False,0);self.notebook.append_page(event_page,tab('Events / Warmup'))
        stats_page=box();self.stats=textview();stats_page.pack_start(scroll(self.stats),True,True,0);stats_page.pack_start(button('Export CAN statistics CSV',self.export_stats),False,False,0);self.notebook.append_page(stats_page,tab('CAN statistics'))
        rawpage=box();row=box(False);rawpage.pack_start(row,False,False,0)
        self.filter=Gtk.Entry();self.filter.set_placeholder_text('CAN ID hex / blank=all');row.pack_start(self.filter,True,True,0)
        self.filter_type=combo(['STD + EXT','STD only','EXT only']);row.pack_start(self.filter_type,False,False,0)
        self.raw_pause=Gtk.CheckButton(label='Pause monitor');row.pack_start(self.raw_pause,False,False,0);row.pack_start(button('CAN CSV start / stop',self.toggle_csv),False,False,0)
        self.console=textview();rawpage.pack_start(scroll(self.console),True,True,0);self.notebook.append_page(rawpage,tab('CAN console'))
        heater=box(False);right.pack_start(heater,False,False,0)
        self.voltage=Gtk.Entry(text='12.0');self.voltage.set_width_chars(6);self.gain=Gtk.SpinButton.new_with_range(0,100,1);self.gain.set_value(100)
        heater.pack_start(label('Supply [V]'),False,False,0);heater.pack_start(self.voltage,False,False,0);heater.pack_start(label('Pump gain [%]'),False,False,0);heater.pack_start(self.gain,False,False,0)
        self.heat_button=self.add_action(heater,'Start heater messages',self.toggle_heat)
        self.status=label('DEMO - simulated data; CAN transmission disabled.' if demo else 'Ready - connect CAN and choose a data index.');self.status.set_margin_start(14);self.status.set_margin_bottom(10);outer.pack_start(self.status,False,False,0)
        self.show_all()
        if demo:
            self.connect_button.set_sensitive(False)
            for b in self.tx_buttons:b.set_sensitive(False)
            for ms in range(0,120000,100):self.demo_frames(ms)
        GLib.timeout_add(100,self.tick);GLib.timeout_add(50,self.heat_tick)
        if smoke:GLib.timeout_add(700,self.run_smoke)
    def ms(self):return int((time.monotonic()-self.start)*1000)+self.offset
    def add_field(self,parent,name,widget):
        col=box(spacing=3);col.pack_start(label(name),False,False,0);col.pack_start(widget,False,False,0);parent.pack_start(col,False,False,0)
    def add_action(self,parent,name,fn):
        b=button(name,fn);parent.pack_start(b,False,False,0);self.tx_buttons.append(b);return b
    def note(self,s):self.status.set_text(s)
    def error(self,s):
        self.note(s)
        d=Gtk.MessageDialog(transient_for=self,modal=True,message_type=Gtk.MessageType.ERROR,buttons=Gtk.ButtonsType.OK,text=s);d.run();d.destroy()
    def confirm(self,s):
        d=Gtk.MessageDialog(transient_for=self,modal=True,message_type=Gtk.MessageType.WARNING,buttons=Gtk.ButtonsType.OK_CANCEL,text=s);ok=d.run()==Gtk.ResponseType.OK;d.destroy();return ok
    def choose(self,title,save=False,name=None):
        d=Gtk.FileChooserDialog(title=title,transient_for=self,action=Gtk.FileChooserAction.SAVE if save else Gtk.FileChooserAction.OPEN)
        d.add_buttons('Cancel',Gtk.ResponseType.CANCEL,'Save' if save else 'Open',Gtk.ResponseType.OK)
        if save:d.set_do_overwrite_confirmation(True)
        if name and save:d.set_current_name(name)
        result=d.get_filename() if d.run()==Gtk.ResponseType.OK else None;d.destroy();return result
    def connect_bus(self):
        if self.demo:return
        if self.link:
            self.stop_heat();self.link.close();self.link=self.service=None;self.target_changed();self.note('Disconnected');return
        try:
            cls=SocketCAN if self.transport.get_active()==0 else SLCAN
            self.link=Link(cls(self.endpoint.get_text().strip(),[500000,1000000][self.bitrate.get_active()]));self.service=Service(self.link)
            self.core.lib.wb_clear();self.target_changed();self.discovered.clear();self.error_seen=False
            self.note('Connected - passive CAN reception active')
        except Exception as e:self.error(str(e))
    def require_link(self):
        if self.demo:raise ValueError('Demo mode blocks CAN transmission')
        if not self.link or self.link.error:raise ValueError('Connect CAN first')
    def work(self,fn,done=None):
        if self.busy:return
        try:self.require_link()
        except Exception as e:self.error(str(e));return
        self.busy=True;self.stop_heat();self.sidebar.set_sensitive(False)
        for b in self.tx_buttons:b.set_sensitive(False)
        self.note('CAN operation in progress…')
        def finish(result,error):
            self.busy=False;self.sidebar.set_sensitive(True)
            for b in self.tx_buttons:b.set_sensitive(True)
            if error:self.error(error)
            elif done:done(result)
            else:self.note('CAN operation completed.')
            return False
        def run():
            try:result=fn();GLib.idle_add(finish,result,None)
            except Exception as e:GLib.idle_add(finish,None,str(e))
        threading.Thread(target=run,daemon=True).start()
    def target(self):return (self.hw.get_value_as_int(),self.target_index.get_value_as_int())
    def settings(self):return [self.index.get_value_as_int(),self.sensor.get_active(),self.module_rate.get_active(),int(self.standard.get_active())+2*int(self.diag.get_active())]
    def result(self,hw,result):
        cfg,trim,ready=result;new=(hw[0],cfg[0]);self.configs.pop(hw,None);self.configs[new]=result
        self.target_index.handler_block_by_func(self.target_changed);self.target_index.set_value(cfg[0]);self.target_index.handler_unblock_by_func(self.target_changed)
        self.ext_report.set_text('No extended diagnostic snapshot read.')
        self.index.set_value(cfg[0]);self.sensor.set_active(cfg[1]);self.module_rate.set_active(cfg[2]);self.standard.set_active(bool(cfg[3]&1));self.diag.set_active(bool(cfg[3]&2));self.trim.set_text(f'{trim/100:.2f}')
        write_text(self.report,f'F0 HW {hw[0]} / data index {cfg[0]} / CAN {0x190+cfg[0]*2:03X}, {0x191+cfg[0]*2:03X}\nSensor: {SENSORS[cfg[1]]}\nSaved bitrate: {[500,1000][cfg[2]]} kbit/s; active: {[500,1000][cfg[4]]} kbit/s\nTX flags: {cfg[3]} / Rtrim: {trim/100:.2f} Ohm / calibration: {"ready" if ready else "not ready"}')
        self.note('F0 configuration and Rtrim read and verified. Bitrate changes require a restart.')
    def target_changed(self,*_):
        self.configs.clear()
        if self.service:self.service.known.clear()
        if hasattr(self,'trim'):self.trim.set_text('')
        if hasattr(self,'report'):write_text(self.report,'Detect / read F0 first. Hardware ID and target data index must identify one module.')
        if hasattr(self,'ext_report'):self.ext_report.set_text('No extended diagnostic snapshot read.')
    def detect(self):
        hw=self.target();self.work(lambda:self.service.detect(hw),lambda r:self.result(hw,r))
    def find_modules(self):self.work(lambda:self.link.send((PING,True,b'\xff')))
    def save_config(self):
        hw=self.target();cfg=self.settings()
        if self.confirm('Save configuration permanently and verify readback? Hardware ID and target data index must identify one module. Check sensor compatibility. Bitrate changes apply after restart; restart after changing sensor type.'):
            self.work(lambda:self.service.apply(hw,cfg),lambda r:self.result(hw,r))
    def save_backup(self):
        hw=self.target()
        try:
            self.require_link();self.service.require(hw)
            if hw not in self.configs:raise ValueError('Detect / read F0 first')
            cfg,trim,_=self.configs[hw];data=self.core.backup(hw[0],cfg,trim)
            path=self.choose('Save verified F0 backup',True,'wideband-f0.wbf')
            if path:Path(path).write_bytes(data);self.note('Verified configuration/Rtrim backup saved with CRC.')
        except Exception as e:self.error(str(e))
    def restore_backup(self):
        path=self.choose('Load F0 backup')
        if not path:return
        try:
            data=Path(path).read_bytes();cfg,trim=self.core.restore(data);hw=self.target()
            old=self.configs.get(hw)
            summary=f'Backup source HW {data[6]}, target HW {hw[0]}\nIndex/sensor/bitrate/TX: {cfg}\nRtrim: {trim/100:.2f} Ohm\nCurrent: {old}\n\nRestore all settings? Sensor and hardware must match. Two verified writes; errors may leave partial changes. Do not interrupt power.'
            if self.confirm(summary):self.work(lambda:self.service.restore(hw,cfg,trim),lambda r:self.result(hw,r))
        except Exception as e:self.error(str(e))
    def save_trim(self):
        try:
            from decimal import Decimal
            resistance=Decimal(self.trim.get_text().replace(',','.'));hundred=resistance*100
            if not hundred.is_finite() or hundred!=hundred.to_integral() or not 3000<=hundred<=30000:raise ValueError('Use your sensor label value, 30.00..300.00 Ohm, at most two decimals.')
            hw=self.target();sensor=self.sensor.get_active();value=int(hundred)
            if sensor not in (0,4):raise ValueError('Rtrim requires Bosch LSU4.9 or LSU4.9 Rtrim SW.')
            if self.confirm('Save labeled sensor Rtrim and selected Bosch mode? Software mode requires the physical trim resistor to be absent. Sensor and mode must match.'):
                self.work(lambda:self.service.rtrim(hw,value,sensor),lambda r:self.result(hw,r))
        except Exception as e:self.error(str(e))
    def physical_trim(self):
        hw=self.target()
        if self.confirm('Select Bosch LSU4.9 with a physical trim resistor? Hardware must match. Software correction will be disabled.'):
            self.work(lambda:self.service.rtrim(hw,0,0),lambda r:self.result(hw,r))
    def identify(self):
        hw=self.target()
        def run():self.service.require(hw);return self.service.request(IDENTIFY,hw,bytes((10,)))
        self.work(run,lambda _:self.note('Identification acknowledged: LEDs alternate for 10 seconds.'))
    def restart(self):
        hw=self.target()
        def run():
            self.service.require(hw);self.service.request(RESET,hw,b'\xa5');self.service.known.pop(hw,None)
        if self.confirm('Restart F0? Saved bitrate becomes active. Adjust adapter/interface bitrate afterwards if necessary.'):
            self.work(run,lambda _:self.note('Restart acknowledged. Detect / read F0 again.'))
    def defaults(self):
        hw=self.target()
        if self.confirm('Write factory defaults? Index follows solder bridges, sensor follows firmware default, TX enabled. Saved bitrate is retained. Back up first.'):
            self.work(lambda:self.service.defaults(hw),lambda r:self.result(hw,r))
    def extended(self):
        hw=self.target()
        def run():
            self.service.require(hw);return self.service.request(DIAG1,hw),self.service.request(DIAG2,hw)
        def done(r):
            a,b=r;ac,target,state=struct.unpack('<HhB',a);current,voltage,supply=struct.unpack('<hHB',b)
            self.ext_report.set_text(f'CAN snapshot / HW {hw[0]} / index {hw[1]}: Nernst AC {ac/1000:.3f} V; pump target {target/1000:.3f} mA; measured {current/1000:.3f} mA; effective heater {voltage/1000:.3f} V; supply over CAN {supply/10:.1f} V; heater state {state}.');self.note('Extended diagnostics updated (snapshot).')
        self.work(run,done)
    def legacy(self,kind):
        hw=self.target();value=self.settings()[{1:0,2:1,3:2}[kind]]
        def run():
            ident={1:INDEX,2:SENSOR,3:BITRATE}[kind];data=bytes((value,hw[0])) if kind==1 else bytes((hw[0],value))
            self.link.exchange((ident,True,data),lambda f:f[1] and ((kind==3 and f[0]==BITRATE+1 and f[2]==bytes((hw[0],value))) or (kind!=3 and f[0]==ACK and not f[2])))
        if self.confirm('Write legacy setting permanently? Hardware ID must be unique. Legacy ACKs have no sender ID or readback. Do not use other configuration tools concurrently.'):
            self.work(run,lambda _:self.note('Legacy command acknowledged; no configuration readback. Bitrate changes require reset.'))
    def flash(self):
        path=self.choose('Select F0 application BIN with CRC')
        if not path:return
        try:
            data=Path(path).read_bytes();hw=self.target()
            if not self.core.valid_image(data):raise ValueError('Invalid F0 app image: expected 25600 bytes, vectors at 0x08001800 and matching CRC. Full/bootloader images rejected.')
            if self.confirm('Update F0 application? Connect exactly ONE compatible module to the bus. Bootloader commands are broadcast. Application will be erased and replaced. Keep power stable.'):
                self.work(lambda:self.service.flash(hw[0],data,lambda x:GLib.idle_add(self.progress.set_fraction,x)),lambda _:self.note('Image transferred and restart acknowledged. Check Pong/live data; protocol has no flash readback.'))
        except Exception as e:self.error(str(e))
    def stop_heat(self):
        if self.heating and self.link and not self.link.ecu:
            try:self.link.send((STATUS,True,bytes((120,0,0))))
            except Exception:pass
        self.heating=False
        if hasattr(self,'heat_button'):self.heat_button.set_label('Start heater messages')
    def toggle_heat(self):
        if self.heating:self.stop_heat();self.note('Heater messages stopped; firmware timeout/fallback applies after 250 ms.');return
        try:
            self.require_link();v=float(self.voltage.get_text())
            if not 0<=v<=25.5:raise ValueError('Supply must be 0..25.5 V, decimal point.')
            if self.link.ecu:raise ValueError('ECU status sender detected. Remove ECU sender and reconnect before bench heater control.')
            if self.confirm('Enable heating on all modules? Enter actual supply voltage. Sensor must be dry and suitable for heating. Commands apply to the entire bus.'):
                self.heat_values=bytes((int(v*10+.5),1,self.gain.get_value_as_int()));self.heating=True;self.heat_button.set_label('Stop heater messages')
        except Exception as e:self.error(str(e))
    def heat_tick(self):
        if self.heating:
            if not self.link or self.link.error or self.link.ecu:self.stop_heat();self.note('Heater messages stopped: ECU sender or CAN error.')
            else:
                try:self.link.send((STATUS,True,self.heat_values))
                except Exception as e:self.stop_heat();self.note(str(e))
        return True
    def toggle_csv(self):
        if self.raw_csv:self.raw_csv.close();self.raw_csv=None;self.note('Raw CAN CSV closed.');return
        path=self.choose('Save raw CAN CSV',True,'wideband-can.csv')
        if path:
            try:self.raw_csv=open(path,'w',newline='');csv.writer(self.raw_csv).writerow(['elapsed_s','direction','format','id_hex','data_hex']);self.note('Raw CAN CSV recording started.')
            except Exception as e:self.error(str(e))
    def export_events(self):
        path=self.choose('Export session events',True,'wideband-events.csv')
        if path:
            try:
                with open(path,'w',newline='') as f:
                    writer=csv.writer(f);writer.writerow(['elapsed_ms','index','event_code']);writer.writerows(self.core.events())
            except Exception as e:self.error(str(e))
    def export_stats(self):
        path=self.choose('Export CAN statistics',True,'wideband-can-statistics.csv')
        if path:
            try:
                with open(path,'w',newline='') as f:
                    writer=csv.writer(f);writer.writerow(['id','ext','dlc','count','first_ms','last_ms','max_gap_ms','average_hz']);writer.writerows(self.core.stats())
            except Exception as e:self.error(str(e))
    def demo_frames(self,ms):
        for idx in range(3):
            phase=ms/1000+idx
            standard=bytes((0xa0,1))+struct.pack('<HH',int((1+.035*math.sin(phase*.8)+idx*.02)*10000),int(790+15*math.sin(phase*.1)))+b'\0\0'
            diag=struct.pack('<HHBBBB',int(300+40*math.sin(phase*.2)),int(450+15*math.sin(phase*.8)),int(100+15*math.sin(phase)),2,int(145+20*math.sin(phase*.3)),2)
            self.core.feed((0x190+2*idx,False,standard),ms);self.core.feed((0x191+2*idx,False,diag),ms)
    def tick(self):
        now=self.ms()
        if self.demo:self.demo_frames(now)
        if self.link:
            for tx,frame,when in self.link.drain():
                ms=int((when-self.start)*1000)+self.offset;self.core.feed(frame,ms,tx)
                ident,ext,data=frame;row=(ms/1000,'TX' if tx else 'RX','EXT' if ext else 'STD',f'{ident:08X}',data.hex().upper());self.raw.append(row)
                if self.raw_csv:
                    try:csv.writer(self.raw_csv).writerow(row)
                    except OSError as e:self.raw_csv.close();self.raw_csv=None;self.note(str(e))
                if not tx and ext and ident==ACK and len(data)==8:
                    self.discovered[data[0]]=f'HW {data[0]} / v{data[1]:02X} / build {2000+data[2]:04}-{data[3]:02}-{data[4]:02}'
            if self.link.error and not self.error_seen:self.error_seen=True;self.stop_heat();self.note('CAN error: '+self.link.error)
        self.core.lib.wb_tick(now);self.graph.area.queue_draw();pts=self.core.points(self.index.get_value_as_int());latest=pts[-1] if pts else None
        for card,k in zip(self.cards,(0,1,2,4)):
            card.set_text((f'{latest[1+k]:.4f}' if k==0 else f'{latest[1+k]:.1f}')+(' '+UNITS[k] if UNITS[k] else '') if latest and latest[7+k] and now-latest[0]<=500 else '--')
        self.badge.set_text('DEMO / SIMULATED' if self.demo else 'CAN ERROR' if self.link and self.link.error else 'CAN CONNECTED' if self.link else 'OFFLINE')
        self.modules.set_text('\n'.join(self.discovered.values()) or ('DEMO: data indices 0 / 1 / 2' if self.demo else 'No modules discovered.'))
        view=self.notebook.get_current_page()
        if view==2:
            rows=self.core.events();lines=['SESSION EVENTS / PC elapsed time; no error history stored in module.']
            for t,i,code in rows[-300:]:lines.append(f'{t/1000:9.2f}s / Index {i} / '+(STATES[code] if code<8 else {100:'WARNING: data lost',101:'Data restored',102:'WARNING: invalid lambda in closed loop'}.get(code,'Unknown')))
            for idx in range(256):
                warm=[(t,code) for t,i,code in rows if i==idx and code in (1,2)]
                if len(warm)>=2 and warm[-2][1]==1 and warm[-1][1]==2:lines.append(f'Index {idx}: observed warmup {(warm[-1][0]-warm[-2][0])/1000:.2f}s')
            write_text(self.events,'\n'.join(lines))
        if view==3:write_text(self.stats,'PC timestamp statistics; not hardware bus-load/error counters.\n'+'\n'.join(f'{"EXT" if r[1] else "STD"} {int(r[0]):08X} / {int(r[3])} frames / DLC {int(r[2])} / {r[7]:.1f} Hz / max gap {r[6]:.0f} ms' for r in self.core.stats()))
        if view==4 and not self.raw_pause.get_active():
            text=self.filter.get_text().strip();kind=self.filter_type.get_active()
            try:ident=int(text,16) if text else None
            except ValueError:ident=-1
            rows=[r for r in self.raw if (ident is None or int(r[3],16)==ident) and (kind==0 or (r[2]=='STD')==(kind==1))]
            write_text(self.console,'\n'.join(f'{r[0]:10.3f} {r[1]} {r[2]} {r[3]} {r[4]}' for r in rows[-500:]))
        return True
    def closing(self,*_):
        if self.busy:self.note('CAN operation in progress. Wait for completion or timeout before closing.');return True
        self.stop_heat()
        if self.link:self.link.close()
        if self.raw_csv:self.raw_csv.close()
        Gtk.main_quit();return False
    def screenshot(self,path):
        window=self.get_window();allocation=self.get_allocation();pix=Gdk.pixbuf_get_from_window(window,0,0,allocation.width,allocation.height);pix.savev(str(path),'png',[],[])
    def run_smoke(self):
        try:
            assert self.graph.draws>0, "Graph draw callback did not run"
            folder=Path(self.smoke);folder.mkdir(parents=True,exist_ok=True)
            if self.demo:
                assert not self.connect_button.get_sensitive() and all(not b.get_sensitive() for b in self.tx_buttons)
                assert len(self.graph.points())>1000
                self.graph.toggle_pause();self.graph.export_to(folder/'paused-a.csv');before=list(self.graph.visible());self.demo_frames(self.ms()+500);self.graph.export_to(folder/'paused-b.csv')
                assert (folder/'paused-a.csv').read_bytes()==(folder/'paused-b.csv').read_bytes()
                self.graph.pan(-10000);assert self.graph.visible()[-1][0]<before[-1][0]-9000
                self.graph.window.set_active(0);assert self.graph.span==15
                for b in self.graph.toggles:b.set_active(False)
                for b in self.graph.toggles:b.set_active(True)
                self.graph.auto(True);self.graph.auto(False);self.graph.window.set_active(2);self.graph.toggle_pause();self.index.set_value(1);assert self.graph.points();self.index.set_value(0)
            else:assert not self.graph.points() and self.connect_button.get_sensitive()
            self.result((4,0),([7,4,1,3,0],12345,True))
            assert self.target()==(4,7) and self.trim.get_text()=='123.45'
            assert (4,0) not in self.configs and (4,7) in self.configs
            self.ext_report.set_text('Snapshot from previous target')
            self.target_index.set_value(1)
            assert not self.configs and not self.trim.get_text()
            assert self.ext_report.get_text()=='No extended diagnostic snapshot read.'
            self.target_index.set_value(0)
            self.result((4,0),([0,4,1,3,0],12345,True))
            self.graph.area.queue_draw()
            def capture():
                try:
                    self.screenshot(folder/('preview-demo.png' if self.demo else 'preview-disconnected.png'));print('GTK GUI smoke passed: '+('demo safety, six traces, paused CSV, history, time windows, scales and index switch' if self.demo else 'normal disconnected startup; no synthetic data'))
                    if self.demo:
                        self.notebook.set_current_page(1)
                        GLib.timeout_add(200,capture_tools)
                        return False
                except Exception as e:self.smoke_failed=True;print('SMOKE FAILED:',e)
                self.closing();return False
            def capture_tools():
                try:self.screenshot(folder/'preview-tools.png');self.notebook.set_current_page(0);self.resize(1280,800);GLib.timeout_add(250,capture_small)
                except Exception as e:self.smoke_failed=True;print('SMOKE FAILED:',e);self.closing()
                return False
            def capture_small():
                try:self.screenshot(folder/'preview-small.png');assert self.graph.area.get_allocated_width()>=640;print('GTK tools tab and smaller window rendering passed.')
                except Exception as e:self.smoke_failed=True;print('SMOKE FAILED:',e)
                self.closing();return False
            GLib.timeout_add(300,capture)
        except Exception as e:self.smoke_failed=True;print('SMOKE FAILED:',e);self.closing()
        return False

def main():
    parser=argparse.ArgumentParser(description='rusEFI Wideband Manager - native Linux CAN client')
    parser.add_argument('--demo',action='store_true',help='Simulated data; CAN transmission disabled')
    parser.add_argument('--smoke-test',metavar='OUTPUT_DIR',help=argparse.SUPPRESS)
    args=parser.parse_args();app=Manager(args.demo,args.smoke_test);Gtk.main();return int(app.smoke_failed)
if __name__=='__main__':raise SystemExit(main())
