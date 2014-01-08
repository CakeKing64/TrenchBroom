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

#include "SetEntityDefinitionFileCommand.h"

#include "Model/Entity.h"
#include "Model/EntityProperties.h"
#include "View/MapDocument.h"

namespace TrenchBroom {
    namespace Controller {
        const Command::CommandType SetEntityDefinitionFileCommand::Type = Command::freeType();

        SetEntityDefinitionFileCommand::Ptr SetEntityDefinitionFileCommand::setEntityDefinitionFile(View::MapDocumentWPtr document, const IO::Path& file) {
            return Ptr(new SetEntityDefinitionFileCommand(document, file));
        }

        SetEntityDefinitionFileCommand::SetEntityDefinitionFileCommand(View::MapDocumentWPtr document, const IO::Path& file) :
        Command(Type, "Set Entity Definition File", true, true),
        m_document(document),
        m_newFile(file),
        m_oldFile("") {}
        
        bool SetEntityDefinitionFileCommand::doPerformDo() {
            View::MapDocumentSPtr document = lock(m_document);
            Model::Entity* worldspawn = document->worldspawn();
            document->objectWillChangeNotifier(worldspawn);
            m_oldFile = document->entityDefinitionFile().path();
            worldspawn->addOrUpdateProperty(Model::PropertyKeys::EntityDefinitions, m_newFile.asString());
            document->objectDidChangeNotifier(worldspawn);
            document->entityDefinitionsDidChangeNotifier();
            return true;
        }
        
        bool SetEntityDefinitionFileCommand::doPerformUndo() {
            View::MapDocumentSPtr document = lock(m_document);
            Model::Entity* worldspawn = document->worldspawn();
            document->objectWillChangeNotifier(worldspawn);
            worldspawn->addOrUpdateProperty(Model::PropertyKeys::EntityDefinitions, m_oldFile.asString());
            document->objectDidChangeNotifier(worldspawn);
            document->entityDefinitionsDidChangeNotifier();
            return true;
        }
    }
}
