/***************************************************************************
**                                                                        **
**  Polyphone, a soundfont editor                                         **
**  Copyright (C) 2013-2019 Davy Triponney                                **
**                2014      Andrea Celani                                 **
**                                                                        **
**  This program is free software: you can redistribute it and/or modify  **
**  it under the terms of the GNU General Public License as published by  **
**  the Free Software Foundation, either version 3 of the License, or     **
**  (at your option) any later version.                                   **
**                                                                        **
**  This program is distributed in the hope that it will be useful,       **
**  but WITHOUT ANY WARRANTY; without even the implied warranty of        **
**  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the          **
**  GNU General Public License for more details.                          **
**                                                                        **
**  You should have received a copy of the GNU General Public License     **
**  along with this program. If not, see http://www.gnu.org/licenses/.    **
**                                                                        **
****************************************************************************
**           Author: Davy Triponney                                       **
**  Website/Contact: https://www.polyphone-soundfonts.com                 **
**             Date: 01.01.2013                                           **
***************************************************************************/

#include "modulatoreditor.h"
#include "ui_modulatoreditor.h"
#include "contextmanager.h"
#include "modulatorcell.h"
#include "soundfontmanager.h"
#include "dialogselection.h"
#include <QMessageBox>
#include <QKeyEvent>

QList<ModulatorEditor *> ModulatorEditor::s_instances;
QList<ModulatorEditor::Modulator> ModulatorEditor::s_modulatorCopy;

ModulatorEditor::ModulatorEditor(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::ModulatorEditor)
{
    s_instances << this;
    ui->setupUi(this);

    // Icons
    ui->pushExpand->setIcon(ContextManager::theme()->getColoredSvg(":/icons/arrow_up.svg", QSize(16, 16), ThemeManager::BUTTON_TEXT));
    ui->pushAdd->setIcon(ContextManager::theme()->getColoredSvg(":/icons/document-new.svg", QSize(16, 16), ThemeManager::BUTTON_TEXT));
    ui->pushCopy->setIcon(ContextManager::theme()->getColoredSvg(":/icons/copy.svg", QSize(16, 16), ThemeManager::BUTTON_TEXT));
    ui->pushPaste->setIcon(ContextManager::theme()->getColoredSvg(":/icons/paste.svg", QSize(16, 16), ThemeManager::BUTTON_TEXT));
    ui->pushClone->setIcon(ContextManager::theme()->getColoredSvg(":/icons/clone.svg", QSize(16, 16), ThemeManager::BUTTON_TEXT));
    ui->pushDelete->setIcon(ContextManager::theme()->getColoredSvg(":/icons/minus.svg", QSize(16, 16), ThemeManager::BUTTON_TEXT));
    ui->pushCollapse->setIcon(ContextManager::theme()->getColoredSvg(":/icons/arrow_down.svg", QSize(16, 16), ThemeManager::BUTTON_TEXT));

    // Style
    ui->frameButtons->setStyleSheet("QFrame{border:1px solid " +
                                    this->palette().dark().color().name() +
                                    ";border-top:0;border-bottom:0;border-left:0}");
    ui->labelNoModulators->setStyleSheet("QLabel{color:" + this->palette().dark().color().name() +
                                         ";background-color:" + ContextManager::theme()->getColor(ThemeManager::LIST_BACKGROUND).name() +
                                         "}");
    ui->labelSelectDivision->setStyleSheet("QLabel{color:" + this->palette().dark().color().name() +
                                           ";background-color:" + ContextManager::theme()->getColor(ThemeManager::LIST_BACKGROUND).name() +
                                           "}");

    // Initialize the expanded / collapsed state
    if (ContextManager::configuration()->getValue(ConfManager::SECTION_DISPLAY, "modulator_section_collapsed", false).toBool())
        on_pushCollapse_clicked();
    else
        on_pushExpand_clicked();

    // Buttons
    updateButtons(false);

    // Connection
    connect(ui->listWidget, SIGNAL(copied()), this, SLOT(on_pushCopy_clicked()));
    connect(ui->listWidget, SIGNAL(pasted()), this, SLOT(on_pushPaste_clicked()));
    connect(ui->listWidget, SIGNAL(deleted()), this, SLOT(on_pushDelete_clicked()));
}

ModulatorEditor::~ModulatorEditor()
{
    s_instances.removeOne(this);
    delete ui;
}

