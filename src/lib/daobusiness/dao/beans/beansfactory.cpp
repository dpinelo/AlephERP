/***************************************************************************
 *   Copyright (C) 2010 by David Pinelo   *
 *   alepherp@alephsistemas.es   *
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
#if (QT_VERSION < QT_VERSION_CHECK(5,0,0))
#include <QtGui>
#else
#include <QtWidgets>
#endif
#include "configuracion.h"
#include <aerpcommon.h>
#include <globales.h>
#include "beansfactory.h"
#include "dao/basedao.h"
#include "dao/systemdao.h"
#ifdef ALEPHERP_LOCALMODE
#include "dao/batchdao.h"
#endif
#include "dao/beans/aerpsystemobject.h"
#include "dao/beans/basebean.h"
#include "dao/beans/basebeanmetadata.h"
#include "dao/beans/dbrelation.h"
#include "dao/beans/dbrelationmetadata.h"
#include "dao/beans/dbfieldmetadata.h"
#include "dao/beans/reportmetadata.h"
#include "dao/beans/relatedelement.h"
#include "dao/database.h"
#include "models/envvars.h"
#include "business/aerpscheduledjobs.h"
#include "business/aerploggeduser.h"
#ifdef ALEPHERP_DEVTOOLS
#include "dao/modulesdao.h"
#include "forms/aerpconsistencymetadatatabledlg.h"
#endif

QList<AERPSystemModule *> BeansFactory::systemModules;
QList<QPointer<BaseBeanMetadata> > BeansFactory::metadataBeans;
QList<ReportMetadata *> BeansFactory::metadataReports;
QList<AERPScheduledJobMetadata *> BeansFactory::metadataJobs;
QList<AERPScheduledJob *> BeansFactory::jobs;
QStringList BeansFactory::systemWidgets;
QStringList BeansFactory::systemReports;
QHash<QString, QString> BeansFactory::systemScripts;
QHash<QString, bool> BeansFactory::systemScriptsDebug;
QHash<QString, bool> BeansFactory::systemScriptsDebugOnInit;
bool BeansFactory::systemInited = false;
bool BeansFactory::batchMode = false;
QDateTime BeansFactory::dateTimeBatchMode;
QString BeansFactory::m_lastError;

QThreadStorage<BeansFactory *> threadBeansFactory;
QStringList loadedResources;

#ifdef ALEPHERP_LOCALMODE
static BatchDAO *batchDAO;
#endif

#define MSG_NO_EXISTE_UI QT_TR_NOOP("No existe un fichero UI en base de datos con la defición del formulario de búsqueda")

#define SQL_CONSISTENCY_PSQL "SELECT column_name, data_type, is_nullable, character_maximum_length FROM information_schema.columns WHERE table_name = :tablename;"

BeansFactory::BeansFactory(QObject *parent) : QObject(parent)
{
}

BeansFactory::~BeansFactory()
{
}

/*!
  Esta función crea una estructura con una copia maestra de todos los metadatas de beans existentes
  en la aplicación
  */
bool BeansFactory::buildMetadataBeans()
{
    bool result = false;

    if ( SystemDAO::countSystemObjects() == 0 )
    {
        //return false;
    }
    QList<AERPSystemObject *> list = SystemDAO::localSystemObjectsForThisDevice();
    if ( SystemDAO::lastErrorMessage().isEmpty() )
    {
        foreach (AERPSystemObject *object, list)
        {
            // ¿Existe un objeto hijo de éste, que se referencie y esté en otro módulo? Si es así, se escogerá ese
            if ( object->type() == "table" && object->idOrigin() == 0 )
            {
                QLogger::QLog_Debug(AlephERP::stLogOther, QString("BeansFactory::buildMetadataBeans: Metadata disponible: [%1][%2]").
                                    arg(object->module()->name()).arg(object->name()));
                BaseBeanMetadata *metadata = new BaseBeanMetadata(qApp);
                metadata->setTableName(object->name());
                metadata->setModule(object->module());
                metadata->setXml(object->content());
                BeansFactory::metadataBeans.append(metadata);
                BeansFactory::instance()->emitProgressValue();
            }
        }
        foreach (QPointer<BaseBeanMetadata> m, BeansFactory::metadataBeans)
        {
            if ( m )
            {
                m->consolidateTemp();
            }
        }
        result = true;
    }
    return result;
}

void BeansFactory::insertMetadataBean(AERPSystemObject *object)
{
    // ¿Existe un objeto hijo de éste, que se referencie y esté en otro módulo? Si es así, se escogerá ese
    if ( object->type() == "table" && object->idOrigin() == 0 )
    {
        BaseBeanMetadata *metadata = new BaseBeanMetadata(qApp);
        metadata->setTableName(object->name());
        metadata->setModule(object->module());
        metadata->setXml(object->content());
        BeansFactory::metadataBeans.append(metadata);
        metadata->consolidateTemp();
    }
}

/*!
  Esta función almacena en el directorio temporal todos los widgets que se crean desde fuera
  */
bool BeansFactory::buildUIWidgets()
{
    bool result = false;

    QList<AERPSystemObject *> list = SystemDAO::localSystemObjectsForThisDevice();
    if ( SystemDAO::lastErrorMessage().isEmpty() )
    {
        foreach (AERPSystemObject *object, list)
        {
            if ( object->type() == "ui" && object->idOrigin() == 0 )
            {
                QString fileName = QString("%1/%2").
                                   arg(QDir::fromNativeSeparators(alephERPSettings->dataPath())).
                                   arg(object->name());
                QFile file (fileName);
                if ( !file.open( QFile::ReadWrite | QFile::Truncate ) )
                {
                    QMessageBox::warning(NULL, qApp->applicationName(), trUtf8(MSG_NO_EXISTE_UI), QMessageBox::Ok);
                    return false;
                }
                QTextStream out (&file);
                //out.setCodec(configuracion.fileSystemCodec());
                out.setCodec("UTF-8");
                out << object->content();
                file.flush();
                file.close();
                // Ahora comprobamos que el archivo se ha guardado correctamente
                if ( file.open(QFile::ReadOnly) )
                {
                    BeansFactory::systemWidgets << object->name();
                }
                else
                {
                    QLogger::QLog_Debug(AlephERP::stLogOther, QString("BeansFactory::buildTableWidgets: No se ha creado el fichero: %1").arg(fileName));
                    QString message = trUtf8("No se ha podido crear el fichero: %1").arg(fileName);
                    QMessageBox::warning(NULL, qApp->applicationName(), message, QMessageBox::Ok);
                    return false;
                }
                file.close();
                BeansFactory::instance()->emitProgressValue();
            }
        }
        result = true;
    }
    return result;
}

