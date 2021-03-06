/***************************************************************************
 *   Copyright (C) 2010 by David Pinelo                                    *
 *   alepherp@alephsistemas.es                                             *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************/
#include <QtGlobal>
#include <QtXml>
#if (QT_VERSION < QT_VERSION_CHECK(5,0,0))
#include <QtGui>
#else
#include <QtWidgets>
#endif
#include <QtCore>
#include <QtScript>
#ifdef ALEPHERP_DEVTOOLS
#include <QtScriptTools>
#endif
#include "configuracion.h"
#include <globales.h>
#include "ui_dbformdlg.h"
#include "dbformdlg.h"
#include "business/aerploggeduser.h"
#include "models/dbbasebeanmodel.h"
#include "models/filterbasebeanmodel.h"
#include "models/treebasebeanmodel.h"
#include "forms/dbsearchdlg.h"
#include "forms/dbrecorddlg.h"
#include "forms/sendemaildlg.h"
#include "forms/aerptransactioncontextprogressdlg.h"
#include "forms/dbwizarddlg.h"
#ifdef ALEPHERP_ADVANCED_EDIT
#include "forms/aerpsystemobjecteditdlg.h"
#endif
#include "dao/beans/basebean.h"
#include "dao/beans/basebeanmetadata.h"
#include "dao/beans/beansfactory.h"
#include "dao/beans/dbfield.h"
#include "dao/beans/dbrelationmetadata.h"
#include "dao/beans/aerpsystemobject.h"
#include "dao/basedao.h"
#include "dao/relateddao.h"
#include "dao/aerptransactioncontext.h"
#include "widgets/fademessage.h"
#include "widgets/dbtableview.h"
#include "widgets/dbtreeview.h"
#include "widgets/dbabstractfilterview.h"
#include "widgets/dbfiltertableview.h"
#include "widgets/dbfiltertreeview.h"
#include "widgets/dbfilterscheduleview.h"
#include "scripts/perpscript.h"
#include "reports/reportrun.h"
#include "forms/perpmainwindow.h"
#include "business/aerpspreadsheet.h"

class DBFormDlgPrivate
{
public:
    QPointer<DBAbstractFilterView> m_itemView;
    /** Metadatos del objeto a editar */
    BaseBeanMetadata *m_metadata;
    /** Tabla o vista de la que se presentarán los registros */
    QSignalMapper *m_signalMapper;
    /** Para botones especiales añadidos */
    QSignalMapper *m_specialButtonSignalMapper;
    QPointer<AERPMainWindow> m_mainWindow;
    BaseBeanSharedPointer *m_selectedBean;
    DBFormDlg::DBFormButtons m_buttons;
    /** Nombre del bean que edita este formulario, si edita alguno */
    QString m_tableName;
    /** Indicará si el formulario se ha abierto correctamente */
    bool m_openSuccess;
    /** Motor de script para las funciones */
    AERPScriptQsObject m_engine;
    DBFormDlg *q_ptr;
    QString m_lastMessage;
    QHash<QPushButton *, QString> m_specialButtonUi;
    QHash<QPushButton *, QString> m_specialButtonQs;
    /** Puntero al diálogo que se edita */
    QPointer<DBRecordDlg> m_dlg;
    QString m_helpUrl;
    bool m_canDefrostModel;
    QPersistentModelIndex m_currentIndex;
    QModelIndex m_recentInsertIndex;
    bool m_operationCanceled;

    DBFormDlgPrivate(DBFormDlg *qq) : q_ptr(qq)
    {
        m_metadata = NULL;
        m_signalMapper = NULL;
        m_specialButtonSignalMapper = NULL;
        m_selectedBean = NULL;
        m_openSuccess = false;
        m_canDefrostModel = true;
        m_operationCanceled = false;
    }

    bool isPrintButtonVisible();
    bool isEmailButtonVisible();
    bool hasWizard();
    bool checkConditionsForRemoteOnTree(const QModelIndex &idx);
};

/**
 * @brief DBFormDlgPrivate::isPrintButtonVisible
 * Determina si el botón de imprimir documentos es visible. Será visible cuando existe algún ReportMetadata
 * con type="record" y además linkedTo = "tabla_de_este_dbrecord"
 */
bool DBFormDlgPrivate::isPrintButtonVisible()
{
    QList<ReportMetadata *> reports = ReportRun::availableReports(m_metadata->tableName());
    return reports.size() > 0;
}

bool DBFormDlgPrivate::isEmailButtonVisible()
{
#ifdef ALEPHERP_SMTP_SUPPORT
    return m_metadata->canSendEmail();
#else
    return false;
#endif
}

bool DBFormDlgPrivate::hasWizard()
{
    QString fileNameFilter = QString("%1*.wizard.ui").
                  arg(m_metadata->uiWizard().isEmpty() ? m_metadata->tableName() : m_metadata->uiWizard());

    QDir dir(alephERPSettings->dataPath());
    QStringList nameFilters;
    nameFilters << fileNameFilter;
    QStringList wizardPages = dir.entryList(nameFilters, QDir::Files, QDir::Name);
    return wizardPages.size() > 0;
}

/**
 * @brief DBFormDlgPrivate::checkConditionsForRemoteOnTree
 * Comprobará si se dan las condiciones adecuadas para borrar el registro en la estructura en árbol.
 * @param idx
 * @return
 */
bool DBFormDlgPrivate::checkConditionsForRemoteOnTree(const QModelIndex &idx)
{
    // Si el índice a borrar es directamente del último parte del árbol, no hay problema
    FilterBaseBeanModel *model = qobject_cast<FilterBaseBeanModel *> (m_itemView->filterModel());
    if ( model == NULL )
    {
        m_lastMessage = QObject::trUtf8("Se ha producido un error general.");
        return false;
    }
    if ( m_metadata == NULL )
    {
        m_lastMessage = QObject::trUtf8("Se ha producido un error general. No existen los metadatos.");
        return false;
    }
    BaseBeanSharedPointer beanSelected = model->bean(idx);
    if ( beanSelected.isNull() )
    {
        m_lastMessage = QObject::trUtf8("Se ha producido un error general. No se puede obtener el registro seleccionado.");
        return false;
    }
    if ( beanSelected->metadata()->tableName() == m_metadata->tableName() )
    {
        m_lastMessage.clear();
        return true;
    }
    // No estamos en la última rama. ¿Qué hacer? Comprobemos si existe una relación con delete on Cascade, y si existe
    // se avisará al usuario
    TreeBaseBeanModel *sourceModel = qobject_cast<TreeBaseBeanModel *> (model->sourceModel());
    if ( sourceModel == NULL )
    {
        m_lastMessage = QObject::trUtf8("Se ha producido un error general. No se puede obtener el registro seleccionado.");
        return false;
    }
    QStringList tableNames = sourceModel->tableNames();
    bool deleteCascade = true;
    int init = tableNames.indexOf(beanSelected->metadata()->tableName());
    if ( init == -1 )
    {
        m_lastMessage = QObject::trUtf8("Aplicación mal configurada.");
        return false;
    }
    for ( int i = init ; i < tableNames.size() - 1 ; i++ )
    {
        BaseBeanMetadata *m = BeansFactory::instance()->metadataBean(tableNames.at(i+1));
        if ( m == NULL )
        {
            m_lastMessage = QObject::trUtf8("Aplicación mal configurada. No existe la tabla %1").arg(tableNames.at(i+1));
            return false;
        }
        DBRelationMetadata *rel = m->relation(tableNames.at(i));
        if ( rel != NULL )
        {
            if ( rel->type() == DBRelationMetadata::MANY_TO_ONE )
            {
                if ( !rel->deleteCascade() )
                {
                    deleteCascade = false;
                }
            }
        }
    }
    if ( deleteCascade )
    {
        // Preguntemos al usuario si está seguro de lo que va a hacer
        int ret = QMessageBox::question(q_ptr, qApp->applicationName(), QObject::trUtf8("Si borra este registro, se borrarán automáticamente "
                                        "todos los registros que dependen de este, produciéndose un "
                                        "borrado en cascada. ¿Está usted seguro de continuar?"), QMessageBox::Yes | QMessageBox::No);
        if ( ret == QMessageBox::No )
        {
            m_lastMessage.clear();
            return false;
        }
        else
        {
            return true;
        }
    }
    m_lastMessage = QObject::trUtf8("No se permite el borrado en cascada de registros. Es decir, debe usted seleccionar un elemento final del árbol (una hoja) para borrar.");
    return false;
}

DBFormDlg::DBFormDlg(QWidget *parent, Qt::WindowFlags f)
    : QWidget( parent, f ), ui(new Ui::DBFormDlg), d(new DBFormDlgPrivate(this))
{
}

