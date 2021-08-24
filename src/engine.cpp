/* Swift Assistant is a simple, user-friendly assistant based on an extension system.

   Copyright (C) <2021>  <SwiftApp>

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <https://www.gnu.org/licenses/>. */

#include "engine.h"

#include <QFile>
#include <QApplication>
#include <QIODevice>
#include <QTextStream>
#include <QRandomGenerator64>
#include <QDebug>
#include <QDesktopServices>
#include <QUrl>
#include <QPluginLoader>
#include <QDir>
#include <QCoreApplication>
#include <QMessageBox>
#include <QDateTime>

Engine::Engine(QObject *parent) : QObject(parent)
{
    scanPlugin();
}

void Engine::scanPlugin()
{
    prop.clear();
    main_prop.clear();
    showedProp.clear();
    mainVolatil_prop.clear();
    listPlugins.clear();

    QDir pluginsDir(QDir::homePath());
    if (!pluginsDir.exists("SwiftPlugins")) pluginsDir.mkdir("SwiftPlugins");
    pluginsDir.cd("SwiftPlugins");

    const QStringList entries = pluginsDir.entryList(QDir::Files);

    foreach (QString fileName , entries) {
        QString ext = fileName.right(fileName.length()-1-fileName.lastIndexOf("."));

        if (ext == "sw") {
            QPluginLoader pluginLoader(pluginsDir.absoluteFilePath(fileName));
            QObject *plugin = pluginLoader.instance();

            if (plugin) {
                PluginInterface *pluginsInterface = qobject_cast<PluginInterface *>(plugin);
                if (pluginsInterface) {
                    connect(pluginsInterface->getObject(), SIGNAL(sendMessage(QString,bool,QString,QList<QString>,QList<QString>)), this, SLOT(sendReply(QString,bool,QString,QList<QString>,QList<QString>)));
                    connect(pluginsInterface->getObject(), SIGNAL(showQml(QString,QString)), this, SLOT(showQml(QString,QString)));
                    connect(pluginsInterface->getObject(), SIGNAL(sendMessageToQml(QString,QString)), this, SLOT(receiveMessageSendedToQml(QString,QString)));
                    connect(this, SIGNAL(signalSendMessageToPlugin(QString,QString)), pluginsInterface->getObject(), SLOT(messageReceived(QString,QString)));
                    QList<QString> plug_prop = pluginsInterface->getCommande();

                    std::uniform_real_distribution<double> dist(0, plug_prop.length());
                    int val = dist(*QRandomGenerator::global());

                    main_prop.append(plug_prop.at(val));
                    prop.append(plug_prop);
                    listPlugins.append(pluginsInterface);
                }
            }
        }
    }
}

void Engine::removePlugin(QString iid)
{
    QDir pluginsDir(QDir::homePath());
    pluginsDir.cd("SwiftPlugins");

    const QStringList entries = pluginsDir.entryList(QDir::Files);

    for (const QString &fileName : entries) {
        QPluginLoader pluginLoader(pluginsDir.absoluteFilePath(fileName));
        QObject *plugin = pluginLoader.instance();

        if (plugin) {
            PluginInterface *pluginsInterface = qobject_cast<PluginInterface *>(plugin);
            if (pluginsInterface) {
                if (pluginsInterface->pluginIid() == iid) {
                    QFile::remove(pluginsDir.absoluteFilePath(fileName));
                }
            }
        }
    }

    scanPlugin();
}

void Engine::getAllPlugin()
{
    foreach (PluginInterface *plugin, listPlugins) {
        emit pluginTrouved(plugin->pluginIid());
    }
}

void Engine::showQml(QString qml, QString iid)
{
    QDir dir(QDir::homePath());
    if (!dir.exists(".swift_cache")) dir.mkdir(".swift_cache");
    dir.cd(".swift_cache");

    QString path = dir.path()+"/"+iid.replace(".", "_")+".qml";
    QString qmlUrl = "file:"+dir.path()+"/"+iid.replace(".", "_")+".qml";

    QFile file(path);
    if(!file.open(QIODevice::ReadWrite | QIODevice::Text)) {
        qDebug("Error write qml file");
        return;
    }

    QTextStream flux(&file);
    flux.setCodec("UTF-8");

    flux << qml;

    emit showQmlFile(qmlUrl);
}

void Engine::textChanged(QString text)
{
    for (int i = showedProp.length()-1; i >= 0; i--) {
        QString compareText = showedProp.at(i);
        compareText.truncate(text.length());

        if (text.compare(compareText, Qt::CaseInsensitive) != 0) {
            emit removeProp(i);
            showedProp.removeAt(i);
        }
    }

    foreach (QString myText, prop) {
        QString compareText = myText;
        compareText.truncate(text.length());

        if (text.compare(compareText, Qt::CaseInsensitive) == 0 && showedProp.indexOf(myText) == -1) {
            emit addProp(myText);
            showedProp.append(myText);
        }
    }
}

void Engine::sendMessageToPlugin(QString message, QString pluginIid)
{
    emit signalSendMessageToPlugin(message, pluginIid);
}