bool BeansFactory::buildScripts()
{
    QList<AERPSystemObject *> list = SystemDAO::localSystemObjectsForThisDevice();
    if ( SystemDAO::lastErrorMessage().isEmpty() )
    {
        foreach (AERPSystemObject *systemObject, list)
        {
            if ( (systemObject->type() == "qs" || systemObject->type() == "js") && systemObject->idOrigin() == 0 )
            {
                if ( systemObject->name() == "__init__.js" || systemObject->name().contains("/") )
                {
                    QString fileName, dir, fullDir, fullFileName;
                    if ( systemObject->name() == "__init__.js" )
                    {
                        dir = "script/";
                        fileName = "__init__.js";
                    }
                    else
                    {
                        QStringList tree = systemObject->name().split("/");
                        for (int i = 0 ; i < tree.size() - 1 ; i++ )
                        {
                            if ( dir.isEmpty() )
                            {
                                dir = QString("script/%1").arg(tree.at(i));
                            }
                            else
                            {
                                dir = QString("%1/%2").arg(dir).arg(tree.at(i));
                            }
                        }
                        fileName = tree.at(tree.size()-1);
                    }
                    fullDir = QString("%1/%2").arg(QDir::fromNativeSeparators(alephERPSettings->dataPath())).arg(dir);
                    QDir initDir(fullDir);
                    if ( !initDir.exists() )
                    {
                        initDir.setPath(QDir::fromNativeSeparators(alephERPSettings->dataPath()));
                        if ( !initDir.mkpath(dir) )
                        {
                            QMessageBox::warning(NULL, qApp->applicationName(), trUtf8("No se pudo crear el subdirectorio: ").arg(fullDir), QMessageBox::Ok);
                            return false;
                        }
                    }
                    fullFileName = QString("%1/%2").arg(fullDir).arg(fileName);
                    QFile file (fullFileName);
                    if ( !file.open( QFile::ReadWrite | QFile::Truncate ) )
                    {
                        QMessageBox::warning(NULL, qApp->applicationName(), trUtf8("No se pudo crear el archivo: ").arg(fullDir),
                                             QMessageBox::Ok);
                        return false;
                    }
                    QTextStream out (&file);
                    // OJO: En este caso particular no exportamos en UTF-8, la razón es que el motor QtScript espera los archivos .js
                    // en la condificación estándar, por eso, comentamos.
                    // out.setCodec("UTF-8");
                    out << systemObject->content();
                    file.flush();
                    file.close();
                }
                BeansFactory::systemScripts[systemObject->name()] = systemObject->content();
                BeansFactory::systemScriptsDebug[systemObject->name()] = systemObject->debug();
                BeansFactory::systemScriptsDebugOnInit[systemObject->name()] = systemObject->onInitDebug();
                BeansFactory::instance()->emitProgressValue();
            }
        }
    }
    return true;
}

/*!
  Cargamos y guardamos los informes que se hayan almacenado en base de datos
  */
bool BeansFactory::buildTableReports()
{
    bool result = false;

    QList<AERPSystemObject *> list = SystemDAO::localSystemObjectsForThisDevice();
    if ( SystemDAO::lastErrorMessage().isEmpty() )
    {
        foreach (AERPSystemObject *systemObject, list)
        {
            if ( systemObject->type() == "report" && systemObject->idOrigin() == 0 )
            {
                QString fileName = QString("%1/%2").
                                   arg(QDir::fromNativeSeparators(alephERPSettings->dataPath())).
                                   arg(systemObject->name());
                QFile file (fileName);
                if ( !file.open( QFile::ReadWrite | QFile::Truncate ) )
                {
                    QMessageBox::warning(NULL, qApp->applicationName(), trUtf8(MSG_NO_EXISTE_UI),
                                         QMessageBox::Ok);
                    return false;
                }
                QTextStream out (&file);
                out.setCodec("UTF-8");
                out << systemObject->content();
                file.flush();
                file.close();
                // Ahora comprobamos que el archivo se ha guardado correctamente
                if ( file.open(QFile::ReadOnly) )
                {
                    BeansFactory::systemReports << systemObject->name();
                }
                else
                {
                    QLogger::QLog_Debug(AlephERP::stLogOther, QString("BeansFactory::buildTableWidgets: No se ha creado el fichero: %1").arg(fileName));
                    QString message = trUtf8("No se ha podido crear el fichero: %1").arg(fileName);
                    QMessageBox::warning(NULL, qApp->applicationName(), message, QMessageBox::Ok);
                    return false;
                }
                file.close();
                BeansFactory::instance()->emitProgressValue();
            }
        }
        result = true;
    }
    return result;
}

/**
  Leemos los resources (.rcc) ficheros con iconos, imágenes... asociados a los widgets anteriores
  para cargarlos
  */
bool BeansFactory::buildResources()
{
    bool result = false;

    QList<AERPSystemObject *> list = SystemDAO::localSystemObjectsForThisDevice();
    if ( SystemDAO::lastErrorMessage().isEmpty() )
    {
        foreach (AERPSystemObject *systemObject, list)
        {
            if ( (systemObject->type() == "rcc" || systemObject->type() == "binary") && systemObject->idOrigin() == 0 )
            {
                bool canRegister = systemObject->type() == "rcc";
                QByteArray binaryContent = QByteArray::fromBase64(systemObject->content().toUtf8());
                QString fileName = QString("%1/%2").
                                   arg(QDir::fromNativeSeparators(alephERPSettings->dataPath())).
                                   arg(systemObject->name());
                QFile file (fileName);
                if ( !file.open( QFile::ReadWrite | QFile::Truncate ) )
                {
                    return false;
                }
                file.write(binaryContent);
                file.flush();
                file.close();
                // Ahora comprobamos que el archivo se ha guardado correctamente
                if ( !file.open(QFile::ReadOnly) || systemObject->type() != "rcc" )
                {
                    canRegister = false;
                }
                file.close();
                QLogger::QLog_Debug(AlephERP::stLogOther, QString("BeansFactory::buildResource: Registrando en sistema el archivo de recursos: %1").arg(fileName));
                if ( canRegister && !QResource::registerResource(fileName) )
                {
                    QLogger::QLog_Error(AlephERP::stLogOther, QString("BeansFactory::buildResource: No se pudo registrar en sistema el archivo de recursos: %1").arg(fileName));
                }
                else
                {
                    loadedResources << fileName;
                }
                BeansFactory::instance()->emitProgressValue();
            }
        }
        result = true;
    }
    return result;
}