DBFormDlg::DBFormDlg(const QString &tableName, QWidget* parent, Qt::WindowFlags f)
    : QWidget( parent, f ), ui(new Ui::DBFormDlg), d(new DBFormDlgPrivate(this))
{
    init(tableName);
}

bool DBFormDlg::construct(const QString &tableName)
{
    setObjectName(QString("%1%2").arg(objectName()).arg(tableName));
    if ( !checkPermissionsToOpen() )
    {
        return false;
    }

    BaseBeanMetadata *metadata = BeansFactory::metadataBean(tableName);
    if ( metadata != NULL )
    {
        setWindowTitle(metadata->alias());
    }
    else
    {
        qDebug() << "No existe la tabla: " << tableName;
        QMessageBox::warning(this, qApp->applicationName(), trUtf8("No existe la tabla %1").arg(tableName), QMessageBox::Ok);
        close();
        return false;
    }

    if ( metadata->showOnTree() )
    {
        DBFilterTreeView *tv = new DBFilterTreeView(this);
        d->m_itemView = tv;
        tv->setTreeDefinition(metadata->treeDefinitions());
    }
    else
    {
        if ( metadata->isScheduleValid() )
        {
            DBFilterScheduleView *sv = new DBFilterScheduleView(this);
            d->m_itemView = sv;
        }
        else
        {
            DBFilterTableView *tv = new DBFilterTableView(this);
            d->m_itemView = tv;
        }
    }
    d->m_itemView->setFocusPolicy(Qt::StrongFocus);
    d->m_itemView->setTableName(tableName);
    CommonsFunctions::setOverrideCursor(QCursor(Qt::WaitCursor));
    // Este orden es muy importante para que funcione adecuadamente los focus
    ui->setupUi(this);
    d->m_itemView->init();
    ui->groupBox->layout()->addWidget(d->m_itemView);
    setTabOrder(d->m_itemView.data(), ui->pbWizard);
    setFocusProxy(d->m_itemView);
    CommonsFunctions::restoreOverrideCursor();

    actions();

    QObject *tmp = this->parent();
    while ( tmp != 0 )
    {
        d->m_mainWindow = qobject_cast<AERPMainWindow *>(tmp);
        if ( !d->m_mainWindow.isNull() )
        {
            tmp = 0;
        }
        else
        {
            tmp = tmp->parent();
        }
    };

    d->m_helpUrl = metadata->helpUrl();

    if ( d->m_helpUrl.isEmpty() )
    {
        d->m_helpUrl = QString("qthelp://%1/doc/%2.html").
                arg(metadata->module()->id()).
                arg(metadata->tableName());
    }
    if ( !d->m_mainWindow.isNull() )
    {
        ui->pbHelp->setVisible(d->m_mainWindow->existsHelpForUrl(d->m_helpUrl));
    }
    else
    {
        ui->pbHelp->setVisible(false);
    }

    if ( ! (d->m_metadata->canHaveRelatedElements() ||
         d->m_metadata->showSomeRelationOnRelatedElementsModel() ||
         d->m_metadata->canSendEmail() ||
         d->m_metadata->canHaveRelatedDocuments()) )
    {
        d->m_mainWindow->showOrHideRelatedElements(false);
    }

    d->m_signalMapper = new QSignalMapper(this);
    d->m_specialButtonSignalMapper = new QSignalMapper(this);
    d->m_signalMapper->setMapping(ui->actionEdit, QString("false"));
    d->m_signalMapper->setMapping(ui->actionCreate, QString("true"));
    connect (ui->actionView, SIGNAL(triggered()), this, SLOT(view()));
    connect (ui->actionEdit, SIGNAL(triggered()), d->m_signalMapper, SLOT(map()));
    connect (ui->actionCreate, SIGNAL(triggered()), d->m_signalMapper, SLOT(map()));
    connect (d->m_signalMapper, SIGNAL(mapped(const QString &)), this, SLOT(edit(const QString &)));
    connect (d->m_specialButtonSignalMapper, SIGNAL(mapped(QString)), this, SLOT(specialEdit(QString)));
    connect (ui->actionExit, SIGNAL(triggered()), this, SLOT(close()));
    connect (ui->actionAdjustLines, SIGNAL(triggered()), d->m_itemView, SLOT(resizeRowsToContents()));
    connect (ui->actionCopy, SIGNAL(triggered()), this, SLOT(copy()));
    connect (ui->actionDelete, SIGNAL(triggered()), this, SLOT(deleteRecord()));
    connect (ui->actionSearch, SIGNAL(triggered()), this, SLOT(search()));
    connect (ui->actionPrint, SIGNAL(triggered()), this, SLOT(printRecord()));
    connect (ui->actionEmail, SIGNAL(triggered()), this, SLOT(emailRecord()));
    connect (ui->actionRelatedElements, SIGNAL(triggered()), this, SLOT(showOrHideRelatedElements()));
    connect (ui->actionOk, SIGNAL(triggered()), this, SLOT(ok()));
    connect (ui->actionWizard, SIGNAL(triggered()), this, SLOT(wizard()));
    connect (ui->actionHelp, SIGNAL(triggered()), this, SLOT(showHelp()));
    connect (ui->actionExportSpreadSheet, SIGNAL(triggered()), this, SLOT(exportSpreadSheet()));

    return true;
}

DBFormDlg::~DBFormDlg()
{
    delete ui;
    delete d;
}

AERPScriptQsObject *DBFormDlg::engine()
{
    return &(d->m_engine);
}

QString DBFormDlg::tableName() const
{
    return d->m_tableName;
}

void DBFormDlg::init(const QString &value)
{
    d->m_tableName = value;
    d->m_metadata = BeansFactory::metadataBean(value);
    if ( construct(value) )
    {
        if ( d->m_metadata )
        {
            ui->actionCreate->setText(trUtf8("Insertar registro '%1'").arg(d->m_metadata->alias()));
            ui->actionEdit->setText(trUtf8("Editar registro '%1'").arg(d->m_metadata->alias()));
            ui->actionDelete->setText(trUtf8("Borrar registro '%1'").arg(d->m_metadata->alias()));
        }
        ui->pbPrint->setVisible(d->isPrintButtonVisible());
        ui->pbSendEmail->setVisible(d->isEmailButtonVisible());
        ui->pbWizard->setVisible(d->hasWizard());
        ui->pbOk->setVisible(false);
        ui->pbRelatedElements->setVisible(d->m_metadata->canHaveRelatedElements() ||
                                          d->m_metadata->showSomeRelationOnRelatedElementsModel() ||
                                          d->m_metadata->canSendEmail() ||
                                          d->m_metadata->canHaveRelatedDocuments());
        execQs();
        if ( d->m_itemView->filterModel() )
        {
            d->m_itemView->filterModel()->setQsObjectEngine(engine());
        }
        setOpenSuccess(true);
    }
    else
    {
        setOpenSuccess(false);
        close();
        return;
    }
}

/**
 * @brief DBFormDlg::dbRecordsView
 * Devuelve el widget que presenta los beans
 * @return
 */
QWidget *DBFormDlg::dbRecordsView() const
{
    return qobject_cast<QWidget *>(d->m_itemView);
}

BaseBeanPointer DBFormDlg::selectedBean()
{
    if ( d->m_metadata == NULL )
    {
        return BaseBeanPointer ();
    }
    return d->m_itemView->selectedBean();
}

bool DBFormDlg::openSuccess()
{
    return d->m_openSuccess;
}

void DBFormDlg::setOpenSuccess(bool value)
{
    d->m_openSuccess = value;
}

void DBFormDlg::actions()
{
    connect(ui->pbAdjust, SIGNAL(clicked()), ui->actionAdjustLines, SLOT(trigger()));
    connect(ui->pbClose, SIGNAL(clicked()), ui->actionExit, SLOT(trigger()));
    connect(ui->pbView, SIGNAL(clicked()), ui->actionView, SLOT(trigger()));
    connect(ui->pbDelete, SIGNAL(clicked()), ui->actionDelete, SLOT(trigger()));
    connect(ui->pbOk, SIGNAL(clicked()), ui->actionOk, SLOT(trigger()));
    connect(ui->pbEdit, SIGNAL(clicked()), ui->actionEdit, SLOT(trigger()));
    connect(ui->pbNew, SIGNAL(clicked()), ui->actionCreate, SLOT(trigger()));
    connect(ui->pbSearch, SIGNAL(clicked()), ui->actionSearch, SLOT(trigger()));
    connect(ui->pbCopy, SIGNAL(clicked()), ui->actionCopy, SLOT(trigger()));
    connect(ui->pbPrint, SIGNAL(clicked()), ui->actionPrint, SLOT(trigger()));
    connect(ui->pbSendEmail, SIGNAL(clicked()), ui->actionEmail, SLOT(trigger()));
    connect(ui->pbRelatedElements, SIGNAL(clicked()), ui->actionRelatedElements, SLOT(trigger()));
    connect(ui->pbWizard, SIGNAL(clicked()), ui->actionWizard, SLOT(trigger()));
    connect(ui->pbHelp, SIGNAL(clicked()), ui->actionHelp, SLOT(trigger()));
    connect(ui->pbExportSpreadSheet, SIGNAL(clicked()), ui->actionExportSpreadSheet, SLOT(trigger()));
}

