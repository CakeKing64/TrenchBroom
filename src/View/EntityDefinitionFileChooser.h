/*
 Copyright (C) 2010-2014 Kristian Duske
 
 This file is part of TrenchBroom.
 
 TrenchBroom is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.
 
 TrenchBroom is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.
 
 You should have received a copy of the GNU General Public License
 along with TrenchBroom. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __TrenchBroom__EntityDefinitionFileChooser__
#define __TrenchBroom__EntityDefinitionFileChooser__

#include "View/ViewTypes.h"

#include <wx/panel.h>

class wxButton;
class wxListBox;
class wxStaticText;

namespace TrenchBroom {
    namespace View {
        class EntityDefinitionFileChooser : public wxPanel {
        private:
            MapDocumentWPtr m_document;
            ControllerWPtr m_controller;
            
            wxListBox* m_builtin;
            wxStaticText* m_external;
            wxButton* m_chooseExternal;
        public:
            EntityDefinitionFileChooser(wxWindow* parent, MapDocumentWPtr document, ControllerWPtr controller);
            ~EntityDefinitionFileChooser();
            
            void OnBuiltinSelectionChanged(wxCommandEvent& event);
            void OnChooseExternalClicked(wxCommandEvent& event);
        private:
            void createGui();
            void bindEvents();
            
            void bindObservers();
            void unbindObservers();
            
            void documentWasNewed();
            void documentWasLoaded();
            void entityDefinitionsDidChange();
            
            void updateControls();
        };
    }
}

#endif /* defined(__TrenchBroom__EntityDefinitionFileChooser__) */