bool BeansFactory::buildMetadataReports()
{
    bool result = false;

    QList<AERPSystemObject *> list = SystemDAO::localSystemObjectsForThisDevice();
    if ( SystemDAO::lastErrorMessage().isEmpty() )
    {
        foreach (AERPSystemObject *systemObject, list)
        {
            if ( systemObject->type() == "reportDef" && systemObject->idOrigin() == 0 )
            {
                ReportMetadata *metadata = new ReportMetadata(qApp);
                metadata->setName(systemObject->name());
                metadata->setModule(systemObject->module());
                metadata->setXml(systemObject->content());
                BeansFactory::metadataReports.append(metadata);
                BeansFactory::instance()->emitProgressValue();
            }
        }
        result = true;
    }
    return result;
}

bool BeansFactory::buildScheduledJobs()
{
    bool result = false;

    QList<AERPSystemObject *> list = SystemDAO::localSystemObjectsForThisDevice();
    if ( SystemDAO::lastErrorMessage().isEmpty() )
    {
        foreach (AERPSystemObject *systemObject, list)
        {
            if ( systemObject->type() == "job" && systemObject->idOrigin() == 0 )
            {
                AERPScheduledJobMetadata *metadata = new AERPScheduledJobMetadata(qApp);
                metadata->setName(systemObject->name());
                metadata->setModule(systemObject->module());
                metadata->setXml(systemObject->content());
                BeansFactory::metadataJobs.append(metadata);
                BeansFactory::instance()->emitProgressValue();
            }
        }
        result = true;
    }
    return result;
}

bool BeansFactory::buildHelpResources()
{
    bool result = false;

    QList<AERPSystemObject *> list = SystemDAO::localSystemObjectsForThisDevice();
    if ( SystemDAO::lastErrorMessage().isEmpty() )
    {
        foreach (AERPSystemObject *systemObject, list)
        {
            if ( systemObject->type() == "help" && systemObject->idOrigin() == 0 )
            {
                QByteArray binaryContent = QByteArray::fromBase64(systemObject->content().toUtf8());
                QString fileName = QString("%1/%2").
                                   arg(QDir::fromNativeSeparators(alephERPSettings->dataPath())).
                                   arg(systemObject->name());
                QFile file (fileName);
                if ( !file.open( QFile::ReadWrite | QFile::Truncate ) )
                {
                    return false;
                }
                file.write(binaryContent);
                file.flush();
                file.close();
                BeansFactory::instance()->emitProgressValue();
            }
        }
        result = true;
    }
    return result;
}

/**
 * @brief AERPScheduledJob::initJobs
 * Inicializa los trabajos programados del sistema
 */
void BeansFactory::initJobs()
{
    foreach (AERPScheduledJobMetadata *m, BeansFactory::metadataJobs)
    {
        if ( m->userName() == AERPLoggedUser::instance()->userName() ||
                m->userName() == "*" ||
                AERPLoggedUser::instance()->checkRole(m->roleName()) ||
                m->roleName() == "*" )
        {
            AERPScheduledJob *job = new AERPScheduledJob(m, qApp);
            BeansFactory::jobs.append(job);
            job->init();
        }
    }
}

/**
 * @brief BeansFactory::buildCalculatedFieldRules
 * Hace un análisis de los campos que están involucrados en el cálculo de otros campos calculados.
 * Debe llamarse cuando se tiene constancia total de que los metadatos se han cargado.
 */
void BeansFactory::buildCalculatedFieldRules()
{
    foreach (QPointer<BaseBeanMetadata> m, BeansFactory::metadataBeans)
    {
        if ( m )
        {
            m->buildFieldsCalculatedRelations();
        }
    }
}

bool BeansFactory::unloadResources()
{
    foreach (QString resource, loadedResources)
    {
        QResource::unregisterResource(resource);
    }
    return true;
}

/**
  Refresca el UI
  */
bool BeansFactory::refreshSystemObject(const QString &name, const QString &type)
{
    bool result = false;

    AERPSystemObject *object = SystemDAO::systemObject(name, type);
    if ( object != NULL )
    {
        QString fileName = QString("%1/%2").
                           arg(QDir::fromNativeSeparators(alephERPSettings->dataPath())).
                           arg(name);
        QFile file (fileName);
        if ( !file.open( QFile::ReadWrite | QFile::Truncate ) )
        {
            QMessageBox::warning(NULL, qApp->applicationName(), trUtf8(MSG_NO_EXISTE_UI),
                                 QMessageBox::Ok);
            return false;
        }
        QTextStream out (&file);
        out.setCodec("UTF-8");
        out << object->content();
        file.flush();
        file.close();
        // Ahora comprobamos que el archivo se ha guardado correctamente
        if ( !file.open(QFile::ReadOnly) )
        {
            QLogger::QLog_Debug(AlephERP::stLogOther, QString("BeansFactory::buildTableWidgets: No se ha creado el fichero: %1").arg(fileName));
            QString message = trUtf8("No se ha podido crear el fichero: %1").arg(fileName);
            QMessageBox::warning(NULL, qApp->applicationName(), message, QMessageBox::Ok);
            return false;
        }
        file.close();
        result = true;
    }
    return result;
}

void BeansFactory::cancelLoadBatch()
{
#ifdef ALEPHERP_LOCALMODE
    if ( batchDAO != NULL )
    {
        batchDAO->cancel();
    }
#endif
}

void BeansFactory::setProgressOffset(int value)
{
    m_progressOffset = value / 2;
}

void BeansFactory::emitProgressValue()
{
    emit initProgressValue(m_progressOffset);
    m_progressOffset++;
}

QString BeansFactory::lastErrorMessage()
{
    return BeansFactory::m_lastError;
}

/**
 * @brief BeansFactory::enterOnBatchMode El sistema entrará en modo trabajo local, para aquellas tablas así configuradas.
 * Puede ser que el programa se inicie ya en modo local, lo que implicará que no hace falta preparar el sistema (ya estaba
 * preparado). Eso se especifica con fromInit
 * @return
 */
