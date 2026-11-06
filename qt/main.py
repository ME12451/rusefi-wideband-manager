#!/usr/bin/env python3
"""rusEFI Wideband Manager 0.6: one Qt application for Windows and Linux."""
import argparse
import collections
import csv
import json
import os
from pathlib import Path
import struct
import sys
import threading
import time

from PySide6.QtCore import Qt, QTimer, QObject, Signal, QSettings
from PySide6.QtGui import QFont
from PySide6.QtWidgets import (QApplication,QMainWindow,QWidget,QLabel,QPushButton,QLineEdit,
    QSpinBox,QComboBox,QCheckBox,QVBoxLayout,QHBoxLayout,QGridLayout,QScrollArea,QTabWidget,
    QPlainTextEdit,QMessageBox,QFileDialog,QProgressBar,QTableWidget,QTableWidgetItem,QHeaderView)
from backend import *
from transports import create_transport
from actions import Actions
from graph import Graph,NAMES,UNITS
from i18n import I18n

# Small uniform field API keeps the platform-independent action code GUI-neutral.
class Entry(QLineEdit):
    def get_text(self):return self.text()
    def set_text(self,value):self.setText(value)
class Spin(QSpinBox):
    def get_value_as_int(self):return self.value()
    def set_value(self,value):self.setValue(value)
class Combo(QComboBox):
    def get_active(self):return self.currentIndex()
    def set_active(self,value):self.setCurrentIndex(value)
class Check(QCheckBox):
    def get_active(self):return self.isChecked()
    def set_active(self,value):self.setChecked(value)
class Button(QPushButton):
    def set_label(self,text):self.setText(text)
class Text(QPlainTextEdit):
    def set_text(self,text):self.setPlainText(text)
class Progress(QProgressBar):
    def set_fraction(self,value):self.setValue(round(value*100))
class Bridge(QObject):
    finished=Signal(object,object)
    progress=Signal(float)