void Engine::addBaseProp()
{
    emit removeAllProp();
    showedProp.clear();

    foreach (QString text , main_prop) {
        emit addProp(text);
        showedProp.append(text);
    }
}

void Engine::messageReceived(QString message)
{
    format(message);
}

void Engine::receiveMessageSendedToQml(QString message, QString pluginIid)
{
    emit pluginToQml(message, pluginIid);
}

void Engine::sendReply(QString reply, bool isFin, QString typeMessage, QList<QString> url, QList<QString> text)
{
    emit reponseSended(reply, isFin, typeMessage, url, text);
}

void Engine::updateSettingsVar()
{
    QVariant var = settings.value(key_settings_name, "Inconnue");
    userName = var.toString();
    var = settings.value(key_settings_sound, true);
    soundEnabled = var.toBool();
    var = settings.value(key_settings_proposition, true);
    propEnabled = var.toBool();
}

void Engine::executeAction(QString action)
{
    QList<QString> cmd;
    QString word;
    for (int i = 0; i < action.length(); i++) {
        if (action.at(i) == ' ') {
            if (!word.isEmpty()) {
                cmd.append(word);
                word.clear();
            }
        }
        else {
            word.append(action.at(i));
        }

        if (i == action.length()-1) {
            cmd.append(word);
            word.clear();
        }
    }

    execAction(cmd);
}

void Engine::execAction(QList<QString> cmd)
{
    if (cmd[0] == "settings") {
        if (cmd[1] == "name") {
            if (cmd[2] != "") {
                QVariant variant = readVarInText(cmd[2], var);
                settings.setValue(key_settings_name, variant.toString());
            }
        }

        else if (cmd[1] == "prop") {
            if (cmd[2] != "") {
                QVariant variant = readVarInText(cmd[2], var);
                settings.setValue(key_settings_proposition, variant.toBool() ? true : false);
            }
        }

        else if (cmd[1] == "show") {
            emit reponseSended("", true, "settings", QList<QString>(), QList<QString>());
        }
    }

    else if (cmd[0] == "application") {
        if (cmd[1] != "quit") {
            QApplication::quit();
        }

        else if (cmd[1] == "hideWindow") {
            emit hideWindow();
        }
    }

    else if (cmd[0] == "web_message") {
        if (cmd[1] == "without_action_btn") {
            if (cmd[2] == "search") {
                if (cmd[3] != "") {
                    QString search = "";
                    for (int i = 3; i < cmd.length(); i++) {
                        search.append(cmd.at(i));
                        if (i != cmd.length()-1) search.append(" ");
                    }
                    readVarInText(search, var).replace(" ", "+");

                    QString url = "https://www.google.com/search?channel=fs&client=linux&q="+search;

                    emit reponseSended("", true, "web_without_action_btn", QList<QString>() << url, QList<QString>());
                }
            }

            else if (cmd[2] == "site") {
                if (cmd[3] != "") {
                    bool isUserEntry = true;

                    for (int i = 0; i < cmd[2].length(); i++) {
                        int nextIndex = 1 == cmd[2].length() ? i : i+1;
                        int nextIndexB = 2 == cmd[2].length() ? i : i+2;
                        int nextIndexC = 3 == cmd[2].length() ? i : i+3;

                        if (cmd[2].at(0) == "h" && cmd[2].at(nextIndex) == "t" && cmd[2].at(nextIndexB) == "t" && cmd[2].at(nextIndexC) == "p")
                            isUserEntry = false;
                    }

                    if (isUserEntry) emit reponseSended("", true, "web_without_action_btn", QList<QString>() << QUrl::fromUserInput(readVarInText(cmd[2], var)).toString(), QList<QString>());
                    else emit reponseSended("", true, "web_without_action_btn", QList<QString>() << QUrl(readVarInText(cmd[2], var)).toString(), QList<QString>());
                }
            }
        }

        else if (cmd[1] == "with_action_btn") {
            if (cmd[2] == "search") {
                if (cmd[3] != "") {
                    QString search = "";
                    for (int i = 3; i < cmd.length(); i++) {
                        search.append(cmd.at(i));
                        if (i != cmd.length()-1) search.append(" ");
                    }
                    readVarInText(search, var).replace(" ", "+");

                    QString url = "https://www.google.com/search?channel=fs&client=linux&q="+search;

                    emit reponseSended("", true, "web_with_action_btn", QList<QString>() << url, QList<QString>());
                }
            }

            else if (cmd[2] == "site") {
                if (cmd[3] != "") {
                    bool isUserEntry = true;

                    for (int i = 0; i < cmd[2].length(); i++) {
                        int nextIndex = 1 == cmd[2].length() ? i : i+1;
                        int nextIndexB = 2 == cmd[2].length() ? i : i+2;
                        int nextIndexC = 3 == cmd[2].length() ? i : i+3;

                        if (cmd[2].at(0) == "h" && cmd[2].at(nextIndex) == "t" && cmd[2].at(nextIndexB) == "t" && cmd[2].at(nextIndexC) == "p")
                            isUserEntry = false;
                    }

                    if (isUserEntry) emit reponseSended("", true, "web_with_action_btn", QList<QString>() << QUrl::fromUserInput(readVarInText(cmd[2], var)).toString(), QList<QString>());
                    else emit reponseSended("", true, "web_with_action_btn", QList<QString>() << QUrl(readVarInText(cmd[2], var)).toString(), QList<QString>());
                }
            }
        }
    }
}

