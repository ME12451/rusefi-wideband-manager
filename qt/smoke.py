"""Automated shared GUI checks; explicit smoke-test flag only, never opens CAN."""
from pathlib import Path
import json
import time
from PySide6.QtCore import QObject,QTimer,Qt,QSettings
from PySide6.QtTest import QTest
from PySide6.QtWidgets import QApplication

class Smoke(QObject):
    def __init__(self,app,folder):super().__init__(app);self.app=app;self.folder=Path(folder);self.folder.mkdir(parents=True,exist_ok=True);self.checks=[]
    def check(self,value,message):
        if not value:raise AssertionError(message)
        self.checks.append(message)
    def snap(self,name):
        self.check(self.app.grab().save(str(self.folder/(name+'.png'))),'Screenshot: '+name)
    def run(self):
        a=self.app
        self.check(a.link is None,'No CAN connection opened')
        self.check(a.graph.area.draws>0,'Qt graph rendered')
        self.check(a.language.isEnabled(),'Language dropdown enabled')
        if a.demo:
            self.check(not a.connect_button.isEnabled() and all(not b.isEnabled() for b in a.tx_buttons),'Demo blocks CAN actions')
            self.check(len(a.graph.points())>1000,'Shared analytics contains demo traces')
            a.graph.area.setFocus();QTest.keyClick(a.graph.area,Qt.Key.Key_Space);self.check(a.graph.frozen is not None,'Space pauses graph')
            a.graph.export_to(self.folder/'paused-en.csv');self.snapshot=list(a.graph.frozen);end=a.graph.end
            a.trim.setText('123.45');a.voltage.setText('13.2');a.sensor.setCurrentIndex(4);a.module_rate.setCurrentIndex(1);a.standard.setChecked(False)
            a.language.setCurrentIndex(1)
            self.check(a.tr('Connection')=='Verbindung' and a.graph.pause.text()=='Weiter / Live','German labels and graph controls')
            self.check(a.trim.text()=='123.45' and a.voltage.text()=='13.2' and a.sensor.currentIndex()==4 and a.module_rate.currentIndex()==1 and not a.standard.isChecked(),'Language change preserves unsaved inputs')
            self.check(a.graph.frozen==self.snapshot and a.graph.end==end,'Language change preserves frozen history')
            a.graph.export_to(self.folder/'paused-de.csv');self.check((self.folder/'paused-en.csv').read_bytes()==(self.folder/'paused-de.csv').read_bytes(),'CSV identical across languages during pause')
            a.graph.pan(-10000);self.check(a.graph.visible()[-1][0]<self.snapshot[-1][0]-9000,'History panning uses older samples')
            a.graph.window.setCurrentIndex(0);self.check(a.graph.span==15,'Time window selection')
            for toggle in a.graph.toggles:toggle.setChecked(False)
            for toggle in a.graph.toggles:toggle.setChecked(True)
            a.graph.auto_check.setChecked(True);a.graph.auto_check.setChecked(False);a.graph.window.setCurrentIndex(2)
            QTest.keyClick(a.graph.area,Qt.Key.Key_Home);self.check(a.graph.frozen is None,'Home returns to live')
            a.index.setValue(1);self.check(bool(a.graph.points()),'Data index selection');a.index.setValue(0)
        else:
            self.check(not a.graph.points(),'Normal startup has no synthetic data')
            self.check(a.connect_button.isEnabled(),'Normal startup allows connection')
            a.language.setCurrentIndex(1)
        a.configs[4]=([7,4,1,3,0],12345,True);a.render_report()
        self.check('Gespeicherte Bitrate' in a.report.toPlainText(),'German configuration report')
        self.check(a.i18n.error('Configuration readback mismatch')=='Konfigurationsrücklesung weicht ab','German backend errors')
        a.note('F0 configuration and Rtrim read and verified. Bitrate changes require a restart.')
        QTimer.singleShot(200,self.german_capture)
    def german_capture(self):
        a=self.app;self.snap('demo-de' if a.demo else 'disconnected-de')
        a.tabs.setCurrentIndex(2);QTimer.singleShot(120,self.tools_capture)
    def tools_capture(self):
        a=self.app;self.snap('tools-de' if a.demo else 'tools-offline-de')
        a.language.setCurrentIndex(0);self.check('Saved bitrate' in a.report.toPlainText(),'English configuration report restored')
        a.tabs.setCurrentIndex(0);QTimer.singleShot(120,self.english_capture)
    def english_capture(self):
        a=self.app;self.snap('demo-en' if a.demo else 'disconnected-en')
        # A running task and language changes must coexist without restarting it.
        a.start_work(lambda:time.sleep(.25),lambda _:self.after_work(),require=False)
        self.check(a.busy and not a.sidebar.isEnabled(),'Worker disables configuration actions')
        a.language.setCurrentIndex(1);self.check(a.busy and a.language.isEnabled(),'Language can change while an operation is pending')
    def after_work(self):
        a=self.app;self.check(not a.busy,'Worker completes after language change')
        if a.demo:self.check(not a.connect_button.isEnabled() and all(not b.isEnabled() for b in a.tx_buttons),'Demo transmission blocks survive worker completion')
        a.settings.sync();settings=QSettings(a.args.settings_file,QSettings.Format.IniFormat)
        self.check(settings.value('language')=='de','Language preference persisted')
        a.resize(1280,800);QTimer.singleShot(150,self.finish)
    def finish(self):
        a=self.app;self.snap('small-de' if a.demo else 'small-offline-de')
        self.check(a.graph.area.width()>=620,'Graph remains usable in smaller window')
        self.check(not a.i18n.missing,'Every requested UI text has a German translation: '+str(sorted(a.i18n.missing)))
        self.folder.joinpath('gui-'+('demo' if a.demo else 'offline')+'.json').write_text(json.dumps({'passed':True,'checks':self.checks},indent=2)+'\n')
        print('Qt 0.6 GUI smoke passed:',len(self.checks),'checks; demo='+str(a.demo),flush=True)
        a.close();QApplication.instance().quit()
