/***************************************************************************
**                                                                        **
**  Polyphone, a soundfont editor                                         **
**  Copyright (C) 2013-2019 Davy Triponney                                **
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

#include "grandorguedatathrough.h"

GrandOrgueDataThrough::GrandOrgueDataThrough() :
    _maxGain(0)
{

}

void GrandOrgueDataThrough::setMaxRankGain(int rankId, double gain)
{
    if (_maxGainPerRank.contains(rankId))
        _maxGainPerRank[rankId] = qMax(_maxGainPerRank[rankId], gain);
    else
        _maxGainPerRank[rankId] = gain;
}

double GrandOrgueDataThrough::getMaxRankGain(int rankId)
{
    return _maxGainPerRank.contains(rankId) ? _maxGainPerRank[rankId] : 0;
}

void GrandOrgueDataThrough::finalizePreprocess()
{
    foreach (double val, _maxGainPerRank.values())
        if (val > _maxGain)
            _maxGain = val;
}

void GrandOrgueDataThrough::setSf2InstId(int grandOrgueInstId, int sf2ElementId)
{
    _instIds[grandOrgueInstId] = sf2ElementId;
}

int GrandOrgueDataThrough::getSf2InstId(int grandOrgueInstId)
{
    return _instIds.contains(grandOrgueInstId) ? _instIds[grandOrgueInstId] : -1;
}

void GrandOrgueDataThrough::setSf2SmplId(QString filePath, QList<int> sf2ElementIds)
{
    _smplIds[filePath] = sf2ElementIds;
}

QList<int> GrandOrgueDataThrough::getSf2SmplId(QString filePath)
{
    return _smplIds.contains(filePath) ? _smplIds[filePath] : QList<int>();
}

void GrandOrgueDataThrough::storeSampleName(QString sampleName)
{
    _sampleNames << sampleName.toLower();
}

bool GrandOrgueDataThrough::sampleNameExists(QString sampleName)
{
    return _sampleNames.contains(sampleName.toLower());
}