bool BeansFactory::enterOnBatchMode(const QString &messageTemplate, bool fromInit)
{
#ifdef ALEPHERP_LOCALMODE
    if ( !Database::createBatchConnection() )
    {
        BeansFactory::m_lastError = Database::lastErrorMessage();
        return false;
    }
    CommonsFunctions::processEvents();
    if ( fromInit )
    {
        BeansFactory::batchMode = true;
        BeansFactory::dateTimeBatchMode = alephERPSettings->initLocalMode();
        return true;
    }
    else
    {
        batchDAO = new BatchDAO(BeansFactory::instance(), BeansFactory::instance());
        QObject::connect(batchDAO, SIGNAL(initLoad(int)), BeansFactory::instance(), SIGNAL(initEnterWorkMode(int)));
        QObject::connect(batchDAO, SIGNAL(loadProgress(int)), BeansFactory::instance(), SIGNAL(enterWorkModeProgress(int)));
        QObject::connect(batchDAO, SIGNAL(loadProgress(QString)), BeansFactory::instance(), SIGNAL(enterWorkModeProgressMessage(QString)));
        QObject::connect(batchDAO, SIGNAL(finishLoad()), BeansFactory::instance(), SIGNAL(endEnterWorkMode()));
        batchDAO->setDatabases(BASE_CONNECTION, Database::batchDatabaseName());
        bool result = batchDAO->prepareSystemToLocalWork(messageTemplate);
        if ( result )
        {
            // Esperamos un segundo a terminar, para que el tiempo de entrada en el modo de trabajo sea el correcto.
            BaseDAO::clearAllCache();
            BeansFactory::batchMode = true;
            BeansFactory::dateTimeBatchMode = QDateTime::currentDateTime().addSecs(5);
            alephERPSettings->setLocalMode(true);
            alephERPSettings->setInitLocalMode(BeansFactory::dateTimeBatchMode);
            alephERPSettings->save();
        }
        else
        {
            BeansFactory::m_lastError = batchDAO->lastError();
        }
        delete batchDAO;
        batchDAO = NULL;
        return result;
    }
    return false;
#else
    return false;
#endif
}

/**
 * @brief BeansFactory::finishBatchMode Finaliza el trabajo en local, y vuelca los datos en el sistema en remoto
 * @return
 */
bool BeansFactory::finishBatchMode(const QString &messageTemplate, QString &report)
{
#ifdef ALEPHERP_LOCALMODE
    BeansFactory::dateTimeBatchMode = alephERPSettings->initLocalMode();
    batchDAO = new BatchDAO(BeansFactory::instance(), BeansFactory::instance());
    batchDAO->setDatabases(BASE_CONNECTION, Database::batchDatabaseName());
    QObject::connect(batchDAO, SIGNAL(initLoad(int)), BeansFactory::instance(), SIGNAL(initUploadData(int)));
    QObject::connect(batchDAO, SIGNAL(loadProgress(int)), BeansFactory::instance(), SIGNAL(loadUploadProgress(int)));
    QObject::connect(batchDAO, SIGNAL(loadProgress(QString)), BeansFactory::instance(), SIGNAL(loadUploadProgress(QString)));
    QObject::connect(batchDAO, SIGNAL(finishLoad()), BeansFactory::instance(), SIGNAL(finishUpload()));
    bool result = batchDAO->uploadChanges(messageTemplate, report);
    if ( result )
    {
        BeansFactory::batchMode = false;
        alephERPSettings->setLocalMode(false);
        alephERPSettings->save();
        BaseDAO::clearAllCache();
    }
    else
    {
        BeansFactory::m_lastError = batchDAO->lastError();
    }
    delete batchDAO;
    batchDAO = NULL;
    return result;
#else
    return false;
#endif
}

/**
 * @brief BeansFactory::isOnBatchMode Indica el modo de trabajo
 * @return
 */
bool BeansFactory::isOnBatchMode()
{
    return BeansFactory::batchMode;
}

/**
 * @brief BeansFactory::initOfBatchMode
 * Devuelve el instante en el que se entró en modo de trabajo local
 * @return
 */
QDateTime BeansFactory::initOfBatchMode()
{
    return BeansFactory::dateTimeBatchMode;
}

BeansFactory * BeansFactory::instance()
{
    if ( !threadBeansFactory.hasLocalData() )
    {
        threadBeansFactory.setLocalData(new BeansFactory());
    }
    return threadBeansFactory.localData();
}

AERPSystemModule *BeansFactory::newModule(const QString &id, const QString &name, const QString &description, const QString &showedText, const QString &iconName, bool enabled)
{
    AERPSystemModule *so = new AERPSystemModule(qApp);
    so->setId(id);
    so->setName(name);
    so->setDescription(description);
    so->setShowedName(showedText);
    so->setIcon(iconName);
    so->setEnabled(enabled);
    BeansFactory::systemModules.append(so);
    return so;
}

AERPSystemObject *BeansFactory::newSystemObject(AERPSystemModule *module)
{
    return new AERPSystemObject(module);
}

AERPSystemObject *BeansFactory::newSystemObject(const QString &objectName, const QString &type, const QString &content, bool debug, bool debugOnInit, int version, const QStringList &deviceTypes, AERPSystemModule *module)
{
    return new AERPSystemObject(objectName, type, content, debug, debugOnInit, version, deviceTypes, module);
}

/**
 * @brief BeansFactory::systemObject
 * Devuelve el objeto de sistema
 * @param objectName
 * @param type
 * @return
 */
AERPSystemObject *BeansFactory::systemObject(const QString &objectName, const QString &type)
{
    QList<AERPSystemObject *> list = SystemDAO::localSystemObjectsForThisDevice();
    foreach (AERPSystemObject *so, list) {
        if ( so->type() == type && so->name() == objectName ) {
            return so;
        }
    }
    return NULL;
}

AERPSystemModule *BeansFactory::module(const QString &id)
{
    QStringList availableModules;
    foreach (AERPSystemModule *mod, BeansFactory::systemModules)
    {
        availableModules << mod->id();
        if ( mod->id() == id )
        {
            return mod;
        }
    }
    QLogger::QLog_Info(AlephERP::stLogOther, QString("BeansFactory::module: No existe el módulo [%1].\nMódulos disponibles: [%2]").
                        arg(id).arg(availableModules.join("\n")));
    return NULL;
}

/*!
  Función de la factoría que crea los objetos necesarios. Es posible especificar
  si se crearan los valores por defecto
  */
