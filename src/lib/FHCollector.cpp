/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * This file is part of the libfreehand project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <librevenge/librevenge.h>
#include "FHCollector.h"
#include "FHConstants.h"
#include "libfreehand_utils.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#ifndef DUMP_BINARY_OBJECTS
#define DUMP_BINARY_OBJECTS 0
#endif

#define FH_UNINITIALIZED(pI) \
  FH_ALMOST_ZERO(pI.m_minX) && FH_ALMOST_ZERO(pI.m_minY) && FH_ALMOST_ZERO(pI.m_maxY) && FH_ALMOST_ZERO(pI.m_maxX)

namespace
{

librevenge::RVNGString _getColorString(const libfreehand::FHRGBColor &color)
{
  ::librevenge::RVNGString colorString;
  colorString.sprintf("#%.2x%.2x%.2x", color.m_red >> 8, color.m_green >> 8, color.m_blue >> 8);
  return colorString;
}

static void _composePath(::librevenge::RVNGPropertyListVector &path, bool isClosed)
{
  bool firstPoint = true;
  bool wasMove = false;
  double initialX = 0.0;
  double initialY = 0.0;
  double previousX = 0.0;
  double previousY = 0.0;
  double x = 0.0;
  double y = 0.0;
  std::vector<librevenge::RVNGPropertyList> tmpPath;

  librevenge::RVNGPropertyListVector::Iter i(path);
  for (i.rewind(); i.next();)
  {
    if (!i()["librevenge:path-action"])
      continue;
    if (i()["svg:x"] && i()["svg:y"])
    {
      bool ignoreM = false;
      x = i()["svg:x"]->getDouble();
      y = i()["svg:y"]->getDouble();
      if (firstPoint)
      {
        initialX = x;
        initialY = y;
        firstPoint = false;
        wasMove = true;
      }
      else if (i()["librevenge:path-action"]->getStr() == "M")
      {
        if (FH_ALMOST_ZERO(previousX - x) && FH_ALMOST_ZERO(previousY - y))
          ignoreM = true;
        else
        {
          if (!tmpPath.empty())
          {
            if (!wasMove)
            {
              if ((FH_ALMOST_ZERO(initialX - previousX) && FH_ALMOST_ZERO(initialY - previousY)) || isClosed)
              {
                librevenge::RVNGPropertyList node;
                node.insert("librevenge:path-action", "Z");
                tmpPath.push_back(node);
              }
            }
            else
              tmpPath.pop_back();
          }
        }

        if (!ignoreM)
        {
          initialX = x;
          initialY = y;
          wasMove = true;
        }

      }
      else
        wasMove = false;

      if (!ignoreM)
      {
        tmpPath.push_back(i());
        previousX = x;
        previousY = y;
      }

    }
    else if (i()["librevenge:path-action"]->getStr() == "Z")
    {
      if (tmpPath.back()["librevenge:path-action"] && tmpPath.back()["librevenge:path-action"]->getStr() != "Z")
        tmpPath.push_back(i());
    }
  }
  if (!tmpPath.empty())
  {
    if (!wasMove)
    {
      if ((FH_ALMOST_ZERO(initialX - previousX) && FH_ALMOST_ZERO(initialY - previousY)) || isClosed)
      {
        if (tmpPath.back()["librevenge:path-action"] && tmpPath.back()["librevenge:path-action"]->getStr() != "Z")
        {
          librevenge::RVNGPropertyList closedPath;
          closedPath.insert("librevenge:path-action", "Z");
          tmpPath.push_back(closedPath);
        }
      }
    }
    else
      tmpPath.pop_back();
  }
  if (!tmpPath.empty())
  {
    path.clear();
    for (std::vector<librevenge::RVNGPropertyList>::const_iterator iter = tmpPath.begin(); iter != tmpPath.end(); ++iter)
      path.append(*iter);
  }
}

}

libfreehand::FHCollector::FHCollector() :
  m_pageInfo(), m_fhTail(), m_block(), m_transforms(), m_paths(), m_strings(), m_names(), m_lists(),
  m_layers(), m_groups(), m_currentTransforms(), m_compositePaths(), m_fonts(), m_paragraphs(), m_textBloks(),
  m_textObjects(), m_charProperties(), m_rgbColors(), m_basicFills(), m_propertyLists(), m_displayTexts(),
  m_graphicStyles(), m_attributeHolders(), m_data(), m_dataLists(), m_images(), m_multiColorLists(),
  m_linearFills(), m_tints(), m_lensFills(), m_strokeId(0), m_fillId(0), m_contentId(0)
{
}

libfreehand::FHCollector::~FHCollector()
{
}

void libfreehand::FHCollector::collectPageInfo(const FHPageInfo &pageInfo)
{
  m_pageInfo = pageInfo;
}

void libfreehand::FHCollector::collectString(unsigned recordId, const librevenge::RVNGString &str)
{
  m_strings[recordId] = str;
}

void libfreehand::FHCollector::collectName(unsigned recordId, const librevenge::RVNGString &name)
{
  m_names[name] = recordId;
  if (name == "stroke")
    m_strokeId = recordId;
  if (name == "fill")
    m_fillId = recordId;
  if (name == "content")
    m_contentId = recordId;
}

void libfreehand::FHCollector::collectPath(unsigned recordId, const libfreehand::FHPath &path)
{
  m_paths[recordId] = path;
}

void libfreehand::FHCollector::collectXform(unsigned recordId,
                                            double m11, double m21, double m12, double m22, double m13, double m23)
{
  m_transforms[recordId] = FHTransform(m11, m21, m12, m22, m13, m23);
}

void libfreehand::FHCollector::collectFHTail(unsigned /* recordId */, const FHTail &fhTail)
{
  m_fhTail = fhTail;
}

void libfreehand::FHCollector::collectBlock(unsigned recordId, const libfreehand::FHBlock &block)
{
  if (m_block.first && m_block.first != recordId)
  {
    FH_DEBUG_MSG(("FHCollector::collectBlock -- WARNING: Several \"Block\" records in the file\n"));
  }
  m_block = std::make_pair(recordId, block);
}

