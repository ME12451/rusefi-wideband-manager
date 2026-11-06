"""Shared Qt time-series view. Acquisition remains independent from pause/export."""
import csv
from PySide6.QtCore import Qt, QRectF, QPointF
from PySide6.QtGui import QColor, QPainter, QPen, QPainterPath, QFont
from PySide6.QtWidgets import QWidget, QVBoxLayout, QHBoxLayout, QPushButton, QComboBox, QCheckBox, QLabel

NAMES=('Lambda','Temperature','Heater','Pump PWM','Nernst DC','ESR')
UNITS=('','°C','%','%','V','Ohm')
COLORS=('#3ddceb','#ffb265','#ffd567','#b39cff','#4fdaa6','#ff87bb')
SPANS=(15,30,60,300)

class Plot(QWidget):
    def __init__(self,graph):
        super().__init__();self.graph=graph;self.setMinimumSize(620,380);self.setMouseTracking(True);self.setFocusPolicy(Qt.FocusPolicy.StrongFocus)
        self.cursor=None;self.drag=None;self.draws=0
    def paintEvent(self,event):
        self.draws+=1;g=self.graph;t=g.app.tr;p=QPainter(self);p.setRenderHint(QPainter.RenderHint.Antialiasing)
        p.fillRect(self.rect(),QColor('#111e2e'));p.setFont(QFont('Arial',10))
        def text(x,y,s,color='#a5b4c5',size=10):
            p.setFont(QFont('Arial',size));p.setPen(QColor(color));p.drawText(QPointF(x,y),s)
        width,height=self.width(),self.height();left,right=76,width-76;top=60;lane=(height-112)/4
        self.chart=QRectF(left,top,right-left,height-top-62)
        points=g.visible();end=g.endpoint();index=g.frozen_index if g.frozen is not None else g.app.index.value()
        title=t('PAUSED' if g.frozen is not None else 'LIVE')+'  /  '+t('Index {index} / {span} s',index=index,span=g.span)
        if g.app.demo:title+='  /  '+t('DEMO / SIMULATED')
        text(16,25,title,size=11)
        nearest=None
        if self.cursor is not None and points and left<=self.cursor<=right:
            target=end-g.span*1000+(self.cursor-left)/(right-left)*g.span*1000;nearest=min(points,key=lambda point:abs(point[0]-target))
        enabled=[w.isChecked() for w in g.toggles]
        for row,metrics in enumerate(((0,),(1,5),(2,3),(4,))):
            y=top+row*lane;bottom=y+lane-27
            p.setPen(QPen(QColor('#2b3b4e'),1))
            for j in range(3):p.drawLine(QPointF(left,y+(bottom-y)*j/2),QPointF(right,y+(bottom-y)*j/2))
            for j in range(5):
                x=left+(right-left)*j/4;p.setPen(QPen(QColor('#2b3b4e'),1));p.drawLine(QPointF(x,y),QPointF(x,bottom))
                if row==3:text(x-20,bottom+22,f'{(end-g.span*1000+g.span*1000*j/4)/1000:.1f}s',size=9)
            common=[point[1+k] for k in metrics if enabled[k] for point in points if point[7+k]] if row==2 else []
            for pos,k in enumerate(metrics):
                if not enabled[k]:continue
                values=common if row==2 else [point[1+k] for point in points if point[7+k]]
                lo,hi=g.app.core.bounds(k,g.automatic,values);color=COLORS[k]
                axis=right+8 if pos==1 and row==1 else 8
                if pos==0 or row==1:
                    for j in range(3):
                        value=hi-(hi-lo)*j/2;text(axis,y+5+(bottom-y)*j/2,f'{value:.2f}' if k in (0,4) else f'{value:.0f}',color,9)
                value=f'{nearest[1+k]:.4g} {UNITS[k]}' if nearest is not None and nearest[7+k] else '--'
                text(left+pos*min(260,(right-left)/2),y-10,f'{t(NAMES[k])}  {value}',color)
                def py(v):return bottom-max(0,min(1,(v-lo)/(hi-lo)))*(bottom-y)
                if k==0:
                    p.setPen(QPen(QColor('#77818d'),1,Qt.PenStyle.DashLine));p.drawLine(QPointF(left,py(1)),QPointF(right,py(1)))
                path=QPainterPath();previous=None
                for point in points:
                    x=left+(point[0]-(end-g.span*1000))/(g.span*1000)*(right-left)
                    if not point[7+k]:
                        if k==0 and point[13]:p.setPen(QPen(QColor('#ff5555'),1));p.drawLine(QPointF(x,bottom-5),QPointF(x,bottom))
                        previous=None;continue
                    yy=py(point[1+k])
                    if previous is None or point[0]-previous[0]>350:path.moveTo(x,yy)
                    else:path.lineTo(x,yy)
                    previous=point
                p.setPen(QPen(QColor(color),1.6));p.drawPath(path)
        if self.cursor is not None and left<=self.cursor<=right:
            p.setPen(QPen(QColor('#b9c8d9'),1,Qt.PenStyle.DashLine));p.drawLine(QPointF(self.cursor,top),QPointF(self.cursor,height-67))
        footer=t('Time since program start / Gaps: missing or stale data / Red: invalid lambda')
        if not points:footer=t('No measurements yet - connect CAN and select a data index.')
        elif not any(enabled):footer=t('Select at least one trace.')
        elif nearest is not None:footer=t('Cursor {time:.3f} s / Lambda {valid}',time=nearest[0]/1000,valid=t('valid' if nearest[7] else 'invalid or missing'))
        text(16,height-17,footer,size=9);p.end()
    def mousePressEvent(self,event):
        if event.button()==Qt.MouseButton.LeftButton:
            self.setFocus()
            if self.graph.frozen is None:self.graph.toggle_pause()
            self.drag=(event.position().x(),self.graph.end)
    def mouseMoveEvent(self,event):
        self.cursor=event.position().x()
        if self.drag:
            self.graph.end=self.drag[1];self.graph.pan(-(self.cursor-self.drag[0])*self.graph.span*1000/max(1,self.width()-152))
        self.update()
    def mouseReleaseEvent(self,event):self.drag=None
    def leaveEvent(self,event):self.cursor=None;self.update()
    def wheelEvent(self,event):
        up=event.angleDelta().y()>0
        if event.modifiers()&Qt.KeyboardModifier.ShiftModifier:self.graph.pan((-1 if up else 1)*self.graph.span*100)
        else:self.graph.window.setCurrentIndex(max(0,min(3,self.graph.window.currentIndex()+(-1 if up else 1))))
        event.accept()
    def keyPressEvent(self,event):
        key=event.key();g=self.graph
        if key==Qt.Key.Key_Space:g.toggle_pause()
        elif key==Qt.Key.Key_Home:
            if g.frozen is not None:g.toggle_pause()
        elif key in (Qt.Key.Key_Plus,Qt.Key.Key_Equal,Qt.Key.Key_Minus):g.window.setCurrentIndex(max(0,min(3,g.window.currentIndex()+(1 if key==Qt.Key.Key_Minus else -1))))
        else:super().keyPressEvent(event)

