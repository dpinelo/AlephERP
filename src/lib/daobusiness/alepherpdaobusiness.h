#ifndef ALEPHERPDAOBUSINESS_H
#define ALEPHERPDAOBUSINESS_H
// EL ORDEN DE INCLUSIÓN ES IMPORTANTE
#include "configuracion.h"
#include "globales.h"
#include "aerpcommon.h"
#include "call_once.h"
#include "singleton.h"
#include "aerpapplication.h"
#include "dao/database.h"
#include "dao/dbobject.h"
#include "dao/dbfieldobserver.h"
#include "dao/basebeanobserver.h"
#include "dao/observerfactory.h"
#include "dao/dbrelationobserver.h"
#include "dao/userdao.h"
#include "dao/historydao.h"
#include "dao/systemdao.h"
#include "dao/modulesdao.h"
#include "dao/aerptransactioncontext.h"
#include "dao/basedao.h"
#include "dao/beans/basebean.h"
#include "dao/beans/dbfield.h"
#include "dao/beans/beansfactory.h"
#include "dao/beans/dbrelation.h"
#include "dao/beans/basebeanvalidator.h"
#include "dao/beans/basebeanmetadata.h"
#include "dao/beans/dbfieldmetadata.h"
#include "dao/beans/dbrelationmetadata.h"
#include "dao/beans/reportmetadata.h"
#include "models/basebeanmodel.h"
#include "models/treeitem.h"
#include "models/treeviewmodel.h"
#include "models/perpquerymodel.h"
#include "models/aerphtmldelegate.h"
#include "models/relationbasebeanmodel.h"
#include "models/filterbasebeanmodel.h"
#include "models/envvars.h"
#include "models/aerpimageitemdelegate.h"
#include "models/aerpinlineedititemdelegate.h"
#include "reports/aerpreportsinterface.h"
#include "widgets/dbbasewidget.h"
#include "widgets/dbabstractview.h"
#include "widgets/dbcombobox.h"
#include "widgets/dbdetailview.h"
#include "widgets/dbtableview.h"
#include "widgets/fademessage.h"
#include "widgets/dblineedit.h"
#include "widgets/dblabel.h"
#include "widgets/dbgrouprelationmmhelper.h"
#include "widgets/dbmapposition.h"
#ifdef ALEPHERP_BARCODE
#include "widgets/dbbarcode.h"
#endif
#include "widgets/dbcheckbox.h"
#include "widgets/dbtextedit.h"
#include "widgets/dbdatetimeedit.h"
#include "widgets/dbnumberedit.h"
#include "widgets/dbscheduleview.h"
#include "widgets/dbfullscheduleview.h"
#include "widgets/dbfilterscheduleview.h"
#ifdef ALEPHERP_ADVANCED_EDIT
#include "widgets/dbrichtextedit.h"
#include "widgets/dbcodeedit.h"
#endif
#include "widgets/dbtabwidget.h"
#include "widgets/dbfiltertableview.h"
#include "widgets/dbframebuttons.h"
#include "widgets/dbtreeview.h"
#include "widgets/dblistview.h"
#include "widgets/graphicsstackedwidget.h"
#include "widgets/waitwidget.h"
#include "widgets/dbtableviewcolumnorderform.h"
#include "widgets/menutreewidget.h"
#include "widgets/dbchooserecordbutton.h"
#include "widgets/dbfileupload.h"
#include "widgets/dbfiltertreeview.h"
#include "widgets/aerpsplashscreen.h"
#include "widgets/aerpwaitdb.h"
#include "widgets/dbrelatedelementsview.h"
#include "widgets/dbchooserelatedrecordbutton.h"
#include "forms/perpbasedialog.h"
#include "forms/dbsearchdlg.h"
#include "forms/dbrecorddlg.h"
#include "forms/perpbasedialog_p.h"
#include "forms/dbformdlg.h"
#include "forms/scriptdlg.h"
#include "forms/qdlgacercade.h"
#include "forms/seleccionestilodlg.h"
#include "forms/logindlg.h"
#include "forms/changepassworddlg.h"
#include "forms/historyviewdlg.h"
#include "forms/registereddialogs.h"
#include "forms/perpmainwindow.h"
#include "forms/aerpmdiarea.h"
#include "forms/aerpconsistencymetadatatabledlg.h"
#include "forms/dbreportrundlg.h"
#include "qlogger.h"
#include "business/aerpscheduledjobs.h"
#include "business/aerploggeduser.h"
#include "qxtglobal.h"
#include "qxtnamespace.h"
#ifdef ALEPHERP_LOCALMODE
#include "dao/batchdao.h"
#include "business/aerpbatchload.h"
#include "forms/aerpdiffremotelocalrecord.h"
#endif
#ifdef ALEPHERP_FIREBIRD_SUPPORT
#include "dao/aerpfirebirddatabase.h"
#endif
#ifdef ALEPHERP_DEVTOOLS
#include "widgets/aerpsystemobjecteditorwidget.h"
#endif
#ifdef ALEPHERP_DOC_MANAGEMENT
#include "widgets/dbrecorddocuments.h"
#include "business/docmngmnt/aerpdocumentdaopluginiface.h"
#include "dao/beans/aerpdocmngmntdocument.h"
#include "dao/beans/aerpdocmngmntitem.h"
#include "dao/beans/aerpdocmngmntnode.h"
#endif
#endif // ALEPHERPDAOBUSINESS_H