bool DBFormDlg::event(QEvent *e)
{
    if( e->type() == QEvent::StatusTip )
    {
        QStatusTipEvent *ev = (QStatusTipEvent*) e;
        if ( !d->m_mainWindow.isNull() && d->m_mainWindow->statusBar() != NULL )
        {
            d->m_mainWindow->statusBar()->showMessage(ev->tip());
        }
        return true;
    }
    return QWidget::event(e);
}

void DBFormDlg::closeEvent(QCloseEvent * event)
{
    QString functionName = "closeEvent";
    if ( engine()->existQsFunction(functionName) )
    {
        QScriptValue result;
        d->m_engine.callQsObjectFunction(result, functionName);
    }

    // Guardamos las dimensiones del usuario
    alephERPSettings->savePosForm(this);
    alephERPSettings->saveDimensionForm(this);
    QWidget::closeEvent(event);
    emit closingWindow(this);
}

void DBFormDlg::keyPressEvent (QKeyEvent * e)
{
    if ( e->key() == Qt::Key_Escape )
    {
        BaseBeanMetadata *metadata = BeansFactory::metadataBean(d->m_tableName);
        if ( metadata != NULL && metadata->showOnTree() )
        {
            QPushButton *pbBranchTree = d->m_itemView->findChild<QPushButton*>("pbBranchTree");
            if ( pbBranchTree->isChecked() )
            {
                pbBranchTree->setChecked(false);
                (qobject_cast<DBFilterTreeView*>(d->m_itemView))->showOrHideBranchFilter();
                return;
            }
        }
        close();
        e->accept();
    }
    QWidget::keyPressEvent(e);
}

void DBFormDlg::showEvent(QShowEvent *ev)
{
    Q_UNUSED(ev)
    if ( !d->m_mainWindow->isVisibleRelatedWidget() )
    {
        d->m_itemView->defrostModel();
    }
    if ( !property(AlephERP::stInited).toBool() )
    {
        // Importante hacerlo aquí ya que sabemos seguro que se ha inicializado los items.
        connect (d->m_itemView->itemView(), SIGNAL(enterPressedOnValidIndex(QModelIndex)), ui->actionEdit, SLOT(trigger()));
        connect (d->m_itemView->itemView(), SIGNAL(doubleClickOnValidIndex(const QModelIndex&)), ui->actionEdit, SLOT(trigger()));
        connect (d->m_itemView->selectionModel(), SIGNAL(currentChanged(QModelIndex,QModelIndex)), this, SLOT(availableButtonsFromBean()));
        connect (d->m_itemView->selectionModel(), SIGNAL(currentChanged(QModelIndex,QModelIndex)), this, SLOT(setBeanOnRelatedWidget()));
        connect (d->m_itemView->itemView(), SIGNAL(customContextMenuRequested(QPoint)), this, SLOT(showContextMenu(QPoint)));
        setProperty(AlephERP::stInited, true);
    }

    // Damos uniformidad a los botones de la barra de herramientas
    QList<QPushButton *> pushButtons = ui->gbButtons->findChildren<QPushButton *>();
    foreach (QPushButton *pb, pushButtons)
    {
        pb->setMaximumHeight(ui->pbNew->height());
        pb->setMinimumHeight(ui->pbNew->height());
    }
}

void DBFormDlg::hideEvent(QHideEvent *ev)
{
    Q_UNUSED(ev)
    if ( d->m_itemView->itemView()->selectionModel() != NULL && d->m_itemView->itemView()->selectionModel()->currentIndex().isValid() )
    {
        d->m_currentIndex = QPersistentModelIndex(d->m_itemView->itemView()->selectionModel()->currentIndex());
    }
    else
    {
        d->m_currentIndex = QPersistentModelIndex();
    }
    d->m_itemView->freezeModel();
}

/*!
  Vamos a obtener y guardar cuándo el usuario ha modificado un control
  */
bool DBFormDlg::eventFilter (QObject *target, QEvent *event)
{
    if ( !target->inherits("QTextEdit") && !target->inherits("DBHtmlEditor") &&
            !target->inherits("DBCodeEdit") && !target->inherits("QTableView")
            && event->type() == QEvent::KeyPress )
    {
        QKeyEvent *ev = static_cast<QKeyEvent *>(event);
        if ( ev->key() == Qt::Key_Enter || ev->key() == Qt::Key_Return )
        {
            event->accept();
            focusNextChild();
            return true;
        }
    }
    return QWidget::eventFilter(target, event);
}

/*!
  Este formulario puede contener cierto código script a ejecutar en su inicio. Esta función lo lanza
  inmediatamente. El código script está en alepherp_system, con el nombre de la tabla principal
  acabado en dbform.qs
  */
void DBFormDlg::execQs()
{
    QString qsName;
    if ( d->m_metadata->qsDbForm().isEmpty() )
    {
        qsName = QString ("%1.dbform.qs").arg(tableName());
    }
    else
    {
        qsName = d->m_metadata->qsDbForm();
    }

    /** Ejecutamos el script asociado. La filosofía fundamental de ese script es proporcionar
      algo de código básico que justifique este formulario de búsqueda */
    if ( !BeansFactory::systemScripts.contains(qsName) )
    {
        return;
    }

    engine()->setScript(BeansFactory::systemScripts.value(qsName), qsName);
    engine()->setPrototypeScript(BeansFactory::systemScripts.value(d->m_metadata->qsPrototypeDbForm()));
    engine()->setDebug(BeansFactory::systemScriptsDebug.value(qsName));
    engine()->setOnInitDebug(BeansFactory::systemScriptsDebugOnInit.value(qsName));
    engine()->setScriptObjectName("DBFormDlg");
    engine()->setUi(this);
    engine()->setThisFormObject(this);
    exposeAERPControlToQsEngine();
    CommonsFunctions::setOverrideCursor(QCursor(Qt::ArrowCursor));
    bool result = engine()->createQsObject();
    CommonsFunctions::restoreOverrideCursor();
    if ( !result )
    {
        QMessageBox::warning(this,qApp->applicationName(), trUtf8("Ha ocurrido un error al cargar el script asociado a este "
                             "formulario. Es posible que algunas funciones no estén disponibles."),
                             QMessageBox::Ok);
    }
}

/**
  Hace accesible a todos los objetos de tipo AERPControl en el motor de javascript directamente
  desde thisForm, de la forma: thisForm.db_nombre donde db_nombre es el objectName dado en el Qt Designer
  */
void DBFormDlg::exposeAERPControlToQsEngine()
{
    QList<QWidget *> list = this->findChildren<QWidget *>();
    foreach ( QWidget *widget, list )
    {
        if ( widget->property(AlephERP::stAerpControl).toBool() )
        {
            d->m_engine.addPropertyToThisForm(widget->objectName(), widget);
        }
    }
    d->m_engine.addPropertyToThisForm("dbItemView", d->m_itemView);
}

void DBFormDlg::edit()
{
    edit("false", "", "");
}

void DBFormDlg::edit(const QString &insert)
{
    edit(insert, "", "");
}