void libfreehand::FHCollector::collectList(unsigned recordId, const libfreehand::FHList &lst)
{
  m_lists[recordId] = lst;
}

void libfreehand::FHCollector::collectLayer(unsigned recordId, const libfreehand::FHLayer &layer)
{
  m_layers[recordId] = layer;
}

void libfreehand::FHCollector::collectGroup(unsigned recordId, const libfreehand::FHGroup &group)
{
  m_groups[recordId] = group;
}

void libfreehand::FHCollector::collectCompositePath(unsigned recordId, const libfreehand::FHCompositePath &compositePath)
{
  m_compositePaths[recordId] = compositePath;
}

void libfreehand::FHCollector::collectTString(unsigned recordId, const std::vector<unsigned> &elements)
{
  m_tStrings[recordId] = elements;
}

void libfreehand::FHCollector::collectAGDFont(unsigned recordId, const FHAGDFont &font)
{
  m_fonts[recordId] = font;
}

void libfreehand::FHCollector::collectParagraph(unsigned recordId, const FHParagraph &paragraph)
{
  m_paragraphs[recordId] = paragraph;
}

void libfreehand::FHCollector::collectTextBlok(unsigned recordId, const std::vector<unsigned short> &characters)
{
  m_textBloks[recordId]  = characters;
}

void libfreehand::FHCollector::collectTextObject(unsigned recordId, const FHTextObject &textObject)
{
  m_textObjects[recordId] = textObject;
}

void libfreehand::FHCollector::collectCharProps(unsigned recordId, const FHCharProperties &charProps)
{
  m_charProperties[recordId] = charProps;
}

void libfreehand::FHCollector::collectColor(unsigned recordId, const FHRGBColor &color)
{
  m_rgbColors[recordId] = color;
}

void libfreehand::FHCollector::collectTintColor(unsigned recordId, const FHTintColor &color)
{
  m_tints[recordId] = color;
}

void libfreehand::FHCollector::collectBasicFill(unsigned recordId, const FHBasicFill &fill)
{
  m_basicFills[recordId] = fill;
}

void libfreehand::FHCollector::collectBasicLine(unsigned recordId, const FHBasicLine &line)
{
  m_basicLines[recordId] = line;
}

void libfreehand::FHCollector::collectPropList(unsigned recordId, const FHPropList &propertyList)
{
  m_propertyLists[recordId] = propertyList;
}

void libfreehand::FHCollector::collectDisplayText(unsigned recordId, const FHDisplayText &displayText)
{
  m_displayTexts[recordId] = displayText;
}

void libfreehand::FHCollector::collectGraphicStyle(unsigned recordId, const FHGraphicStyle &graphicStyle)
{
  m_graphicStyles[recordId] = graphicStyle;
}

void libfreehand::FHCollector::collectAttributeHolder(unsigned recordId, const FHAttributeHolder &attributeHolder)
{
  m_attributeHolders[recordId] = attributeHolder;
}

void libfreehand::FHCollector::collectData(unsigned recordId, const ::librevenge::RVNGBinaryData &data)
{
  m_data[recordId] = data;
}

void libfreehand::FHCollector::collectDataList(unsigned recordId, const FHDataList &list)
{
  m_dataLists[recordId] = list;
}

void libfreehand::FHCollector::collectImage(unsigned recordId, const FHImageImport &image)
{
  m_images[recordId] = image;
}

void libfreehand::FHCollector::collectMultiColorList(unsigned recordId, const std::vector<FHColorStop> &colorStops)
{
  m_multiColorLists[recordId] = colorStops;
}

void libfreehand::FHCollector::collectLinearFill(unsigned recordId, const FHLinearFill &fill)
{
  m_linearFills[recordId] = fill;
}

void libfreehand::FHCollector::collectLensFill(unsigned recordId, const FHLensFill &fill)
{
  m_lensFills[recordId] = fill;
}

void libfreehand::FHCollector::_normalizePath(libfreehand::FHPath &path)
{
  FHTransform trafo(1.0, 0.0, 0.0, -1.0, - m_pageInfo.m_minX, m_pageInfo.m_maxY);
  path.transform(trafo);
}

void libfreehand::FHCollector::_normalizePoint(double &x, double &y)
{
  FHTransform trafo(1.0, 0.0, 0.0, -1.0, - m_pageInfo.m_minX, m_pageInfo.m_maxY);
  trafo.applyToPoint(x, y);
}

void libfreehand::FHCollector::_outputPath(const libfreehand::FHPath *path, ::librevenge::RVNGDrawingInterface *painter)
{
  if (!painter || !path || path->empty())
    return;

  FHPath fhPath(*path);
  librevenge::RVNGPropertyList propList;
  _appendStrokeProperties(propList, fhPath.getGraphicStyleId());
  _appendFillProperties(propList, fhPath.getGraphicStyleId());
  if (fhPath.getEvenOdd())
    propList.insert("svg:fill-rule", "evenodd");

  unsigned short xform = fhPath.getXFormId();

  if (xform)
  {
    const FHTransform *trafo = _findTransform(xform);
    if (trafo)
      fhPath.transform(*trafo);
  }
  std::stack<FHTransform> groupTransforms = m_currentTransforms;
  if (!m_currentTransforms.empty())
    fhPath.transform(m_currentTransforms.top());
  _normalizePath(fhPath);

  librevenge::RVNGPropertyListVector propVec;
  fhPath.writeOut(propVec);
  _composePath(propVec, fhPath.isClosed());
  librevenge::RVNGPropertyList pList;
  pList.insert("svg:d", propVec);
  painter->setStyle(propList);
  painter->drawPath(pList);
#ifdef DEBUG_BOUNDING_BOX
  {
    librevenge::RVNGPropertyList rectangleProps;
    rectangleProps.insert("draw:fill", "none");
    rectangleProps.insert("draw:stroke", "solid");
    painter->setStyle(rectangleProps);
    double xmin, ymin, xmax, ymax;
    fhPath.getBoundingBox(xmin, ymin, xmax, ymax);
    librevenge::RVNGPropertyList rectangleList;
    rectangleList.insert("svg:x", xmin);
    rectangleList.insert("svg:y", ymin);
    rectangleList.insert("svg:width", xmax - xmin);
    rectangleList.insert("svg:height", ymax - ymin);
    painter->drawRectangle(rectangleList);
  }
#endif
}

