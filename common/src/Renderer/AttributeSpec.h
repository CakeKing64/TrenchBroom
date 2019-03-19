/*
 Copyright (C) 2010-2017 Kristian Duske
 
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

#ifndef TrenchBroom_AttributeSpec_h
#define TrenchBroom_AttributeSpec_h

#include "Renderer/GL.h"

#include <vecmath/vec.h>

namespace TrenchBroom {
    namespace Renderer {
        typedef enum {
            AttributeType_User,
            AttributeType_Position,
            AttributeType_Normal,
            AttributeType_Color,
            AttributeType_TexCoord0,
            AttributeType_TexCoord1,
            AttributeType_TexCoord2,
            AttributeType_TexCoord3
        } AttributeType;
        
        template <AttributeType type, GLenum D, size_t S>
        class AttributeSpec {
        public:
            using DataType = typename GLType<D>::Type;
            using ElementType = vm::vec<DataType,S>;
            static const size_t Size = sizeof(DataType) * S;
            
            static void setup(const size_t index, const size_t stride, const size_t offset) {}
            static void cleanup(const size_t index) {}
        };
        
        template <GLenum D, size_t S>
        class AttributeSpec<AttributeType_User, D, S> {
        public:
            using DataType = typename GLType<D>::Type;
            using ElementType = vm::vec<DataType,S>;
            static const size_t Size = sizeof(DataType) * S;
            
            static void setup(const size_t index, const size_t stride, const size_t offset) {
                glAssert(glEnableVertexAttribArray(static_cast<GLuint>(index)));
                glAssert(glVertexAttribPointer(static_cast<GLuint>(index), static_cast<GLint>(S), D, 0, static_cast<GLsizei>(stride), reinterpret_cast<GLvoid*>(offset)));
            }
            
            static void cleanup(const size_t index) {
                glAssert(glDisableVertexAttribArray(static_cast<GLuint>(index)));
            }
        };
        
        template <GLenum D, size_t S>
        class AttributeSpec<AttributeType_Position, D, S> {
        public:
            using DataType = typename GLType<D>::Type;
            using ElementType = vm::vec<DataType,S>;
            static const size_t Size = sizeof(DataType) * S;
            
            static void setup(const size_t index, const size_t stride, const size_t offset) {
                glAssert(glEnableClientState(GL_VERTEX_ARRAY));
                glAssert(glVertexPointer(static_cast<GLint>(S), D, static_cast<GLsizei>(stride), reinterpret_cast<GLvoid*>(offset)));
            }
            
            static void cleanup(const size_t index) {
                glAssert(glDisableClientState(GL_VERTEX_ARRAY));
            }
        };
        
        template <GLenum D, const size_t S>
        class AttributeSpec<AttributeType_Normal, D, S> {
        public:
            using DataType = typename GLType<D>::Type;
            using ElementType = vm::vec<DataType,S>;
            static const size_t Size = sizeof(DataType) * S;
            
            static void setup(const size_t index, const size_t stride, const size_t offset) {
                assert(S == 3);
                glAssert(glEnableClientState(GL_NORMAL_ARRAY));
                glAssert(glNormalPointer(D, static_cast<GLsizei>(stride), reinterpret_cast<GLvoid*>(offset)));
            }
            
            static void cleanup(const size_t index) {
                glAssert(glDisableClientState(GL_NORMAL_ARRAY));
            }
        };
        
        template <GLenum D, size_t S>
        class AttributeSpec<AttributeType_Color, D, S> {
        public:
            using DataType = typename GLType<D>::Type;
            using ElementType = vm::vec<DataType,S>;
            static const size_t Size = sizeof(DataType) * S;
            
            static void setup(const size_t index, const size_t stride, const size_t offset) {
                glAssert(glEnableClientState(GL_COLOR_ARRAY));
                glAssert(glColorPointer(static_cast<GLint>(S), D, static_cast<GLsizei>(stride), reinterpret_cast<GLvoid*>(offset)));
            }
            
            static void cleanup(const size_t index) {
                glAssert(glDisableClientState(GL_COLOR_ARRAY));
            }
        };
        
        template <GLenum D, size_t S>
        class AttributeSpec<AttributeType_TexCoord0, D, S> {
        public:
            using DataType = typename GLType<D>::Type;
            using ElementType = vm::vec<DataType,S>;
            static const size_t Size = sizeof(DataType) * S;
            
            static void setup(const size_t index, const size_t stride, const size_t offset) {
                glAssert(glClientActiveTexture(GL_TEXTURE0));
                glAssert(glEnableClientState(GL_TEXTURE_COORD_ARRAY));
                glAssert(glTexCoordPointer(static_cast<GLint>(S), D, static_cast<GLsizei>(stride), reinterpret_cast<GLvoid*>(offset)));
            }
            
            static void cleanup(const size_t index) {
                glAssert(glClientActiveTexture(GL_TEXTURE0));
                glAssert(glDisableClientState(GL_TEXTURE_COORD_ARRAY));
            }
        };
        
        template <GLenum D, size_t S>
        class AttributeSpec<AttributeType_TexCoord1, D, S> {
        public:
            using DataType = typename GLType<D>::Type;
            using ElementType = vm::vec<DataType,S>;
            static const size_t Size = sizeof(DataType) * S;
            
            static void setup(const size_t index, const size_t stride, const size_t offset) {
                glAssert(glClientActiveTexture(GL_TEXTURE1));
                glAssert(glEnableClientState(GL_TEXTURE_COORD_ARRAY));
                glAssert(glTexCoordPointer(static_cast<GLint>(S), D, static_cast<GLsizei>(stride), reinterpret_cast<GLvoid*>(offset)));
            }
            
            static void cleanup(const size_t index) {
                glAssert(glClientActiveTexture(GL_TEXTURE1));
                glAssert(glDisableClientState(GL_TEXTURE_COORD_ARRAY));
                glAssert(glClientActiveTexture(GL_TEXTURE0));
            }
        };
        
        template <GLenum D, size_t S>
        class AttributeSpec<AttributeType_TexCoord2, D, S> {
        public:
            using DataType = typename GLType<D>::Type;
            using ElementType = vm::vec<DataType,S>;
            static const size_t Size = sizeof(DataType) * S;

            static void setup(const size_t index, const size_t stride, const size_t offset) {
                glAssert(glClientActiveTexture(GL_TEXTURE2));
                glAssert(glEnableClientState(GL_TEXTURE_COORD_ARRAY));
                glAssert(glTexCoordPointer(static_cast<GLint>(S), D, static_cast<GLsizei>(stride), reinterpret_cast<GLvoid*>(offset)));
            }
            
            static void cleanup(const size_t index) {
                glAssert(glClientActiveTexture(GL_TEXTURE2));
                glAssert(glDisableClientState(GL_TEXTURE_COORD_ARRAY));
                glAssert(glClientActiveTexture(GL_TEXTURE0));
            }
        };
        
        template <GLenum D, size_t S>
        class AttributeSpec<AttributeType_TexCoord3, D, S> {
        public:
            using DataType = typename GLType<D>::Type;
            using ElementType = vm::vec<DataType,S>;
            static const size_t Size = sizeof(DataType) * S;
            
            static void setup(const size_t index, const size_t stride, const size_t offset) {
                glAssert(glClientActiveTexture(GL_TEXTURE3));
                glAssert(glEnableClientState(GL_TEXTURE_COORD_ARRAY));
                glAssert(glTexCoordPointer(static_cast<GLint>(S), D, static_cast<GLsizei>(stride), reinterpret_cast<GLvoid*>(offset)));
            }
            
            static void cleanup(const size_t index) {
                glAssert(glClientActiveTexture(GL_TEXTURE3));
                glAssert(glDisableClientState(GL_TEXTURE_COORD_ARRAY));
                glAssert(glClientActiveTexture(GL_TEXTURE0));
            }
        };
        
        namespace AttributeSpecs {
            using P2 = AttributeSpec<AttributeType_Position, GL_FLOAT, 2>;
            using P3 = AttributeSpec<AttributeType_Position, GL_FLOAT, 3>;
            using N = AttributeSpec<AttributeType_Normal, GL_FLOAT, 3>;
            using T02 = AttributeSpec<AttributeType_TexCoord0, GL_FLOAT, 2>;
            using T12 = AttributeSpec<AttributeType_TexCoord1, GL_FLOAT, 2>;
            using C4 = AttributeSpec<AttributeType_Color, GL_FLOAT, 4>;
        }
    }
}

#endif