void DBFormDlg::edit(const QString &insert, const QString &uiCode, const QString &qsCode)
{
    AlephERP::FormOpenType openType;
    QString functionName;

    if ( !d->m_dlg.isNull() )
    {
        return;
    }

    d->m_canDefrostModel = false;
    d->m_itemView->freezeModel();

    if ( insert == "false" )
    {
        openType = AlephERP::Update;
        functionName = "beforeEdit";
    }
    else
    {
        openType = AlephERP::Insert;
        functionName = "beforeInsert";
    }
    if ( engine()->existQsFunction(functionName) )
    {
        QScriptValue result;
        if ( engine()->callQsObjectFunction(result, functionName) )
        {
            if ( !result.toBool() )
            {
                d->m_canDefrostModel = true;
                if ( !d->m_mainWindow->isVisibleRelatedWidget() )
                {
                    d->m_itemView->defrostModel();
                }
                return;
            }
        }
    }

    FilterBaseBeanModel *model = d->m_itemView->filterModel();;
    QItemSelectionModel *selectionModel = d->m_itemView->selectionModel();;
    QHash<QString, QVariant> filterValuesToSet = this->filterValuesToSetOnBean();
    CommonsFunctions::setOverrideCursor(QCursor(Qt::WaitCursor));
    if ( d->m_metadata->tableName() == QString("%1_system").arg(alephERPSettings->systemTablePrefix()) )
    {
#if defined(ALEPHERP_ADVANCED_EDIT) && defined (ALEPHERP_DEVTOOLS)
        d->m_dlg = new AERPSystemObjectEditDlg(model, selectionModel, filterValuesToSet, openType, this);
#else
        d->m_canDefrostModel = true;
        d->m_itemView->defrostModel();
        return;
#endif
    }
    else
    {
        d->m_dlg = new DBRecordDlg(model, selectionModel, filterValuesToSet, openType, this);
    }
    CommonsFunctions::restoreOverrideCursor();
    if ( !uiCode.isEmpty() )
    {
        d->m_dlg->setUiCode(uiCode);
    }
    if ( !qsCode.isEmpty() )
    {
        d->m_dlg->setQsCode(qsCode);
    }
    if ( d->m_dlg->openSuccess() && d->m_dlg->init() )
    {
        if ( openType == AlephERP::Insert )
        {
            d->m_recentInsertIndex = d->m_dlg->recentInsertIndex();
        }
        d->m_itemView->disableRestoreSaveState();
        connect(d->m_dlg.data(), SIGNAL(accepted()), this, SLOT(recordDlgClosed()));
        connect(d->m_dlg.data(), SIGNAL(rejected()), this, SLOT(recordDlgCanceled()));
        d->m_dlg->setModal(true);
        d->m_dlg->setCanChangeModality(true);
        d->m_dlg->exec();
    }
    else
    {
        delete d->m_dlg;
        d->m_canDefrostModel = true;
        if ( !d->m_mainWindow->isVisibleRelatedWidget() )
        {
            d->m_itemView->defrostModel();
        }
    }
}

/**
 * @brief DBFormDlg::insertChild
 * Inserta un bean hijo (si es posible) del índice seleccionado en el item view.
 */
void DBFormDlg::insertChild()
{
    QString functionName = "beforeInsert";

    if ( !d->m_dlg.isNull() )
    {
        return;
    }

    d->m_recentInsertIndex = QModelIndex();
    d->m_canDefrostModel = false;
    d->m_itemView->freezeModel();

    if ( engine()->existQsFunction(functionName) )
    {
        QScriptValue result;
        if ( engine()->callQsObjectFunction(result, functionName) )
        {
            if ( !result.toBool() )
            {
                d->m_canDefrostModel = true;
                if ( !d->m_mainWindow->isVisibleRelatedWidget() )
                {
                    d->m_itemView->defrostModel();
                }
                return;
            }
        }
    }

    FilterBaseBeanModel *model = d->m_itemView->filterModel();
    QItemSelectionModel *selectionModel = d->m_itemView->selectionModel();
    // El selection model anterior, se refiere al modelo FilterBaseBeanModel
    BaseBeanModel *sourceModel = d->m_itemView->sourceModel();
    if ( selectionModel->selectedIndexes().size() > 0 )
    {
        // Debemos trabajar con el sourceModel. ¿Porqué? La secuencia que ocurre es:
        // 1.- Se obtiene el selectedIndex del selectionModel que apunta al FilterBaseBeanModel
        // 2.- Se inserta un nuevo registro
        // 3.- Se produce el invalidado del modelo, e internamente se recrea la estructura mapToSource
        // 4.- El selectedIndex obtenido en el paso 1 ya no es válido, apunta a otro sitio, y nos quedamos sin
        // padre para el registro (útil en caso de registros en árbol)
        QModelIndex parentIdx = selectionModel->selectedIndexes().at(0);
        QModelIndex sourceParentIdx = model->mapToSource(parentIdx);

        int row = sourceModel->rowCount(sourceParentIdx);
        if ( sourceModel->insertRow(row, sourceParentIdx) )
        {
            QModelIndex childIdx = sourceParentIdx.child(row, 0);
            if ( childIdx.isValid() )
            {
                d->m_recentInsertIndex = childIdx;
                QVariant vBean = childIdx.data(AlephERP::BaseBeanRole);
                BaseBeanPointer bean = (BaseBean *)(vBean.value<void *>());
                if ( bean )
                {
                    if ( QString(sourceModel->metaObject()->className()) == "TreeBaseBeanModel" )
                    {
                        // Los modelos en árbol hacen cosas raras con los filtros... no andan finos. Mejor invalidamos.
                        model->invalidate();
                    }
                    CommonsFunctions::setOverrideCursor(QCursor(Qt::WaitCursor));
                    d->m_dlg = new DBRecordDlg(bean, AlephERP::Insert, this);
                    CommonsFunctions::restoreOverrideCursor();
                    if ( d->m_dlg->openSuccess() && d->m_dlg->init() )
                    {
                        d->m_itemView->disableRestoreSaveState();
                        connect(d->m_dlg.data(), SIGNAL(accepted()), this, SLOT(recordDlgClosed()));
                        connect(d->m_dlg.data(), SIGNAL(rejected()), this, SLOT(recordDlgCanceled()));
                        d->m_dlg->setModal(true);
                        d->m_dlg->setCanChangeModality(true);
                        d->m_dlg->exec();
                        return;
                    }
                    else
                    {
                        delete d->m_dlg;
                    }
                }
            }
        }
    }
    d->m_canDefrostModel = true;
    if ( !d->m_mainWindow->isVisibleRelatedWidget() )
    {
        d->m_itemView->defrostModel();
    }
}

void DBFormDlg::wizard()
{
    QString functionName = "beforeInsert";

    d->m_canDefrostModel = false;
    d->m_itemView->freezeModel();

    if ( engine()->existQsFunction(functionName) )
    {
        QScriptValue result;
        if ( engine()->callQsObjectFunction(result, functionName) )
        {
            if ( !result.toBool() )
            {
                if ( !d->m_mainWindow->isVisibleRelatedWidget() )
                {
                    d->m_itemView->defrostModel();
                }
                return;
            }
        }
    }
    FilterBaseBeanModel *model = d->m_itemView->filterModel();;
    QItemSelectionModel *selectionModel = d->m_itemView->selectionModel();;
    CommonsFunctions::setOverrideCursor(QCursor(Qt::WaitCursor));
    QScopedPointer<DBWizardDlg> dlg (new DBWizardDlg(model, selectionModel, this));
    CommonsFunctions::restoreOverrideCursor();
    if ( dlg->init() )
    {
        d->m_itemView->disableRestoreSaveState();
        dlg->setModal(true);
        int status = dlg->exec();
        if ( status == QDialog::Accepted )
        {
            emit afterEdit(true);
        }
    }
    d->m_canDefrostModel = true;
    if ( !d->m_mainWindow->isVisibleRelatedWidget() )
    {
        d->m_itemView->defrostModel();
    }
}

void DBFormDlg::recordDlgClosed()
{
    emit afterEdit(d->m_dlg->userSaveData());
    if (!d->m_dlg->userSaveData())
    {
        BaseBeanModel *sourceModel = d->m_itemView->sourceModel();
        if ( d->m_recentInsertIndex.isValid() )
        {
            sourceModel->removeRow(d->m_recentInsertIndex.row(), d->m_recentInsertIndex.parent());
        }
        else
        {
            sourceModel->removeInsertedRows();
        }
    }
    d->m_itemView->enableRestoreSaveState();
    d->m_itemView->reSort();
    d->m_dlg->deleteLater();
    // Forzamos un refresco del modelo
    setBeanOnRelatedWidget();
    if ( !d->m_mainWindow->isVisibleRelatedWidget() )
    {
        d->m_itemView->defrostModel();
    }
    d->m_canDefrostModel = true;
    CommonsFunctions::processEvents();
}

void DBFormDlg::recordDlgCanceled()
{
    recordDlgClosed();
    if ( d->m_recentInsertIndex.isValid() )
    {
        d->m_itemView->filterModel()->removeRow(d->m_recentInsertIndex.row(), d->m_recentInsertIndex.parent());
        d->m_itemView->filterModel()->invalidate();
    }
}

/**
 * @brief DBFormDlg::deleteRecord
 * Encargada de borrar el registro seleccionado. Debe tenerse un especial cuidado en el caso de árboles.
 */
