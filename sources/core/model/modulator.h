#ifndef MODULATOR_H
#define MODULATOR_H

#include "basetypes.h"

class Modulator
{
public:
    Modulator(int id);

    void setHidden(bool isHidden);
    bool isHidden() { return _hidden; }

    SFModulator _sfModSrcOper;
    AttributeType _sfModDestOper;
    short _modAmount;
    SFModulator _sfModAmtSrcOper;
    SFTransform _sfModTransOper;
    int _index;

private:
    bool _hidden;
};

#endif // MODULATOR_H