void libfreehand::FHCollector::_outputSomething(unsigned somethingId, ::librevenge::RVNGDrawingInterface *painter)
{
  if (!painter || !somethingId)
    return;

  _outputGroup(_findGroup(somethingId), painter);
  _outputPath(_findPath(somethingId), painter);
  _outputCompositePath(_findCompositePath(somethingId), painter);
  _outputTextObject(_findTextObject(somethingId), painter);
  _outputDisplayText(_findDisplayText(somethingId), painter);
  _outputImageImport(_findImageImport(somethingId), painter);
}

void libfreehand::FHCollector::_outputGroup(const libfreehand::FHGroup *group, ::librevenge::RVNGDrawingInterface *painter)
{
  if (!painter || !group)
    return;

  if (group->m_xFormId)
  {
    std::map<unsigned, FHTransform>::const_iterator iterTransform = m_transforms.find(group->m_xFormId);
    if (iterTransform != m_transforms.end())
      m_currentTransforms.push(iterTransform->second);
    else
      m_currentTransforms.push(libfreehand::FHTransform());
  }
  else
    m_currentTransforms.push(libfreehand::FHTransform());

  std::vector<unsigned> elements;
  if (!_findListElements(elements, group->m_elementsId))
  {
    FH_DEBUG_MSG(("ERROR: The pointed element list does not exist\n"));
    return;
  }

  if (!elements.empty())
  {
    painter->openGroup(::librevenge::RVNGPropertyList());
    for (std::vector<unsigned>::const_iterator iterVec = elements.begin(); iterVec != elements.end(); ++iterVec)
      _outputSomething(*iterVec, painter);
    painter->closeGroup();
  }

  if (!m_currentTransforms.empty())
    m_currentTransforms.pop();
}

void libfreehand::FHCollector::outputDrawing(::librevenge::RVNGDrawingInterface *painter)
{

#if DUMP_BINARY_OBJECTS
  for (std::map<unsigned, FHImageImport>::const_iterator iterImage = m_images.begin(); iterImage != m_images.end(); ++iterImage)
  {
    librevenge::RVNGBinaryData data = getImageData(iterImage->second.m_dataListId);
    librevenge::RVNGString filename;
    filename.sprintf("freehanddump%.4x.%s", iterImage->first, iterImage->second.m_format.empty() ? "bin" : iterImage->second.m_format.cstr());
    FILE *f = fopen(filename.cstr(), "wb");
    if (f)
    {
      const unsigned char *tmpBuffer = data.getDataBuffer();
      for (unsigned long k = 0; k < data.size(); k++)
        fprintf(f, "%c",tmpBuffer[k]);
      fclose(f);
    }
  }
#endif

  if (!painter)
    return;

  if (!m_fhTail.m_blockId || m_fhTail.m_blockId != m_block.first)
  {
    FH_DEBUG_MSG(("WARNING: FHTail points to an invalid Block ID\n"));
    m_fhTail.m_blockId = m_block.first;
  }
  if (!m_fhTail.m_blockId)
  {
    FH_DEBUG_MSG(("ERROR: Block record is absent from this file\n"));
    return;
  }

  if (FH_UNINITIALIZED(m_pageInfo))
    m_pageInfo = m_fhTail.m_pageInfo;

  painter->startDocument(librevenge::RVNGPropertyList());
  librevenge::RVNGPropertyList propList;
  propList.insert("svg:height", m_pageInfo.m_maxY - m_pageInfo.m_minY);
  propList.insert("svg:width", m_pageInfo.m_maxX - m_pageInfo.m_minX);
  painter->startPage(propList);

  unsigned layerListId = m_block.second.m_layerListId;

  std::vector<unsigned> elements;
  if (_findListElements(elements, layerListId))
  {
    for (std::vector<unsigned>::const_iterator iter = elements.begin(); iter != elements.end(); ++iter)
    {
      _outputLayer(*iter, painter);
    }
  }
  painter->endPage();
  painter->endDocument();
}

void libfreehand::FHCollector::_outputLayer(unsigned layerId, ::librevenge::RVNGDrawingInterface *painter)
{
  if (!painter)
    return;

  std::map<unsigned, FHLayer>::const_iterator layerIter = m_layers.find(layerId);
  if (layerIter == m_layers.end())
  {
    FH_DEBUG_MSG(("ERROR: Could not find the referenced layer\n"));
    return;
  }

  if (layerIter->second.m_visibility != 3)
    return;

  unsigned layerElementsListId = layerIter->second.m_elementsId;
  if (!layerElementsListId)
  {
    FH_DEBUG_MSG(("ERROR: Layer points to invalid element list\n"));
    return;
  }

  std::vector<unsigned> elements;
  if (!_findListElements(elements, layerElementsListId))
  {
    FH_DEBUG_MSG(("ERROR: The pointed element list does not exist\n"));
    return;
  }

  for (std::vector<unsigned>::const_iterator iterVec = elements.begin(); iterVec != elements.end(); ++iterVec)
    _outputSomething(*iterVec, painter);
}

void libfreehand::FHCollector::_outputCompositePath(const libfreehand::FHCompositePath *compositePath, ::librevenge::RVNGDrawingInterface *painter)
{
  if (!painter || !compositePath)
    return;

  std::vector<unsigned> elements;
  if (_findListElements(elements, compositePath->m_elementsId))
  {
    for (std::vector<unsigned>::const_iterator iter = elements.begin(); iter != elements.end(); ++iter)
    {
      const libfreehand::FHPath *path = _findPath(*iter);
      if (path)
      {
        libfreehand::FHPath fhPath(*path);
        if (!fhPath.getGraphicStyleId())
          fhPath.setGraphicStyleId(compositePath->m_graphicStyleId);
        _outputPath(&fhPath, painter);
      }
    }
  }
}