void DBFormDlg::deleteRecord(void)
{
    if ( !d->m_dlg.isNull() )
    {
        return;
    }

    // Desde este momentos, dejamos estático el modelo del DBForm
    // La función de script beforeDelete puede modificar la estructura interna de beans (marcando por ejemplo a borrar
    // elementos relacionados). Por ello, es importante que el bean que se pase al script beforeDelete sea el mismo
    // que después la función removeRows invocará. Eso se hace poniendo staticModel a true
    d->m_canDefrostModel = false;
    d->m_itemView->freezeModel();

    QItemSelectionModel *selModel = d->m_itemView->selectionModel();
    QAbstractItemModel *mdl = const_cast<QAbstractItemModel *>(selModel->model());
    FilterBaseBeanModel *filterModel = qobject_cast<FilterBaseBeanModel *>(mdl);

    if ( selModel == NULL || mdl == NULL )
    {
        QMessageBox::warning(this, qApp->applicationName(),
                             QString::fromUtf8("Ha ocurrido un error inesperado. Es posible que haya perdido la conexión a la base de datos."),
                             QMessageBox::Ok);
        d->m_canDefrostModel = true;
        if ( !d->m_mainWindow->isVisibleRelatedWidget() )
        {
            d->m_itemView->defrostModel();
        }
        return;
    }

    QModelIndexList indexes = selModel->selectedRows();

    if ( indexes.isEmpty() )
    {
        QMessageBox::warning(this, qApp->applicationName(),
                             trUtf8("Debe seleccionar algún registro para borrar"),
                             QMessageBox::Ok);
        d->m_canDefrostModel = true;
        if ( !!d->m_mainWindow->isVisibleRelatedWidget() )
        {
            d->m_itemView->defrostModel();
        }
        return;
    }

    if ( d->m_metadata->showOnTree() && !d->checkConditionsForRemoteOnTree(indexes.at(0)) )
    {
        if ( !d->m_lastMessage.isEmpty() )
        {
            QMessageBox::information(this, qApp->applicationName(), d->m_lastMessage, QMessageBox::Ok);
        }
        d->m_canDefrostModel = true;
        if ( !d->m_mainWindow->isVisibleRelatedWidget() )
        {
            d->m_itemView->defrostModel();
        }
        return;
    }

    // Buscamos confirmación
    QString message = trUtf8("¿Está seguro de que desea borrar el(los) registro(s) actualmente seleccionado(s)?");
    int ret = QMessageBox::information(this, qApp->applicationName(), message, QMessageBox::Yes | QMessageBox::No);

    if ( ret == QMessageBox::Yes )
    {
        emit beforeDelete();
        BaseBeanModel *sourceModel = qobject_cast<BaseBeanModel *>(filterModel->sourceModel());
        QModelIndexList rows = selModel->selectedRows();
        while (rows.size() > 0)
        {
            QModelIndex idx = rows.takeLast();
            CommonsFunctions::setOverrideCursor(Qt::WaitCursor);
            BaseBeanSharedPointer bean;
            if ( filterModel != NULL )
            {
                bean = filterModel->beanToBeEdited(idx);
            }
            else if ( sourceModel != NULL )
            {
                bean = sourceModel->beanToBeEdited(idx);
            }
            CommonsFunctions::restoreOverrideCursor();
            if ( bean.isNull() )
            {
                QMessageBox::warning(this, qApp->applicationName(),
                                     QString::fromUtf8("Ha ocurrido un error inesperado. No se ha podido borrar el registro. "
                                                       "Es posible que otro usuario lo haya borrado o que no se haya refrescado el modelo. "
                                                       "Cierre el formulario, ábralo de nuevo y vuelva a intentarlo."),
                                     QMessageBox::Ok);
                sourceModel->rollback();
                d->m_canDefrostModel = true;
                if ( !d->m_mainWindow->isVisibleRelatedWidget() )
                {
                    d->m_itemView->defrostModel();
                }
                return;
            }
            else
            {
                if ( engine()->existQsFunction("beforeDelete") )
                {
                    QScriptValue result;
                    QScriptValueList args;
                    args.append(engine()->createScriptValue(bean.data()));
                    if ( engine()->callQsObjectFunction(result, "beforeDelete", args) )
                    {
                        if (!result.toBool())
                        {
                            sourceModel->rollback();
                            d->m_canDefrostModel = true;
                            if ( !d->m_mainWindow->isVisibleRelatedWidget() )
                            {
                                d->m_itemView->defrostModel();
                            }
                            emit afterDelete(false);
                            return;
                        }
                    }
                }
                if ( !mdl->removeRows(idx.row(), 1, idx.parent()) )
                {
                    QMessageBox::warning(this, qApp->applicationName(),
                                         QString::fromUtf8("Ha ocurrido un error. No se ha podido borrar el registro. "
                                                           "<br/><i>Error</i>: %1").arg(AERPTransactionContext::instance()->lastErrorMessage()),
                                         QMessageBox::Ok);
                    sourceModel->rollback();
                    d->m_canDefrostModel = true;
                    if ( !d->m_mainWindow->isVisibleRelatedWidget() )
                    {
                        d->m_itemView->defrostModel();
                    }
                    emit afterDelete(false);
                    return;
                }
            }
        }
        AERPTransactionContextProgressDlg::showDialog(AlephERP::stDeleteContext, this);
        if ( !sourceModel->commit() )
        {
            QMessageBox::warning(this, qApp->applicationName(),
                                 QString::fromUtf8("Ha ocurrido un error. No se ha podido borrar el registro. "
                                                   "<br/><i>Error</i>: %1").arg(AERPTransactionContext::instance()->lastErrorMessage()),
                                 QMessageBox::Ok);
            sourceModel->rollback();
            emit afterDelete(false);
        }
        else
        {
            emit afterDelete(true);
        }
    }
    d->m_canDefrostModel = true;
    if ( !d->m_mainWindow->isVisibleRelatedWidget() )
    {
        d->m_itemView->defrostModel();
    }
    if ( filterModel != NULL )
    {
        filterModel->clearAcceptedRows();
    }
}

/*!
    Esta función realiza la siguiente acción: Si no se estaba filtrando, se abre el formulario
    de búsqueda para que el usuario seleccione los presupuestos que desea ver.
    Si se estaba filtrando, esta función elimina y deshabilita el filtro
*/
void DBFormDlg::search(void)
{
    if ( !d->m_dlg.isNull() )
    {
        return;
    }

    d->m_canDefrostModel = true;
    d->m_itemView->freezeModel();

    QScopedPointer<DBSearchDlg> dlg (new DBSearchDlg(tableName(), this));
    if ( dlg->openSuccess() )
    {
        dlg->setModal(true);
        dlg->init();
        dlg->exec();
    }
    d->m_canDefrostModel = true;
    if ( !d->m_mainWindow->isVisibleRelatedWidget() )
    {
        d->m_itemView->defrostModel();
    }
}

void DBFormDlg::copy()
{
    if ( !d->m_dlg.isNull() )
    {
        return;
    }

    QItemSelectionModel *selModel = d->m_itemView->selectionModel();
    FilterBaseBeanModel *mdl = qobject_cast<FilterBaseBeanModel *>(selModel == NULL ? NULL : const_cast<QAbstractItemModel *>(selModel->model()));
    if ( selModel == NULL || mdl == NULL) {
        QMessageBox::warning(this, qApp->applicationName(),
                             QString::fromUtf8("Ha ocurrido un error inesperado. Es posible que haya perdido la conexión a la base de datos."),
                             QMessageBox::Ok);
        return;
    }

    d->m_canDefrostModel = true;
    d->m_itemView->freezeModel();

    QModelIndexList indexes = selModel->selectedRows();
    BaseBeanSharedPointerList copies;
    QModelIndexList indexesCopy;
    QHash<int, QModelIndex> addedRows;


    if ( indexes.size() > 0 )
    {
        QString message = trUtf8("¿Desea realizar una copia de los registros actualmente seleccionados?");
        int ret = QMessageBox::information(this, qApp->applicationName(), message, QMessageBox::Yes | QMessageBox::No);
        if ( ret == QMessageBox::Yes )
        {
            foreach (const QModelIndex &idx, indexes)
            {
                if ( idx.isValid() )
                {
                    int row = mdl->rowCount();
                    BaseBeanSharedPointer orig = mdl->beanToBeEdited(idx);
                    if ( !orig.isNull() )
                    {
                        if ( !mdl->insertRow(row, idx.parent()) )
                        {
                            if ( !mdl->lastErrorMessage().isEmpty() )
                            {
                                QMessageBox::warning(this, qApp->applicationName(), trUtf8("Ha ocurrido un error al intentar agregar copiar el registro. \nEl error es: %1").arg(mdl->lastErrorMessage()));
                                mdl->setProperty(AlephERP::stLastErrorMessage, "");
                            }
                            return;
                        }
                        addedRows[row] = idx.parent();
                        QModelIndex newIdx = mdl->index(row, 0, idx.parent());
                        indexesCopy.append(newIdx);
                        BaseBeanSharedPointer dest = mdl->beanToBeEdited(newIdx);
                        if ( !dest.isNull() )
                        {
                            // Creamos un contexto para esta transacción de copia.
                            AERPTransactionContext::instance()->addToContext(AERPTransactionContext::instance()->masterContext(), dest.data());
                            if ( !BaseDAO::copyBaseBean(orig.data(), dest.data()) )
                            {
                                AERPTransactionContext::instance()->discardContext(AERPTransactionContext::instance()->masterContext());
                                CommonsFunctions::restoreOverrideCursor();
                                if ( !d->m_mainWindow->isVisibleRelatedWidget() )
                                {
                                    d->m_itemView->defrostModel();
                                }
                                return;
                            }
                            copies.append(dest);
                        }
                    }
                }
            }
            AERPTransactionContextProgressDlg::showDialog(AERPTransactionContext::instance()->masterContext(), this);
            bool commitResult = AERPTransactionContext::instance()->commit(AERPTransactionContext::instance()->masterContext());
            AERPTransactionContext::instance()->waitCommitToEnd(AERPTransactionContext::instance()->masterContext());
            if ( !commitResult )
            {
                QMessageBox::warning(this,qApp->applicationName(), trUtf8("Ha ocurrido un error generando la copia."), QMessageBox::Ok);
                QHashIterator<int, QModelIndex> it(addedRows);
                while (it.hasNext())
                {
                    it.next();
                    mdl->removeRow(it.key(), it.value());
                }
                if ( !d->m_mainWindow->isVisibleRelatedWidget() )
                {
                    d->m_itemView->defrostModel();
                }
                return;
            }
            if ( indexesCopy.size() == 1 )
            {
                QModelIndex newIdx = indexesCopy.at(0);
                selModel->setCurrentIndex(newIdx, QItemSelectionModel::Rows);
                selModel->select(newIdx, QItemSelectionModel::ClearAndSelect |QItemSelectionModel::Rows);
                int ret = QMessageBox::information(this,qApp->applicationName(),
                                                   trUtf8("¿Desea editar el nuevo registro copiado?"), QMessageBox::Yes | QMessageBox::No);
                if ( ret == QMessageBox::Yes )
                {
                    edit("false");
                }
            }
        }
    }
    else
    {
        QMessageBox::warning(this, qApp->applicationName(),
                             trUtf8("Debe seleccionar algún registro para copiar"), QMessageBox::Ok);
    }
    d->m_canDefrostModel = true;
    if ( !d->m_mainWindow->isVisibleRelatedWidget() )
    {
        d->m_itemView->defrostModel();
    }
}