void Engine::format(QString _text)
{
    QList<QString> array;
    QString word = "";

    QString text = _text.toLower();

    for (int i = 0; i < text.length(); i++) {
        if (text.at(i) == ' ' || text.at(i) == '-') {
            if (!word.isEmpty()) {
                array.append(word);
                word.clear();
            }
        }
        else if (text.at(i) == ',' || text.at(i) == '&') {
            array.append(word);
            word.clear();
            array.append(",");
        }
        else if (text.at(i) != '!' && text.at(i) != '?') {
            QString ch = text.at(i);

            if (ch == "é" || ch == "è" || ch == "ê" || ch == "ë") word.append("e");
            else if (ch == "î" || ch == "ï") word.append("i");
            else if (ch == "û" || ch == "ü" || ch == "ù") word.append("u");
            else if (ch == "ô" || ch == "ö" || ch == "ò") word.append("o");
            else if (ch == "â" || ch == "ä" || ch == "à") word.append("a");
            else if (ch == "ç") word.append("c");
            else word.append(text.at(i));
        }

        if (i == text.length()-1 && !word.isEmpty()) {
            array.append(word);
            word.clear();
        }
    }

    QList<QList<QString>> arrayFinal;
    QList<QString> arraySuite;
    int i = 0;

    foreach (QString txt , array) {
        if ((txt == "bonjour" || txt == "salut" || txt == "hello" || txt == "coucou") && i == 0) {
            arraySuite.append(txt);
            arrayFinal.append(arraySuite);
            arraySuite.clear();
        }
        else if ((txt == "&" || txt == ",") && !arraySuite.isEmpty()) {
            arrayFinal.append(arraySuite);
            arraySuite.clear();
        }
        else {
            arraySuite.append(txt);
        }

        if (i == array.length()-1 && !arraySuite.isEmpty()) {
            arrayFinal.append(arraySuite);
            arraySuite.clear();
        }

        i++;
    }

    analize(arrayFinal);
}

void Engine::analize(QList<QList<QString>> array_cmd)
{
    foreach (QList<QString> cmd , array_cmd) {
        if (nextReplyItemId != "") {
            bool returnBool = analizePlugin(array_cmd, cmd);

            if (!returnBool)
                analizeAllPlugins(array_cmd, cmd);
        }
        else {
            analizeAllPlugins(array_cmd, cmd);
        }
    }
}