void libfreehand::FHCollector::_outputTextObject(const libfreehand::FHTextObject *textObject, ::librevenge::RVNGDrawingInterface *painter)
{
  if (!painter || !textObject)
    return;

  double xa = textObject->m_startX;
  double ya = textObject->m_startY;
  double xb = textObject->m_startX + textObject->m_width;
  double yb = textObject->m_startY + textObject->m_height;
  double xc = xa;
  double yc = yb;
  unsigned xFormId = textObject->m_xFormId;
  if (xFormId)
  {
    const FHTransform *trafo = _findTransform(xFormId);
    if (trafo)
    {
      trafo->applyToPoint(xa, ya);
      trafo->applyToPoint(xb, yb);
      trafo->applyToPoint(xc, yc);
    }
  }
  if (!m_currentTransforms.empty())
  {
    m_currentTransforms.top().applyToPoint(xa, ya);
    m_currentTransforms.top().applyToPoint(xb, yb);
    m_currentTransforms.top().applyToPoint(xc, yc);
  }
  _normalizePoint(xa, ya);
  _normalizePoint(xb, yb);
  _normalizePoint(xc, yc);
  double rotation = atan2(yb-yc, xb-xc);
  double height = sqrt((xc-xa)*(xc-xa) + (yc-ya)*(yc-ya));
  double width = sqrt((xc-xb)*(xc-xb) + (yc-yb)*(yc-yb));
  double xmid = (xa + xb) / 2.0;
  double ymid = (ya + yb) / 2.0;

  ::librevenge::RVNGPropertyList textObjectProps;
  textObjectProps.insert("svg:x", xmid - textObject->m_width / 2.0);
  textObjectProps.insert("svg:y", ymid + textObject->m_height / 2.0);
  textObjectProps.insert("svg:height", height);
  textObjectProps.insert("svg:width", width);
  if (!FH_ALMOST_ZERO(rotation))
    textObjectProps.insert("librevenge:rotate", rotation * 180.0 / M_PI);
  painter->startTextObject(textObjectProps);

  const std::vector<unsigned> *elements = _findTStringElements(textObject->m_tStringId);
  if (elements && !elements->empty())
  {
    for (std::vector<unsigned>::const_iterator iter = elements->begin(); iter != elements->end(); ++iter)
      _outputParagraph(_findParagraph(*iter), painter);
  }

  painter->endTextObject();
}

void libfreehand::FHCollector::_outputParagraph(const libfreehand::FHParagraph *paragraph, ::librevenge::RVNGDrawingInterface *painter)
{
  if (!painter || !paragraph)
    return;
  librevenge::RVNGPropertyList propList;
  _appendParagraphProperties(propList, paragraph->m_paraStyleId);
  painter->openParagraph(propList);

  std::map<unsigned, std::vector<unsigned short> >::const_iterator iter = m_textBloks.find(paragraph->m_textBlokId);
  if (iter != m_textBloks.end())
  {

    for (std::vector<std::pair<unsigned, unsigned> >::size_type i = 0; i < paragraph->m_charStyleIds.size(); ++i)
    {
      _outputTextRun(&(iter->second), paragraph->m_charStyleIds[i].first,
                     (i+1 < paragraph->m_charStyleIds.size() ? paragraph->m_charStyleIds[i+1].first : iter->second.size()) - paragraph->m_charStyleIds[i].first,
                     paragraph->m_charStyleIds[i].second, painter);
    }

  }

  painter->closeParagraph();
}

void libfreehand::FHCollector::_appendCharacterProperties(::librevenge::RVNGPropertyList &propList, unsigned charPropsId)
{
  std::map<unsigned, FHCharProperties>::const_iterator iter = m_charProperties.find(charPropsId);
  if (iter == m_charProperties.end())
    return;
  const FHCharProperties &charProps = iter->second;
  if (charProps.m_fontNameId)
  {
    std::map<unsigned, ::librevenge::RVNGString>::const_iterator iterString = m_strings.find(charProps.m_fontNameId);
    if (iterString != m_strings.end())
      propList.insert("fo:font-name", iterString->second);
  }
  propList.insert("fo:font-size", charProps.m_fontSize, librevenge::RVNG_POINT);
  if (charProps.m_fontId)
    _appendFontProperties(propList, charProps.m_fontId);
  if (charProps.m_textColorId)
  {
    std::map<unsigned, FHBasicFill>::const_iterator iterBasicFill = m_basicFills.find(charProps.m_textColorId);
    if (iterBasicFill != m_basicFills.end() && iterBasicFill->second.m_colorId)
    {
      librevenge::RVNGString color = getColorString(iterBasicFill->second.m_colorId);
      if (!color.empty())
        propList.insert("fo:color", color);
    }
  }
  propList.insert("style:text-scale", charProps.m_horizontalScale, librevenge::RVNG_PERCENT);
}

void libfreehand::FHCollector::_appendCharacterProperties(::librevenge::RVNGPropertyList &propList, const FH3CharProperties &charProps)
{
  if (charProps.m_fontNameId)
  {
    std::map<unsigned, ::librevenge::RVNGString>::const_iterator iterString = m_strings.find(charProps.m_fontNameId);
    if (iterString != m_strings.end())
      propList.insert("fo:font-name", iterString->second);
  }
  propList.insert("fo:font-size", charProps.m_fontSize, librevenge::RVNG_POINT);
  if (charProps.m_fontColorId)
  {
    librevenge::RVNGString color = getColorString(charProps.m_fontColorId);
    if (!color.empty())
      propList.insert("fo:color", color);
  }
  if (charProps.m_fontStyle & 1)
    propList.insert("fo:font-weight", "bold");
  if (charProps.m_fontStyle & 2)
    propList.insert("fo:font-style", "italic");
}

void libfreehand::FHCollector::_appendParagraphProperties(::librevenge::RVNGPropertyList & /* propList */, const FH3ParaProperties & /* paraProps */)
{
}

void libfreehand::FHCollector::_appendParagraphProperties(::librevenge::RVNGPropertyList & /* propList */, unsigned /* paraPropsId */)
{
}