void DBFormDlg::ok()
{
    QItemSelectionModel *selModel = d->m_itemView->selectionModel();
    FilterBaseBeanModel *mdl = qobject_cast<FilterBaseBeanModel *>(selModel == NULL ? NULL : const_cast<QAbstractItemModel *>(selModel->model()));
    if ( selModel == NULL || mdl == NULL ) {
        QMessageBox::warning(this, qApp->applicationName(),
                             QString::fromUtf8("Ha ocurrido un error inesperado. Es posible que haya perdido la conexión a la base de datos."),
                             QMessageBox::Ok);
        return;
    }

    QModelIndexList indexes = selModel->selectedRows();

    if ( !indexes.isEmpty() )
    {
        QModelIndex idx = indexes.at(0);
        if ( d->m_selectedBean != NULL )
        {
            *(d->m_selectedBean) = mdl->bean(idx);
        }
    }
    close();
}

DBFormDlg::DBFormButtons DBFormDlg::visibleButtons() const
{
    return d->m_buttons;
}

/*!
  Permite ocultar botones
  */
void DBFormDlg::setVisibleButtons(DBFormDlg::DBFormButtons buttons)
{
    d->m_buttons = buttons;
    ui->pbNew->setVisible(buttons.testFlag(DBFormDlg::CreateButton));
    ui->pbEdit->setVisible(buttons.testFlag(DBFormDlg::EditButton));
    ui->pbDelete->setVisible(buttons.testFlag(DBFormDlg::DeleteButton));
    ui->pbSearch->setVisible(buttons.testFlag(DBFormDlg::SearchButton));
    ui->pbCopy->setVisible(buttons.testFlag(DBFormDlg::CopyButton));
    ui->pbAdjust->setVisible(buttons.testFlag(DBFormDlg::AdjustLineButton));
    ui->pbClose->setVisible(buttons.testFlag(DBFormDlg::ExitButton));
    ui->pbOk->setVisible(buttons.testFlag(DBFormDlg::OkButton));
    ui->pbPrint->setVisible(buttons.testFlag(DBFormDlg::PrintButton));
    ui->pbWizard->setVisible(buttons.testFlag(DBFormDlg::WizardButton));
    ui->pbExportSpreadSheet->setVisible(buttons.testFlag((DBFormDlg::ExportSpreadSheet)));
    if ( d->m_metadata->canHaveRelatedElements() || d->m_metadata->canHaveRelatedDocuments() || d->m_metadata->canSendEmail() || d->m_metadata->showSomeRelationOnRelatedElementsModel() )
    {
        ui->pbRelatedElements->setVisible(buttons.testFlag(DBFormDlg::RelatedElementsButton));
    }
    else
    {
        ui->pbRelatedElements->setVisible(false);
    }
    ui->pbSendEmail->setVisible(buttons.testFlag(DBFormDlg::EmailButton));
}

/*!
  Tenemos que decirle al motor de scripts, que DBFormDlg se convierte de esta forma a un valor script
  */
QScriptValue DBFormDlg::toScriptValue(QScriptEngine *engine, DBFormDlg * const &in)
{
    return engine->newQObject(in, QScriptEngine::QtOwnership, QScriptEngine::PreferExistingWrapperObject);
}

void DBFormDlg::fromScriptValue(const QScriptValue &object, DBFormDlg * &out)
{
    out = qobject_cast<DBFormDlg *>(object.toQObject());
}

/**
 * @brief DBFormDlg::createPushButton Permite al programador QS crear un nuevo botón en la botonera principal de este formulario
 * de forma cómoda
 * @param position Indica la posición donde se ubicará el botón, dentro del layout horizontal que componen los botones.
 * @param text Texto del botón. Sólo será visible si img no es una imágen válida.
 * @param toolTip Tool tip
 * @param img Imagen (formato :/img/imagen.png
 * @return El botón creado
 */
QPushButton *DBFormDlg::createPushButton(int position, const QString &text, const QString &toolTip, const QString &img, const QString &methodNameToInvokeOnClicked, int width, int height)
{
    QHBoxLayout *layout = qobject_cast<QHBoxLayout *>(ui->gbButtons->layout());
    if ( position < 0 || position > layout->count() )
    {
        return NULL;
    }

    QSizePolicy sizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
    sizePolicy.setHorizontalStretch(25);
    sizePolicy.setVerticalStretch(0);

    QPushButton *pb = new QPushButton(this);
    pb->setToolTip(toolTip);
    pb->setWhatsThis(toolTip);
    sizePolicy.setHeightForWidth(pb->sizePolicy().hasHeightForWidth());
    pb->setSizePolicy(sizePolicy);

    QPixmap pixmap(img);
    pb->setIcon(pixmap);
    pb->setText(text);
    if ( width != -1 )
    {
        pb->setMinimumWidth(width);
        pb->setMaximumWidth(width);
    }
    layout->insertWidget(position, pb);
    if ( width != -1 )
    {
        if ( height != -1 )
        {
            pb->resize(width, height);
        }
        else
        {
            pb->setMinimumSize(QSize(width, ui->pbNew->minimumSize().height()));
            pb->setMaximumSize(QSize(width, ui->pbNew->maximumSize().height()));
            pb->resize(width, ui->pbNew->height());
        }
    }
    QScriptContext *ctx = context();
    while ( ctx != NULL )
    {
        QScriptValue thisObject = ctx->thisObject();
        if ( thisObject.isValid() )
        {
            QScriptValue function = thisObject.property(methodNameToInvokeOnClicked);
            QScriptValue functionOnProtoype = thisObject.prototype().property(methodNameToInvokeOnClicked);
            if ( function.isValid() && function.isFunction() )
            {
                qScriptConnect(pb, SIGNAL(clicked()), thisObject, function);
                ctx = NULL;
            }
            else if ( functionOnProtoype.isValid() && functionOnProtoype.isFunction() )
            {
                qScriptConnect(pb, SIGNAL(clicked()), thisObject, functionOnProtoype);
                ctx = NULL;
            }
            else
            {
                ctx = ctx->parentContext();
            }
        }
        else
        {
            ctx = ctx->parentContext();
        }
    }
    return pb;
}

/**
 * @brief DBFormDlg::createEditButton
 * Crea un botón especial de edición de registros, que puede utilizar, por ejemplo, otra interfaz Qs o Ui. Muy útil por ejemplo para
 * crear formularios de inserción o edición de registros a los configurados por defecto.
 * @param position Posición del botón
 * @param text Texto del botón
 * @param toolTip Tooltio
 * @param img
 * @param editType
 * @param uiCode Nombre del objeto ui
 * @param qsCode Nombre del objeto qs que se abrirá
 * @return
 */
