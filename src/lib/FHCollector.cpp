/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * This file is part of the libfreehand project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <libwpg/libwpg.h>
#include "FHCollector.h"

libfreehand::FHCollector::FHCollector(libwpg::WPGPaintInterface *painter) :
  m_painter(painter)
{
  m_painter->startGraphics(WPXPropertyList());
}

libfreehand::FHCollector::~FHCollector()
{
  m_painter->endGraphics();
}

void libfreehand::FHCollector::collectUString(unsigned /* recordId */, const std::vector<unsigned short> & /* ustr */)
{
}

void libfreehand::FHCollector::collectMName(unsigned /* recordId */, const WPXString & /* name */)
{
}

void libfreehand::FHCollector::collectPath(unsigned short /* graphicStyle */, const std::vector<std::vector<std::pair<double, double> > > &path)
{
  if (path.empty())
    return;
  WPXPropertyListVector propVec;
  WPXPropertyList propList;
  propList.insert("draw:fill", "none");
  propList.insert("draw:stroke", "solid");
  propList.insert("svg:stroke-width", 0.0);
  propList.insert("svg:stroke-color", "#000000");
  m_painter->setStyle(propList, WPXPropertyListVector());

  propList.clear();
  propList.insert("libwpg:path-action", "M");
  propList.insert("svg:x", path[0][0].first / 72.0);
  propList.insert("svg:y", (1000.0 - path[0][0].second) / 72.0);
  propVec.append(propList);
  propList.clear();
  for (unsigned i = 0; i<path.size()-1; ++i)
  {
    propList.insert("libwpg:path-action", "C");
    propList.insert("svg:x1", path[i][2].first / 72.0);
    propList.insert("svg:y1", (1000.0 - path[i][2].second) / 72.0);
    propList.insert("svg:x2", path[i+1][1].first / 72.0);
    propList.insert("svg:y2", (1000.0 - path[i+1][1].second) / 72.0);
    propList.insert("svg:x", path[i+1][0].first / 72.0);
    propList.insert("svg:y", (1000.0 - path[i+1][0].second) / 72.0);
    propVec.append(propList);
    propList.clear();
  }

  m_painter->drawPath(propVec);
}

/* vim:set shiftwidth=2 softtabstop=2 expandtab: */
