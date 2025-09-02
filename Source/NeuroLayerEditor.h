/*
 ------------------------------------------------------------------

 This file is part of the Open Ephys GUI
 Copyright (C) 2025 VIB Haesler lab
 Developed by Marine Guyot - CodingResearcher

 ------------------------------------------------------------------

 This program is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program.  If not, see <http://www.gnu.org/licenses/>.

 */

#ifndef NEUROLAYEREDITOR_H_DEFINED
#define NEUROLAYEREDITOR_H_DEFINED

#include <EditorHeaders.h>

#include "NeuroLayer.h"

class NeuroLayerEditor : public GenericEditor
{
public:
    /** The class constructor, used to initialize any members. */
    NeuroLayerEditor (GenericProcessor* parentNode, NeuroLayer* thread);

    /** The class destructor, used to deallocate memory */
    ~NeuroLayerEditor() {}

private:

    /** A pointer to the underlying NeuroLayer */
    NeuroLayer* thread;
};

#endif