BaseBeanSharedPointer BeansFactory::newQBaseBean(const QString & tableName, bool setDefaultValue, BaseBeanPointerList fatherBeans, QPointer<DBObject> owner)
{
    // Los punteros QSharedPointer no se les establece el padre, ya que se produciría una duplicidad
    // de responsables en el borrado. Ver ejemplo en
    // http://blog.codef00.com/2011/12/15/not-so-much-fun-with-qsharedpointer/
    BaseBeanSharedPointer sharedBean = BaseBeanSharedPointer (newBaseBean(tableName, setDefaultValue, false, fatherBeans, owner));
    return sharedBean;
}

/*!
  Función de la factoría que crea los objetos necesarios
  */
BaseBean * BeansFactory::newBaseBean(const QString &tableName, bool setDefaultValue, bool setDefaultParent, BaseBeanPointerList fatherBeans, QPointer<DBObject> owner)
{
    QPointer<BaseBeanMetadata> metadata = BeansFactory::metadataBean(tableName);
    if ( metadata.isNull() )
    {
        QLogger::QLog_Debug(AlephERP::stLogOther, QString("BeansFactory::newBaseBean: No existe la tabla: [%1]").arg(tableName));
        return NULL;
    }

    BaseBean *obj = new BaseBean();
    bool b = obj->blockSignals(true);
    obj->setOwner(owner);
    if ( setDefaultParent )
    {
        obj->setParent(this);
    }
    // Al llamar a setMetadata, también se introducen los valores por defecto en el nuevo bean.
    obj->init(metadata, setDefaultValue, fatherBeans);
    // Asignamos valores únicos a los campos seriales, por si hay que encontrar
    // e identificar a un bean y no ha pasado por base de datos
    obj->setSerialUniqueId();
    obj->blockSignals(b);
    return obj;
}

BaseBean *BeansFactory::originalBean(BaseBeanPointer view, QObject *parent)
{
    if ( view.isNull() )
    {
        return NULL;
    }
    foreach (BaseBeanMetadata *m, BeansFactory::metadataBeans)
    {
        if ( m->viewOnGrid() == view->metadata()->tableName() )
        {
            BaseBean *obj = newBaseBean(m->tableName(), false, false);
            obj->setParent(parent == NULL ? this : parent);
            BaseDAO::originalBeanFromView(view, obj);
            return obj;
        }
    }
    return NULL;
}

BaseBeanSharedPointer BeansFactory::originalQBean(BaseBeanPointer view, QObject *parent)
{
    if ( view.isNull() )
    {
        return BaseBeanSharedPointer();
    }
    foreach (BaseBeanMetadata *m, BeansFactory::metadataBeans)
    {
        if ( m->viewOnGrid() == view->metadata()->tableName() )
        {
            BaseBeanSharedPointer obj = newQBaseBean(m->tableName(), false);
            obj->setParent(parent == NULL ? this : parent);
            BaseDAO::originalBeanFromView(view, obj.data());
            return obj;
        }
    }
    return BaseBeanSharedPointer();
}

/**
 * @brief BeansFactory::init
 * Inicializa toda la estructura de datos del sistema, y sobre la que se sustenta: Tablas, UI, scripts, informes...
 * @return
 */
bool BeansFactory::init()
{
    // Importante el orden
    if ( BeansFactory::systemWidgets.isEmpty() )
    {
        if ( !buildUIWidgets() )
        {
            return false;
        }
    }
    buildResources();
    buildHelpResources();
    if ( BeansFactory::systemScripts.isEmpty() )
    {
        if ( !buildScripts() )
        {
            return false;
        }
    }
    // Necesitamos una copia de beans maestros de la aplicación
    if ( BeansFactory::metadataBeans.isEmpty() )
    {
        if ( !buildMetadataBeans() )
        {
            return false;
        }
    }
    if ( BeansFactory::systemReports.isEmpty() )
    {
        if ( !buildTableReports() )
        {
            return false;
        }
    }
    if ( BeansFactory::metadataReports.isEmpty() )
    {
        if ( !buildMetadataReports() )
        {
            return false;
        }
    }
    if ( BeansFactory::metadataJobs.isEmpty() )
    {
        if ( !buildScheduledJobs() )
        {
            return false;
        }
    }
    BeansFactory::initJobs();
    return true;
}

bool BeansFactory::end()
{
    return BeansFactory::unloadResources();
}

/**
 * @brief BeansFactory::clearSystemObjects
 * Limpia todos los objetos de sistema. Interesante, si se quieren recargar.
 * @return
 */
void BeansFactory::clearSystemObjects()
{
    // Necesitamos una copia de beans maestros de la aplicación
    qDeleteAll(BeansFactory::metadataBeans);
    BeansFactory::metadataBeans.clear();

    qDeleteAll(BeansFactory::metadataReports);
    BeansFactory::metadataReports.clear();

    qDeleteAll(BeansFactory::metadataJobs);
    BeansFactory::metadataJobs.clear();

    foreach (AERPScheduledJob *job, BeansFactory::jobs)
    {
        job->stop();
    }
    qDeleteAll(BeansFactory::jobs);
    BeansFactory::jobs.clear();

    BeansFactory::systemWidgets.clear();
    BeansFactory::systemScripts.clear();
    BeansFactory::systemReports.clear();
}


/*!
  Inicia toda la estructura de beans y datos comunes del sistema.
  Si falla un objeto la aplicación no puede iniciarse, y se devuelve el nombre del mismo en failedBean.
  */
bool BeansFactory::initSystemsBeans(QString &failedBean)
{
    // Aquí actualizamos la base de datos local, con todos los cambios que pudiese haber de la base de datos remota.
    bool result = SystemDAO::checkModules();
    if ( !result )
    {
        failedBean = trUtf8("Ha ocurrido un error obteniendo los módulos disponibles");
        return false;
    }
    result = SystemDAO::checkSystemObjectsOnLocal(failedBean);
    if ( result )
    {
        BeansFactory::systemInited = BeansFactory::init();
        return BeansFactory::systemInited;
    }
    return false;
}

/**
 * @brief BeansFactory::cleanTempPath Elimina todos los objetos del directorio temporal
 * @return
 */
void BeansFactory::cleanTempPath()
{
    QString serverDatabase = Database::serversDatabaseName().append(".db.sqlite");
    CommonsFunctions::removeContentsFromDir(QDir::fromNativeSeparators(alephERPSettings->dataPath()), QStringList(serverDatabase));
}

bool BeansFactory::metadataSystemInited()
{
    return BeansFactory::systemInited;
}

/*!
  Obtiene el master bean de la tabla name
  */