class Graph(QWidget):
    def __init__(self,app):
        super().__init__();self.app=app;self.span=60;self.frozen=None;self.end=0;self.paused_at=0;self.automatic=False;self.frozen_index=0
        layout=QVBoxLayout(self);layout.setContentsMargins(0,0,0,0);toolbar=QHBoxLayout();layout.addLayout(toolbar);self.toggles=[]
        for name,color in zip(NAMES,COLORS):
            w=QPushButton();w.setCheckable(True);w.setChecked(True);w.setStyleSheet(f'QPushButton:checked {{border:1px solid {color};background:#20374b;color:{color};}}');toolbar.addWidget(w);self.toggles.append(w)
            w.toggled.connect(lambda _=None:self.area.update())
        toolbar.addStretch();self.pause=QPushButton();self.pause.clicked.connect(self.toggle_pause);toolbar.addWidget(self.pause)
        row=QHBoxLayout();layout.addLayout(row);self.time_label=QLabel();row.addWidget(self.time_label)
        self.window=QComboBox();self.window.setMinimumWidth(138);self.window.addItems(['']*4);self.window.setCurrentIndex(2);self.window.currentIndexChanged.connect(self.change_span);row.addWidget(self.window)
        self.auto_check=QCheckBox();self.auto_check.toggled.connect(self.auto);row.addWidget(self.auto_check)
        self.export_button=QPushButton();self.export_button.clicked.connect(self.export);row.addWidget(self.export_button)
        self.hint=QLabel();row.addWidget(self.hint);row.addStretch()
        self.area=Plot(self);layout.addWidget(self.area,1);app.i18n.changed.connect(self.retranslate);self.retranslate()
    def retranslate(self):
        t=self.app.tr
        for w,name in zip(self.toggles,NAMES):w.setText(t(name))
        self.pause.setText(t('Resume / Live' if self.frozen is not None else 'Pause'));self.time_label.setText(t('Time'))
        for i,name in enumerate(('15 seconds','30 seconds','60 seconds','5 minutes')):self.window.setItemText(i,t(name))
        self.auto_check.setText(t('Auto scales'));self.export_button.setText(t('CSV time window'));self.hint.setText(t('Wheel: zoom / Drag: history'));self.area.update()
    def change_span(self,index):self.span=SPANS[index];self.area.update()
    def auto(self,value):self.automatic=value;self.area.update()
    def points(self):return self.frozen if self.frozen is not None else self.app.core.points(self.app.index.value())
    def endpoint(self):return self.end if self.frozen is not None else self.app.ms()
    def visible(self):
        end=self.endpoint();return [point for point in self.points() if 0<=end-point[0]<=self.span*1000]
    def toggle_pause(self):
        if self.frozen is None:self.frozen=self.points();self.end=self.paused_at=self.app.ms();self.frozen_index=self.app.index.value()
        else:self.frozen=None
        self.retranslate()
    def pan(self,delta):
        if self.frozen is None:self.toggle_pause()
        oldest=self.frozen[0][0] if self.frozen else self.paused_at;self.end=max(oldest,min(self.paused_at,self.end+delta));self.area.update()
    def export_to(self,path):
        index=self.frozen_index if self.frozen is not None else self.app.index.value()
        with open(path,'w',newline='',encoding='utf-8') as f:
            writer=csv.writer(f);writer.writerow(['source','elapsed_s','index','lambda','temp_C','heater_percent','pump_percent','nernst_V','esr_ohm','lambda_valid','std_fresh','diag_fresh'])
            for point in self.visible():writer.writerow(['DEMO' if self.app.demo else 'CAN',f'{point[0]/1000:.3f}',index]+[f'{point[1+i]:.4f}' if point[7+i] else '' for i in range(6)]+[int(point[7]),int(point[13]),int(point[14])])
    def export(self):
        path=self.app.choose('Save visible graph window',True,'wideband-traces.csv')
        if path:
            try:self.export_to(path);self.app.note('Visible graph window exported.')
            except OSError as e:self.app.error(str(e))