void libfreehand::FHCollector::_outputDisplayText(const libfreehand::FHDisplayText *displayText, ::librevenge::RVNGDrawingInterface *painter)
{
  if (!painter || !displayText)
    return;

  double xa = displayText->m_startX;
  double ya = displayText->m_startY;
  double xb = displayText->m_startX + displayText->m_width;
  double yb = displayText->m_startY + displayText->m_height;
  double xc = xa;
  double yc = yb;
  unsigned xFormId = displayText->m_xFormId;
  if (xFormId)
  {
    const FHTransform *trafo = _findTransform(xFormId);
    if (trafo)
    {
      trafo->applyToPoint(xa, ya);
      trafo->applyToPoint(xb, yb);
      trafo->applyToPoint(xc, yc);
    }
  }
  std::stack<FHTransform> groupTransforms = m_currentTransforms;
  if (!m_currentTransforms.empty())
  {
    m_currentTransforms.top().applyToPoint(xa, ya);
    m_currentTransforms.top().applyToPoint(xb, yb);
    m_currentTransforms.top().applyToPoint(xc, yc);
  }
  _normalizePoint(xa, ya);
  _normalizePoint(xb, yb);
  _normalizePoint(xc, yc);
  double rotation = atan2(yb-yc, xb-xc);
  double height = sqrt((xc-xa)*(xc-xa) + (yc-ya)*(yc-ya));
  double width = sqrt((xc-xb)*(xc-xb) + (yc-yb)*(yc-yb));
  double xmid = (xa + xb) / 2.0;
  double ymid = (ya + yb) / 2.0;

  ::librevenge::RVNGPropertyList textObjectProps;
  textObjectProps.insert("svg:x", xmid - displayText->m_width / 2.0);
  textObjectProps.insert("svg:y", ymid + displayText->m_height / 2.0);
  textObjectProps.insert("svg:height", height);
  textObjectProps.insert("svg:width", width);
  if (!FH_ALMOST_ZERO(rotation))
    textObjectProps.insert("librevenge:rotate", rotation * 180.0 / M_PI);

  painter->startTextObject(textObjectProps);

  std::vector<FH3ParaProperties>::const_iterator iterPara = displayText->m_paraProps.begin();
  std::vector<FH3CharProperties>::const_iterator iterChar = displayText->m_charProps.begin();

  FH3ParaProperties paraProps = *iterPara++;
  FH3CharProperties charProps = *iterChar++;
  librevenge::RVNGString text;
  std::vector<unsigned char>::size_type i = 0;

  librevenge::RVNGPropertyList paraPropList;
  _appendParagraphProperties(paraPropList, paraProps);
  painter->openParagraph(paraPropList);
  bool isParagraphOpened = true;

  librevenge::RVNGPropertyList charPropList;
  _appendCharacterProperties(charPropList, charProps);
  painter->openSpan(charPropList);
  bool isSpanOpened = true;

  while (i < displayText->m_characters.size())
  {
    _appendMacRoman(text, displayText->m_characters[i++]);
    if (i > paraProps.m_offset)
    {
      if (!text.empty())
        painter->insertText(text);
      text.clear();
      if (isParagraphOpened)
      {
        if (isSpanOpened)
        {
          painter->closeSpan();
          isSpanOpened = false;
        }
        painter->closeParagraph();
        isParagraphOpened = false;
      }
      if (iterPara != displayText->m_paraProps.end())
      {
        paraProps = *iterPara++;
      }
    }
    if (i > charProps.m_offset)
    {
      if (!text.empty())
        painter->insertText(text);
      text.clear();
      if (isSpanOpened)
      {
        painter->closeSpan();
        isSpanOpened = false;
      }
      if (iterChar != displayText->m_charProps.end())
      {
        charProps = *iterChar++;
      }
    }
    if (i >= displayText->m_characters.size())
      break;
    if (!isParagraphOpened)
    {
      paraPropList.clear();
      _appendParagraphProperties(paraPropList, paraProps);
      painter->openParagraph(paraPropList);
      isParagraphOpened = true;
      if (!isSpanOpened)
      {
        charPropList.clear();
        _appendCharacterProperties(charPropList, charProps);
        painter->openSpan(charPropList);
        isSpanOpened = true;
      }
    }
    if (!isSpanOpened)
    {
      charPropList.clear();
      _appendCharacterProperties(charPropList, charProps);
      painter->openSpan(charPropList);
      isSpanOpened = true;
    }
  }
  if (!text.empty())
    painter->insertText(text);
  if (isSpanOpened)
    painter->closeSpan();
  if (isParagraphOpened)
    painter->closeParagraph();

  painter->endTextObject();
}

void libfreehand::FHCollector::_outputImageImport(const FHImageImport *image, ::librevenge::RVNGDrawingInterface *painter)
{
  if (!painter || !image)
    return;

  double xa = image->m_startX;
  double ya = image->m_startY;
  double xb = image->m_startX + image->m_width;
  double yb = image->m_startY + image->m_height;
  double xc = xa;
  double yc = yb;
  unsigned xFormId = image->m_xFormId;
  if (xFormId)
  {
    const FHTransform *trafo = _findTransform(xFormId);
    if (trafo)
    {
      trafo->applyToPoint(xa, ya);
      trafo->applyToPoint(xb, yb);
      trafo->applyToPoint(xc, yc);
    }
  }
  std::stack<FHTransform> groupTransforms = m_currentTransforms;
  if (!m_currentTransforms.empty())
  {
    m_currentTransforms.top().applyToPoint(xa, ya);
    m_currentTransforms.top().applyToPoint(xb, yb);
    m_currentTransforms.top().applyToPoint(xc, yc);
  }
  _normalizePoint(xa, ya);
  _normalizePoint(xb, yb);
  _normalizePoint(xc, yc);
  double rotation = atan2(yb-yc, xb-xc);
  double height = sqrt((xc-xa)*(xc-xa) + (yc-ya)*(yc-ya));
  double width = sqrt((xc-xb)*(xc-xb) + (yc-yb)*(yc-yb));
  double xmid = (xa + xb) / 2.0;
  double ymid = (ya + yb) / 2.0;

  ::librevenge::RVNGPropertyList imageProps;
  imageProps.insert("svg:x", xmid - width / 2.0);
  imageProps.insert("svg:y", ymid - height / 2.0);
  imageProps.insert("svg:height", height);
  imageProps.insert("svg:width", width);
  if (!FH_ALMOST_ZERO(rotation))
    imageProps.insert("librevenge:rotate", rotation * 180.0 / M_PI);

  imageProps.insert("librevenge:mime-type", "whatever");
  librevenge::RVNGBinaryData data = getImageData(image->m_dataListId);

  if (data.empty())
    return;
  imageProps.insert("office:binary-data", data);
  painter->drawGraphicObject(imageProps);
}