void Engine::analizeAllPlugins(QList<QList<QString>> array_cmd, QList<QString> cmd)
{
    nextReplyNeedId.clear();
    nextReplyPluginName.clear();
    nextReplyItemId.clear();

    if (!mainVolatil_prop.isEmpty()) {
        main_prop = mainVolatil_prop;
        mainVolatil_prop.clear();
        while (removePropNuber != 0) {
            prop.removeLast();
            removePropNuber--;
        }
        addBaseProp();
    }

    bool isOk = false;
    bool isRep = false;

    foreach (PluginInterface *plug , listPlugins) {
        QString xml = plug->getDataXml();
        QDomDocument m_doc;
        m_doc.setContent(xml, false);

        QDomElement root = m_doc.documentElement();
        QDomElement item = root.firstChildElement();

        while (!item.isNull() && !isOk) {
            QDomElement props = item.firstChildElement();
            var.clear();
            QString cmdString;
            for (int i = 0; i < cmd.length(); i++) {
                if (i == cmd.length()-1) cmdString.append(cmd.at(i));
                else cmdString.append(cmd.at(i)+" ");
            }
            var.append(cmdString);

            while (!props.isNull()) {
                if (props.tagName() == "Prop" && nextReplyItemId != "") {
                    QDomElement mProp = props.firstChildElement();
                    QList<QString> listSecondProp;

                    while (!mProp.isNull()) {
                        listSecondProp.append(mProp.text());
                        mProp = mProp.nextSiblingElement();
                    }

                    mainVolatil_prop = main_prop;
                    main_prop = listSecondProp;
                    prop.append(listSecondProp);
                    removePropNuber = listSecondProp.length();
                    addBaseProp();
                }

                if (props.tagName() == "Keywords" && cmd.length() >= props.attribute("minWord").toInt() && cmd.length() <= props.attribute("maxWord").toInt()) {
                    QDomElement words = props.firstChildElement();
                    isOk = true;

                    while (!words.isNull() && isOk) {
                        if (words.tagName() == "Words") {
                            QDomElement word = words.firstChildElement();
                            bool isContains = false;

                            while (!word.isNull() && !isContains) {
                                isContains = cmd.contains(word.text());
                                word = word.nextSiblingElement();
                            }

                            if (!isContains) isOk = false;
                        }

                        if (words.tagName() == "NoWords") {
                            QDomElement word = words.firstChildElement();
                            bool isContains = false;

                            while (!word.isNull() && !isContains) {
                                isContains = cmd.contains(word.text());
                                word = word.nextSiblingElement();
                            }

                            if (isContains) isOk = false;
                        }

                        words = words.nextSiblingElement();
                    }
                }

                if (props.tagName() == "Var" && isOk) {
                    int index = -1;
                    QList<QString> keywords;
                    QString text;
                    QDomElement word = props.firstChildElement();

                    while (!word.isNull()) {
                        keywords.append(word.text());
                        word = word.nextSiblingElement();
                    }

                    foreach (QString _word , keywords) {
                        int i = cmd.lastIndexOf(_word);
                        if (i >= index) index = i;
                    }

                    if (index != -1) {
                        for (int i = index+1; i < cmd.length(); i++) {
                            text.isEmpty() ? text.append(cmd[i]) : text.append(" "+cmd[i]);
                            if (i == index+props.attribute("max").toInt()) break;
                        }
                    }

                    if (text != "") var.append(text);
                }

                if (props.tagName() == "Reply" && isOk) {
                    QDomElement condi_or_rep = props.firstChildElement();
                    bool result = false;

                    while (!condi_or_rep.isNull()) {
                        if (condi_or_rep.tagName() == "rep") {
                            QList<QString> repList;

                            while (!condi_or_rep.isNull()) {
                                repList.append(condi_or_rep.text());
                                condi_or_rep = condi_or_rep.nextSiblingElement();
                            }

                            std::uniform_real_distribution<double> dist(0, repList.length());
                            int val = dist(*QRandomGenerator::global());

                            sendReply(readVarInText(repList[val], var), array_cmd[array_cmd.length()-1] == cmd ? true : false, "message", QList<QString>(), QList<QString>());
                            isRep = true;
                            if (item.attribute("id", "") != "" && item.attribute("needId", "") != "") {
                                nextReplyPluginName = plug->pluginIid();
                                nextReplyNeedId = item.attribute("needId");
                                nextReplyItemId = item.attribute("id");
                            }
                        }
                        else if (condi_or_rep.tagName() == "condition" && condi_or_rep.attribute("if") != "") {
                            QString conditionA;
                            QString conditionB;
                            QString m_operator;
                            bool isConditionA = true;

                            for (int i = 0; i < condi_or_rep.attribute("if").length(); i++) {
                                if (condi_or_rep.attribute("if").at(i) == "!") { m_operator = "!"; isConditionA = false; }
                                else if (condi_or_rep.attribute("if").at(i) == "=") { m_operator = "="; isConditionA = false; }
                                else {
                                    if (isConditionA) conditionA.append(condi_or_rep.attribute("if").at(i));
                                    else conditionB.append(condi_or_rep.attribute("if").at(i));
                                }
                            }

                            conditionA = readVarInText(conditionA, var);
                            conditionB = readVarInText(conditionB, var);

                            result = false;

                            if (m_operator == "!") {
                                if (conditionA != conditionB) result = true;
                                else result = false;
                            }
                            if (m_operator == "=") {
                                if (conditionA == conditionB) result = true;
                                else result = false;
                            }

                            if (result) {
                                QDomElement m_rep = condi_or_rep.firstChildElement();
                                QList<QString> repList;

                                while (!m_rep.isNull()) {
                                    repList.append(m_rep.text());
                                    m_rep = m_rep.nextSiblingElement();
                                }

                                std::uniform_real_distribution<double> dist(0, repList.length());
                                int val = dist(*QRandomGenerator::global());

                                sendReply(readVarInText(repList[val], var), array_cmd[array_cmd.length()-1] == cmd ? true : false, "message", QList<QString>(), QList<QString>());
                                isRep = true;
                                if (item.attribute("id", "") != "" && item.attribute("needId", "") != "") {
                                    nextReplyPluginName = plug->pluginIid();
                                    nextReplyNeedId = item.attribute("needId");
                                    nextReplyItemId = item.attribute("id");
                                }
                            }
                        }
                        else if (condi_or_rep.tagName() == "else" && !result) {
                            QDomElement m_rep = condi_or_rep.firstChildElement();
                            QList<QString> repList;

                            while (!m_rep.isNull()) {
                                repList.append(m_rep.text());
                                m_rep = m_rep.nextSiblingElement();
                            }

                            std::uniform_real_distribution<double> dist(0, repList.length());
                            int val = dist(*QRandomGenerator::global());

                            sendReply(readVarInText(repList[val], var), array_cmd[array_cmd.length()-1] == cmd ? true : false, "message", QList<QString>(), QList<QString>());
                            isRep = true;
                            if (item.attribute("id", "") != "" && item.attribute("needId", "") != "") {
                                nextReplyPluginName = plug->pluginIid();
                                nextReplyNeedId = item.attribute("needId");
                                nextReplyItemId = item.attribute("id");
                            }
                        }

                        condi_or_rep = condi_or_rep.nextSiblingElement();
                    }
                }

                if (props.tagName() == "Actions" && isOk) {
                    QDomElement action = props.firstChildElement();
                    bool result = false;

                    while (!action.isNull()) {
                        if (action.tagName() == "action") {
                            QList<QString> cmd;
                            QString word = "";

                            for (int i = 0; i < action.text().length(); i++) {
                                if (action.text().at(i) == ' ') {
                                    if (!word.isEmpty()) {
                                        cmd.append(word);
                                        word.clear();
                                    }
                                }
                                else {
                                    word.append(action.text().at(i));
                                }

                                if (i == action.text().length()-1) {
                                    cmd.append(word);
                                    word.clear();
                                }
                            }

                            if (cmd[0] == "settings" || cmd[0] == "application" || cmd[0] == "web_message") {
                                execAction(cmd);
                            }
                            else {
                                for (int i = 0; i < cmd.length(); i++) {
                                    cmd[i] = readVarInText(cmd.at(i), var);
                                }

                                plug->execAction(cmd);
                            }
                        }
                        else if (action.tagName() == "condition" && action.attribute("if") != "") {
                            QString conditionA;
                            QString conditionB;
                            QString m_operator;
                            bool isConditionA = true;

                            for (int i = 0; i < action.attribute("if").length(); i++) {
                                if (action.attribute("if").at(i) == "!") { m_operator = "!"; isConditionA = false; }
                                else if (action.attribute("if").at(i) == "=") { m_operator = "="; isConditionA = false; }
                                else {
                                    if (isConditionA) conditionA.append(action.attribute("if").at(i));
                                    else conditionB.append(action.attribute("if").at(i));
                                }
                            }

                            conditionA = readVarInText(conditionA, var);
                            conditionB = readVarInText(conditionB, var);

                            result = false;

                            if (m_operator == "!") {
                                if (conditionA != conditionB) result = true;
                                else result = false;
                            }
                            if (m_operator == "=") {
                                if (conditionA == conditionB) result = true;
                                else result = false;
                            }

                            if (result) {
                                QDomElement m_action = action.firstChildElement();

                                while (!m_action.isNull()) {
                                    QList<QString> cmd;
                                    QString word = "";

                                    for (int i = 0; i < m_action.text().length(); i++) {
                                        if (m_action.text().at(i) == ' ') {
                                            if (!word.isEmpty()) {
                                                cmd.append(word);
                                                word.clear();
                                            }
                                        }
                                        else {
                                            word.append(m_action.text().at(i));
                                        }

                                        if (i == m_action.text().length()-1) {
                                            cmd.append(word);
                                            word.clear();
                                        }
                                    }

                                    if (cmd[0] == "settings" || cmd[0] == "web_message") {
                                        execAction(cmd);
                                    }
                                    else {
                                        for (int i = 0; i < cmd.length(); i++) {
                                            cmd[i] = readVarInText(cmd.at(i), var);
                                        }

                                        plug->execAction(cmd);
                                    }

                                    m_action = m_action.nextSiblingElement();
                                }
                            }
                        }
                        else if (action.tagName() == "else" && !result) {
                            QDomElement m_action = action.firstChildElement();

                            while (!m_action.isNull()) {
                                QList<QString> cmd;
                                QString word = "";

                                for (int i = 0; i < m_action.text().length(); i++) {
                                    if (m_action.text().at(i) == ' ') {
                                        if (!word.isEmpty()) {
                                            cmd.append(word);
                                            word.clear();
                                        }
                                    }
                                    else {
                                        word.append(m_action.text().at(i));
                                    }

                                    if (i == m_action.text().length()-1) {
                                        cmd.append(word);
                                        word.clear();
                                    }
                                }

                                if (cmd[0] == "settings" || cmd[0] == "web_message") {
                                    execAction(cmd);
                                }
                                else {
                                    for (int i = 0; i < cmd.length(); i++) {
                                        cmd[i] = readVarInText(cmd.at(i), var);
                                    }

                                    plug->execAction(cmd);
                                }

                                m_action = m_action.nextSiblingElement();
                            }
                        }

                        action = action.nextSiblingElement();
                    }
                }

                props = props.nextSiblingElement();
            }

            var.clear();
            item = item.nextSiblingElement();
        }
    }

    if (!isRep) {
        QString search = "";
        for (int i = 0; i < cmd.length(); i++) {
            search.append(cmd.at(i));
            if (i != cmd.length()-1) search.append(" ");
        }

        sendReply(tr("Désolé, je ne comprends pas ! :("), true, "message", QList<QString>() << "web_message with_action_btn search "+search, QList<QString>() << tr("Chercher sur le web"));
    }
}

