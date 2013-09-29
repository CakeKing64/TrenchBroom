/*
 Copyright (C) 2010-2013 Kristian Duske
 
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

#include "AddRemoveObjectsCommand.h"

#include "CollectionUtils.h"
#include "Model/Entity.h"
#include "Model/Object.h"
#include "Model/ModelUtils.h"
#include "View/MapDocument.h"

namespace TrenchBroom {
    namespace Controller {
        const Command::CommandType AddRemoveObjectsCommand::Type = Command::freeType();

        AddRemoveObjectsCommand::~AddRemoveObjectsCommand() {
            VectorUtils::clearAndDelete(m_removedObjects);
        }

        AddRemoveObjectsCommand::Ptr AddRemoveObjectsCommand::addObjects(View::MapDocumentPtr document, const Model::ObjectParentList& objects) {
            return AddRemoveObjectsCommand::Ptr(new AddRemoveObjectsCommand(document, AAdd, objects));
        }

        AddRemoveObjectsCommand::Ptr AddRemoveObjectsCommand::removeObjects(View::MapDocumentPtr document, const Model::ObjectParentList& objects) {
            return AddRemoveObjectsCommand::Ptr(new AddRemoveObjectsCommand(document, ARemove, objects));
        }

        const Model::ObjectList& AddRemoveObjectsCommand::addedObjects() const {
            return m_addedObjects;
        }
        
        const Model::ObjectList& AddRemoveObjectsCommand::removedObjects() const {
            return m_removedObjects;
        }

        AddRemoveObjectsCommand::AddRemoveObjectsCommand(View::MapDocumentPtr document, const Action action, const Model::ObjectParentList& objects) :
        Command(Type, makeName(action, objects), true, true),
        m_document(document),
        m_action(action) {
            if (action == AAdd)
                m_objectsToAdd = objects;
            else
                m_objectsToRemove = objects;
        }

        String AddRemoveObjectsCommand::makeName(const Action action, const Model::ObjectParentList& objects) {
            StringStream name;
            name << (action == AAdd ? "Add " : "Remove ");
            name << (objects.size() == 1 ? "object" : "objects");
            return name.str();
        }

        bool AddRemoveObjectsCommand::doPerformDo() {
            m_addedObjects.clear();
            m_removedObjects.clear();
            
            if (m_action == AAdd)
                addObjects(m_objectsToAdd);
            else
                removeObjects(m_objectsToRemove);
            std::swap(m_objectsToAdd, m_objectsToRemove);
            return true;
        }
        
        bool AddRemoveObjectsCommand::doPerformUndo() {
            m_addedObjects.clear();
            m_removedObjects.clear();
            
            if (m_action == AAdd)
                removeObjects(m_objectsToRemove);
            else
                addObjects(m_objectsToAdd);
            std::swap(m_objectsToAdd, m_objectsToRemove);
            return true;
        }

        struct AddObjectToDocument {
        private:
            View::MapDocumentPtr m_document;
            Model::ObjectList& m_addedObjects;
        public:
            AddObjectToDocument(View::MapDocumentPtr document, Model::ObjectList& addedObjects) :
            m_document(document),
            m_addedObjects(addedObjects) {}
            
            void operator()(const Model::ObjectParentPair& pair) {
                m_document->addObject(pair.object, pair.parent);
                m_addedObjects.push_back(pair.object);
            }
        };
        
        struct RemoveObjectFromDocument {
        private:
            View::MapDocumentPtr m_document;
            Model::ObjectList& m_removedObjects;
        public:
            RemoveObjectFromDocument(View::MapDocumentPtr document, Model::ObjectList& removedObjects) :
            m_document(document),
            m_removedObjects(removedObjects) {}
            
            void operator()(const Model::ObjectParentPair& pair) {
                m_document->removeObject(pair.object);
                m_removedObjects.push_back(pair.object);
            }
        };
        
        void AddRemoveObjectsCommand::addObjects(const Model::ObjectParentList& objects) {
            AddObjectToDocument addObjectToDocument(m_document, m_addedObjects);
            Model::each(objects.begin(), objects.end(), addObjectToDocument, Model::MatchAll());
        }
        
        void AddRemoveObjectsCommand::removeObjects(const Model::ObjectParentList& objects) {
            RemoveObjectFromDocument removeObjectFromDocument(m_document, m_removedObjects);
            Model::each(objects.begin(), objects.end(), removeObjectFromDocument, Model::MatchAll());
        }
    }
}