void ModulatorEditor::on_pushExpand_clicked()
{
    foreach (ModulatorEditor * elt, s_instances)
    {
        elt->ui->frameCollapsed->hide();
        elt->ui->frameExpanded->show();
    }

    ContextManager::configuration()->setValue(ConfManager::SECTION_DISPLAY, "modulator_section_collapsed", false);
}

void ModulatorEditor::on_pushCollapse_clicked()
{
    foreach (ModulatorEditor * elt, s_instances)
    {
        elt->ui->frameExpanded->hide();
        elt->ui->frameCollapsed->show();
    }

    ContextManager::configuration()->setValue(ConfManager::SECTION_DISPLAY, "modulator_section_collapsed", true);
}

void ModulatorEditor::setIds(IdList ids, QList<AttributeType> attributes)
{
    // Reset buttons
    ui->pushAdd->setEnabled(false);
    ui->pushClone->setEnabled(false);
    ui->pushCopy->setEnabled(false);
    ui->pushDelete->setEnabled(false);
    ui->pushPaste->setEnabled(false);

    if (ids.empty())
    {
        // Shouldn't happen
        _currentId.typeElement = elementUnknown;
        ui->stackedWidget->setCurrentIndex(0);
        ui->labelModSummary->setText("<b>" + trUtf8("Modulators:") + "</b> -");

    }
    else if (ids.count() > 1)
    {
        _currentId.typeElement = elementUnknown;
        ui->stackedWidget->setCurrentIndex(1);
        ui->labelModSummary->setText("<b>" + trUtf8("Modulators:") + "</b> -");
    }
    else
    {
        // Buttons
        ui->pushAdd->setEnabled(true);
        ui->pushCopy->setEnabled(true);
        ui->pushPaste->setEnabled(true);

        // Update the interface with the current division
        _currentId = ids[0];

        // Adapt the attributes to select
        if (attributes.contains(champ_startAddrsOffset))
            attributes << champ_startAddrsCoarseOffset;
        if (attributes.contains(champ_endAddrsOffset))
            attributes << champ_endAddrsCoarseOffset;
        if (attributes.contains(champ_startloopAddrsOffset))
            attributes << champ_startloopAddrsCoarseOffset;
        if (attributes.contains(champ_endloopAddrsOffset))
            attributes << champ_endloopAddrsCoarseOffset;

        // Update the interface
        updateInterface(attributes);
    }
}

void ModulatorEditor::updateInterface(QList<AttributeType> attributes)
{
    // List of mods associated to the current id
    EltID modId = _currentId;
    switch (_currentId.typeElement)
    {
    case elementInst:
        modId.typeElement = elementInstMod;
        break;
    case elementPrst:
        modId.typeElement = elementPrstMod;
        break;
    case elementInstSmpl:
        modId.typeElement = elementInstSmplMod;
        break;
    case elementPrstInst:
        modId.typeElement = elementPrstInstMod;
        break;
    default:
        modId.typeElement = elementUnknown;
        break;
    }

    // Clear the cell list
    ui->listWidget->clear();

    // Foreach modulator
    QStringList modTargets;
    int modCount = 0;
    foreach (int i, SoundfontManager::getInstance()->getSiblings(modId))
    {
        modId.indexMod = i;

        // Target list
        AttributeValue value = SoundfontManager::getInstance()->get(modId, champ_sfModDestOper);
        if (value.wValue < 99) // Link are rejected in the summary
            modTargets << Attribute::getDescription(value.sfGenValue, modId.typeElement == elementPrstMod || modId.typeElement == elementPrstInstMod);

        // Add a new cell
        ModulatorCell * cell = new ModulatorCell(modId);
        QListWidgetItem * item = new QListWidgetItem();
        item->setSizeHint(cell->size());
        ui->listWidget->addItem(item);
        ui->listWidget->setItemWidget(item, cell);

        // Selection
        item->setSelected(attributes.contains((AttributeType)value.wValue));

        // Modulator count
        modCount++;
    }
    ui->stackedWidget->setCurrentIndex(modCount == 0 ? 0 : 2);

    // Fill the summary
    QString summary = "<b>" + trUtf8("Modulators (%1): ").arg(modCount) + "</b> ";
    if (modTargets.count() == 0)
        summary += "-";
    else
        for (int i = 0; i < modTargets.count(); i++)
            summary += (i > 0 ? ", " : "") + modTargets[i];
    ui->labelModSummary->setText(summary);

    // Button visibility
    ui->pushClone->setEnabled(modCount > 0 && (_currentId.typeElement == elementInst || _currentId.typeElement == elementPrst));
    ui->pushCopy->setEnabled(modCount > 0);
}