void libfreehand::FHCollector::_outputTextRun(const std::vector<unsigned short> *characters, unsigned offset, unsigned length,
                                              unsigned charStyleId, ::librevenge::RVNGDrawingInterface *painter)
{
  if (!painter || !characters || characters->empty())
    return;
  librevenge::RVNGPropertyList propList;
  _appendCharacterProperties(propList, charStyleId);
  painter->openSpan(propList);
  std::vector<unsigned short> tmpChars;
  for (unsigned i = offset; i < length+offset && i < characters->size(); ++i)
    tmpChars.push_back((*characters)[i]);
  if (!tmpChars.empty())
  {
    librevenge::RVNGString text;
    _appendUTF16(text, tmpChars);
    painter->insertText(text);
  }
  painter->closeSpan();
}

bool libfreehand::FHCollector::_findListElements(std::vector<unsigned> &elements, unsigned id)
{
  std::map<unsigned, FHList>::const_iterator iter = m_lists.find(id);
  if (iter == m_lists.end())
    return false;
  elements = iter->second.m_elements;
  return true;
}


void libfreehand::FHCollector::_appendFontProperties(::librevenge::RVNGPropertyList &propList, unsigned agdFontId)
{
  std::map<unsigned, FHAGDFont>::const_iterator iter = m_fonts.find(agdFontId);
  if (iter == m_fonts.end())
    return;
  const FHAGDFont &font = iter->second;
  if (font.m_fontNameId)
  {
    std::map<unsigned, ::librevenge::RVNGString>::const_iterator iterString = m_strings.find(font.m_fontNameId);
    if (iterString != m_strings.end())
      propList.insert("fo:font-name", iterString->second);
  }
  propList.insert("fo:font-size", font.m_fontSize, librevenge::RVNG_POINT);
  if (font.m_fontStyle & 1)
    propList.insert("fo:font-weight", "bold");
  if (font.m_fontStyle & 2)
    propList.insert("fo:font-style", "italic");
}

void libfreehand::FHCollector::_appendFillProperties(::librevenge::RVNGPropertyList &propList, unsigned graphicStyleId)
{
  if (!graphicStyleId)
  {
    propList.insert("draw:fill", "none");
  }
  else
  {
    const FHPropList *propertyList = _findPropList(graphicStyleId);
    if (!propertyList)
    {
      const FHGraphicStyle *graphicStyle = _findGraphicStyle(graphicStyleId);
      if (!graphicStyle)
        _appendFillProperties(propList, 0);
      else
      {
        if (graphicStyle->m_parentId)
          _appendFillProperties(propList, graphicStyle->m_parentId);
        unsigned fillId = _findFillId(*graphicStyle);;
        if (fillId)
        {
          _appendBasicFill(propList, _findBasicFill(fillId));
          _appendLinearFill(propList, _findLinearFill(fillId));
          _appendLensFill(propList, _findLensFill(fillId));
        }
        else
          _appendFillProperties(propList, 0);
      }
    }
    else
    {
      if (propertyList->m_parentId)
        _appendFillProperties(propList, propertyList->m_parentId);
      std::map<unsigned, unsigned>::const_iterator iter = propertyList->m_elements.find(m_fillId);
      if (iter != propertyList->m_elements.end())
      {
        _appendBasicFill(propList, _findBasicFill(iter->second));
        _appendLinearFill(propList, _findLinearFill(iter->second));
        _appendLensFill(propList, _findLensFill(iter->second));
      }
    }
  }
}

void libfreehand::FHCollector::_appendStrokeProperties(::librevenge::RVNGPropertyList &propList, unsigned graphicStyleId)
{
  if (!graphicStyleId)
  {
    propList.insert("draw:stroke", "solid");
    propList.insert("svg:stroke-width", 0.0);
    propList.insert("svg:stroke-color", "#000000");
  }
  else
  {
    const FHPropList *propertyList = _findPropList(graphicStyleId);
    if (!propertyList)
    {
      const FHGraphicStyle *graphicStyle = _findGraphicStyle(graphicStyleId);
      if (!graphicStyle)
        _appendStrokeProperties(propList, 0);
      else
      {
        if (graphicStyle->m_parentId)
          _appendStrokeProperties(propList, graphicStyle->m_parentId);
        unsigned strokeId = _findStrokeId(*graphicStyle);
        if (strokeId)
          _appendBasicLine(propList, _findBasicLine(strokeId));
        else
          _appendStrokeProperties(propList, 0);
      }
    }
    else
    {
      if (propertyList->m_parentId)
        _appendStrokeProperties(propList, propertyList->m_parentId);
      std::map<unsigned, unsigned>::const_iterator iter = propertyList->m_elements.find(m_strokeId);
      if (iter != propertyList->m_elements.end())
      {
        _appendBasicLine(propList, _findBasicLine(iter->second));
      }
    }
  }
}

void libfreehand::FHCollector::_appendBasicFill(::librevenge::RVNGPropertyList &propList, const libfreehand::FHBasicFill *basicFill)
{
  if (!basicFill)
    return;
  propList.insert("draw:fill", "solid");
  librevenge::RVNGString color = getColorString(basicFill->m_colorId);
  if (!color.empty())
    propList.insert("draw:fill-color", color);
  else
    propList.insert("draw:fill-color", "#000000");
}