class Manager(QMainWindow,Actions):
    def __init__(self,args):
        super().__init__();self.args=args;self.demo=args.demo;self.start=time.monotonic();self.offset=120000 if self.demo else 0
        self.core=Core();self.link=None;self.service=None;self.busy=False;self.heating=False;self.configs={};self.raw=collections.deque(maxlen=3000);self.raw_csv=None
        self.error_seen=False;self.tx_buttons=[];self.bindings=[];self.tab_keys=[];self.discovered={};self.active_indices=set();self.extra=None
        self.status_key='DEMO - simulated data; CAN transmission disabled.' if self.demo else 'Ready - connect CAN and choose a data index.';self.status_values={};self.last_error=None
        self.settings=QSettings(args.settings_file,QSettings.Format.IniFormat) if args.settings_file else QSettings('rusEFI','WidebandManager06')
        self.i18n=I18n(QApplication.instance(),args.language or self.settings.value('language','en'));self.tr=self.i18n.text
        self.bridge=Bridge();self.bridge.finished.connect(self.finish_work);self.bridge.progress.connect(self.set_progress);self.done_callback=None;self.operation_is_flash=False
        self.setWindowTitle('rusEFI Wideband Manager 0.6');self.resize(1540,1040);self.setMinimumSize(1160,760)
        root=QWidget();self.setCentralWidget(root);outer=QVBoxLayout(root);outer.setContentsMargins(0,0,0,0);outer.setSpacing(0)
        header=QWidget();header.setObjectName('Header');hr=QHBoxLayout(header);hr.setContentsMargins(22,14,22,14);outer.addWidget(header)
        brand=QLabel('rusEFI  /  WIDEBAND');brand.setObjectName('Brand');hr.addWidget(brand)
        subtitle=self.label('CAN diagnostics & configuration');subtitle.setMinimumWidth(325);hr.addWidget(subtitle);hr.addWidget(QLabel('0.6'));hr.addStretch()
        self.badge=QLabel();hr.addWidget(self.badge);hr.addSpacing(18);hr.addWidget(self.label('Language'))
        self.language=QComboBox();self.language.setObjectName('Language');self.language.addItem('English','en');self.language.addItem('Deutsch','de');self.language.setCurrentIndex(1 if self.i18n.language=='de' else 0);hr.addWidget(self.language)
        self.language.currentIndexChanged.connect(self.language_changed)
        body=QHBoxLayout();body.setContentsMargins(14,14,14,10);body.setSpacing(14);outer.addLayout(body,1)
        self.sidebar=QWidget();self.sidebar.setObjectName('Card');side=QVBoxLayout(self.sidebar);side.setContentsMargins(14,14,14,14);side.setSpacing(9)
        side_scroll=QScrollArea();side_scroll.setWidgetResizable(True);side_scroll.setWidget(self.sidebar);side_scroll.setMinimumWidth(316);side_scroll.setMaximumWidth(342);side_scroll.setFrameShape(QScrollArea.Shape.NoFrame);body.addWidget(side_scroll)
        section=self.label('Connection');section.setObjectName('Section');side.addWidget(section)
        self.transport=Combo();self.transport.addItem('PCAN-USB','pcan') if os.name=='nt' else self.transport.addItem('SocketCAN','socketcan');self.transport.addItem('SLCAN / Lawicel','slcan')
        self.endpoint=Entry('PCAN_USBBUS1' if os.name=='nt' else 'can0');self.bitrate=Combo();self.bitrate.addItems(['500 kbit/s','1 Mbit/s'])
        self.field(side,'CAN adapter',self.transport);self.field(side,'Interface / port',self.endpoint);self.field(side,'Bus bitrate',self.bitrate)
        self.transport.currentIndexChanged.connect(self.adapter_changed)
        self.bus_hint=self.label('SocketCAN bitrate is configured on the Linux interface before connecting.');self.bus_hint.setObjectName('Muted');side.addWidget(self.bus_hint);self.bus_hint.setVisible(os.name!='nt')
        self.connect_button=self.button('Connect / Disconnect',self.connect_bus);side.addWidget(self.connect_button)
        self.action(side,'Find modules',self.find_modules);self.modules=Combo();self.modules.addItem('');self.modules.currentIndexChanged.connect(self.module_selected);side.addWidget(self.modules)
        side.addSpacing(6);section=self.label('Module / settings');section.setObjectName('Section');side.addWidget(section)
        grid=QGridLayout();side.addLayout(grid);self.hw=Spin();self.hw.setRange(0,8);self.hw.setValue(4);self.index=Spin();self.index.setRange(0,255)
        grid.addWidget(self.label('Hardware ID'),0,0);grid.addWidget(self.label('Data index'),0,1);grid.addWidget(self.hw,1,0);grid.addWidget(self.index,1,1)
        self.hw.valueChanged.connect(self.render_report)
        side.addWidget(self.label('Hardware ID is mapped to data index only after reading configuration.'))
        self.action(side,'Detect / read F0',self.detect)
        self.sensor=Combo();self.sensor.addItems(SENSORS);self.field(side,'Sensor type',self.sensor)
        self.module_rate=Combo();self.module_rate.addItems(['500 kbit/s','1 Mbit/s']);self.field(side,'Module bitrate after reset',self.module_rate)
        self.standard=Check();self.standard.setChecked(True);self.bind(self.standard,'Send standard data');side.addWidget(self.standard)
        self.diag=Check();self.diag.setChecked(True);self.bind(self.diag,'Send diagnostic data');side.addWidget(self.diag)
        self.action(side,'Save configuration',self.save_config);side.addStretch()
        main=QVBoxLayout();main.setSpacing(12);body.addLayout(main,1)
        cards=QHBoxLayout();main.addLayout(cards);self.cards=[]
        for key in ('Lambda','Sensor temperature','Heater','Nernst DC'):
            card=QWidget();card.setObjectName('Card');layout=QVBoxLayout(card);layout.addWidget(self.label(key));value=QLabel('--');value.setObjectName('Value');layout.addWidget(value);cards.addWidget(card,1);self.cards.append(value)
        self.tabs=QTabWidget();main.addWidget(self.tabs,1)
        self.graph=Graph(self);self.add_tab(self.graph,'Live graph')
        self.dashboard=self.table(10);self.add_tab(self.dashboard,'Dashboard')
        config=QWidget();config_layout=QVBoxLayout(config);config_layout.setContentsMargins(12,12,12,12)
        self.report=Text();self.report.setReadOnly(True);self.report.setMinimumHeight(130);config_layout.addWidget(self.report,1)
        row=QHBoxLayout();config_layout.addLayout(row)
        for key,fn in [('Save backup',self.save_backup),('Restore backup',self.restore_backup),('Identify for 10s',self.identify),('Restart',self.restart),('Factory defaults',self.defaults)]:self.action(row,key,fn)
        row=QHBoxLayout();config_layout.addLayout(row);row.addWidget(self.label('Sensor Rtrim [Ohm]'));self.trim=Entry();self.trim.setMaximumWidth(110);row.addWidget(self.trim)
        self.action(row,'Save Rtrim',self.save_trim);self.action(row,'Physical Rtrim',self.physical_trim);row.addStretch()
        config_layout.addWidget(self.label("Select LSU 4.9 Rtrim SW for software correction. Enter your own sensor's labeled value: 30.00..300.00 Ohm. Software mode requires the physical trim resistor to be absent. Sensor and mode must match."))
        self.action(config_layout,'Read extended diagnostics',self.extended);self.ext_report=QLabel();self.ext_report.setWordWrap(True);config_layout.addWidget(self.ext_report)
        row=QHBoxLayout();config_layout.addLayout(row)
        for key,fn in [('Legacy index',lambda:self.legacy(1)),('Legacy sensor',lambda:self.legacy(2)),('Legacy bitrate',lambda:self.legacy(3))]:self.action(row,key,fn)
        config_layout.addWidget(self.label('Legacy commands support compatible earlier firmware. Their ACKs have no sender ID or configuration readback.'))
        self.action(config_layout,'Update F0 firmware over CAN…',self.flash);self.progress=Progress();self.progress.setRange(0,100);config_layout.addWidget(self.progress)
        config_scroll=QScrollArea();config_scroll.setWidgetResizable(True);config_scroll.setWidget(config);config_scroll.setFrameShape(QScrollArea.Shape.NoFrame);self.add_tab(config_scroll,'F0 tools / Rtrim')
        events=QWidget();event_layout=QVBoxLayout(events);self.events=self.table(3);event_layout.addWidget(self.events);self.warmup=QLabel();self.warmup.setWordWrap(True);event_layout.addWidget(self.warmup);event_layout.addWidget(self.button('Export events CSV',self.export_events));self.add_tab(events,'Events / Warmup')
        stats=QWidget();stats_layout=QVBoxLayout(stats);stats_layout.addWidget(self.label('PC timestamp statistics; not hardware bus-load/error counters.'));self.stats=self.table(6);stats_layout.addWidget(self.stats);stats_layout.addWidget(self.button('Export CAN statistics CSV',self.export_stats));self.add_tab(stats,'CAN statistics')
        monitor=QWidget();monitor_layout=QVBoxLayout(monitor);row=QHBoxLayout();monitor_layout.addLayout(row);self.filter=Entry();self.bind(self.filter,'CAN ID hex / blank=all','setPlaceholderText');row.addWidget(self.filter,1)
        self.filter_type=Combo();self.filter_type.addItems(['','','']);row.addWidget(self.filter_type);self.raw_pause=Check();self.bind(self.raw_pause,'Pause monitor');row.addWidget(self.raw_pause);row.addWidget(self.button('CAN CSV start / stop',self.toggle_csv))
        self.console=Text();self.console.setReadOnly(True);self.console.setMaximumBlockCount(600);self.console.setFont(QFont('Monospace',10));monitor_layout.addWidget(self.console);self.add_tab(monitor,'CAN console')
        heater=QWidget();heater.setObjectName('Card');heater_layout=QVBoxLayout(heater);heater_layout.addWidget(self.label('Heater enable / Bench test'));row=QHBoxLayout();heater_layout.addLayout(row)
        row.addWidget(self.label('Actual supply [V]'));self.voltage=Entry('12.0');self.voltage.setMaximumWidth(85);row.addWidget(self.voltage);row.addWidget(self.label('Pump gain [%]'));self.gain=Spin();self.gain.setRange(0,100);self.gain.setValue(100);row.addWidget(self.gain)
        self.heat_button=self.action(row,'Start heater messages',self.toggle_heat);self.heat_button.set_label=lambda _:self.heat_button.setText(self.tr('Stop heater messages' if self.heating else 'Start heater messages'));main.addWidget(heater)
        self.status=QLabel();self.status.setObjectName('Status');self.status.setWordWrap(True);outer.addWidget(self.status)
        self.i18n.changed.connect(self.retranslate);self.retranslate()
        if self.demo:
            self.connect_button.setEnabled(False)
            for w in self.tx_buttons:w.setEnabled(False)
            self.active_indices.update((0,1,2))
            for ms in range(0,120000,100):self.demo_frames(ms)
        self.timer=QTimer(self);self.timer.timeout.connect(self.tick);self.timer.start(100)
        self.heater_timer=QTimer(self);self.heater_timer.timeout.connect(self.heat_tick);self.heater_timer.start(50)
        self.refresh_counter=0;self.show()
        if args.smoke_test:
            from smoke import Smoke
            self.smoke=Smoke(self,args.smoke_test);QTimer.singleShot(600,self.smoke.run)
    def label(self,key):
        widget=QLabel();widget.setWordWrap(True);self.bind(widget,key);return widget
    def bind(self,widget,key,method='setText'):
        self.bindings.append((widget,key,method));getattr(widget,method)(self.tr(key))
    def button(self,key,fn):
        widget=Button();self.bind(widget,key);widget.clicked.connect(lambda _=False:fn());return widget
    def action(self,layout,key,fn):
        widget=self.button(key,fn);layout.addWidget(widget);self.tx_buttons.append(widget);return widget
    def field(self,layout,key,widget):layout.addWidget(self.label(key));layout.addWidget(widget)
    def add_tab(self,widget,key):self.tabs.addTab(widget,self.tr(key));self.tab_keys.append(key)
    def table(self,columns):
        widget=QTableWidget(0,columns);widget.setAlternatingRowColors(True);widget.setEditTriggers(QTableWidget.EditTrigger.NoEditTriggers);widget.verticalHeader().hide();widget.horizontalHeader().setSectionResizeMode(QHeaderView.ResizeMode.Stretch);return widget
    def language_changed(self,index):
        language=self.language.itemData(index);self.settings.setValue('language',language);self.settings.sync();self.i18n.set_language(language)
    def retranslate(self):
        for widget,key,method in self.bindings:getattr(widget,method)(self.tr(key))
        if hasattr(self,'heat_button'):self.heat_button.set_label('')
        for index,key in enumerate(self.tab_keys):self.tabs.setTabText(index,self.tr(key))
        if hasattr(self,'filter_type'):
            for index,key in enumerate(('STD + EXT','STD only','EXT only')):self.filter_type.setItemText(index,self.tr(key))
        if hasattr(self,'dashboard'):
            self.dashboard.setHorizontalHeaderLabels([self.tr(k) for k in ('Index','State','Lambda','Temperature','Heater','Pump PWM','Nernst DC','ESR','Data age [s]','Warmup [s]')])
        if hasattr(self,'events'):self.events.setHorizontalHeaderLabels([self.tr(k) for k in ('Elapsed [s]','Index','Event')])
        if hasattr(self,'stats'):self.stats.setHorizontalHeaderLabels([self.tr(k) for k in ('Format','ID','DLC','Frames','Average [Hz]','Max gap [ms]')])
        self.render_report();self.render_extra();self.render_status();self.render_modules();self.refresh_tables()
    def render_status(self):
        if hasattr(self,'status'):self.status.setText(self.tr(self.status_key,**self.status_values) if self.status_key in __import__('i18n').DE else self.i18n.error(self.status_key))
        if hasattr(self,'badge'):self.badge.setText(self.tr('DEMO / SIMULATED' if self.demo else 'CAN ERROR' if self.link and self.link.error else 'CAN CONNECTED' if self.link else 'OFFLINE'))
    def note(self,key,**values):self.status_key=key;self.status_values=values;self.render_status()
    def error(self,message):
        self.last_error=message;self.note(message)
        if self.args.smoke_test:raise RuntimeError(message)
        dialog=QMessageBox(self);dialog.setIcon(QMessageBox.Icon.Critical);dialog.setWindowTitle(self.tr('Error'));dialog.setText(self.i18n.error(message));dialog.addButton(self.tr('OK'),QMessageBox.ButtonRole.AcceptRole);dialog.exec()
    def confirm(self,key):
        dialog=QMessageBox(self);dialog.setIcon(QMessageBox.Icon.Warning);dialog.setWindowTitle(self.tr('Confirm'));dialog.setText(self.i18n.error(key));yes=dialog.addButton(self.tr('OK'),QMessageBox.ButtonRole.AcceptRole);dialog.addButton(self.tr('Cancel'),QMessageBox.ButtonRole.RejectRole);dialog.exec();return dialog.clickedButton()==yes
    def choose(self,key,save=False,name=None):
        dialog=QFileDialog(self,self.tr(key));dialog.setOption(QFileDialog.Option.DontUseNativeDialog,True);dialog.setAcceptMode(QFileDialog.AcceptMode.AcceptSave if save else QFileDialog.AcceptMode.AcceptOpen);dialog.setFileMode(QFileDialog.FileMode.AnyFile if save else QFileDialog.FileMode.ExistingFile)
        dialog.setLabelText(QFileDialog.DialogLabel.Accept,self.tr('Save' if save else 'Open'));dialog.setLabelText(QFileDialog.DialogLabel.Reject,self.tr('Cancel'))
        if name:dialog.selectFile(name)
        return dialog.selectedFiles()[0] if dialog.exec() else None
    def ms(self):return int((time.monotonic()-self.start)*1000)+self.offset
    def adapter_changed(self,index):
        kind=self.transport.currentData();self.endpoint.setText('COM3' if os.name=='nt' and kind=='slcan' else '/dev/ttyUSB0' if kind=='slcan' else 'PCAN_USBBUS1' if kind=='pcan' else 'can0');self.bus_hint.setVisible(kind=='socketcan')
    def module_selected(self,index):
        value=self.modules.itemData(index)
        if value is not None:self.hw.setValue(value)
    def render_modules(self):
        if not hasattr(self,'modules'):return
        selected=self.modules.currentData();self.modules.blockSignals(True);self.modules.clear()
        if self.discovered:
            for hw,text in sorted(self.discovered.items()):self.modules.addItem(text,hw)
            found=self.modules.findData(selected)
            if found>=0:self.modules.setCurrentIndex(found)
        else:self.modules.addItem(self.tr('DEMO: data indices 0 / 1 / 2' if self.demo else 'No modules discovered.'),None)
        self.modules.blockSignals(False)
    def connect_bus(self):
        if self.demo:return
        if self.link:
            self.stop_heat();self.link.close();self.link=self.service=None;self.configs.clear();self.note('Disconnected');self.render_report();return
        kind=self.transport.currentData();channel=self.endpoint.text().strip();rate=[500000,1000000][self.bitrate.currentIndex()]
        def connected(link):
            self.link=link;self.service=Service(link);self.core.lib.wb_clear();self.configs.clear();self.extra=None;self.discovered.clear();self.active_indices.clear();self.error_seen=False;self.raw.clear();self.graph.frozen=None;self.graph.retranslate();self.render_modules();self.render_report();self.render_extra();self.note('Connected - passive CAN reception active')
        self.start_work(lambda:Link(create_transport(kind,channel,rate)),connected,require=False)
    def work(self,fn,done=None):self.start_work(fn,done,require=True)
    def flash_work(self,fn,done):
        self.operation_is_flash=True
        self.work(fn,done)
        if not self.busy:self.operation_is_flash=False
    def start_work(self,fn,done=None,require=True):
        if self.busy:return
        if require:
            try:self.require_link()
            except Exception as e:self.error(str(e));return
        self.busy=True;self.stop_heat();self.sidebar.setEnabled(False)
        for widget in self.tx_buttons:widget.setEnabled(False)
        self.done_callback=done;self.note('CAN operation in progress…')
        def run():
            try:self.bridge.finished.emit(fn(),None)
            except Exception as e:self.bridge.finished.emit(None,str(e))
        threading.Thread(target=run,daemon=True).start()
    def finish_work(self,result,error):
        self.busy=False;self.sidebar.setEnabled(True)
        for widget in self.tx_buttons:widget.setEnabled(not self.demo)
        done=self.done_callback;self.done_callback=None
        flashing=self.operation_is_flash;self.operation_is_flash=False
        if error:self.error(self.tr('Firmware update failed. Keep module powered; bootloader recovery may be required.\n{detail}',detail=self.i18n.error(error)) if flashing else error)
        elif done:done(result)
        else:self.note('CAN operation completed.')
    def set_progress(self,value):self.progress.set_fraction(value)
    def result(self,hw,result):
        self.configs[hw]=result;cfg,trim,_=result
        self.index.setValue(cfg[0]);self.sensor.setCurrentIndex(cfg[1]);self.module_rate.setCurrentIndex(cfg[2]);self.standard.setChecked(bool(cfg[3]&1));self.diag.setChecked(bool(cfg[3]&2));self.trim.setText(f'{trim/100:.2f}');self.active_indices.add(cfg[0]);self.render_report();self.note('F0 configuration and Rtrim read and verified. Bitrate changes require a restart.')
    def summary(self,hw,result):
        cfg,trim,ready=result;t=self.tr
        return '\n'.join((t('F0 HW {hw} / data index {index} / CAN {first:03X}, {second:03X}',hw=hw,index=cfg[0],first=0x190+2*cfg[0],second=0x191+2*cfg[0]),t('Sensor: {sensor}',sensor=SENSORS[cfg[1]]),t('Saved bitrate: {saved} kbit/s / active: {active} kbit/s',saved=[500,1000][cfg[2]],active=[500,1000][cfg[4]]),t('TX flags: {flags} / Rtrim: {trim:.2f} Ohm / calibration: {ready}',flags=cfg[3],trim=trim/100,ready=t('ready' if ready else 'not ready'))))
    def render_report(self,*_):
        if not hasattr(self,'report'):return
        hw=self.hw.value()
        if hw in self.configs:text=self.summary(hw,self.configs[hw])
        else:text='\n'.join(self.tr(key) for key in ('Detect / read F0 to load configuration and selectable Rtrim.','Hardware ID must be unique on the CAN bus.','Extended service features require compatible F0 Manager firmware 0.4.','No module configuration has been read yet.'))
        self.report.setPlainText(text)
    def restore_backup(self):
        path=self.choose('Load F0 backup')
        if not path:return
        try:
            data=Path(path).read_bytes();cfg,trim=self.core.restore(data);hw=self.target();old=self.configs.get(hw)
            text=self.tr('Backup source HW {source}, target HW {target}',source=data[6],target=hw)+'\n'+self.tr('Backup: index {index}, sensor {sensor}, bitrate {rate} kbit/s, TX {flags}, Rtrim {trim:.2f} Ohm',index=cfg[0],sensor=SENSORS[cfg[1]],rate=[500,1000][cfg[2]],flags=cfg[3],trim=trim/100)+'\n'+self.tr('Current: {current}',current=self.summary(hw,old) if old else self.tr('Unknown'))+'\n\n'+self.tr('Restore all settings? Sensor and hardware must match. Two verified writes; errors may leave partial changes. Do not interrupt power.')
            if self.confirm(text):self.work(lambda:self.service.restore(hw,cfg,trim),lambda r:self.result(hw,r))
        except Exception as e:self.error(str(e))
    def extended(self):
        hw=self.target()
        def run():self.service.require(hw);return self.service.request(DIAG1,hw),self.service.request(DIAG2,hw)
        def done(result):self.extra=(hw,result);self.render_extra();self.note('Extended diagnostics updated (snapshot).')
        self.work(run,done)
    def render_extra(self):
        if not hasattr(self,'ext_report'):return
        if not self.extra:self.ext_report.setText(self.tr('No extended diagnostic snapshot read.'));return
        hw,(a,b)=self.extra;ac,target,state=struct.unpack('<HhB',a);current,voltage,supply=struct.unpack('<hHB',b)
        self.ext_report.setText(self.tr('CAN snapshot / HW {hw}: Nernst AC {ac:.3f} V; pump target {target:.3f} mA; measured {current:.3f} mA; effective heater {voltage:.3f} V; supply over CAN {supply:.1f} V; heater state {state}.',hw=hw,ac=ac/1000,target=target/1000,current=current/1000,voltage=voltage/1000,supply=supply/10,state=state))
    def event_name(self,code):return self.tr(STATES[code] if code<8 else {100:'WARNING: data lost',101:'Data restored',102:'WARNING: invalid lambda in closed loop'}.get(code,'Unknown'))
    def fill_table(self,table,rows):
        table.setRowCount(len(rows))
        for i,row in enumerate(rows):
            for j,value in enumerate(row):
                item=table.item(i,j)
                if item is None:item=QTableWidgetItem();table.setItem(i,j,item)
                item.setText(str(value))
    def refresh_tables(self):
        if not hasattr(self,'tabs') or not hasattr(self,'stats'):return
        view=self.tabs.currentIndex();events=self.core.events();now=self.ms()
        if view==1:
            rows=[]
            for index in sorted(self.active_indices):
                points=self.core.points(index);latest=points[-1] if points else None;state=[code for _,idx,code in events if idx==index and code<8];warm=[(t,code) for t,idx,code in events if idx==index and code in (1,2)]
                warm_ms=(warm[-1][0]-warm[-2][0])/1000 if len(warm)>=2 and warm[-2][1]==1 and warm[-1][1]==2 else None
                values=[f'{latest[1+k]:.4g}' if latest and latest[7+k] and now-latest[0]<=500 else '--' for k in range(6)]
                rows.append([index,self.event_name(state[-1]) if state else self.tr('Unknown')]+values+[f'{(now-latest[0])/1000:.1f}' if latest else '--',f'{warm_ms:.2f}' if warm_ms is not None else '--'])
            self.fill_table(self.dashboard,rows)
        elif view==3:
            self.fill_table(self.events,[(f'{t/1000:.3f}',index,self.event_name(code)) for t,index,code in events[-500:]])
            lines=[]
            for index in sorted(self.active_indices):
                warm=[(t,c) for t,i,c in events if i==index and c in (1,2)]
                if len(warm)>=2 and warm[-2][1]==1 and warm[-1][1]==2:lines.append(self.tr('Index {index}: observed warmup {seconds:.2f} s',index=index,seconds=(warm[-1][0]-warm[-2][0])/1000))
            self.warmup.setText(self.tr('SESSION EVENTS / PC elapsed time; no error history stored in module.')+'\n'+'\n'.join(lines))
        elif view==4:self.fill_table(self.stats,[('EXT' if r[1] else 'STD',f'{int(r[0]):08X}',int(r[2]),int(r[3]),f'{r[7]:.1f}',int(r[6])) for r in self.core.stats()])
        elif view==5 and not self.raw_pause.isChecked():
            text=self.filter.text().strip();kind=self.filter_type.currentIndex()
            try:ident=int(text,16) if text else None
            except ValueError:ident=-1
            rows=[r for r in self.raw if (ident is None or int(r[3],16)==ident) and (kind==0 or (r[2]=='STD')==(kind==1))]
            self.console.setPlainText('\n'.join(f'{r[0]:10.3f} {r[1]} {r[2]} {r[3]} {r[4]}' for r in rows[-500:]))
    def tick(self):
        now=self.ms();self.refresh_counter+=1
        if self.demo:self.demo_frames(now)
        if self.link:
            for tx,frame,when in self.link.drain():
                ms=int((when-self.start)*1000)+self.offset;self.core.feed(frame,ms,tx);ident,ext,data=frame
                row=(ms/1000,'TX' if tx else 'RX','EXT' if ext else 'STD',f'{ident:08X}',data.hex().upper());self.raw.append(row)
                if self.raw_csv:
                    try:csv.writer(self.raw_csv).writerow(row)
                    except OSError as e:self.raw_csv.close();self.raw_csv=None;self.note(str(e))
                if not tx and not ext and 0x190<=ident<=0x38f and len(data)==8:self.active_indices.add((ident-0x190)//2)
                if not tx and ext and ident==ACK and len(data)==8:
                    self.discovered[data[0]]=f'HW {data[0]} / v{data[1]:02X} / {2000+data[2]:04}-{data[3]:02}-{data[4]:02}';self.render_modules()
            if self.link.error and not self.error_seen:self.error_seen=True;self.stop_heat();self.note('CAN error: {detail}',detail=self.i18n.error(self.link.error))
        self.core.lib.wb_tick(now);self.graph.area.update();points=self.core.points(self.index.value());latest=points[-1] if points else None
        for card,k in zip(self.cards,(0,1,2,4)):
            card.setText((f'{latest[1+k]:.4f}' if k in (0,4) else f'{latest[1+k]:.1f}')+(' '+UNITS[k] if UNITS[k] else '') if latest and latest[7+k] and now-latest[0]<=500 else '--')
        self.render_status()
        if self.refresh_counter%3==0:self.refresh_tables()
    def closeEvent(self,event):
        if self.busy:self.note('CAN operation in progress. Wait for completion or timeout before closing.');event.ignore();return
        self.stop_heat()
        if self.link:self.link.close();self.link=None
        if self.raw_csv:self.raw_csv.close();self.raw_csv=None
        self.settings.sync();event.accept()

STYLE='''
QMainWindow,QWidget {font-family:Arial;font-size:14px;color:#1a2c3d;}
QMainWindow {background:#f1f5fa;} QWidget#Header {background:#122438;} QWidget#Header QLabel {color:#eef5ff;}
QLabel#Brand {font-size:24px;font-weight:bold;} QLabel#Section {color:#587592;font-size:13px;font-weight:bold;}
QWidget#Card {background:white;border:1px solid #d7e1ed;border-radius:5px;} QLabel#Value {color:#008896;font-size:30px;font-weight:bold;}
QLabel#Muted {color:#587592;font-size:12px;} QLabel#Status {padding:10px 18px;background:white;color:#304f6d;}
QPushButton {background:#fff;border:1px solid #c8d5e3;border-radius:3px;padding:7px 10px;min-height:20px;}
QPushButton:hover {background:#e4f3f5;border-color:#008896;} QPushButton:pressed {background:#d1e9ed;}
QPushButton:disabled {color:#9ba7b4;background:#edf1f6;} QLineEdit,QSpinBox,QComboBox {background:white;border:1px solid #c8d5e3;border-radius:3px;padding:5px;min-height:23px;}
QTabWidget::pane {background:white;border:1px solid #d7e1ed;} QTabBar::tab {background:#e7edf4;padding:9px 12px;border-bottom:3px solid transparent;} QTabBar::tab:selected {background:white;border-bottom:3px solid #008896;}
QPlainTextEdit,QTableWidget {background:white;border:1px solid #d7e1ed;selection-background-color:#b8e1e7;alternate-background-color:#f4f7fb;}
QHeaderView::section {background:#e7edf4;padding:6px;border:0;color:#304f6d;}
QCheckBox {spacing:6px;} QScrollArea {border:0;background:transparent;}
'''

def main():
    parser=argparse.ArgumentParser(description='rusEFI Wideband Manager 0.6 - Windows/Linux Qt CAN client')
    parser.add_argument('--demo',action='store_true');parser.add_argument('--language',choices=('de','en'))
    parser.add_argument('--settings-file',help=argparse.SUPPRESS);parser.add_argument('--smoke-test',help=argparse.SUPPRESS)
    args=parser.parse_args();app=QApplication(sys.argv[:1]);app.setStyle('Fusion');app.setStyleSheet(STYLE);app.setOrganizationName('rusEFI');app.setApplicationName('WidebandManager06')
    window=Manager(args);exceptions=[]
    def exception_hook(kind,value,traceback):
        import traceback as tb
        tb.print_exception(kind,value,traceback);exceptions.append(str(value))
        if args.smoke_test:app.exit(1)
    sys.excepthook=exception_hook
    code=app.exec();return 1 if exceptions else code
if __name__=='__main__':raise SystemExit(main())