BaseBeanMetadata *BeansFactory::metadataBean(const QString &name)
{
    foreach (QPointer<BaseBeanMetadata> metadata, BeansFactory::metadataBeans)
    {
        if ( metadata && metadata->tableName() == name )
        {
            return metadata;
        }
    }
    QLogger::QLog_Debug(AlephERP::stLogOther, QString("BaseBeanMetadata::metadataBean: No existe la tabla: [%1]").arg(name));
    return NULL;
}

QList<BaseBeanMetadata *> BeansFactory::metadataBeansList(const QStringList &names)
{
    QList<BaseBeanMetadata *> list;
    foreach (QString name, names)
    {
        BaseBeanMetadata *m = BeansFactory::metadataBean(name);
        if ( m != NULL )
        {
            list.append(m);
        }
    }
    return list;
}

ReportMetadata *BeansFactory::metadataReport(const QString &name)
{
    foreach (ReportMetadata *metadata, BeansFactory::metadataReports)
    {
        if ( metadata->name() == name )
        {
            return metadata;
        }
    }
    QLogger::QLog_Debug(AlephERP::stLogOther, QString("BeansFactory::metadataReport: No existe el informe: [%1]").arg(name));
    return NULL;
}

/**
 * @brief BeansFactory::metadataReportsByLinkedTo
 * Devuelve TODOS los informes ligados a la tabla, SIN tener en cuenta los permisos.
 * @param linked
 * @return
 */
QList<ReportMetadata *> BeansFactory::metadataReportsByLinkedTo(const QString &linked)
{
    QList<ReportMetadata *> list;
    foreach (ReportMetadata *metadata, BeansFactory::metadataReports)
    {
        if ( metadata->linkedTo().contains(linked) )
        {
            list.append(metadata);
        }
    }
    return list;
}


/**
 * @brief SystemDAO::checkConsistencyMetadataTable Comprueba que los metadatos coinciden con las columnas de la tabla.
 * @param failTable
 * @param error
 * @return
 */
bool BeansFactory::checkConsistencyMetadataDatabase(QVariantList &log)
{
    // TODO: Por el momento sólo funciona para PostgreSQL
    QScopedPointer<QSqlQuery> qry (new QSqlQuery(Database::getQDatabase()));
    bool r = false;
    foreach (QPointer<BaseBeanMetadata> m, BeansFactory::metadataBeans)
    {
        if ( m )
        {
            if (m->dbObjectType() == AlephERP::NotValid || !SystemDAO::checkIfTableExists(m->tableName()))
            {
                // La tabla no existe. La creamos si es necesario
                QHash<QString, QVariant> tableErrors;
                AlephERP::ConsistencyTableErrors flagErrors;
                tableErrors["tablename"] = m->tableName();
                tableErrors["column"] = "";
                tableErrors["error"] = trUtf8("La tabla %1 no existe en base de datos.").arg(m->tableName());
                flagErrors = AlephERP::TableNotExists;
                tableErrors["code"] = QString("%1").arg(flagErrors);
                log.append(tableErrors);
            }
            if ( m->dbObjectType() == AlephERP::Table )
            {
                if ( m->tableName().size() > MAX_LENGTH_OBJECT_NAME_FIREBIRD )
                {
                    QHash<QString, QVariant> tableErrors;
                    AlephERP::ConsistencyTableErrors flagErrors;
                    tableErrors["tablename"] = m->tableName();
                    tableErrors["column"] = "";
                    tableErrors["error"] = trUtf8("El nombre de la tabla excede de 30 caracteres, lo que en bases de datos como Firebird darán errores.");
                    flagErrors = AlephERP::TableNameLengthTooLong;
                    tableErrors["code"] = QString("%1").arg(flagErrors);
                    log.append(tableErrors);
                }
                QHash<QString, QString> columnTypes;
                QHash<QString, bool> nullable;
                QHash<QString, int> maxChars;
                if ( qry->prepare(SQL_CONSISTENCY_PSQL) )
                {
                    qry->bindValue(":tablename", m->tableName());
                    r = qry->exec();
                    if ( r )
                    {
                        if ( qry->first() )
                        {
                            do
                            {
                                columnTypes[qry->value(0).toString()] = qry->value(1).toString();
                                nullable[qry->value(0).toString()] = qry->value(2).toString() == "YES" ? true : false;
                                maxChars[qry->value(0).toString()] = qry->value(3).toInt();
                            }
                            while ( qry->next() );
                            foreach ( DBFieldMetadata *fld, m->fields() )
                            {
                                QHash<QString, QVariant> errors;
                                AlephERP::ConsistencyTableErrors flagErrors;
                                errors["tablename"] = m->tableName();
                                errors["column"] = fld->dbFieldName();
                                // ¿Alguno duplicado?
                                foreach ( DBFieldMetadata *tmpFld, m->fields() )
                                {
                                    if ( tmpFld->dbFieldName() == fld->dbFieldName() && tmpFld != fld )
                                    {
                                        errors["error"] = trUtf8("Columna duplicada en los metadatos.");
                                        flagErrors = AlephERP::DbFieldNameDuplicate;
                                    }
                                }
                                if ( !fld->calculated() || fld->calculatedSaveOnDb() )
                                {
                                    if ( !columnTypes.contains(fld->dbFieldName()) )
                                    {
                                        errors["error"] = trUtf8("Columna presente en metadatos, pero no definida en la tabla.");
                                        flagErrors = AlephERP::ColumnOnMetadataNotOnTable;
                                    }
                                    else
                                    {
                                        QString err;
                                        if ( fld->checkDatabaseType(columnTypes.value(fld->dbFieldName()), Database::driverConnection(), err) )
                                        {
                                            if ( fld->type() == QVariant::String && fld->length() > maxChars[fld->dbFieldName()] )
                                            {
                                                if ( !err.isEmpty() )
                                                {
                                                    err = err + " - ";
                                                }
                                                err = trUtf8("Columna definida en metadatos con longitud (%1) excede a la disponible en base de datos (%2).").arg(fld->length()).arg(maxChars[fld->dbFieldName()]);
                                                flagErrors = flagErrors | AlephERP::ColumnOnMetadataWithLengthOverDatabaseLength;
                                            }
                                        }
                                        if ( fld->canBeNull() && !nullable.value(fld->dbFieldName()) )
                                        {
                                            if ( !err.isEmpty() )
                                            {
                                                err = err + " - ";
                                            }
                                            err = trUtf8("Columna definida en metadatos como nullable, pero en base de datos no puede ser nula.");
                                            flagErrors = flagErrors | AlephERP::ColumnOnMetadataIsNullableButNotOnDatabase;
                                        }
                                        if ( !fld->canBeNull() && nullable.value(fld->dbFieldName()) )
                                        {
                                            if ( !err.isEmpty() )
                                            {
                                                err = err + " - ";
                                            }
                                            err = trUtf8("Columna definida en metadatos como no nullable, pero en base de datos puede ser nula.");
                                            flagErrors = flagErrors | AlephERP::ColumnOnMetadataNotNullButCanBeOnDatabase;
                                        }
                                        if ( !err.isEmpty() )
                                        {
                                            errors["error"] = err;
                                        }
                                    }
                                }
                                if ( errors.contains("error") )
                                {
                                    errors["code"] = QString("%1").arg(flagErrors);
                                    log.append(errors);
                                }
                            }
                            qry->first();
                            do
                            {
                                DBFieldMetadata *fld = m->field(qry->value(0).toString());
                                if ( fld == NULL )
                                {
                                    bool nullable = qry->value(2).toString() == "YES" ? true : false;
                                    QHash<QString, QVariant> errors;
                                    AlephERP::ConsistencyTableErrors flagErrors;
                                    errors["tablename"] = m->tableName();
                                    errors["column"] = qry->value(0).toString();
                                    if ( nullable )
                                    {
                                        flagErrors = AlephERP::ColumnNotOnMetadataButOnDatabase;
                                        errors["error"] = trUtf8("Columna presente en base de datos y no presente en metadatos");
                                    }
                                    else
                                    {
                                        flagErrors = AlephERP::ColumnNotOnMetadataButOnDatabaseNotNull;
                                        errors["error"] = trUtf8("Columna presente en base de datos NO NULLABLE y no presente en metadatos");
                                    }
                                    errors["code"] = QString("%1").arg(flagErrors);
                                    log.append(errors);
                                }
                            }
                            while (qry->next());
                        }
                    }
                    else
                    {
                        QHash<QString, QVariant> tableErrors;
                        AlephERP::ConsistencyTableErrors flagErrors;
                        tableErrors["tablename"] = m->tableName();
                        tableErrors["column"] = "";
                        tableErrors["error"] = trUtf8("La tabla %1 contiene ccolumnas o una definición diferente a la de los metadatos.").arg(m->tableName());
                        flagErrors = AlephERP::TableNotMatchMetadata;
                        tableErrors["code"] = QString("%1").arg(flagErrors);
                        log.append(tableErrors);
                    }
                }
            }
        }
    }
    return log.isEmpty();
}