bool Engine::analizePlugin(QList<QList<QString>> array_cmd, QList<QString> cmd)
{
    bool isOk = false;
    bool isRep = false;

    foreach (PluginInterface *plug , listPlugins) {
        if (plug->pluginIid() == nextReplyPluginName) {
            QString xml = plug->getDataXml();
            QDomDocument m_doc;
            m_doc.setContent(xml, false);

            QDomElement root = m_doc.documentElement();
            QDomElement item = root.firstChildElement();

            while (!item.isNull() && !isOk) {
                var.clear();

                if (item.attribute("id") == nextReplyItemId) {
                    QDomElement secondItem = item.firstChildElement();

                    while (!secondItem.isNull() && !isOk) {
                        if (secondItem.tagName() == "Item" && secondItem.attribute("id") == nextReplyNeedId) {
                            QDomElement props = secondItem.firstChildElement();
                            QString cmdString;
                            for (int i = 0; i < cmd.length(); i++) {
                                if (i == cmd.length()-1) cmdString.append(cmd.at(i));
                                else cmdString.append(cmd.at(i)+" ");
                            }
                            var.append(cmdString);

                            while (!props.isNull()) {
                                if (props.tagName() == "Keywords" && cmd.length() >= props.attribute("minWord").toInt() && cmd.length() <= props.attribute("maxWord").toInt()) {
                                    QDomElement words = props.firstChildElement();
                                    isOk = true;

                                    while (!words.isNull() && isOk) {
                                        if (words.tagName() == "Words") {
                                            QDomElement word = words.firstChildElement();
                                            bool isContains = false;

                                            while (!word.isNull() && !isContains) {
                                                isContains = cmd.contains(word.text());
                                                word = word.nextSiblingElement();
                                            }

                                            if (!isContains) isOk = false;
                                        }

                                        if (words.tagName() == "NoWords") {
                                            QDomElement word = words.firstChildElement();
                                            bool isContains = false;

                                            while (!word.isNull() && !isContains) {
                                                isContains = cmd.contains(word.text());
                                                word = word.nextSiblingElement();
                                            }

                                            if (isContains) isOk = false;
                                        }

                                        words = words.nextSiblingElement();
                                    }
                                }

                                if (props.tagName() == "Var" && isOk) {
                                    int index = -1;
                                    QList<QString> keywords;
                                    QString text;
                                    QDomElement word = props.firstChildElement();

                                    while (!word.isNull()) {
                                        keywords.append(word.text());
                                        word = word.nextSiblingElement();
                                    }

                                    foreach (QString _word , keywords) {
                                        index = cmd.lastIndexOf(_word);
                                        if (index != -1) break;
                                    }

                                    if (index != -1) {
                                        for (int i = index+1; i < cmd.length(); i++) {
                                            text.isEmpty() ? text.append(cmd[i]) : text.append(" "+cmd[i]);
                                            if (i == index+props.attribute("max").toInt()) break;
                                        }
                                    }

                                    if (text != "") var.append(text);
                                }

                                if (props.tagName() == "Reply" && isOk) {
                                    QDomElement condi_or_rep = props.firstChildElement();
                                    bool result = false;

                                    while (!condi_or_rep.isNull()) {
                                        if (condi_or_rep.tagName() == "rep") {
                                            QList<QString> repList;

                                            while (!condi_or_rep.isNull()) {
                                                repList.append(condi_or_rep.text());
                                                condi_or_rep = condi_or_rep.nextSiblingElement();
                                            }

                                            std::uniform_real_distribution<double> dist(0, repList.length());
                                            int val = dist(*QRandomGenerator::global());

                                            sendReply(readVarInText(repList[val], var), array_cmd[array_cmd.length()-1] == cmd ? true : false, "message", QList<QString>(), QList<QString>());
                                            isRep = true;
                                            if (secondItem.attribute("needId", "") != "")
                                                nextReplyNeedId = secondItem.attribute("needId");
                                            if (secondItem.attribute("needId", "") == "null") {
                                                nextReplyNeedId.clear();
                                                nextReplyPluginName.clear();
                                                nextReplyItemId.clear();

                                                if (!mainVolatil_prop.isEmpty()) {
                                                    main_prop = mainVolatil_prop;
                                                    mainVolatil_prop.clear();
                                                    while (removePropNuber != 0) {
                                                        prop.removeLast();
                                                        removePropNuber--;
                                                    }
                                                    addBaseProp();
                                                }
                                            }
                                        }
                                        else if (condi_or_rep.tagName() == "condition" && condi_or_rep.attribute("if") != "") {
                                            QString conditionA;
                                            QString conditionB;
                                            QString m_operator;
                                            bool isConditionA = true;

                                            for (int i = 0; i < condi_or_rep.attribute("if").length(); i++) {
                                                if (condi_or_rep.attribute("if").at(i) == "!") { m_operator = "!"; isConditionA = false; }
                                                else if (condi_or_rep.attribute("if").at(i) == "=") { m_operator = "="; isConditionA = false; }
                                                else {
                                                    if (isConditionA) conditionA.append(condi_or_rep.attribute("if").at(i));
                                                    else conditionB.append(condi_or_rep.attribute("if").at(i));
                                                }
                                            }

                                            conditionA = readVarInText(conditionA, var);
                                            conditionB = readVarInText(conditionB, var);

                                            result = false;

                                            if (m_operator == "!") {
                                                if (conditionA != conditionB) result = true;
                                                else result = false;
                                            }
                                            if (m_operator == "=") {
                                                if (conditionA == conditionB) result = true;
                                                else result = false;
                                            }

                                            if (result) {
                                                QDomElement m_rep = condi_or_rep.firstChildElement();
                                                QList<QString> repList;

                                                while (!m_rep.isNull()) {
                                                    repList.append(m_rep.text());
                                                    m_rep = m_rep.nextSiblingElement();
                                                }

                                                std::uniform_real_distribution<double> dist(0, repList.length());
                                                int val = dist(*QRandomGenerator::global());

                                                sendReply(readVarInText(repList[val], var), array_cmd[array_cmd.length()-1] == cmd ? true : false, "message", QList<QString>(), QList<QString>());
                                                isRep = true;
                                                if (secondItem.attribute("needId", "") != "")
                                                    nextReplyNeedId = secondItem.attribute("needId");
                                                if (secondItem.attribute("needId", "") == "null") {
                                                    nextReplyNeedId.clear();
                                                    nextReplyPluginName.clear();
                                                    nextReplyItemId.clear();

                                                    if (!mainVolatil_prop.isEmpty()) {
                                                        main_prop = mainVolatil_prop;
                                                        mainVolatil_prop.clear();
                                                        while (removePropNuber != 0) {
                                                            prop.removeLast();
                                                            removePropNuber--;
                                                        }
                                                        addBaseProp();
                                                    }
                                                }
                                            }
                                        }
                                        else if (condi_or_rep.tagName() == "else" && !result) {
                                            QDomElement m_rep = condi_or_rep.firstChildElement();
                                            QList<QString> repList;

                                            while (!m_rep.isNull()) {
                                                repList.append(m_rep.text());
                                                m_rep = m_rep.nextSiblingElement();
                                            }

                                            std::uniform_real_distribution<double> dist(0, repList.length());
                                            int val = dist(*QRandomGenerator::global());

                                            sendReply(readVarInText(repList[val], var), array_cmd[array_cmd.length()-1] == cmd ? true : false, "message", QList<QString>(), QList<QString>());
                                            isRep = true;
                                            if (secondItem.attribute("needId", "") != "")
                                                nextReplyNeedId = secondItem.attribute("needId");
                                            if (secondItem.attribute("needId", "") == "null") {
                                                nextReplyNeedId.clear();
                                                nextReplyPluginName.clear();
                                                nextReplyItemId.clear();

                                                if (!mainVolatil_prop.isEmpty()) {
                                                    main_prop = mainVolatil_prop;
                                                    mainVolatil_prop.clear();
                                                    while (removePropNuber != 0) {
                                                        prop.removeLast();
                                                        removePropNuber--;
                                                    }
                                                    addBaseProp();
                                                }
                                            }
                                        }

                                        condi_or_rep = condi_or_rep.nextSiblingElement();
                                    }
                                }

                                if (props.tagName() == "Actions" && isOk) {
                                    QDomElement action = props.firstChildElement();
                                    bool result = false;

                                    while (!action.isNull()) {
                                        if (action.tagName() == "action") {
                                            QList<QString> cmd;
                                            QString word = "";

                                            for (int i = 0; i < action.text().length(); i++) {
                                                if (action.text().at(i) == ' ') {
                                                    if (!word.isEmpty()) {
                                                        cmd.append(word);
                                                        word.clear();
                                                    }
                                                }
                                                else {
                                                    word.append(action.text().at(i));
                                                }

                                                if (i == action.text().length()-1) {
                                                    cmd.append(word);
                                                    word.clear();
                                                }
                                            }

                                            if (cmd[0] == "settings" || cmd[0] == "web_message") {
                                                execAction(cmd);
                                            }
                                            else {
                                                for (int i = 0; i < cmd.length(); i++) {
                                                    cmd[i] = readVarInText(cmd.at(i), var);
                                                }

                                                plug->execAction(cmd);
                                            }
                                        }
                                        else if (action.tagName() == "condition" && action.attribute("if") != "") {
                                            QString conditionA;
                                            QString conditionB;
                                            QString m_operator;
                                            bool isConditionA = true;

                                            for (int i = 0; i < action.attribute("if").length(); i++) {
                                                if (action.attribute("if").at(i) == "!") { m_operator = "!"; isConditionA = false; }
                                                else if (action.attribute("if").at(i) == "=") { m_operator = "="; isConditionA = false; }
                                                else {
                                                    if (isConditionA) conditionA.append(action.attribute("if").at(i));
                                                    else conditionB.append(action.attribute("if").at(i));
                                                }
                                            }

                                            conditionA = readVarInText(conditionA, var);
                                            conditionB = readVarInText(conditionB, var);

                                            result = false;

                                            if (m_operator == "!") {
                                                if (conditionA != conditionB) result = true;
                                                else result = false;
                                            }
                                            if (m_operator == "=") {
                                                if (conditionA == conditionB) result = true;
                                                else result = false;
                                            }

                                            if (result) {
                                                QDomElement m_action = action.firstChildElement();

                                                while (!m_action.isNull()) {
                                                    QList<QString> cmd;
                                                    QString word = "";

                                                    for (int i = 0; i < m_action.text().length(); i++) {
                                                        if (m_action.text().at(i) == ' ') {
                                                            if (!word.isEmpty()) {
                                                                cmd.append(word);
                                                                word.clear();
                                                            }
                                                        }
                                                        else {
                                                            word.append(m_action.text().at(i));
                                                        }

                                                        if (i == m_action.text().length()-1) {
                                                            cmd.append(word);
                                                            word.clear();
                                                        }
                                                    }

                                                    if (cmd[0] == "settings" || cmd[0] == "web_message") {
                                                        execAction(cmd);
                                                    }
                                                    else {
                                                        for (int i = 0; i < cmd.length(); i++) {
                                                            cmd[i] = readVarInText(cmd.at(i), var);
                                                        }

                                                        plug->execAction(cmd);
                                                    }

                                                    m_action = m_action.nextSiblingElement();
                                                }
                                            }
                                        }
                                        else if (action.tagName() == "else" && !result) {
                                            QDomElement m_action = action.firstChildElement();

                                            while (!m_action.isNull()) {
                                                QList<QString> cmd;
                                                QString word = "";

                                                for (int i = 0; i < m_action.text().length(); i++) {
                                                    if (m_action.text().at(i) == ' ') {
                                                        if (!word.isEmpty()) {
                                                            cmd.append(word);
                                                            word.clear();
                                                        }
                                                    }
                                                    else {
                                                        word.append(m_action.text().at(i));
                                                    }

                                                    if (i == m_action.text().length()-1) {
                                                        cmd.append(word);
                                                        word.clear();
                                                    }
                                                }

                                                if (cmd[0] == "settings" || cmd[0] == "web_message") {
                                                    execAction(cmd);
                                                }
                                                else {
                                                    for (int i = 0; i < cmd.length(); i++) {
                                                        cmd[i] = readVarInText(cmd.at(i), var);
                                                    }

                                                    plug->execAction(cmd);
                                                }

                                                m_action = m_action.nextSiblingElement();
                                            }
                                        }

                                        action = action.nextSiblingElement();
                                    }
                                }

                                props = props.nextSiblingElement();
                            }
                        }

                        secondItem = secondItem.nextSiblingElement();
                    }
                }

                var.clear();
                item = item.nextSiblingElement();
            }
        }
    }

    if (!isRep) return false;

    return true;
}