void ModulatorEditor::on_listWidget_itemSelectionChanged()
{
    // Prepare the list of selected attributes
    QList<QListWidgetItem *> selection = ui->listWidget->selectedItems();
    QList<AttributeType> attributes;
    foreach (QListWidgetItem * item, selection)
    {
        ModulatorCell * cell = (ModulatorCell *)ui->listWidget->itemWidget(item);
        AttributeType attribute = cell->getTargetAttribute();
        if (attribute != champ_unknown && !attributes.contains(attribute))
            attributes << attribute;
    }

    // Notify the change
    emit(attributesSelected(attributes));

    // Adapt the button tooltips
    updateButtons(!selection.isEmpty());
}

void ModulatorEditor::updateButtons(bool withSelection)
{
    if (withSelection)
    {
        ui->pushClone->setToolTip(trUtf8("Duplicate the selection toward..."));
        ui->pushCopy->setToolTip(trUtf8("Copy the selected modulators"));
        ui->pushDelete->setEnabled(true);
    }
    else
    {
        ui->pushClone->setToolTip(trUtf8("Duplicate modulators toward..."));
        ui->pushCopy->setToolTip(trUtf8("Copy all modulators"));
        ui->pushDelete->setEnabled(false);
    }
}

void ModulatorEditor::on_pushAdd_clicked()
{
    // Create a new mod
    if (_currentId.typeElement == elementUnknown)
        return;

    EltID modId = _currentId;
    switch (_currentId.typeElement)
    {
    case elementInst:
        modId.typeElement = elementInstMod;
        break;
    case elementPrst:
        modId.typeElement = elementPrstMod;
        break;
    case elementInstSmpl:
        modId.typeElement = elementInstSmplMod;
        break;
    case elementPrstInst:
        modId.typeElement = elementPrstInstMod;
        break;
    default:
        return;
        break;
    }
    SoundfontManager * sm = SoundfontManager::getInstance();
    modId.indexMod = sm->add(modId);

    // Initialization
    AttributeValue val;
    val.sfGenValue = (AttributeType)0;
    val.wValue = 0;
    sm->set(modId, champ_modAmount, val);
    sm->set(modId, champ_sfModTransOper, val);
    val.sfModValue.CC = 0;
    val.sfModValue.D = 0;
    val.sfModValue.Index = 0;
    val.sfModValue.P = 0;
    val.sfModValue.Type = 0;
    sm->set(modId, champ_sfModSrcOper, val);
    sm->set(modId, champ_sfModAmtSrcOper, val);
    if (modId.typeElement == elementPrstMod || modId.typeElement == elementPrstInstMod)
        val.sfGenValue = (AttributeType)52;
    sm->set(modId, champ_sfModDestOper, val);

    sm->endEditing("modulatorEditor");
}

void ModulatorEditor::on_pushCopy_clicked()
{
    // Save des mods
    if (_currentId.typeElement == elementUnknown)
        return;

    EltID id = _currentId;
    if (id.typeElement == elementInst)
        id.typeElement = elementInstMod;
    else if (id.typeElement == elementPrst)
        id.typeElement = elementPrstMod;
    else if (id.typeElement == elementInstSmpl)
        id.typeElement = elementInstSmplMod;
    else if (id.typeElement == elementPrstInst)
        id.typeElement = elementPrstInstMod;
    else return;

    s_modulatorCopy = getModList(id);
}

void ModulatorEditor::on_pushPaste_clicked()
{
    if (_currentId.typeElement == elementUnknown)
        return;
    pasteMod(_currentId, s_modulatorCopy);
    SoundfontManager::getInstance()->endEditing("modulatorEditor");
}

void ModulatorEditor::on_pushClone_clicked()
{
    // Duplication des mods sélectionnés dans tous les autres instruments ou presets
    if (_currentId.typeElement != elementInst && _currentId.typeElement != elementPrst)
        return;

    DialogSelection * dial = new DialogSelection(SoundfontManager::getInstance(), _currentId, this);
    connect(dial, SIGNAL(listChosen(QList<int>)), this, SLOT(duplicateMod(QList<int>)));
    dial->show();
}