QPushButton *DBFormDlg::createEditButton(int position, const QString &text, const QString &toolTip, const QString &img,
        DBFormDlg::DBFormButtons editType, const QString &uiCode, const QString &qsCode)
{
    if ( ! (editType.testFlag(DBFormDlg::EditButton) || editType.testFlag(DBFormDlg::CreateButton)) )
    {
        QLogger::QLog_Error(AlephERP::stLogOther, QString("DBFormDlg::createEditButton: No se ha indicado qué tipo de edición se hará"));
        return NULL;
    }
    QPushButton *button = createPushButton(position, text, toolTip, img);
    if ( button == NULL )
    {
        return NULL;
    }
    QString code;
    if ( editType.testFlag(DBFormDlg::EditButton) )
    {
        code = QString("false;%1;%2").arg(uiCode).arg(qsCode);
    }
    else if ( editType.testFlag(DBFormDlg::CreateButton) )
    {
        code = QString("true;%1;%2").arg(uiCode).arg(qsCode);
    }
    if ( !code.isEmpty() )
    {
        d->m_specialButtonSignalMapper->setMapping(button, code);
        connect (button, SIGNAL(clicked()), d->m_specialButtonSignalMapper, SLOT(map()));
    }
    return button;
}

/**
 * @brief DBFormDlg::createLabel
 * Permite agregar un label a la botonera
 * @param position
 * @param text
 * @return
 */
QLabel *DBFormDlg::createLabel(int position, const QString &text)
{
    QHBoxLayout *layout = qobject_cast<QHBoxLayout *>(ui->gbButtons->layout());
    if ( position < 0 || position > layout->count() )
    {
        return NULL;
    }
    QLabel *lbl = new QLabel(text, this);
    layout->insertWidget(position, lbl);
    return lbl;
}

/*!
  Fuerza un refresco de los datos del filterTableView
  */
void DBFormDlg::refreshFilterTableView()
{
    if ( d->m_dlg.isNull() )
    {
        if ( d->m_canDefrostModel && !AERPTransactionContext::instance()->doingCommit() )
        {
            if ( d->m_mainWindow && !d->m_mainWindow->isVisibleRelatedWidget() )
            {
                d->m_itemView->defrostModel();
            }
            if ( d->m_currentIndex.isValid() && d->m_itemView->itemView()->selectionModel() != NULL )
            {
                d->m_itemView->itemView()->selectionModel()->setCurrentIndex(d->m_currentIndex, QItemSelectionModel::ToggleCurrent);
            }
        }
    }
    if ( d->m_metadata->canHaveRelatedElements() ||
         d->m_metadata->showSomeRelationOnRelatedElementsModel() ||
         d->m_metadata->canSendEmail() ||
         d->m_metadata->canHaveRelatedDocuments() )
    {
        d->m_mainWindow->showOrHideRelatedElements(ui->pbRelatedElements->isChecked());
        if ( d->m_mainWindow->isVisibleRelatedWidget() )
        {
            setBeanOnRelatedWidget();
        }
    }
    else
    {
        d->m_mainWindow->showOrHideRelatedElements(false);
    }
}

void DBFormDlg::freezeModel()
{
    if ( !d->m_itemView.isNull() ) {
        d->m_itemView->freezeModel();
    }
}

void DBFormDlg::defrostModel()
{
    if ( !d->m_itemView.isNull() && !d->m_mainWindow->isVisibleRelatedWidget() ) {
        d->m_itemView->defrostModel();
    }
}

void DBFormDlg::setFilterKeyColumn(const QString &dbFieldName, const QString &op, const QVariant &value, int level)
{
    if ( d->m_itemView->filterModel() )
    {
        d->m_itemView->filterModel()->setFilterKeyColumn(dbFieldName, value, op, level);
    }
}

void DBFormDlg::resetFilter()
{
    if ( d->m_itemView->filterModel() )
    {
        d->m_itemView->filterModel()->resetFilter();
    }
}

void DBFormDlg::setFilter(const QString &filter)
{
    if ( d->m_itemView->filterModel() )
    {
        d->m_itemView->filterModel()->setFilter(filter);
    }
}

/*!
  Comprueba si el usuario tiene permisos para siquiera abrir este formulario
*/
bool DBFormDlg::checkPermissionsToOpen()
{
    if ( !AERPLoggedUser::instance()->checkMetadataAccess('r', d->m_tableName) )
    {
        QMessageBox::warning(this,qApp->applicationName(), trUtf8("No tiene permisos para acceder a estos datos."), QMessageBox::Ok);
        return false;
    }
    return true;
}

bool DBFormDlg::isFrozenModel() const
{
    return d->m_itemView->isFrozenModel();
}

/*!
  Permite llamar a un método de la clase que controla al formulario. Muy interesante
  para obtener valores determinados del formulario.
  */
QScriptValue DBFormDlg::callQSMethod(const QString &method)
{
    QScriptValue result;
    d->m_engine.callQsObjectFunction(result, method);
    return result;
}

/**
  En los DBForm, y si en la definición de la <table> se han establecido las variables "itemsFilterColumn"
  el usuario está visualizando los registros segun un filtro. Si la variable setFilterValueOnNewRecords está puesta
  a true, entonces, esos valores deben precargarse en el bean. Aquí se devuelve una lista con valores:
  campo, valor con los filtros */
QHash<QString, QVariant> DBFormDlg::filterValuesToSetOnBean()
{
    QHash<QString, QVariant> list;
    BaseBeanMetadata *metadata = BeansFactory::metadataBean(d->m_tableName);
    if ( metadata == NULL )
    {
        return list;
    }
    QList<QHash<QString, QString> > itemsFilter = metadata->itemsFilterColumn();
    foreach ( HashString hash, itemsFilter )
    {
        if ( hash.contains("filterValueOnNewRecords") )
        {
            bool setFilterValue = hash["filterValueOnNewRecords"] == "true" ? true : false;
            if ( setFilterValue )
            {
                QVariant filterValue = d->m_itemView->filterValue(hash.value("fieldToFilter"));
                if ( filterValue.isValid() )
                {
                    list[hash.value("fieldToFilter")] = filterValue;
                }
            }
        }
    }
    return list;
}

/**
 * @brief DBRecordDlg::printRecord
 * Slot para imprimir el registro.
 */
void DBFormDlg::printRecord()
{
    if ( !d->m_dlg.isNull() )
    {
        return;
    }

    QList<ReportMetadata *> reports = ReportRun::availableReports(d->m_metadata->tableName());
    if ( reports.size() == 0 )
    {
        return;
    }

    d->m_canDefrostModel = false;
    d->m_itemView->freezeModel();

    QItemSelectionModel *selModel = d->m_itemView->selectionModel();
    QModelIndexList indexes = selModel->selectedRows();

    ReportRun run;
    if ( !indexes.isEmpty() )
    {
        FilterBaseBeanModel *model = d->m_itemView->filterModel();
        if ( model != NULL )
        {
            foreach (const QModelIndex rowIndex, indexes)
            {
                BaseBeanSharedPointer bean = model->beanToBeEdited(rowIndex);
                if ( !bean.isNull() )
                {
                    // Se pasa de forma redundante el tableName ya que este puede ser
                    run.setBean(bean.data());
                }
            }
        }
    }
    else
    {
        run.setLinkedTo(d->m_metadata->tableName());
    }

    run.setParentWidget(this);
    run.showDialog();

    d->m_canDefrostModel = true;
    if ( !d->m_mainWindow->isVisibleRelatedWidget() )
    {
        d->m_itemView->defrostModel();
    }
}

void DBFormDlg::emailRecord()
{
#ifdef ALEPHERP_SMTP_SUPPORT
    if ( !d->m_dlg.isNull() )
    {
        return;
    }

    BaseBeanPointer bean = selectedBean();
    if ( bean.isNull() )
    {
        QMessageBox::information(this, qApp->applicationName(), trUtf8("Debe seleccionar un registro desde el que enviar el correo electrónico."));
        return;
    }
    QScopedPointer<SendEmailDlg> dlg (new SendEmailDlg(bean.data(), this));
    if ( dlg->openSuccess() )
    {
        d->m_canDefrostModel = false;
        d->m_itemView->freezeModel();

        if ( d->isPrintButtonVisible() )
        {
            int ret = QMessageBox::question(this, qApp->applicationName(),
                                            trUtf8("¿Desea enviar con el correo electrónico el impreso asociado a este registro?"), QMessageBox::Yes | QMessageBox::No);
            if ( ret == QMessageBox::Yes )
            {
                // Ejecutamos el informe asociado, a este bean.
                ReportRun reportRun;
                reportRun.setBean(bean.data());
                reportRun.setParentWidget(this);
                if ( !reportRun.pdf(1, false) )
                {
                    QMessageBox::information(this, qApp->applicationName(),
                                             trUtf8("Se ha producido un error al ejecutar el informe. El error es: %1").arg(reportRun.message()), QMessageBox::Ok);
                }
                else
                {
                    dlg->addAttachment(reportRun.pathToGeneratedFile(), "application/pdf");
                }
            }
        }
        dlg->setModal(true);
        dlg->exec();
        if ( dlg->wasSent() )
        {
            EmailObject email = dlg->sentEmail();
            if (!EmailDAO::saveEmail(email))
            {
                QMessageBox::warning(this, qApp->applicationName(), trUtf8("No se ha podido guardar el correo electrónico en los históricos, aunque se envió correctamente."), QMessageBox::Ok);
            }
            RelatedElementPointer element = bean.data()->newRelatedElement(email);
            RelatedDAO::saveRelatedElement(element);
            // Esto fuerza el refresco del modelo
            setBeanOnRelatedWidget();
        }
        d->m_canDefrostModel = true;
        if ( !d->m_mainWindow->isVisibleRelatedWidget() )
        {
            d->m_itemView->defrostModel();
        }
    }
#endif
}