QString Engine::readVarInText(QString text, QList<QString> var)
{
    QString reply;

    for (int i = 0; i < text.length(); i++) {
        QString ch = text.at(i);
        int nextIndex = i+1 == text.length() ? i : i+1;
        int nextIndexB = i+2 == text.length() ? i : i+2;
        int nextIndexC = i+3 == text.length() ? i : i+3;
        int nextIndexD = i+4 == text.length() ? i : i+4;

        if (ch == "?" && (text.at(nextIndex) == "0" || text.at(nextIndex) == "1" || text.at(nextIndex) == "2")) {
            QString volatil = text.at(nextIndex);
            if (volatil.toInt() < var.length()) reply.append(var[volatil.toInt()]);
            i++;
        }
        else if (ch == "?" && text.at(nextIndex) == "n" && text.at(nextIndexB) == "a" && text.at(nextIndexC) == "m" && text.at(nextIndexD) == "e") {
            updateSettingsVar();
            reply.append(userName);
            i += 4;
        }
        else if (ch == "?" && text.at(nextIndex) == "p" && text.at(nextIndexB) == "r" && text.at(nextIndexC) == "o" && text.at(nextIndexD) == "p") {
            updateSettingsVar();
            reply.append(propEnabled ? "activé" : "desactivé");
            i += 4;
        }
        else if (ch == "?" && text.at(nextIndex) == "d" && text.at(nextIndexB) == "a" && text.at(nextIndexC) == "t" && text.at(nextIndexD) == "e") {
            QString date = QDateTime::currentDateTime().toString("dddd dd MMMM yyyy");
            reply.append(date);
            i += 4;
        }
        else if (ch == "?" && text.at(nextIndex) == "h" && text.at(nextIndexB) == "o" && text.at(nextIndexC) == "u" && text.at(nextIndexD) == "r") {
            QString hour = QDateTime::currentDateTime().toString("hh:mm:ss");
            reply.append(hour);
            i += 4;
        }
        else if (ch == "?" && text.at(nextIndex) == "d" && text.at(nextIndexB) == "t") {
            QString dt = QDateTime::currentDateTime().toString("dddd dd MMMM yyyy hh:mm");
            reply.append(dt);
            i += 2;
        }
        else {
            reply.append(ch);
        }
    }

    return reply;
}