void ModulatorEditor::duplicateMod(QList<int> listIndex)
{
    if (_currentId.typeElement != elementInst && _currentId.typeElement != elementPrst)
        return;

    EltID idMod = _currentId;
    if (idMod.typeElement == elementInst)
        idMod.typeElement = elementInstMod;
    else
        idMod.typeElement = elementPrstMod;

    // Copy mods
    QList<Modulator> modulators = getModList(idMod);
    EltID idDest = _currentId;
    foreach (int numElement, listIndex)
    {
        idDest.indexElt = numElement;
        pasteMod(idDest, modulators);
    }

    SoundfontManager::getInstance()->endEditing("modulatorEditor");
}

void ModulatorEditor::on_pushDelete_clicked()
{
    if (_currentId.typeElement == elementUnknown)
        return;

    int rowToSelect = ui->listWidget->currentIndex().row();
    int nbRow = ui->listWidget->count();

    // List of selected modulators
    QList<EltID> listIDs = getSelectedModulators();
    if (listIDs.isEmpty())
        return;

    // Delete the selected mods
    SoundfontManager * sm = SoundfontManager::getInstance();
    foreach (EltID id, listIDs)
    {
        // First remove all links toward the modulator to delete
        foreach (int i, sm->getSiblings(id))
        {
            if (i == id.indexMod)
                continue;

            EltID otherModId = id;
            otherModId.indexMod = i;
            int destIndex = sm->get(otherModId, champ_sfModDestOper).wValue;
            if (destIndex == 32768 + id.indexMod)
            {
                AttributeValue val2;
                val2.wValue = 0;
                sm->set(otherModId, champ_sfModDestOper, val2);
            }
        }

        sm->remove(id);
    }

    sm->endEditing("modulatorEditor");

    if (rowToSelect >= nbRow - listIDs.size())
        rowToSelect = nbRow - listIDs.size() - 1;

    ui->listWidget->setCurrentRow(rowToSelect);
}

QList<ModulatorEditor::Modulator> ModulatorEditor::getModList(EltID id)
{
    QList<Modulator> listRet;
    SoundfontManager * sm = SoundfontManager::getInstance();

    QList<EltID> listSelectedMods = this->getSelectedModulators();
    if (listSelectedMods.isEmpty())
    {
        // All modulators are copied
        foreach (int i, sm->getSiblings(id))
        {
            id.indexMod = i;
            Modulator modTmp;
            modTmp.modSrcOper = sm->get(id, champ_sfModSrcOper).sfModValue;
            modTmp.modDestOper = sm->get(id, champ_sfModDestOper).sfGenValue;
            modTmp.modAmount = sm->get(id, champ_modAmount).shValue;
            modTmp.modAmtSrcOper = sm->get(id, champ_sfModAmtSrcOper).sfModValue;
            modTmp.modTransOper = sm->get(id, champ_sfModTransOper).sfTransValue;
            modTmp.index = id.indexMod;
            listRet << modTmp;
        }
    }
    else
    {
        // Only selected mods are copied
        foreach (EltID id, listSelectedMods)
        {
            Modulator modTmp;
            modTmp.modSrcOper = sm->get(id, champ_sfModSrcOper).sfModValue;
            modTmp.modDestOper = sm->get(id, champ_sfModDestOper).sfGenValue;
            modTmp.modAmount = sm->get(id, champ_modAmount).shValue;
            modTmp.modAmtSrcOper = sm->get(id, champ_sfModAmtSrcOper).sfModValue;
            modTmp.modTransOper = sm->get(id, champ_sfModTransOper).sfTransValue;
            modTmp.index = id.indexMod;
            listRet << modTmp;
        }
    }

    // List all indexes
    QList<int> listIndex;
    foreach (Modulator mod, listRet)
        if (!listIndex.contains(mod.index))
            listIndex << mod.index;

    for (int i = 0; i < listRet.size(); i++)
    {
        Modulator mod = listRet.at(i);

        if ((int)mod.modDestOper >= 32768)
        {
            // Broken links are removed
            int link = mod.modDestOper - 32768;
            if (listIndex.contains(link))
                mod.modDestOper = (AttributeType)(32768 + listIndex.indexOf(link));
            else
                mod.modDestOper = champ_fineTune;
        }

        // Indexes start at 0
        mod.index = listIndex.indexOf(mod.index);

        listRet[i] = mod;
    }

    for (int i = 0; i < listRet.size(); i++)
    {
        Modulator mod = listRet.at(i);
        if (mod.modSrcOper.Index == 127 && mod.modSrcOper.CC == 0)
        {
            // We find the link
            bool found = false;
            for (int j = 0; j < listRet.size(); j++)
            {
                if (i != j && listRet.at(j).modDestOper == 32768 + i)
                    found = true;
            }

            if (!found)
            {
                mod.modSrcOper.Index = 0;
                listRet[i] = mod;
            }
        }
    }

    return listRet;
}