void DBFormDlg::showOrHideRelatedElements()
{
    if ( ui->pbRelatedElements->isChecked() )
    {
        d->m_itemView->freezeModel();
    }
    d->m_mainWindow->showOrHideRelatedElements(ui->pbRelatedElements->isChecked());
    if ( ui->pbRelatedElements->isChecked() )
    {
        setBeanOnRelatedWidget();
    }
}

void DBFormDlg::availableButtonsFromBean()
{
    BaseBeanPointer bean = selectedBean();
    if ( bean.isNull() )
    {
        return;
    }
    ui->pbEdit->setEnabled(!bean->readOnly());
    ui->pbDelete->setEnabled(!bean->readOnly());
}

void DBFormDlg::setBeanOnRelatedWidget()
{
    if ( d->m_metadata->canHaveRelatedElements() ||
         d->m_metadata->showSomeRelationOnRelatedElementsModel() ||
         d->m_metadata->canSendEmail() ||
         d->m_metadata->canHaveRelatedDocuments() )
    {
        BaseBeanPointer bean = selectedBean();
        d->m_mainWindow->setRelatedWidgetBean(bean.data());
    }
}

void DBFormDlg::showHelp()
{
    if ( !d->m_mainWindow.isNull() )
    {
        d->m_mainWindow->showHelp(d->m_helpUrl);
    }
}

void DBFormDlg::view()
{
    AlephERP::FormOpenType openType = AlephERP::ReadOnly;

    if ( !d->m_dlg.isNull() )
    {
        return;
    }

    FilterBaseBeanModel *model = d->m_itemView->filterModel();;
    QItemSelectionModel *selectionModel = d->m_itemView->selectionModel();;
    QHash<QString, QVariant> filterValuesToSet = this->filterValuesToSetOnBean();
    CommonsFunctions::setOverrideCursor(QCursor(Qt::WaitCursor));
    if ( d->m_metadata->tableName() == QString("%1_system").arg(alephERPSettings->systemTablePrefix()) )
    {
#if defined(ALEPHERP_ADVANCED_EDIT) && defined (ALEPHERP_DEVTOOLS)
        d->m_dlg = new AERPSystemObjectEditDlg(model, selectionModel, filterValuesToSet, openType, this);
#else
        d->m_canDefrostModel = true;
        if ( !d->m_mainWindow->isVisibleRelatedWidget() )
        {
            d->m_itemView->defrostModel();
        }
        return;
#endif
    }
    else
    {
        d->m_dlg = new DBRecordDlg(model, selectionModel, filterValuesToSet, openType, this);
    }
    CommonsFunctions::restoreOverrideCursor();
    if ( d->m_dlg->openSuccess() && d->m_dlg->init() )
    {
        connect(d->m_dlg.data(), SIGNAL(accepted()), this, SLOT(recordDlgClosed()));
        connect(d->m_dlg.data(), SIGNAL(rejected()), this, SLOT(recordDlgClosed()));
        d->m_dlg->setModal(true);
        d->m_dlg->setCanChangeModality(true);
        d->m_dlg->exec();
    }
    else
    {
        delete d->m_dlg;
    }
}

void DBFormDlg::specialEdit(const QString code)
{
    QStringList list = code.split(";");
    edit(list.at(0), list.at(1), list.at(2));
}

void DBFormDlg::exportSpreadSheet()
{
    if ( d->m_itemView->filterModel() == NULL )
    {
        return;
    }
    FilterBaseBeanModel *filterModel = d->m_itemView->filterModel();
    QStringList displayTypes;

    foreach (AERPSpreadSheetIface *iface, AERPSpreadSheet::ifaces())
    {
        if ( iface->canWriteFiles() )
        {
            displayTypes << iface->displayType();
        }
    }

    bool ok;
    QString displayType = QInputDialog::getItem(this,
                                                qApp->applicationName(),
                                                trUtf8("Seleccione el formato al que desea exportar la información."),
                                                displayTypes,
                                                0,
                                                false,
                                                &ok);
    if ( !ok )
    {
        return;
    }

    QString writeTo = QFileDialog::getExistingDirectory(this, trUtf8("Seleccione el directorio en el que guardar los datos"), QDir::homePath());
    if ( writeTo.isEmpty() )
    {
        return;
    }
    AERPSpreadSheetIface *iface;
    foreach (AERPSpreadSheetIface *i, AERPSpreadSheet::ifaces())
    {
        if ( i->displayType() == displayType )
        {
            iface = i;
            break;
        }
    }
    writeTo.append("/").append(d->m_metadata->alias()).append(".").append(iface->type());

    QProgressDialog dlg;
    d->m_operationCanceled = false;
    dlg.setMaximum(d->m_itemView->filterModel()->rowCount());
    dlg.setMinimum(0);
    dlg.setLabelText(trUtf8("Exportando información... Por favor, espere."));
    dlg.setWindowTitle(trUtf8("%1 - Exportando datos").arg(qApp->applicationName()));
    dlg.setWindowModality(Qt::WindowModal);
    connect(&dlg, SIGNAL(canceled()), this, SLOT(operationCanceled()));
    connect(filterModel, SIGNAL(rowProcessed(int)), &dlg, SLOT(setValue(int)));
    dlg.show();
    qApp->processEvents();

    CommonsFunctions::setOverrideCursor(Qt::WaitCursor);
    bool r = filterModel->exportToSpreadSheet(writeTo, iface->type());
    CommonsFunctions::restoreOverrideCursor();
    if ( !r )
    {
        QMessageBox::warning(this, qApp->applicationName(), trUtf8("Ha ocurrido un error exportando los datos. \nEl error es: %1").arg(filterModel->lastErrorMessage()));
    }
}

void DBFormDlg::operationCanceled()
{
    d->m_operationCanceled = true;
}

/**
 * @brief DBFormDlg::showContextMenu
 * @param point
 * Menú contextual para los ItemView TreeView y TableView.
 * Para los TableView muestra tres opciones: Insertar, editar y borrar (las dos últimas dependiendo de donde se pinche)
 * Para los TreeView muestra opciones para insertar registros intermedios en el árbol.
 */
void DBFormDlg::showContextMenu(const QPoint &point)
{
    QModelIndex idx = d->m_itemView->itemView()->indexAt(point);
    QMenu contextMenu;
    QPoint globalPos = d->m_itemView->itemView()->viewport()->mapToGlobal(point);
    QAction *addIntermediateAction = new QAction(QIcon(":/generales/images/edit_add.png"), trUtf8(""), this);

    connect(addIntermediateAction, SIGNAL(triggered()), SLOT(insertChild()));

    if ( !idx.isValid() )
    {
        if ( d->m_itemView->sourceModel()->data(idx, AlephERP::CanAddRowRole).toBool() )
        {
            ui->actionCreate->setText(d->m_itemView->sourceModel()->data(idx, AlephERP::InsertRowTextRole).toString());
            contextMenu.addAction(ui->actionCreate);
        }
    }

    if ( idx.isValid() )
    {
        d->m_itemView->itemView()->selectionModel()->select(idx, QItemSelectionModel::ClearAndSelect);
        if ( idx.data(AlephERP::CanAddRowRole).toBool() )
        {
            addIntermediateAction->setText(idx.data(AlephERP::InsertRowTextRole).toString());
            contextMenu.addAction(addIntermediateAction);
        }
        if ( idx.data(AlephERP::CanEditRole).toBool() )
        {
            ui->actionEdit->setText(idx.data(AlephERP::EditRowTextRole).toString());
            contextMenu.addAction(ui->actionEdit);
        }
        if (idx.data(AlephERP::CanDeleteRole).toBool() )
        {
            contextMenu.addSeparator();
            ui->actionDelete->setText(idx.data(AlephERP::DeleteRowTextRole).toString());
            contextMenu.addAction(ui->actionDelete);
        }
    }
    contextMenu.exec(globalPos);
}