/**
 * @brief BeansFactory::checkConsistencyMetadata
 * Realiza comprobaciones sobre la integridad en sí de los propios metadatos
 * @param log
 * @return
 */
bool BeansFactory::checkConsistencyMetadata(QVariantList &log)
{
    foreach (QPointer<BaseBeanMetadata> m, BeansFactory::metadataBeans)
    {
        if ( m )
        {
            // Comprobemos si las relaciones están bien construidas
            foreach ( DBRelationMetadata *rel, m->relations() )
            {
                BaseBeanMetadata *relatedBean = BeansFactory::metadataBean(rel->tableName());
                if ( relatedBean != NULL )
                {
                    if ( !relatedBean->field(rel->childFieldName()) )
                    {
                        QHash<QString, QVariant> tableErrors;
                        AlephERP::ConsistencyTableErrors flagErrors;
                        tableErrors["tablename"] = m->tableName();
                        tableErrors["column"] = "Relations";
                        tableErrors["error"] = trUtf8("La columna %1 en la tabla relacionada %2 no existe.").
                                               arg(rel->childFieldName()).
                                               arg(rel->tableName());
                        flagErrors = AlephERP::RelatedColumnNotExists;
                        tableErrors["code"] = QString("%1").arg(flagErrors);
                        log.append(tableErrors);
                    }
                }
                else
                {
                    QHash<QString, QVariant> tableErrors;
                    AlephERP::ConsistencyTableErrors flagErrors;
                    tableErrors["tablename"] = m->tableName();
                    tableErrors["column"] = "Relations";
                    tableErrors["error"] = trUtf8("La tabla relacionada %1 no existe.").arg(rel->tableName());
                    flagErrors = AlephERP::RelatedTableNotExists;
                    tableErrors["code"] = QString("%1").arg(flagErrors);
                    log.append(tableErrors);
                }
            }
        }
    }
    return true;
}

/**
 * @brief BeansFactory::dbNotifications
 * Devuelve el listado de notificaciones de base de datos a las que hay que suscribirse
 * @return
 */
QStringList BeansFactory::dbNotifications()
{
    QStringList notifications;
    notifications << AlephERP::stDeleteLockNotification;
    notifications << AlephERP::stNewLockNotification;
    notifications << AlephERP::stDeleteRowNotification;
    foreach (AERPScheduledJobMetadata *m, BeansFactory::metadataJobs)
    {
        if ( !m->databaseNotification().isEmpty() )
        {
            notifications.append(m->databaseNotification());
        }
    }
    return notifications;
}

QList<QPointer<BaseBeanMetadata> > BeansFactory::allowedMetadatasToUser()
{
    QList<QPointer<BaseBeanMetadata> > list;
    foreach (QPointer<BaseBeanMetadata> m, BeansFactory::instance()->metadataBeans)
    {
        if ( m )
        {
            if ( AERPLoggedUser::instance()->checkMetadataAccess('r', m->tableName()) )
            {
                list << m;
            }
        }
    }
    return list;
}

QList<AERPSystemModule *> BeansFactory::modules()
{
    return BeansFactory::systemModules;
}

/**
 * @brief BeansFactory::localWorkOrderedTables Las tablas dependen unas y otras entre sí. Es por ello que debemos ordenar las mismas
 * a la hora de obtener sus datos. Esta función lo hace
 * @return
 */
QStringList BeansFactory::orderMetadataTableNamesForInsertOrUpdate()
{
    QStringList tablesToOrder;

    foreach (QPointer<BaseBeanMetadata> metadata, BeansFactory::metadataBeans)
    {
        if ( metadata && !tablesToOrder.contains(metadata->tableName()) )
        {
            tablesToOrder.append(metadata->tableName());
        }
    }
    return BeansFactory::orderMetadataTableNamesForInsertOrUpdate(tablesToOrder);
}