QList<EltID> ModulatorEditor::getSelectedModulators()
{
    // Get all selected modulators
    QList<EltID> listIDs;
    QList<QListWidgetItem *> selection = ui->listWidget->selectedItems();
    foreach (QListWidgetItem * item, selection)
    {
        ModulatorCell * cell = (ModulatorCell *)ui->listWidget->itemWidget(item);
        listIDs << cell->getID();
    }
    return listIDs;
}

void ModulatorEditor::pasteMod(EltID id, QList<Modulator> modulators)
{
    if (modulators.empty())
        return;
    if (id.typeElement == elementInst)
        id.typeElement = elementInstMod;
    else if (id.typeElement == elementPrst)
        id.typeElement = elementPrstMod;
    else if (id.typeElement == elementInstSmpl)
        id.typeElement = elementInstSmplMod;
    else if (id.typeElement == elementPrstInst)
        id.typeElement = elementPrstInstMod;
    else
        return;

    if (id.typeElement == elementPrstMod || id.typeElement == elementPrstInstMod)
    {
        // Check that all destinations are possible
        AttributeType champTmp;
        for (int i = 0; i < modulators.size(); i++)
        {
            champTmp = modulators[i].modDestOper;
            QString warnQStr = trUtf8("Forbidden action:") + " ";
            if (champTmp == champ_startAddrsOffset ||
                    champTmp == champ_startAddrsCoarseOffset ||
                    champTmp == champ_startloopAddrsOffset ||
                    champTmp == champ_startloopAddrsCoarseOffset ||
                    champTmp == champ_endAddrsOffset ||
                    champTmp == champ_endAddrsCoarseOffset ||
                    champTmp == champ_endloopAddrsOffset ||
                    champTmp == champ_endloopAddrsCoarseOffset)
            {
                QMessageBox::warning(this, trUtf8("Warning"), warnQStr +
                                     trUtf8("offsets cannot be modulated in a preset."));
                return;
            }
            else if (champTmp == champ_keynum || champTmp == champ_velocity ||
                     champTmp == champ_sampleModes ||
                     champTmp == champ_exclusiveClass ||
                     champTmp == champ_overridingRootKey)
            {
                QMessageBox::warning(this, trUtf8("Warning"), warnQStr +
                                     trUtf8("%1 cannot be modulated in a preset.").arg("<b>" + Attribute::getDescription(champTmp, true) + "</b>"));
                return;
            }
        }
    }

    // Create new mods
    QList<int> listIndex;
    SoundfontManager * sm = SoundfontManager::getInstance();
    for (int i = 0; i < modulators.size(); i++)
        listIndex.append(sm->add(id));

    // Minimum index that the mods to copy will have
    int offsetIndex = listIndex[0];
    for (int i = 0; i < modulators.size(); i++)
    {
        Modulator mod = modulators.at(i);
        mod.index += offsetIndex;
        if ((int)mod.modDestOper >= 32768)
            mod.modDestOper = (AttributeType)(mod.modDestOper + offsetIndex);
        modulators[i] = mod;
    }

    // Copy the configuration of the saved mods
    AttributeValue valTmp;
    Modulator modTmp;
    for (int i = 0; i < modulators.size(); i++)
    {
        id.indexMod = listIndex.at(i);
        modTmp = modulators.at(i);
        valTmp.sfModValue = modTmp.modSrcOper;
        sm->set(id, champ_sfModSrcOper, valTmp);
        valTmp.sfGenValue = modTmp.modDestOper;
        sm->set(id, champ_sfModDestOper, valTmp);
        valTmp.shValue = modTmp.modAmount;
        sm->set(id, champ_modAmount, valTmp);
        valTmp.sfModValue = modTmp.modAmtSrcOper;
        sm->set(id, champ_sfModAmtSrcOper, valTmp);
        valTmp.sfTransValue = modTmp.modTransOper;
        sm->set(id, champ_sfModTransOper, valTmp);
    }
}