void libfreehand::FHCollector::_appendLinearFill(::librevenge::RVNGPropertyList &propList, const libfreehand::FHLinearFill *linearFill)
{
  if (!linearFill)
    return;
  propList.insert("draw:fill", "gradient");
  propList.insert("draw:style", "linear");
  propList.insert("draw:angle", 90.0 - linearFill->m_angle, librevenge::RVNG_GENERIC);
  if (linearFill->m_multiColorListId)
  {
    const std::vector<FHColorStop> *multiColorList = _findMultiColorList(linearFill->m_multiColorListId);
    if (multiColorList && multiColorList->size() > 1)
    {
      librevenge::RVNGString color = getColorString((*multiColorList)[0].m_colorId);
      if (!color.empty())
        propList.insert("draw:start-color", color);
      color = getColorString((*multiColorList)[1].m_colorId);
      if (!color.empty())
        propList.insert("draw:end-color", color);
    }
    else
    {
      librevenge::RVNGString color = getColorString(linearFill->m_color1Id);
      if (!color.empty())
        propList.insert("draw:start-color", color);
      color = getColorString(linearFill->m_color2Id);
      if (!color.empty())
        propList.insert("draw:end-color", color);
    }
  }
  else
  {
    librevenge::RVNGString color = getColorString(linearFill->m_color1Id);
    if (!color.empty())
      propList.insert("draw:start-color", color);
    color = getColorString(linearFill->m_color2Id);
    if (!color.empty())
      propList.insert("draw:end-color", color);
  }
}

void libfreehand::FHCollector::_appendLensFill(::librevenge::RVNGPropertyList &propList, const libfreehand::FHLensFill *lensFill)
{
  if (!lensFill)
    return;

  if (lensFill->m_colorId)
  {
    propList.insert("draw:fill", "solid");
    librevenge::RVNGString color = getColorString(lensFill->m_colorId);
    if (!color.empty())
      propList.insert("draw:fill-color", color);
    else
      propList.insert("draw:fill", "none");
  }
  else
    propList.insert("draw:fill", "none");

  switch (lensFill->m_mode)
  {
  case FH_LENSFILL_MODE_TRANSPARENCY:
    propList.insert("draw:opacity", lensFill->m_value / 100.0, librevenge::RVNG_PERCENT);
    break;
  case FH_LENSFILL_MODE_MONOCHROME:
    propList.insert("draw:fill", "none");
    propList.insert("draw:color-mode", "greyscale");
    break;
  case FH_LENSFILL_MODE_MAGNIFY:
    propList.insert("draw:fill", "none");
    break;
  case FH_LENSFILL_MODE_LIGHTEN: // emulate by semi-transparent white fill
    propList.insert("draw:fill", "solid");
    propList.insert("draw:fill-color", "#FFFFFF");
    propList.insert("draw:opacity", lensFill->m_value / 100.0, librevenge::RVNG_PERCENT);
    break;
  case FH_LENSFILL_MODE_DARKEN: // emulate by semi-transparent black fill
    propList.insert("draw:fill", "solid");
    propList.insert("draw:fill-color", "#000000");
    propList.insert("draw:opacity", lensFill->m_value / 100.0, librevenge::RVNG_PERCENT);
    break;
  case FH_LENSFILL_MODE_INVERT:
    propList.insert("draw:fill", "none");
    break;
  default:
    break;
  }
}

void libfreehand::FHCollector::_appendBasicLine(::librevenge::RVNGPropertyList &propList, const libfreehand::FHBasicLine *basicLine)
{
  if (!basicLine)
    return;
  propList.insert("draw:stroke", "solid");
  librevenge::RVNGString color = getColorString(basicLine->m_colorId);
  if (!color.empty())
    propList.insert("svg:stroke-color", color);
  else
    propList.insert("svg:stroke-color", "#000000");
  propList.insert("svg:stroke-width", basicLine->m_width);
}

const libfreehand::FHPath *libfreehand::FHCollector::_findPath(unsigned id)
{
  std::map<unsigned, FHPath>::const_iterator iter = m_paths.find(id);
  if (iter != m_paths.end())
    return &(iter->second);
  return 0;
}

const libfreehand::FHGroup *libfreehand::FHCollector::_findGroup(unsigned id)
{
  std::map<unsigned, FHGroup>::const_iterator iter = m_groups.find(id);
  if (iter != m_groups.end())
    return &(iter->second);
  return 0;
}

const libfreehand::FHCompositePath *libfreehand::FHCollector::_findCompositePath(unsigned id)
{
  std::map<unsigned, FHCompositePath>::const_iterator iter = m_compositePaths.find(id);
  if (iter != m_compositePaths.end())
    return &(iter->second);
  return 0;
}

const libfreehand::FHTextObject *libfreehand::FHCollector::_findTextObject(unsigned id)
{
  std::map<unsigned, FHTextObject>::const_iterator iter = m_textObjects.find(id);
  if (iter != m_textObjects.end())
    return &(iter->second);
  return 0;
}

const libfreehand::FHTransform *libfreehand::FHCollector::_findTransform(unsigned id)
{
  std::map<unsigned, FHTransform>::const_iterator iter = m_transforms.find(id);
  if (iter != m_transforms.end())
    return &(iter->second);
  return 0;
}

const libfreehand::FHParagraph *libfreehand::FHCollector::_findParagraph(unsigned id)
{
  std::map<unsigned, FHParagraph>::const_iterator iter = m_paragraphs.find(id);
  if (iter != m_paragraphs.end())
    return &(iter->second);
  return 0;
}

const std::vector<unsigned> *libfreehand::FHCollector::_findTStringElements(unsigned id)
{
  std::map<unsigned, std::vector<unsigned> >::const_iterator iter = m_tStrings.find(id);
  if (iter != m_tStrings.end())
    return &(iter->second);
  return 0;
}

const libfreehand::FHPropList *libfreehand::FHCollector::_findPropList(unsigned id)
{
  std::map<unsigned, FHPropList>::const_iterator iter = m_propertyLists.find(id);
  if (iter != m_propertyLists.end())
    return &(iter->second);
  return 0;
}

const libfreehand::FHGraphicStyle *libfreehand::FHCollector::_findGraphicStyle(unsigned id)
{
  std::map<unsigned, FHGraphicStyle>::const_iterator iter = m_graphicStyles.find(id);
  if (iter != m_graphicStyles.end())
    return &(iter->second);
  return 0;
}

const libfreehand::FHBasicFill *libfreehand::FHCollector::_findBasicFill(unsigned id)
{
  std::map<unsigned, FHBasicFill>::const_iterator iter = m_basicFills.find(id);
  if (iter != m_basicFills.end())
    return &(iter->second);
  return 0;
}

