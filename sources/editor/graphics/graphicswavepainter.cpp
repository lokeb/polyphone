/***************************************************************************
**                                                                        **
**  Polyphone, a soundfont editor                                         **
**  Copyright (C) 2013-2020 Davy Triponney                                **
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

#include "graphicswavepainter.h"
#include "contextmanager.h"
#include <QPainter>
#include "qmath.h"

static float fastSqrt(float val)
{
    union {
	float f;
	quint32 u;
    } I;
    if (val < 0.0001f)
        return 0.0001f;
    I.f = val;
#define i I.u
    i += 127L << 23; // adjust bias
    i >>= 1; // approximation of square root
#undef i
    return I.f;
}

static quint32 fastCeil(float val)
{
    quint32 i = static_cast<quint32>(val);
    if (i < val)
        i++;
    return i;
}

GraphicsWavePainter::GraphicsWavePainter(QWidget * widget) :
    _widget(widget),
    _sampleSize(0),
    _sampleData(nullptr),
    _pixels(nullptr),
    _image(nullptr),
    _samplePlotMean(nullptr)
{
    // Initialize colors (always use the darkest color for the background of the graphic)
    if (ContextManager::theme()->isDark(ThemeManager::LIST_BACKGROUND, ThemeManager::LIST_TEXT))
    {
        _backgroundColor = ContextManager::theme()->getColor(ThemeManager::LIST_BACKGROUND).rgb();
        _gridColor = ContextManager::theme()->getColor(ThemeManager::LIST_TEXT).rgb();
    }
    else
    {
        _backgroundColor = ContextManager::theme()->getColor(ThemeManager::LIST_TEXT).rgb();
        _gridColor = ContextManager::theme()->getColor(ThemeManager::LIST_BACKGROUND).rgb();
    }

    _redColor = ContextManager::theme()->getFixedColor(ThemeManager::RED, true).rgb();
    _greenColor = ContextManager::theme()->getFixedColor(ThemeManager::GREEN, true).rgb();
    _waveColor = ContextManager::theme()->getColor(ThemeManager::HIGHLIGHTED_BACKGROUND).rgb();
}

GraphicsWavePainter::~GraphicsWavePainter()
{
    delete [] _sampleData;
    delete _image;
    delete [] _pixels;
    delete [] _samplePlotMean;
}

void GraphicsWavePainter::setData(QByteArray baData)
{
    delete [] _sampleData;
    delete _image;
    delete [] _pixels;
    delete [] _samplePlotMean;
    _sampleData = nullptr;
    _image = nullptr;
    _pixels = nullptr;
    _samplePlotMean = nullptr;

    // Extract the waveform
    _sampleSize = static_cast<quint32>(baData.size()) / 2;
    qint16 * data = reinterpret_cast<qint16 *>(baData.data());
    _sampleData = new qint16[_sampleSize];
    memcpy(_sampleData, data, _sampleSize * sizeof(qint16));
}

void GraphicsWavePainter::paint(quint32 start, quint32 end, float zoomY)
{

    if (start >= _sampleSize)
        start = _sampleSize - 1;
    if (end >= _sampleSize)
        end = _sampleSize - 1;
    if (start == end)
        return;

    // Possibly update the image
    if (_image == nullptr || _start != start || _end != end || _zoomY != zoomY ||
            _image->width() != _widget->width() || _image->height() != _widget->height())
    {
        delete _image;
        delete [] _pixels;
        _image = nullptr;
        _pixels = nullptr;

        // Store the current parameters
        _start = start;
        _end = end;
        _zoomY = zoomY;

        // Prepare a new image
        prepareImage();
    }

    // Draw the curve if valid
    if (_image != nullptr)
    {
        QPainter painter(_widget);
        painter.drawImage(0, 0, *_image);

        // Add the mean value
        painter.setPen(QPen(QColor(_waveColor), 1.0, Qt::SolidLine));
        painter.setRenderHint(QPainter::Antialiasing);
//        painter.drawPolyline(_samplePlotMean, _widget->width());
    }
}

void GraphicsWavePainter::prepareImage()
{
    quint32 width = static_cast<quint32>(_widget->width());
    quint32 height = static_cast<quint32>(_widget->height());

    ////////////////////////////////
    /// FIRST STEP: compute data ///
    ////////////////////////////////

    delete [] _samplePlotMean;
    _samplePlotMean = nullptr;

    if (_sampleSize <= 1)
        return;

    // Wave form larger than the number of pixels
    float * samplePlotMin = new float[width];
    float * samplePlotMax = new float[width];
    _samplePlotMean = new QPointF[width];
    float * samplePlotDeviation = new float[width];

    // Temporary arrays
    float * samplePlotSum = new float[width];
    float * samplePlotSquareSum = new float[width];
    memset(samplePlotSum, 0, width * sizeof(float));
    memset(samplePlotSquareSum, 0, width * sizeof(float));

    // Compute data
    float pointSpace = 1.0f * width / (_end - _start);
    float pointSpaceInv = 1.0f / pointSpace;
    float previousPosition = 0.0f;
    float currentPosition;
    qint16 previousValue = _sampleData[_start];
    qint16 currentValue;
    quint32 previousPixelNumber = 999999;

    for (quint32 i = 1; i <= _end - _start; i++)
    {
        // Current value, current position
        currentValue = _sampleData[_start + i];
        currentPosition = pointSpace * i;

        // Process the segment between {previousPosition, previousValue} and {currentPosition, currentValue}
        float slope = pointSpaceInv * (currentValue - previousValue);
        for (quint32 currentPixelNumber = static_cast<quint32>(previousPosition);
             currentPixelNumber < fastCeil(currentPosition); currentPixelNumber++)
        {
            if (currentPixelNumber >= width)
                continue;

            // Part of the segment crossing pixel {pixelNumber}
            float x1 = currentPixelNumber < previousPosition ? previousPosition : currentPixelNumber;
            float x2 = (currentPixelNumber + 1) > currentPosition ? currentPosition : (currentPixelNumber + 1);
            float y1 = previousValue + (x1 - previousPosition) * slope;
            float y2 = previousValue + (x2 - previousPosition) * slope;

            // Compute the weight and the middle value of the segment
            float weight = x2 - x1;
            float middleValue = 0.5f * (y1 + y2);

            // Compute min / max
            if (currentPixelNumber != previousPixelNumber)
            {
                // First time we are seeing this pixel: min and max or defined
                samplePlotMin[currentPixelNumber] = y1 < y2 ? y1 : y2;
                samplePlotMax[currentPixelNumber] = y1 > y2 ? y1 : y2;
            }
            else
            {
                float minY = y1 < y2 ? y1 : y2;
                if (minY < samplePlotMin[currentPixelNumber])
                    samplePlotMin[currentPixelNumber] = minY;
                float maxY = y1 > y2 ? y1 : y2;
                if (maxY > samplePlotMax[currentPixelNumber])
                    samplePlotMax[currentPixelNumber] = maxY;
            }

            // Sum values with ponderation
            samplePlotSum[currentPixelNumber] += middleValue * weight;
            samplePlotSquareSum[currentPixelNumber] += middleValue * middleValue * weight;

            previousPixelNumber = currentPixelNumber;
        }

        previousPosition = currentPosition;
        previousValue = currentValue;
    }

    // Compute mean, standard deviation, adjust min / max
    float coeff = -_zoomY * static_cast<float>(height) / (32768 * 2);
    float offsetY = 0.5f * height;
    for (quint32 i = 0; i < width; i++)
    {
        _samplePlotMean[i].setX(i);
        _samplePlotMean[i].setY(static_cast<double>(coeff * samplePlotSum[i] + offsetY));
        samplePlotDeviation[i] = coeff *
                    //qSqrt(samplePlotSquareSum[i] - samplePlotSum[i] * samplePlotSum[i])
                    fastSqrt(static_cast<float>(samplePlotSquareSum[i] - samplePlotSum[i] * samplePlotSum[i]))
                ;
        samplePlotMin[i] = coeff * samplePlotMin[i] + offsetY;
        samplePlotMax[i] = coeff * samplePlotMax[i] + offsetY;
    }

    // Delete temporary arrays
    delete [] samplePlotSum;
    delete [] samplePlotSquareSum;


    //////////////////////////////////////
    /// SECOND STEP: compute the image ///
    //////////////////////////////////////

    // Prepare a new image
    _pixels = new QRgb[width * height];
    _image = new QImage(reinterpret_cast<uchar*>(_pixels), static_cast<int>(width), static_cast<int>(height), QImage::Format_ARGB32);

    // Background
    for (quint32 i = 0; i < width * height; i++)
        _pixels[i] = _backgroundColor;

    // Horizontal lines
    QPainter painter(_image);
    QColor color = _gridColor;
    color.setAlpha(40);
    painter.setPen(QPen(color, 1.0, Qt::SolidLine));
    painter.drawLine(QPointF(-1, 0.5 * height), QPointF(width + 1, 0.5 * height));
    painter.setPen(QPen(color, 1.0, Qt::DotLine));
    painter.drawLine(QPointF(-1, 0.125 * height), QPointF(width + 1, 0.125 * height));
    painter.drawLine(QPointF(-1, 0.25 * height), QPointF(width + 1, 0.25 * height));
    painter.drawLine(QPointF(-1, 0.375 * height), QPointF(width + 1, 0.375 * height));
    painter.drawLine(QPointF(-1, 0.625 * height), QPointF(width + 1, 0.625 * height));
    painter.drawLine(QPointF(-1, 0.75 * height), QPointF(width + 1, 0.75 * height));
    painter.drawLine(QPointF(-1, 0.875 * height), QPointF(width + 1, 0.875 * height));

    // Waveform
    float mean, x;
    quint32 currentPixelIndex;
    for (quint32 i = 0; i < width; i++)
    {
        mean = static_cast<float>(_samplePlotMean[i].y());
        for (quint32 j = 0; j < height; j++)
        {
            // Index of the current pixel
            currentPixelIndex = i + j * width;

            x = j < mean ? getValueX(samplePlotMax[i], 0, mean + samplePlotDeviation[i], 1, j) :
                            getValueX(mean - samplePlotDeviation[i], 1, samplePlotMin[i], 0, j);
            if (x > 0)
                _pixels[currentPixelIndex] = mergeRgb(_pixels[currentPixelIndex], _waveColor, x);
        }
    }

    // Finally, clear data
    delete [] samplePlotMin;
    delete [] samplePlotMax;
    delete [] samplePlotDeviation;
}

float GraphicsWavePainter::getValueX(float pos1, float value1, float pos2, float value2, float posX)
{
    // Condition: pos1 < pos2
    if (posX <= pos1)
        return value1;
    if (posX >= pos2)
        return value2;
    if (pos1 == pos2)
        return 0;//(value1 + value2) / 2;
    return ((posX - pos1) * value2 + (pos2 - posX) * value1) / (pos2 - pos1);
}

QRgb GraphicsWavePainter::mergeRgb(QRgb color1, QRgb color2, float x)
{
    if (x >= 1)
        return color2;
    return 0xFF000000 +
            ((static_cast<quint32>((1.0f - x) * qRed(color1) + x * qRed(color2)) & 0xFF) << 16) +
            ((static_cast<quint32>((1.0f - x) * qGreen(color1) + x * qGreen(color2)) & 0xFF) << 8) +
            (static_cast<quint32>((1.0f - x) * qBlue(color1) + x * qBlue(color2)) & 0xFF);
}

QPointF * GraphicsWavePainter::getDataAround(quint32 position, quint32 desiredLength, quint32 &pointNumber)
{
    // Limits
    quint32 leftIndex = position > desiredLength ? position - desiredLength : 0;
    quint32 rightIndex = position + desiredLength;
    if (rightIndex >= _sampleSize)
        rightIndex = _sampleSize - 1;

    if (rightIndex <= leftIndex)
        return nullptr;

    // Create data
    float coeff = -_zoomY * static_cast<float>(_widget->height()) / (32768 * 2);
    float offsetY = 0.5f * _widget->height();
    pointNumber = rightIndex - leftIndex + 1;
    QPointF * array = new QPointF[pointNumber];
    for (quint32 i = 0; i < pointNumber; i++)
    {
        array[i].setX(leftIndex + i);
        array[i].setY(static_cast<double>(coeff * _sampleData[leftIndex + i] + offsetY));
    }
    return array;
}