QStringList BeansFactory::orderMetadataTableNamesForInsertOrUpdate(QStringList tablesToOrder)
{
    QStringList orderedTables;

    QList<BaseBeanMetadata *> metadataBeansList = BeansFactory::metadataBeansList(tablesToOrder);

    foreach ( BaseBeanMetadata *metadata, metadataBeansList )
    {
        if ( !orderedTables.contains(metadata->tableName()) )
        {
            QLogger::QLog_Debug(AlephERP::stLogOther, QString("BeansFactory::orderMetadataTableNamesForInsertOrUpdate: Se añade a la lista: %1").arg(metadata->tableName()));
            // Si esta tabla tiene un orden definido de transacción se sitúa en el orden adecuado
            orderedTables.append(metadata->tableName());
        }
        // Para las relaciones M1 que no se hayan agregado, las introducimos antes de esta... Pero además,
        // !debemos introducir las de los padres de los padres!
        // Aquí entra la parte recursiva
        QLogger::QLog_Debug(AlephERP::stLogOther, QString("BeansFactory::orderMetadataTableNamesForInsertOrUpdate: Procesamos %1").arg(metadata->tableName()));
        BeansFactory::processOrderMetadataTableNamesForInsertOrUpdate(metadata, orderedTables, tablesToOrder);
        foreach (DBRelationMetadata *rel, metadata->relations(AlephERP::OneToMany | AlephERP::OneToOne))
        {
            if ( !orderedTables.contains(rel->tableName()) && tablesToOrder.contains(rel->tableName()) )
            {
                QLogger::QLog_Debug(AlephERP::stLogOther, QString("BeansFactory::orderMetadataTableNamesForInsertOrUpdate: Se añade a la lista hijos: %1").arg(rel->tableName()));
                orderedTables.append(rel->tableName());
            }
        }
    }
    // Ahora vamos a comprobar que éstas tablas cumplen con el orden establecido de orden de transacción en los metadatos
    QMap<int, QString> sortedTablesByMetadataInfo;
    foreach (BaseBeanMetadata *metadata, metadataBeansList)
    {
        if ( metadata->orderOnTransaction() != -1 )
        {
            sortedTablesByMetadataInfo.insert(metadata->orderOnTransaction(), metadata->tableName());
        }
    }
    // Las que tienen índice de ordenación, van primeras... es responsabilidad del desarrollador utilizar esto adecuadamente
    QMapIterator<int, QString> it(sortedTablesByMetadataInfo);
    while (it.hasNext())
    {
        it.next();
        int position = orderedTables.indexOf(it.value());
        if ( position != -1 )
        {
            orderedTables.removeAt(position);
        }
    }
    QMapIterator<int, QString> itInsert(sortedTablesByMetadataInfo);
    QStringList temp;
    while (itInsert.hasNext())
    {
        itInsert.next();
        temp.append(itInsert.value());
    }
    for (int i = temp.size() - 1 ; i >= 0; i--)
    {
        orderedTables.prepend(temp.at(i));
    }
    return orderedTables;
}

/**
 * @brief BatchDAO::processParentsOnLocalWorkOrderedTables
 * Las tablas pueden tener dependencias entre sí. Esta función (recursiva) intenta ordenarla según relaciones 1->M
 * @param father
 * @param orderedTables
 */
void BeansFactory::processOrderMetadataTableNamesForInsertOrUpdate(BaseBeanMetadata *father, QStringList &orderedTables, QStringList &tablesToOrder)
{
    foreach(DBRelationMetadata *rel, father->relations(AlephERP::ManyToOne))
    {
        BaseBeanMetadata *previousFather = BeansFactory::metadataBean(rel->tableName());
        if ( previousFather != NULL && tablesToOrder.contains(previousFather->tableName()) )
        {
            if ( !orderedTables.contains(previousFather->tableName()) )
            {
                orderedTables.insert(0, previousFather->tableName());
                QLogger::QLog_Debug(AlephERP::stLogOther, QString("BeansFactory::processOrderMetadataTableNamesForInsertOrUpdate: Hay que insertar ya que orderedTables no contenía [%1]").arg(previousFather->tableName()));
                BeansFactory::processOrderMetadataTableNamesForInsertOrUpdate(previousFather, orderedTables, tablesToOrder);
            }
            else
            {
                int positionFather = orderedTables.indexOf(father->tableName());
                int positionPreviuosFather = orderedTables.indexOf(previousFather->tableName());
                if ( positionFather < positionPreviuosFather )
                {
                    QLogger::QLog_Debug(AlephERP::stLogOther, QString("BeansFactory::processOrderMetadataTableNamesForInsertOrUpdate: Hay que intercambiar elementos, ya que orderedTables contenía %1").arg(previousFather->tableName()));
                    // orderedTables.swap(positionFather, positionPreviuosFather);
                    orderedTables.move(positionFather, positionPreviuosFather);
                }
            }
        }
    }
}

/*!
  Permite actualizar un objeto de sistema (archivo .ui o archivo .qs). Las tablas
  requieren de reinicialización del programa, salvo que se cambie un script, que
  será también actualizado por este objeto.
  */
bool BeansFactory::updateSystemObject(const QString &type, const QString &objectName, const QString &content, int version, bool debugOnInit, bool debug)
{
    bool r = false;
    AERPSystemObject *object = SystemDAO::systemObject(objectName, type);
    if ( object != NULL )
    {
        object->setContent(content);
        object->setDebug(debug);
        object->setOnInitDebug(debugOnInit);
        object->setVersion(version);
    }
    if ( type == "qs" )
    {
        BeansFactory::systemScripts[objectName] = content;
        BeansFactory::systemScriptsDebug[objectName] = debug;
        BeansFactory::systemScriptsDebugOnInit[objectName] = debugOnInit;
        r = true;
    }
    else if ( type == "ui" )
    {
        CommonsFunctions::setOverrideCursor(Qt::WaitCursor);
        BeansFactory::refreshSystemObject(objectName, "ui");
        CommonsFunctions::restoreOverrideCursor();
    }
    return r;
}

/**
 * @brief BeansFactory::updateModuleMetadata Actualiza la información del archivo Module Metadata,
 * según la información del objeto local, el que está en memoria.
 * @param module
 * @param path
 */
#ifdef ALEPHERP_DEVTOOLS
void BeansFactory::updateModuleMetadata(const QString &moduleId, const QString &path)
{
    ModulesDAO::instance()->updateModuleMetadata(moduleId, path);
}
#endif