const libfreehand::FHLinearFill *libfreehand::FHCollector::_findLinearFill(unsigned id)
{
  std::map<unsigned, FHLinearFill>::const_iterator iter = m_linearFills.find(id);
  if (iter != m_linearFills.end())
    return &(iter->second);
  return 0;
}

const libfreehand::FHLensFill *libfreehand::FHCollector::_findLensFill(unsigned id)
{
  std::map<unsigned, FHLensFill>::const_iterator iter = m_lensFills.find(id);
  if (iter != m_lensFills.end())
    return &(iter->second);
  return 0;
}

const libfreehand::FHBasicLine *libfreehand::FHCollector::_findBasicLine(unsigned id)
{
  std::map<unsigned, FHBasicLine>::const_iterator iter = m_basicLines.find(id);
  if (iter != m_basicLines.end())
    return &(iter->second);
  return 0;
}

const libfreehand::FHRGBColor *libfreehand::FHCollector::_findRGBColor(unsigned id)
{
  std::map<unsigned, FHRGBColor>::const_iterator iter = m_rgbColors.find(id);
  if (iter != m_rgbColors.end())
    return &(iter->second);
  return 0;
}

const libfreehand::FHDisplayText *libfreehand::FHCollector::_findDisplayText(unsigned id)
{
  std::map<unsigned, FHDisplayText>::const_iterator iter = m_displayTexts.find(id);
  if (iter != m_displayTexts.end())
    return &(iter->second);
  return 0;
}

const libfreehand::FHImageImport *libfreehand::FHCollector::_findImageImport(unsigned id)
{
  std::map<unsigned, FHImageImport>::const_iterator iter = m_images.find(id);
  if (iter != m_images.end())
    return &(iter->second);
  return 0;
}

const ::librevenge::RVNGBinaryData *libfreehand::FHCollector::_findData(unsigned id)
{
  std::map<unsigned, ::librevenge::RVNGBinaryData>::const_iterator iter = m_data.find(id);
  if (iter != m_data.end())
    return &(iter->second);
  return 0;
}

const std::vector<libfreehand::FHColorStop> *libfreehand::FHCollector::_findMultiColorList(unsigned id)
{
  std::map<unsigned, std::vector<libfreehand::FHColorStop> >::const_iterator iter = m_multiColorLists.find(id);
  if (iter != m_multiColorLists.end())
    return &(iter->second);
  return 0;
}


unsigned libfreehand::FHCollector::_findStrokeId(const libfreehand::FHGraphicStyle &graphicStyle)
{
  unsigned listId = graphicStyle.m_attrId;
  if (!listId)
    return 0;
  std::map<unsigned, FHList>::const_iterator iter = m_lists.find(listId);
  if (iter == m_lists.end())
    return 0;
  unsigned strokeId = 0;
  for (unsigned i = 0; i < iter->second.m_elements.size(); ++i)
  {
    unsigned valueId = _findValueFromAttribute(iter->second.m_elements[i]);
    if (_findBasicLine(valueId))
      strokeId = valueId;
  }
  return strokeId;
}

unsigned libfreehand::FHCollector::_findFillId(const libfreehand::FHGraphicStyle &graphicStyle)
{
  unsigned listId = graphicStyle.m_attrId;
  if (!listId)
    return 0;
  std::map<unsigned, FHList>::const_iterator iter = m_lists.find(listId);
  if (iter == m_lists.end())
    return 0;
  unsigned fillId = 0;
  for (unsigned i = 0; i < iter->second.m_elements.size(); ++i)
  {
    unsigned valueId = _findValueFromAttribute(iter->second.m_elements[i]);
    // Add other fills if we support them
    if (_findBasicFill(valueId) || _findLinearFill(valueId) || _findLensFill(valueId))
      fillId = valueId;
  }
  return fillId;
}

unsigned libfreehand::FHCollector::_findValueFromAttribute(unsigned id)
{
  if (!id)
    return 0;
  std::map<unsigned, FHAttributeHolder>::const_iterator iter = m_attributeHolders.find(id);
  if (iter == m_attributeHolders.end())
    return 0;
  unsigned value = 0;
  if (iter->second.m_parentId)
    value = _findValueFromAttribute(iter->second.m_parentId);
  if (iter->second.m_attrId)
    value = iter->second.m_attrId;
  return value;
}

::librevenge::RVNGBinaryData libfreehand::FHCollector::getImageData(unsigned id)
{
  std::map<unsigned, FHDataList>::const_iterator iter = m_dataLists.find(id);
  ::librevenge::RVNGBinaryData data;
  if (iter == m_dataLists.end())
    return data;
  for (unsigned i = 0; i < iter->second.m_elements.size(); ++i)
  {
    const ::librevenge::RVNGBinaryData *pData = _findData(iter->second.m_elements[i]);
    if (pData)
      data.append(*pData);
  }
  return data;
}

::librevenge::RVNGString libfreehand::FHCollector::getColorString(unsigned id)
{
  const FHRGBColor *color = _findRGBColor(id);
  if (color)
    return _getColorString(*color);
  std::map<unsigned, FHTintColor>::const_iterator iterTint = m_tints.find(id);
  if (iterTint != m_tints.end())
    return getRGBFromTint(iterTint->second);
  return ::librevenge::RVNGString();
}

::librevenge::RVNGString libfreehand::FHCollector::getRGBFromTint(const FHTintColor &tint)
{
  if (!tint.m_baseColorId)
    return librevenge::RVNGString();
  const FHRGBColor *rgbColor = _findRGBColor(tint.m_baseColorId);
  if (rgbColor)
  {
    unsigned red = rgbColor->m_red * tint.m_tint + (65536 - tint.m_tint) * 65536;
    unsigned green = rgbColor->m_green * tint.m_tint + (65536 - tint.m_tint) * 65536;
    unsigned blue = rgbColor->m_blue * tint.m_tint + (65536 - tint.m_tint) * 65536;
    FHRGBColor color;
    color.m_red = (red >> 16);
    color.m_green = (green >> 16);
    color.m_blue = (blue >> 16);
    return _getColorString(color);
  }
  return getColorString(tint.m_baseColorId);
}

/* vim:set shiftwidth=2 softtabstop=2 expandtab: */
